// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKINDEXDETAILSDIALOG_H
#define BLOCKINDEXDETAILSDIALOG_H

#include <QDialog>

#include <primitives/block.h>
#include <primitives/transaction.h>
#include <uint256.h>

#include <vector>

class CBlockIndex;

namespace Ui {
class BlockIndexDetailsDialog;
}

class BlockIndexDetailsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BlockIndexDetailsDialog(QWidget *parent = nullptr);
    ~BlockIndexDetailsDialog();

    void SetBlockIndex(const CBlockIndex* index);

private Q_SLOTS:
    void on_pushButtonLoadTransactions_clicked();
    void on_tableWidgetTransactions_doubleClicked(const QModelIndex& i);

    void on_pushButtonMerkleTree_clicked();

private:
    Ui::BlockIndexDetailsDialog *ui;

    uint256 hashBlock;
    int nHeight;

    CBlock cachedBlock;

    const CBlockIndex* pBlockIndex = nullptr;
    std::vector<CTransactionRef> vtx;

};

std::string MerkleTreeString(const std::vector<uint256>& vLeaf, bool& fMutated);

#endif // BLOCKINDEXDETAILSDIALOG_H
