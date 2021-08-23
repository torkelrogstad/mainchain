// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/mempooltablemodel.h>

#include <qt/drivenetunits.h>

#include <primitives/transaction.h>
#include <txmempool.h>
#include <utilmoneystr.h>
#include <validation.h>

Q_DECLARE_METATYPE(MemPoolTableObject)

MemPoolTableModel::MemPoolTableModel(QObject *parent) :
    QAbstractTableModel(parent)
{
    nTx = 0;
    nBytes = 0;
}

int MemPoolTableModel::rowCount(const QModelIndex & /*parent*/) const
{
    return model.size();
}

int MemPoolTableModel::columnCount(const QModelIndex & /*parent*/) const
{
    return 5;
}

QVariant MemPoolTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return false;
    }

    int row = index.row();
    int col = index.column();

    if (!model.at(row).canConvert<MemPoolTableObject>())
        return QVariant();

    MemPoolTableObject object = model.at(row).value<MemPoolTableObject>();

    switch (role) {
    case Qt::DisplayRole:
    {
        // txid
        if (col == 0) {
            return QString::fromStdString(object.txid.ToString());
        }
        // Fee
        if (col == 1) {
            return BitcoinUnits::formatWithUnit(BitcoinUnit::BTC, object.fee, false, BitcoinUnits::separatorAlways);
        }
        // Fee Rate
        if (col == 2) {
            QString rate = BitcoinUnits::formatWithUnit(BitcoinUnit::BTC, object.feeRate.GetFeePerK(), false, BitcoinUnits::separatorAlways);
            rate += "/kB";
            return rate;
        }
        // Weight
        if (col == 3) {
            return QString::number(object.nWeight) + " wB";
        }
        // Value
        if (col == 4) {
            return BitcoinUnits::formatWithUnit(BitcoinUnit::BTC, object.value, false, BitcoinUnits::separatorAlways);
        }
    }
    case Qt::TextAlignmentRole:
    {
        // txid
        if (col == 0) {
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }
        // Fee
        if (col == 1) {
            return int(Qt::AlignRight | Qt::AlignVCenter);
        }
        // Fee rate
        if (col == 2) {
            return int(Qt::AlignRight | Qt::AlignVCenter);
        }
        // weight
        if (col == 3) {
            return int(Qt::AlignRight | Qt::AlignVCenter);
        }
        // Value
        if (col == 4) {
            return int(Qt::AlignRight | Qt::AlignVCenter);
        }
    }
    case HashRole:
    {
        return QString::fromStdString(object.txid.ToString());
    }
    }
    return QVariant();
}

QVariant MemPoolTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Horizontal) {
            switch (section) {
            case 0:
                return QString("TxID");
            case 1:
                return QString("Fee");
            case 2:
                return QString("Fee Rate");
            case 3:
                return QString("Weight");
            case 4:
                return QString("Value");
            }
        }
    }
    return QVariant();
}

void MemPoolTableModel::updateModel()
{
    // Clear old data
    beginResetModel();
    model.clear();
    endResetModel();

    // Get 6 most recent mempool entries
    std::vector<TxMempoolInfo> vInfo = mempool.InfoRecent(6);

    beginInsertRows(QModelIndex(), model.size(), model.size() + vInfo.size());
    for (const TxMempoolInfo& i : vInfo) {
        if (!i.tx)
            continue;

        MemPoolTableObject object;
        object.txid = i.tx->GetHash();
        object.fee = i.fee;
        object.feeRate = i.feeRate;
        object.nWeight = i.nTxWeight;
        object.value = i.tx->GetValueOut();

        model.append(QVariant::fromValue(object));
    }
    endInsertRows();
}

void MemPoolTableModel::memPoolSizeChanged(long nTxIn, size_t nBytesIn)
{
    if (nTxIn != nTx || nBytesIn != nBytes) {
        nTx = nTxIn;
        nBytes = nBytesIn;

        updateModel();
    }
}

bool MemPoolTableModel::GetTx(const uint256& txid, CTransactionRef& tx) const
{
    if (!mempool.exists(txid))
        return false;

    tx = mempool.get(txid);
    return true;
}
