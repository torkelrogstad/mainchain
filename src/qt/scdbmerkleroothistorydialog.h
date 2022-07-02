// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCDBMERKLEROOTHISTORYDIALOG_H
#define SCDBMERKLEROOTHISTORYDIALOG_H

#include <QDialog>

class ClientModel;
class PlatformStyle;

QT_BEGIN_NAMESPACE
class QTreeWidgetItem;
QT_END_NAMESPACE

namespace Ui {
class SCDBMerkleRootHistoryDialog;
}

class SCDBMerkleRootHistoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SCDBMerkleRootHistoryDialog(const PlatformStyle *platformStyle, QWidget *parent = nullptr);
    ~SCDBMerkleRootHistoryDialog();

    void setClientModel(ClientModel *model);
    void Update();

private:
    Ui::SCDBMerkleRootHistoryDialog *ui;

    const PlatformStyle *platformStyle;
    ClientModel *clientModel = nullptr;

    void AddTreeItem(int index, const QString& hashMT, const int nHeight, QTreeWidgetItem *item);

private Q_SLOTS:
    void numBlocksChanged();
};

#endif // SCDBMERKLEROOTHISTORYDIALOG_H
