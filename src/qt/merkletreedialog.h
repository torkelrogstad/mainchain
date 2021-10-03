// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MERKLETREEDIALOG_H
#define MERKLETREEDIALOG_H

#include <QDialog>

#include <uint256.h>

#include <string>
#include <vector>

namespace Ui {
class MerkleTreeDialog;
}

class MerkleTreeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MerkleTreeDialog(QWidget *parent = nullptr);
    ~MerkleTreeDialog();

    void SetTrees(const std::vector<uint256>& vLeaf, const std::vector<uint256>& vSegwitLeaf);

private:
    Ui::MerkleTreeDialog *ui;

    std::vector<uint256> vLeafCache;
    std::vector<uint256> vSegwitLeafCache;

public Q_SLOTS:
    void on_checkBoxRCB_stateChanged(int checked);
};

std::string MerkleTreeString(const std::vector<uint256>& vLeaf, const std::vector<uint256>& vSegwitLeaf);
std::string RCBTreeString(const std::vector<uint256>& vLeaf, const std::vector<uint256>& vSegwitLeaf);

#endif // MERKLETREEDIALOG_H
