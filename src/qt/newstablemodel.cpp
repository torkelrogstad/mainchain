// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/newstablemodel.h>

#include <chain.h>
#include <txdb.h>
#include <validation.h>

#include <qt/clientmodel.h>

#include <QDateTime>
#include <QMetaType>
#include <QTimer>
#include <QVariant>

Q_DECLARE_METATYPE(NewsTableObject)

NewsTableModel::NewsTableModel(QObject *parent) :
    QAbstractTableModel(parent)
{
}

int NewsTableModel::rowCount(const QModelIndex & /*parent*/) const
{
    return model.size();
}

int NewsTableModel::columnCount(const QModelIndex & /*parent*/) const
{
    return 3;
}

QVariant NewsTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return false;
    }

    int row = index.row();
    int col = index.column();

    if (!model.at(row).canConvert<NewsTableObject>())
        return QVariant();

    NewsTableObject object = model.at(row).value<NewsTableObject>();

    switch (role) {
    case Qt::DisplayRole:
    {
        // Height
        if (col == 0) {
            return object.nHeight;
        }
        // Time
        if (col == 1) {
            return object.nTime;
        }
        // Decode
        if (col == 2) {
            return QString::fromStdString(object.decode);
        }
    }
    case Qt::TextAlignmentRole:
    {
        // Height
        if (col == 0) {
            return int(Qt::AlignHCenter | Qt::AlignVCenter);
        }
        // Time
        if (col == 1) {
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }
        // Decode
        if (col == 2) {
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
    }
    return QVariant();
}

QVariant NewsTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Horizontal) {
            switch (section) {
            case 0:
                return QString("Height");
            case 1:
                return QString("Time");
            case 2:
                return QString("Decode");
            }
        }
    }
    return QVariant();
}

void NewsTableModel::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if(model)
    {
        numBlocksChanged();

        connect(model, SIGNAL(numBlocksChanged(int,QDateTime,double,bool)),
                this, SLOT(numBlocksChanged()));
    }
}

void NewsTableModel::numBlocksChanged()
{
    UpdateModel();
}

// TODO use this to initialize model / resync after filter change and then
// have a function to append new data to the model.
void NewsTableModel::UpdateModel()
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

        // For each block load our cached OP_RETURN data
        std::vector<OPReturnData> vData;
        if (!popreturndb->GetBlockData(index->GetBlockHash(), vData)) {
            continue;
        }

        for (const OPReturnData& d : vData) {
            NewsTableObject object;
            object.nHeight = nHeight - i;
            object.nTime = index->nTime;

            std::string strDecode;
            for (const unsigned char& c : d.script)
                strDecode += c;

            object.decode = strDecode;

            model.append(QVariant::fromValue(object));
        }
    }
    endInsertRows();
}
