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

#include "AddressBookModel.h"
#include "AddressBook.h"
#include "Wallet.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"
#include <QDebug>
#include <QHash>
#include <wallet/api/wallet2_api.h>

AddressBookModel::AddressBookModel(Wallet *parent, AddressBook *addressBook)
    : QAbstractListModel(parent) , m_addressBook(addressBook), m_wallet(parent)
{
    connect(m_addressBook,SIGNAL(refreshStarted()),this,SLOT(startReset()));
    connect(m_addressBook,SIGNAL(refreshFinished()),this,SLOT(endReset()));
    connect(m_wallet, &Wallet::isCarrotChanged, this, [this]() {
        if (rowCount() > 0) {
            emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {AddressBookAddressRole});
        }
    });

}

void AddressBookModel::startReset(){
    beginResetModel();
}
void AddressBookModel::endReset(){
    endResetModel();
}

int AddressBookModel::rowCount(const QModelIndex &) const
{
    return m_addressBook->count();
}

QVariant AddressBookModel::data(const QModelIndex &index, int role) const
{
    QVariant result;
    const int rowNumber = index.row();

    bool found = m_addressBook->getRow(rowNumber, [this, &result, &role, rowNumber](const Monero::AddressBookRow &row) {
        switch (role) {
        case AddressBookAddressRole: {
            result = displayAddress(rowNumber);
            break;
        }
        case AddressBookDescriptionRole:
            result = QString::fromStdString(row.getDescription());
            break;
        case AddressBookPaymentIdRole:
            result = QString::fromStdString(row.getPaymentId());
            break;
        case AddressBookRowIdRole:
            // Qt doesnt support size_t overload type casting
            result.setValue(row.getRowId());
            break;
        default:
            qCritical() << "Unimplemented role " << role;
        }
    });
    if (!found) {
        qCritical("%s: internal error: invalid index %d", __FUNCTION__, index.row());
    }

    return result;
}

QString AddressBookModel::displayAddress(int row) const
{
    QString result;

    const bool found = m_addressBook->getRow(row, [this, &result](const Monero::AddressBookRow &addressBookRow) {
        const std::string address = addressBookRow.getAddress();
        cryptonote::address_parse_info info;
        if (m_wallet && cryptonote::get_account_address_from_str(info, static_cast<cryptonote::network_type>(m_wallet->nettype()), address)) {
            // Temporary GUI-side workaround: the wallet API can return address book
            // rows as CryptoNote-formatted strings after reload even when the user added
            // a Carrot address. Re-render the address using the wallet's current Carrot
            // display mode, matching the account address list. The proper long-term fix
            // belongs in Salvium wallet2 / wallet API, so address book rows are returned
            // already formatted for the active address scheme.
            info.address.m_is_carrot = m_wallet->isCarrot();
            if (info.has_payment_id) {
                result = QString::fromStdString(cryptonote::get_account_integrated_address_as_str(static_cast<cryptonote::network_type>(m_wallet->nettype()), info.address, info.payment_id));
            } else {
                result = QString::fromStdString(cryptonote::get_account_address_as_str(static_cast<cryptonote::network_type>(m_wallet->nettype()), info.is_subaddress, info.address));
            }
        } else {
            result = QString::fromStdString(address);
        }
    });

    return found ? result : QString();
}

bool AddressBookModel::deleteRow(int row)
{
    return m_addressBook->deleteRow(row);
}

QHash<int, QByteArray> AddressBookModel::roleNames() const
{
    QHash<int, QByteArray> roleNames = QAbstractListModel::roleNames();
    roleNames.insert(AddressBookAddressRole, "address");
    roleNames.insert(AddressBookPaymentIdRole, "paymentId");
    roleNames.insert(AddressBookDescriptionRole, "description");
    roleNames.insert(AddressBookRowIdRole, "rowId");


    return roleNames;
}
