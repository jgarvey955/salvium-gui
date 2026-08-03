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
    Q_PROPERTY(QString period READ period)
    Q_PROPERTY(QString payouts READ payouts)

public:
    enum Status {
        Status_Ok       = Monero::PendingTransaction::Status_Ok,
        Status_Error    = Monero::PendingTransaction::Status_Error
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
    QString period() const;
    QString payouts() const;

private:
    explicit YieldInfo(Monero::YieldInfo * yi, QObject *parent = 0);

private:
    friend class Wallet;
    Monero::YieldInfo * m_pYI;
};

#endif // YIELDINFO_H
