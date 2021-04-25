// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKEXPLORER_H
#define BLOCKEXPLORER_H

#include <QDialog>

class BlockExplorerTableModel;
class BlockIndexDetailsDialog;
class ClientModel;
class PlatformStyle;

QT_BEGIN_NAMESPACE
class QDateTime;
QT_END_NAMESPACE

namespace Ui {
class BlockExplorer;
}

class BlockExplorer : public QDialog
{
    Q_OBJECT

public:
    explicit BlockExplorer(const PlatformStyle *platformStyle, QWidget *parent = nullptr);
    ~BlockExplorer();

    void setClientModel(ClientModel *model);

private Q_SLOTS:
    void on_pushButtonRefresh_clicked();
    void numBlocksChanged(int nHeight, const QDateTime& time);
    void on_tableViewBlocks_doubleClicked(const QModelIndex& index);

private:
    Ui::BlockExplorer *ui;

    const PlatformStyle *platformStyle;
    ClientModel *clientModel = nullptr;

    BlockExplorerTableModel* blockExplorerModel = nullptr;
    BlockIndexDetailsDialog* blockIndexDialog = nullptr;
};

#endif // BLOCKEXPLORER_H
