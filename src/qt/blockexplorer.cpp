// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blockexplorer.h>
#include <qt/forms/ui_blockexplorer.h>

#include <QDateTime>
#include <QMessageBox>
#include <QString>

#include <qt/blockexplorertablemodel.h>
#include <qt/blockindexdetailsdialog.h>
#include <qt/clientmodel.h>
#include <qt/platformstyle.h>

#include <chain.h>

BlockExplorer::BlockExplorer(const PlatformStyle *_platformStyle, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BlockExplorer),
    platformStyle(_platformStyle)
{
    ui->setupUi(this);

    blockExplorerModel = new BlockExplorerTableModel(this);
    blockIndexDialog = new BlockIndexDetailsDialog(this);

    ui->tableViewBlocks->setModel(blockExplorerModel);
}

BlockExplorer::~BlockExplorer()
{
    delete ui;
}

void BlockExplorer::on_pushButtonRefresh_clicked()
{

}

void BlockExplorer::numBlocksChanged(int nHeight, const QDateTime& time)
{
    ui->labelNumBlocks->setText(QString::number(nHeight));
    ui->labelBlockTime->setText(time.toString("dd MMMM d yyyy hh:mm"));
}

void BlockExplorer::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if(model)
    {
        connect(model, SIGNAL(numBlocksChanged(int,QDateTime,double,bool)),
                this, SLOT(numBlocksChanged(int, QDateTime)));

        blockExplorerModel->setClientModel(model);
    }
}

void BlockExplorer::on_tableViewBlocks_doubleClicked(const QModelIndex& index)
{
    if (!index.isValid())
        return;

    QMessageBox messageBox;

    QString strHash = index.data(BlockExplorerTableModel::HashRole).toString();
    uint256 hash = uint256S(strHash.toStdString());

    // TODO update error message
    if (hash.IsNull()) {
        messageBox.setWindowTitle("Error - invalid block hash!");
        messageBox.setText("Block hash is null!\n");
        messageBox.exec();
        return;
    }

    // TODO update error message
    CBlockIndex* pBlockIndex = blockExplorerModel->GetBlockIndex(hash);
    if (!pBlockIndex) {
        messageBox.setWindowTitle("Error - couldn't locate block index!");
        messageBox.setText("Invalid block index!\n");
        messageBox.exec();
        return;
    }

    blockIndexDialog->SetBlockIndex(pBlockIndex);
    blockIndexDialog->show();
}
