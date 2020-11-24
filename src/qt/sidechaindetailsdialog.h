// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SIDECHAINDETAILSDIALOG_H
#define SIDECHAINDETAILSDIALOG_H

#include <QDialog>

class Sidechain;

namespace Ui {
class SidechainDetailsDialog;
}

class SidechainDetailsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SidechainDetailsDialog(const Sidechain& sidechain, QWidget *parent = nullptr);
    ~SidechainDetailsDialog();

private:
    Ui::SidechainDetailsDialog *ui;
};

#endif // SIDECHAINDETAILSDIALOG_H
