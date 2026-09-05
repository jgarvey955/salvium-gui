// Copyright (c) 2024, Salvium (author: SRCG)
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

#include "YieldInfo.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>


YieldInfo::Status YieldInfo::status() const
{
    return static_cast<Status>(m_pYI->status());
}

QString YieldInfo::errorString() const
{
    return QString::fromStdString(m_pYI->errorString());
}

bool YieldInfo::update()
{
    return m_pYI->update();
}

quint64 YieldInfo::burnt() const
{
    return m_pYI->burnt();
}

quint64 YieldInfo::locked() const
{
    return m_pYI->locked();
}

quint64 YieldInfo::supply() const
{
    return m_pYI->supply();
}

quint64 YieldInfo::yield() const
{
    return m_pYI->yield();
}

quint64 YieldInfo::yield_per_stake() const
{
    return m_pYI->yield_per_stake();
}

quint64 YieldInfo::total_accrued_from_past_completions() const { return m_pYI->total_accrued_from_past_completions(); }
quint64 YieldInfo::currently_staked() const { return m_pYI->currently_staked(); }
quint64 YieldInfo::accrued_from_current_stake() const { return m_pYI->accrued_from_current_stake(); }
quint64 YieldInfo::blockchain_height() const { return m_pYI->blockchain_height(); }
quint64 YieldInfo::stake_lock_period() const { return m_pYI->stake_lock_period(); }

QString YieldInfo::burntFormatted() const { return QString::fromStdString(Monero::Wallet::displayAmount(m_pYI->burnt())); }
QString YieldInfo::lockedFormatted() const { return QString::fromStdString(Monero::Wallet::displayAmount(m_pYI->locked())); }
QString YieldInfo::yieldFormatted() const { return QString::fromStdString(Monero::Wallet::displayAmount(m_pYI->yield())); }
QString YieldInfo::completedYieldFormatted() const { return QString::fromStdString(Monero::Wallet::displayAmount(m_pYI->total_accrued_from_past_completions())); }
QString YieldInfo::stakedFormatted() const { return QString::fromStdString(Monero::Wallet::displayAmount(m_pYI->currently_staked())); }
QString YieldInfo::activeYieldFormatted() const { return QString::fromStdString(Monero::Wallet::displayAmount(m_pYI->accrued_from_current_stake())); }

QString YieldInfo::period() const
{
  // Take the number of entries and convert to a human-readable period
  return m_pYI->period().c_str();
}

QString YieldInfo::payouts() const
{
  std::vector<std::tuple<size_t, std::string, std::string, uint64_t, uint64_t>> raw_payouts = m_pYI->payouts();
  QJsonArray result;
  for (auto &rp : raw_payouts) {
    size_t height;
    std::string txid;
    std::string asset_type;
    uint64_t burnt;
    uint64_t yield;
    std::tie(height, txid, asset_type, burnt, yield) = rp;
    QJsonObject payout;
    payout.insert("blockheight", static_cast<qint64>(height));
    payout.insert("hash", QString::fromStdString(txid));
    payout.insert("asset_type", QString::fromStdString(asset_type));
    // JSON/JavaScript numbers cannot represent all atomic amounts exactly.
    payout.insert("burnt", QString::number(burnt));
    payout.insert("yield", QString::number(yield));
    payout.insert("burntFormatted", QString::fromStdString(Monero::Wallet::displayAmount(burnt)));
    payout.insert("yieldFormatted", QString::fromStdString(Monero::Wallet::displayAmount(yield)));
    result.append(payout);
  }
  return QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact));
}

YieldInfo::~YieldInfo() = default;

YieldInfo::YieldInfo(Monero::YieldInfo *pt, QObject *parent)
    : QObject(parent), m_pYI(pt)
{

}
