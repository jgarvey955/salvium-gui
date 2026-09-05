// Copyright (c) 2014-2024, The Monero Project
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "PassphraseHelper.h"
#include <QMutexLocker>
#include <QDebug>
#include "qt/ScopeGuard.h"

void PassphraseHelper::onDevicePassphraseRequest(bool& on_device,
    const Monero::WalletListener::DevicePassphraseCallback& receive)
{
    qDebug() << __FUNCTION__;
    QMutexLocker locker(&m_mutex_pass);
    if (m_waiting || m_prompter == nullptr)
        throw std::runtime_error("Passphrase prompt unavailable");
    m_passphrase.clear();
    m_passphrase_on_device = true;
    m_passphrase_abort = false;
    m_ready = false;
    m_waiting = true;
    const auto cleanup = sg::make_scope_guard([this]() noexcept {
        m_passphrase.clear();
        m_waiting = false;
        m_ready = false;
    });

    // Prompt outside the lock so a synchronous response is safe as well.
    locker.unlock();
    try { m_prompter->onWalletPassphraseNeeded(on_device); }
    catch (...) { locker.relock(); throw; }
    locker.relock();
    while (!m_ready)
        m_cond_pass.wait(&m_mutex_pass);

    if (m_passphrase_abort)
    {
        throw std::runtime_error("Passphrase entry abort");
    }

    on_device = m_passphrase_on_device;
    if (!on_device)
        receive(m_passphrase.data(), m_passphrase.size());
}

void PassphraseHelper::onPassphraseEntered(const QString &passphrase, bool enter_on_device, bool entry_abort)
{
    qDebug() << __FUNCTION__;
    QMutexLocker locker(&m_mutex_pass);
    if (!m_waiting || m_ready)
        return;
    m_passphrase.clear();
    m_passphrase_abort = entry_abort;
    m_passphrase_on_device = enter_on_device;
    if (!entry_abort && !enter_on_device)
    {
      try
      {
        // Encode directly into wiping storage; toUtf8/toStdString would leave
        // a second, non-wiping allocation containing the same secret.
        m_passphrase.reserve(static_cast<std::size_t>(passphrase.size()) * 3);
        for (int i = 0; i < passphrase.size(); ++i)
        {
            uint code = passphrase.at(i).unicode();
            if (QChar::isHighSurrogate(code) && i + 1 < passphrase.size() &&
                passphrase.at(i + 1).isLowSurrogate())
            {
                const QChar high = passphrase.at(i);
                ++i;
                code = QChar::surrogateToUcs4(high, passphrase.at(i));
            }
            else if (QChar::isHighSurrogate(code) || QChar::isLowSurrogate(code))
                code = '?';
            if (code < 0x80) m_passphrase.push_back(static_cast<char>(code));
            else
            {
                if (code < 0x800) m_passphrase.push_back(static_cast<char>(0xc0 | (code >> 6)));
                else
                {
                    if (code < 0x10000) m_passphrase.push_back(static_cast<char>(0xe0 | (code >> 12)));
                    else
                    {
                        m_passphrase.push_back(static_cast<char>(0xf0 | (code >> 18)));
                        m_passphrase.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3f)));
                    }
                    m_passphrase.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
                }
                m_passphrase.push_back(static_cast<char>(0x80 | (code & 0x3f)));
            }
        }
      }
      catch (...)
      {
        m_passphrase.clear();
        m_passphrase_abort = true;
      }
    }
    m_ready = true;
    m_cond_pass.wakeAll();
}
