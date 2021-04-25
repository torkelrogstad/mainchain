// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKINDEXDETAILSDIALOG_H
#define BLOCKINDEXDETAILSDIALOG_H

#include <QDialog>

#include <uint256.h>

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

private:
    Ui::BlockIndexDetailsDialog *ui;

    uint256 hashBlock;
    int nHeight;
};

#endif // BLOCKINDEXDETAILSDIALOG_H
