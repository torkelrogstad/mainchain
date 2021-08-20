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

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

struct NewsTableObject
{
    int nHeight;
    int nTime;
    std::string decode;
};

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

public Q_SLOTS:
    void numBlocksChanged();

private:
    QList<QVariant> model;

    ClientModel *clientModel = nullptr;

    void UpdateModel();
};

#endif // NEWSBLOCKTABLEMODEL_H
