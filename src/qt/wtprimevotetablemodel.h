#ifndef WTPRIMEVOTETABLEMODEL_H
#define WTPRIMEVOTETABLEMODEL_H

#include <uint256.h>

#include <QAbstractTableModel>
#include <QList>

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

struct WTPrimeVoteTableObject
{
    unsigned int nSidechain;
    QString hash;
    char vote;
};

class WTPrimeVoteTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit WTPrimeVoteTableModel(QObject *parent = 0);
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;

    bool GetWTPrimeInfoAtRow(int row, uint256& hash, unsigned int& nSidechain) const;

public Q_SLOTS:
    void UpdateModel();

private:
    QList<QVariant> model;
    QTimer *pollTimer;
};

#endif // WTPRIMEVOTETABLEMODEL_H
