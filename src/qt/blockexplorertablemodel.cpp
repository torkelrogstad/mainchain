#include <qt/blockexplorertablemodel.h>

#include <chain.h>
#include <validation.h>

#include <qt/clientmodel.h>

#include <QIcon>
#include <QMetaType>
#include <QTimer>
#include <QVariant>


Q_DECLARE_METATYPE(BlockExplorerTableObject)

BlockExplorerTableModel::BlockExplorerTableModel(QObject *parent) :
    QAbstractTableModel(parent)
{
}

int BlockExplorerTableModel::rowCount(const QModelIndex & /*parent*/) const
{
    return 2;
}

int BlockExplorerTableModel::columnCount(const QModelIndex & /*parent*/) const
{
    return model.size();
}

QVariant BlockExplorerTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return false;
    }

    int row = index.row();
    int col = index.column();

    if (!model.at(col).canConvert<BlockExplorerTableObject>())
        return QVariant();

    BlockExplorerTableObject object = model.at(col).value<BlockExplorerTableObject>();

    switch (role) {
    case Qt::DisplayRole:
    {
        // Height
        if (row == 0) {
            return object.nHeight;
        }
        // Hash
        if (row == 1) {
            return QString::fromStdString(object.hash.ToString());
        }
    }
    case HeightRole:
    {
        return object.nHeight;
    }
    case HashRole:
    {
        return QString::fromStdString(object.hash.ToString());
    }
    case Qt::TextAlignmentRole:
    {
        // Height
        if (row == 0) {
            return int(Qt::AlignRight | Qt::AlignVCenter);
        }
        // Hash
        if (row == 1) {
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
    }
    return QVariant();
}

QVariant BlockExplorerTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Vertical) {
            switch (section) {
            case 0:
                return QString("Height");
            case 1:
                return QString("Hash");
            }
        }
    }
    return QVariant();
}

void BlockExplorerTableModel::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if(model)
    {
        numBlocksChanged();

        connect(model, SIGNAL(numBlocksChanged(int,QDateTime,double,bool)),
                this, SLOT(numBlocksChanged()));
    }
}

void BlockExplorerTableModel::numBlocksChanged()
{
    UpdateModel();
}

void BlockExplorerTableModel::UpdateModel()
{
    // Clear old data
    beginResetModel();
    model.clear();
    endResetModel();

    // TODO figure out range of blocks to display
    int nHeight = chainActive.Height() + 1;
    int nBlocksToDisplay = 10;
    if (nHeight < nBlocksToDisplay)
        nBlocksToDisplay = nHeight;

    beginInsertColumns(QModelIndex(), model.size(), model.size() + nBlocksToDisplay);
    for (int i = nBlocksToDisplay; i; i--) {
        CBlockIndex *index = chainActive[nHeight - i];

        // TODO add error message or something to table?
        if (!index) {
            continue;
        }

        BlockExplorerTableObject object;
        object.nHeight = nHeight - i;
        object.hash = index->GetBlockHash();

        model.append(QVariant::fromValue(object));
    }
    endInsertColumns();
}

CBlockIndex* BlockExplorerTableModel::GetBlockIndex(const uint256& hash) const
{
    if (!mapBlockIndex.count(hash)) {
        return nullptr;
    }

    return chainActive[mapBlockIndex[hash]->nHeight];
}

CBlockIndex* BlockExplorerTableModel::GetBlockIndex(int nHeight) const
{
    return chainActive[nHeight];
}

CBlockIndex* BlockExplorerTableModel::GetTip() const
{
    return chainActive.Tip();
}
