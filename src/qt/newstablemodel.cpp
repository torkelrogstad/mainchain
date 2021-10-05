// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/newstablemodel.h>

#include <chain.h>
#include <txdb.h>
#include <utilmoneystr.h>
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
    nFilter = COIN_NEWS_ALL;
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
        // Fees
        if (col == 0) {
            return QString::fromStdString(object.fees);
        }
        // Time
        if (col == 1) {
            return QDateTime::fromTime_t((int64_t)object.nTime).toString("hh:mm MMMM dd");
        }
        // Decode
        if (col == 2) {
            if (object.decode.size() > NEWS_HEADLINE_CHARS)
                return QString::fromStdString(object.decode).left(NEWS_HEADLINE_CHARS) + "...";
            else
                return QString::fromStdString(object.decode);
        }
    }
    case Qt::TextAlignmentRole:
    {
        // Fees
        if (col == 0) {
            return int(Qt::AlignRight | Qt::AlignVCenter);
        }
        // Time
        if (col == 1) {
            return int(Qt::AlignRight | Qt::AlignVCenter);
        }
        // Decode
        if (col == 2) {
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
    case NewsRole:
    {
        return QString::fromStdString(object.decode);
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
                return QString("Fees");
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

// TODO append new data to the model instead of loading all every time
void NewsTableModel::UpdateModel()
{
    // Clear old data
    beginResetModel();
    model.clear();
    endResetModel();

    int nHeight = chainActive.Height();

    bool fCustomLoaded = false;
    CustomNewsType custom;

    int nDaysToDisplay = 0;
    if (nFilter == COIN_NEWS_ALL ||
            nFilter == COIN_NEWS_TOKYO_DAY ||
            nFilter == COIN_NEWS_US_DAY) {
        nDaysToDisplay = 1;
    } else {
        std::vector<CustomNewsType> vCustom;
        popreturndb->GetCustomTypes(vCustom);

        // Find the custom type we are filtering by
        // TODO figure out a better way to handle custom type lookup.
        size_t nBuiltInTypes = 3;
        size_t nCustomFilter = nFilter - nBuiltInTypes;

        if (nCustomFilter < vCustom.size()) {
            custom = vCustom[nCustomFilter];
            fCustomLoaded = true;
        }
        nDaysToDisplay = custom.nDays;
    }

    QDateTime tipTime = QDateTime::fromTime_t(chainActive.Tip()->GetBlockTime());
    QDateTime targetTime = tipTime.addDays(-nDaysToDisplay);

    // Loop backwards from chainTip until we reach target time or genesis block.
    std::vector<NewsTableObject> vNews;
    for (int i = nHeight; i > 1; i--) {
        CBlockIndex *index = chainActive[i];
        if (!index)
            break;

        // Have we gone back in time far enough?
        QDateTime indexTime = QDateTime::fromTime_t(index->GetBlockTime());

        if (indexTime <= targetTime)
            break;

        // For each block load our cached OP_RETURN data
        std::vector<OPReturnData> vData;
        if (!popreturndb->GetBlockData(index->GetBlockHash(), vData))
            continue;

        // Find the data we want to display
        for (const OPReturnData& d : vData) {
            bool fFound = false;
            if (nFilter == COIN_NEWS_TOKYO_DAY) {
                if (!d.script.IsNewsTokyoDay())
                    continue;
                fFound = true;
            }
            else
            if (nFilter == COIN_NEWS_US_DAY) {
                if (!d.script.IsNewsUSDay())
                    continue;
                fFound = true;
            }
            else
            if (nFilter == COIN_NEWS_ALL) {
                fFound = true;
            }
            else
            if (fCustomLoaded && d.script.size() >= 5 && custom.header.size() >= 5) {
                if (d.script[0] == custom.header[0] &&
                        d.script[1] == custom.header[1] &&
                        d.script[2] == custom.header[2] &&
                        d.script[3] == custom.header[3] &&
                        d.script[4] == custom.header[4])
                {
                            fFound = true;
                }
            }
            if (!fFound)
                continue;

            NewsTableObject object;
            object.nTime = index->nTime;

            // Copy chars from script, skipping non-message bytes
            std::string strDecode;
            size_t nStart = nFilter == COIN_NEWS_ALL ? 2 : 6;
            for (size_t i = nStart; i < d.script.size(); i++)
                strDecode += d.script[i];

            object.decode = strDecode;
            object.fees = FormatMoney(d.fees);

            vNews.push_back(object);
        }
    }

    // Sort by fees
    SortByFees(vNews);

    beginInsertRows(QModelIndex(), model.size(), model.size() + vNews.size() - 1);
    for (const NewsTableObject& o : vNews)
        model.append(QVariant::fromValue(o));
    endInsertRows();
}

void NewsTableModel::setFilter(size_t nFilterIn)
{
    nFilter = nFilterIn;
    UpdateModel();
}

struct CompareByFee
{
    bool operator()(const NewsTableObject& a, const NewsTableObject& b) const
    {
        return a.fees > b.fees;
    }
};

void NewsTableModel::SortByFees(std::vector<NewsTableObject>& vNews)
{
    std::sort(vNews.begin(), vNews.end(), CompareByFee());
}
