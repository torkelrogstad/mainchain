// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/latestblocktablemodel.h>

#include <chain.h>
#include <validation.h>

#include <qt/clientmodel.h>

#include <QDateTime>
#include <QIcon>
#include <QMetaType>
#include <QTimer>
#include <QVariant>

Q_DECLARE_METATYPE(BlockTableObject)

LatestBlockTableModel::LatestBlockTableModel(QObject *parent) :
    QAbstractTableModel(parent)
{
}

int LatestBlockTableModel::rowCount(const QModelIndex & /*parent*/) const
{
    return model.size();
}

int LatestBlockTableModel::columnCount(const QModelIndex & /*parent*/) const
{
    return 6;
}

QVariant LatestBlockTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return false;
    }

    int row = index.row();
    int col = index.column();

    if (!model.at(row).canConvert<BlockTableObject>())
        return QVariant();

    BlockTableObject object = model.at(row).value<BlockTableObject>();

    switch (role) {
    case Qt::DisplayRole:
    {
        // Height
        if (col == 0) {
            return object.nHeight;
        }
        // Hash
        if (col == 1) {
            return QString::fromStdString(object.hash.ToString()).left(32) + "...";
        }
        // hashPrev
        if (col == 2) {
            return QString::fromStdString(object.hashPrev.ToString()).left(32) + "...";
        }
        // hashMerkleRoot
        if (col == 3) {
            return QString::fromStdString(object.hashMerkleRoot.ToString()).left(32) + "...";
        }
        // Time
        if (col == 4) {
            return QDateTime::fromTime_t((int64_t)object.nTime).toString("dd MMMM yyyy hh:mm");
        }
        // nBits
        if (col == 5) {
            return QString::fromStdString(strprintf("%08x", object.nBits));
        }
    }
    case HashRole:
    {
        return QString::fromStdString(object.hash.ToString());
    }
    case Qt::TextAlignmentRole:
    {
        // Height
        if (col == 0) {
            return int(Qt::AlignHCenter | Qt::AlignVCenter);
        }
        // Hash
        if (col == 1) {
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }
        // hashPrev
        if (col == 2) {
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }
        // hashMerkleRoot
        if (col == 3) {
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }
        // Time
        if (col == 4) {
            return int(Qt::AlignRight | Qt::AlignVCenter);
        }
        // nBits
        if (col == 5) {
            return int(Qt::AlignRight | Qt::AlignVCenter);
        }
    }
    }
    return QVariant();
}

QVariant LatestBlockTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Horizontal) {
            switch (section) {
            case 0:
                return QString("Height");
            case 1:
                return QString("Hash");
            case 2:
                return QString("Hash Prev");
            case 3:
                return QString("Merkle Root");
            case 4:
                return QString("Time");
            case 5:
                return QString("Bits");
            }
        }
    }
    return QVariant();
}

void LatestBlockTableModel::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if(model)
    {
        numBlocksChanged();

        connect(model, SIGNAL(numBlocksChanged(int,QDateTime,double,bool)),
                this, SLOT(numBlocksChanged()));
    }
}

void LatestBlockTableModel::numBlocksChanged()
{
    UpdateModel();
}

void LatestBlockTableModel::UpdateModel()
{
    // Clear old data
    beginResetModel();
    model.clear();
    endResetModel();

    int nHeight = chainActive.Height() + 1;
    int nBlocksToDisplay = 10;
    if (nHeight < nBlocksToDisplay)
        nBlocksToDisplay = nHeight;

    beginInsertRows(QModelIndex(), model.size(), model.size() + nBlocksToDisplay - 1);
    for (int i = 0; i < nBlocksToDisplay; i++) {
        CBlockIndex *index = chainActive[nHeight - i];

        // TODO add error message or something to table?
        if (!index) {
            continue;
        }

        BlockTableObject object;
        object.nHeight = nHeight - i;
        object.hash = index->GetBlockHash();
        object.hashMerkleRoot = index->hashMerkleRoot;

        uint256 hashPrev = uint256();
        if (index->pprev)
            hashPrev = index->pprev->GetBlockHash();

        object.hashPrev = hashPrev;
        object.nTime = index->nTime;
        object.nBits = index->nBits;

        model.append(QVariant::fromValue(object));
    }
    endInsertRows();
}

CBlockIndex* LatestBlockTableModel::GetBlockIndex(const uint256& hash) const
{
    if (!mapBlockIndex.count(hash)) {
        return nullptr;
    }

    return chainActive[mapBlockIndex[hash]->nHeight];
}

CBlockIndex* LatestBlockTableModel::GetBlockIndex(int nHeight) const
{
    return chainActive[nHeight];
}

CBlockIndex* LatestBlockTableModel::GetTip() const
{
    return chainActive.Tip();
}
