// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/mempooltablemodel.h>

#include <qt/drivenetunits.h>

#include <txmempool.h>
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
    return 1;
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
        if (col == 1) {
            return QString::fromStdString(object.txid.ToString());
        }
    }
    case Qt::TextAlignmentRole:
    {
        // txid
        if (col == 0) {
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }
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
