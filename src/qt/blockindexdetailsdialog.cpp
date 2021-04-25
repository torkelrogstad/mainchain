// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blockindexdetailsdialog.h>
#include <qt/forms/ui_blockindexdetailsdialog.h>

#include <chain.h>
#include <validation.h>

BlockIndexDetailsDialog::BlockIndexDetailsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BlockIndexDetailsDialog)
{
    ui->setupUi(this);

    nHeight = 0;
    hashBlock = uint256();
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
}
