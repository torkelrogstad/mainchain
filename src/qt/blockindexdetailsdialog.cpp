// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blockindexdetailsdialog.h>
#include <qt/forms/ui_blockindexdetailsdialog.h>

#include <QMessageBox>

#include <chain.h>
#include <chainparams.h>
#include <primitives/block.h>
#include <validation.h>

BlockIndexDetailsDialog::BlockIndexDetailsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BlockIndexDetailsDialog)
{
    ui->setupUi(this);

    nHeight = 0;
    hashBlock = uint256();

    ui->tableWidgetTransactions->setColumnCount(2);
    ui->tableWidgetTransactions->setHorizontalHeaderLabels(QStringList() << "n" << "txid");
    ui->tableWidgetTransactions->verticalHeader()->setVisible(false);

        // Resize cells (in a backwards compatible way)
#if QT_VERSION < 0x050000
    ui->tableWidgetTransactions->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
#else
    ui->tableWidgetTransactions->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
#endif

    // Highlight entire row
    ui->tableWidgetTransactions->setSelectionBehavior(QAbstractItemView::SelectRows);
    // Select only one row
    ui->tableWidgetTransactions->setSelectionMode(QAbstractItemView::SingleSelection);
}

BlockIndexDetailsDialog::~BlockIndexDetailsDialog()
{
    delete ui;
}

void BlockIndexDetailsDialog::SetBlockIndex(const CBlockIndex* index)
{
    if (!index)
        return;

    nHeight = index->nHeight;
    hashBlock = index->GetBlockHash();

    // Show details on dialog

    // Height
    ui->labelHeight->setText(QString::number(nHeight));

    // Hash
    ui->labelHash->setText(QString::fromStdString(hashBlock.ToString()));

    // # Conf
    int nConf = -1;
    if (chainActive.Contains(index))
        nConf = chainActive.Height() - nHeight + 1;

    ui->labelConf->setText(QString::number(nConf));

    // Version
    ui->labelVersion->setText(QString::number(index->nVersion));

    // Version Hex
    ui->labelVersionHex->setText(QString::fromStdString(strprintf("%08x", index->nVersion)));

    // Merkle Root
    ui->labelMerkleRoot->setText(QString::fromStdString(index->hashMerkleRoot.ToString()));

    // Time
    ui->labelTime->setText(QString::number((int64_t)index->nTime));

    // Median Time
    ui->labelMedianTime->setText(QString::number((int64_t)index->GetMedianTimePast()));

    // Nonce
    ui->labelNonce->setText(QString::number((int64_t)index->nNonce));

    // Bits
    ui->labelBits->setText(QString::fromStdString(strprintf("%08x", index->nBits)));

    // TODO should we calculate the difficulty?
    // Diff
    //ui->labelDiff->setText(QString::number(GetDifficulty(chainActive, index)));

    // Chain Work
    ui->labelChainWork->setText(QString::fromStdString(index->nChainWork.ToString()));

    // prevBlockHash
    uint256 hashPrev = uint256();
    if (index->pprev)
        hashPrev = index->pprev->GetBlockHash();

    ui->labelPrevBlockHash->setText(QString::fromStdString(hashPrev.ToString()));

    // Next block hash
    uint256 hashNext = uint256();
    if (chainActive.Next(index))
        hashNext = chainActive.Next(index)->GetBlockHash();

    ui->labelNextBlockHash->setText(QString::fromStdString(hashNext.ToString()));

    pBlockIndex = index;

    ui->tableWidgetTransactions->setRowCount(0);
    vtx.clear();
}

void BlockIndexDetailsDialog::on_pushButtonLoadTransactions_clicked()
{
    if (!pBlockIndex) {
        // TODO error message
        return;
    }

    if (fHavePruned) {
        // TODO display error
        return;
    }

    // Double check that the block is in the chain and should be on disk
    if (!mapBlockIndex.count(pBlockIndex->GetBlockHash())) {
        // TODO display error
        return;
    }

    // Load block from disk

    CBlock block;
    if (!ReadBlockFromDisk(block, pBlockIndex, Params().GetConsensus())) {
        // TODO display error
        return;
    }

    vtx = block.vtx;

    ui->tableWidgetTransactions->setRowCount(0);

    // Add block's transactions to the table
    int nRow = 0;
    for (const CTransactionRef& tx : vtx) {
        ui->tableWidgetTransactions->insertRow(nRow);

        // txn number
        QTableWidgetItem *itemN = new QTableWidgetItem();
        itemN->setText(QString::number(nRow));
        itemN->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        itemN->setFlags(itemN->flags() & ~Qt::ItemIsEditable);
        ui->tableWidgetTransactions->setItem(nRow, 0, itemN);

        // tx hash
        QTableWidgetItem *itemHash = new QTableWidgetItem();
        itemHash->setText(QString::fromStdString(tx->GetHash().ToString()));
        itemHash->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        itemHash->setFlags(itemHash->flags() & ~Qt::ItemIsEditable);
        ui->tableWidgetTransactions->setItem(nRow, 1, itemHash);

        nRow++;
    }
}

void BlockIndexDetailsDialog::on_tableWidgetTransactions_doubleClicked(const QModelIndex& i)
{
    if ((unsigned int)i.row() >= vtx.size())
        return;
}
