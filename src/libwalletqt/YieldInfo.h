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

#ifndef YIELDINFO_H
#define YIELDINFO_H

#include <QObject>
#include <QList>
#include <QString>
#include <QVariant>
#include <memory>

#include <wallet/api/wallet2_api.h>

class YieldInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Status status READ status)
    Q_PROPERTY(QString errorString READ errorString)
    Q_PROPERTY(quint64 burnt READ burnt)
    Q_PROPERTY(quint64 locked READ locked)
    Q_PROPERTY(quint64 supply READ supply)
    Q_PROPERTY(quint64 yield READ yield)
    Q_PROPERTY(quint64 yield_per_stake READ yield_per_stake)
    Q_PROPERTY(quint64 total_accrued_from_past_completions READ total_accrued_from_past_completions)
    Q_PROPERTY(quint64 currently_staked READ currently_staked)
    Q_PROPERTY(quint64 accrued_from_current_stake READ accrued_from_current_stake)
    Q_PROPERTY(quint64 blockchain_height READ blockchain_height)
    Q_PROPERTY(quint64 stake_lock_period READ stake_lock_period)
    Q_PROPERTY(QString period READ period)
    Q_PROPERTY(QString payouts READ payouts)
    Q_PROPERTY(QString burntFormatted READ burntFormatted)
    Q_PROPERTY(QString lockedFormatted READ lockedFormatted)
    Q_PROPERTY(QString yieldFormatted READ yieldFormatted)
    Q_PROPERTY(QString completedYieldFormatted READ completedYieldFormatted)
    Q_PROPERTY(QString stakedFormatted READ stakedFormatted)
    Q_PROPERTY(QString activeYieldFormatted READ activeYieldFormatted)

public:
    // Takes ownership of the core snapshot; the wallet owns this QObject.
    explicit YieldInfo(Monero::YieldInfo *yi, QObject *parent = nullptr);
    ~YieldInfo() override;
    enum Status {
        Status_Ok       = Monero::YieldInfo::Status_Ok,
        Status_Error    = Monero::YieldInfo::Status_Error
    };
    Q_ENUM(Status)

    Status status() const;
    QString errorString() const;
    Q_INVOKABLE bool update();
    quint64 burnt() const;
    quint64 locked() const;
    quint64 supply() const;
    quint64 yield() const;
    quint64 yield_per_stake() const;
    quint64 total_accrued_from_past_completions() const;
    quint64 currently_staked() const;
    quint64 accrued_from_current_stake() const;
    quint64 blockchain_height() const;
    quint64 stake_lock_period() const;
    QString period() const;
    QString payouts() const;
    QString burntFormatted() const;
    QString lockedFormatted() const;
    QString yieldFormatted() const;
    QString completedYieldFormatted() const;
    QString stakedFormatted() const;
    QString activeYieldFormatted() const;

private:
    friend class Wallet;
    std::unique_ptr<Monero::YieldInfo> m_pYI;
};

#endif // YIELDINFO_H
