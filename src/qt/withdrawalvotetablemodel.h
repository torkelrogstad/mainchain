#ifndef WITHDRAWALVOTETABLEMODEL_H
#define WITHDRAWALVOTETABLEMODEL_H

#include <uint256.h>

#include <QAbstractTableModel>
#include <QList>

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

struct WithdrawalVoteTableObject
{
    unsigned int nSidechain;
    QString hash;
    char vote;
};

class WithdrawalVoteTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit WithdrawalVoteTableModel(QObject *parent = 0);
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;

    bool GetWithdrawalInfoAtRow(int row, uint256& hash, unsigned int& nSidechain) const;

public Q_SLOTS:
    void UpdateModel();

private:
    QList<QVariant> model;
    QTimer *pollTimer;
};

#endif // WITHDRAWALVOTETABLEMODEL_H
