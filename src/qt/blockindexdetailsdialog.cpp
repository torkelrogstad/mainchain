// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blockindexdetailsdialog.h>
#include <qt/forms/ui_blockindexdetailsdialog.h>

#include <qt/guiutil.h>
#include <qt/merkletreedialog.h>
#include <qt/txdetails.h>

#include <QMessageBox>

#include <chain.h>
#include <chainparams.h>
#include <streams.h>
#include <utilstrencodings.h>
#include <validation.h>

BlockIndexDetailsDialog::BlockIndexDetailsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BlockIndexDetailsDialog)
{
    ui->setupUi(this);

    nHeight = 0;
    hashBlock = uint256();
    cachedBlock.SetNull();

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

    merkleTreeDialog = new MerkleTreeDialog(this);
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
    cachedBlock.SetNull();
    ui->labelBlockInfo->setText("#Tx: ?    Block Size: ? (click \"Load Transactions\")");
    ui->pushButtonMerkleTree->setEnabled(false);

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

    merkleTreeDialog->close();
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

    cachedBlock = block;

    size_t nTx = cachedBlock.vtx.size();
    size_t nSize = GetSerializeSize(cachedBlock, SER_NETWORK, PROTOCOL_VERSION);

    QString strSize = "";
    if (nSize < 1000000)
        strSize = QString::number(nSize / 1000.0, 'f', 2) + " KB";
    else
        strSize = QString::number(nSize / 1000000.0, 'f', 2) + " MB";

    QString strInfo = "#Tx: " + QString::number(nTx);
    strInfo += " Block size: " + strSize;
    ui->labelBlockInfo->setText(strInfo);

    ui->pushButtonMerkleTree->setEnabled(true);

    // Extract and display witness commit hash
    int commitpos = GetWitnessCommitmentIndex(block);
    if (commitpos != -1) {
        // Get the commitment hash (witness merkle root + nonce) and reverse it
        std::vector<unsigned char> vch(&block.vtx[0]->vout[commitpos].scriptPubKey[6], &block.vtx[0]->vout[commitpos].scriptPubKey[38]);
        std::reverse(vch.begin(), vch.end());
        uint256 hash(vch);

        ui->labelWitnessHash->setText(QString::fromStdString(hash.ToString()));
    }
}

void BlockIndexDetailsDialog::on_tableWidgetTransactions_doubleClicked(const QModelIndex& i)
{
    unsigned int nTx = i.row();
    if (nTx >= vtx.size())
        return;

    TxDetails txDetailsDialog;
    txDetailsDialog.SetTransaction(CMutableTransaction(*vtx[nTx]));
    txDetailsDialog.exec();
}

void BlockIndexDetailsDialog::on_pushButtonMerkleTree_clicked()
{
    if (cachedBlock.IsNull())
        return;

    // Collect leaves for the merkle tree
    std::vector<uint256> vLeaf;
    for (const CTransactionRef& tx : cachedBlock.vtx) {
        vLeaf.push_back(tx->GetHash());
    }

    // Collect witness hashes for the segwit merkle tree
    std::vector<uint256> vSegwitLeaf;
    for (const CTransactionRef& tx : cachedBlock.vtx) {
        vSegwitLeaf.push_back(tx->GetWitnessHash());
    }

    // For the segwit merkle tree, the coinbase hash is made null
    if (vSegwitLeaf.size())
        vSegwitLeaf[0].SetNull();

    merkleTreeDialog->SetTrees(vLeaf, vSegwitLeaf);
    merkleTreeDialog->show();
}

void BlockIndexDetailsDialog::on_pushButtonCopyHeaderHex_clicked()
{
    if (!pBlockIndex)
        return;

    CBlockHeader header = pBlockIndex->GetBlockHeader();

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << header;

    GUIUtil::setClipboard(QString::fromStdString(HexStr(ss.str())));
}
