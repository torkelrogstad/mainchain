// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef NEWSTABLEMODEL_H
#define NEWSTABLEMODEL_H

#include <uint256.h>

#include <QAbstractTableModel>
#include <QList>

class CBlockIndex;
class ClientModel;
class OPReturnData;

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

struct NewsTableObject
{
    int nHeight;
    int nTime;
    std::string decode;
    std::string fees;
};

enum NewsFilters
{
    COIN_NEWS_ALL = 0,
    COIN_NEWS_TOKYO_DAY = 1,
    COIN_NEWS_US_DAY = 2
};

static const size_t NEWS_HEADLINE_CHARS = 64;

class NewsTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit NewsTableModel(QObject *parent = 0);
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;

    void setClientModel(ClientModel *model);
    void setFilter(size_t nFilter);

    enum RoleIndex {
        NewsRole = Qt::UserRole,
    };

public Q_SLOTS:
    void numBlocksChanged();

private:
    QList<QVariant> model;

    ClientModel *clientModel = nullptr;

    void UpdateModel();
    void SortByFees(std::vector<NewsTableObject>& vNews);

    size_t nFilter;
};

#endif // NEWSBLOCKTABLEMODEL_H
