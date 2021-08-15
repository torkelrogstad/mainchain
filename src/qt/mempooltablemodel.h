// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MEMPOOLTABLEMODEL_H
#define MEMPOOLTABLEMODEL_H

#include <uint256.h>

#include <QAbstractTableModel>
#include <QList>

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

struct MemPoolTableObject
{
    uint256 txid;
};

class MemPoolTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit MemPoolTableModel(QObject *parent = 0);
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;

public Q_SLOTS:
    void memPoolSizeChanged(long nTx, size_t nBytes);

private:
    void updateModel();

    QList<QVariant> model;

    long nTx;
    size_t nBytes;
};

#endif // MEMPOOLTABLEMODEL_H
