// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blockindexdetailsdialog.h>
#include <qt/forms/ui_blockindexdetailsdialog.h>

#include <qt/merkletreedialog.h>
#include <qt/txdetails.h>

#include <QMessageBox>

#include <chain.h>
#include <chainparams.h>
#include <consensus/merkle.h>
#include <primitives/block.h>
#include <validation.h>

#include <sstream>

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

    cachedBlock = block;
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

    std::vector<uint256> vLeaf;
    for (const CTransactionRef& tx : cachedBlock.vtx) {
        vLeaf.push_back(tx->GetHash());
    }

    bool fMutated = false;
    std::string strTree = MerkleTreeString(vLeaf, fMutated);

    MerkleTreeDialog dialog;
    dialog.SetTreeString(strTree);
    dialog.exec();
}

// Copy of merkle tree calculator from consensus/merkle.cpp for GUI display
std::string MerkleTreeString(const std::vector<uint256>& vLeaf, bool& fMutated)
{
    if (vLeaf.size() == 0) {
        fMutated = false;
        return "";
    }
    fMutated = false;

    // x = level in tree, y = hash
    // Level 0 is the leaves, and the last level is merkle root
    std::vector<std::vector<uint256>> vTree;

    // count is the number of leaves processed so far.
    uint32_t count = 0;
    // inner is an array of eagerly computed subtree hashes, indexed by tree
    // level (0 being the leaves).
    // For example, when count is 25 (11001 in binary), inner[4] is the hash of
    // the first 16 leaves, inner[3] of the next 8 leaves, and inner[0] equal to
    // the last leaf. The other inner entries are undefined.
    uint256 inner[32];
    // Which position in inner is a hash that depends on the matching leaf.
    int matchlevel = -1;
    // Starting position in branch
    uint32_t branchpos = -1;
    // First process all leaves into 'inner' values.
    while (count < vLeaf.size()) {
        uint256 h = vLeaf[count];
        bool matchh = count == branchpos;
        count++;
        int level;
        // For each of the lower bits in count that are 0, do 1 step. Each
        // corresponds to an inner value that existed before processing the
        // current leaf, and each needs a hash to combine it.
        for (level = 0; !(count & (((uint32_t)1) << level)); level++) {
            fMutated |= (inner[level] == h);
            CHash256().Write(inner[level].begin(), 32).Write(h.begin(), 32).Finalize(h.begin());

            if (level >= (int)vTree.size())
                vTree.push_back(std::vector<uint256>());

            vTree[level].push_back(h);
        }
        // Store the resulting hash at inner position level.
        inner[level] = h;
        if (matchh) {
            matchlevel = level;
        }
        if (level >= (int)vTree.size())
            vTree.push_back(std::vector<uint256>());

        vTree[level].push_back(h);
    }
    // Do a final 'sweep' over the rightmost branch of the tree to process
    // odd levels, and reduce everything to a single top value.
    // Level is the level (counted from the bottom) up to which we've sweeped.
    int level = 0;
    // As long as bit number level in count is zero, skip it. It means there
    // is nothing left at this level.
    while (!(count & (((uint32_t)1) << level))) {
        level++;
    }
    uint256 h = inner[level];
    while (count != (((uint32_t)1) << level)) {
        // If we reach this point, h is an inner value that is not the top.
        // We combine it with itself (Bitcoin's special rule for odd levels in
        // the tree) to produce a higher level one.
        CHash256().Write(h.begin(), 32).Write(h.begin(), 32).Finalize(h.begin());
        // Increment count to the value it would have if two entries at this
        // level had existed.
        count += (((uint32_t)1) << level);
        level++;
        // And propagate the result upwards accordingly.
        while (!(count & (((uint32_t)1) << level))) {
            CHash256().Write(inner[level].begin(), 32).Write(h.begin(), 32).Finalize(h.begin());

            if (level >= (int)vTree.size())
                vTree.push_back(std::vector<uint256>());

            vTree[level].push_back(h);

            level++;
        }
    }

    // Add merkle root hash
    vTree.push_back(std::vector<uint256> { h });

    // Format results

    std::stringstream ss;

    int nTreeLevel = vTree.size() - 1;
    for (auto ritx = vTree.rbegin(); ritx != vTree.rend(); ritx++) {
        ss << "Level " << nTreeLevel;

        if (nTreeLevel == vTree.size() - 1)
            ss << " (Root):\n";
        else
        if (nTreeLevel == 0)
            ss << " (Leaves):\n";
        else
            ss << " :\n";

        for (const uint256& hash : *ritx) {
            ss << hash.ToString() << " ";
        }
        ss << "\n\n";

        nTreeLevel--;
    }

    return ss.str();
}

