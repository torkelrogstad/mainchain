// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MININGDIALOG_H
#define MININGDIALOG_H

#include <QDialog>

namespace Ui {
class MiningDialog;
}

class MiningDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MiningDialog(QWidget *parent = 0);
    ~MiningDialog();

public Q_SLOTS:
    void on_pushButtonAbandonBMM_clicked();

private:
    Ui::MiningDialog *ui;

    void AbandonFailedBMM();
};

#endif // MININGDIALOG_H

