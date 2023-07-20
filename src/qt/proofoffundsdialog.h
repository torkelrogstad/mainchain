// Copyright (c) 2023 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PROOFOFFUNDSDIALOG_H
#define PROOFOFFUNDSDIALOG_H

#include <QDialog>

namespace Ui {
class ProofOfFundsDialog;
}

class ProofOfFundsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProofOfFundsDialog(QWidget *parent = nullptr);
    ~ProofOfFundsDialog();

private:
    Ui::ProofOfFundsDialog *ui;

private Q_SLOTS:
    void on_pushButtonGenerate_clicked();
    void on_pushButtonVerify_clicked();

};

#endif // PROOFOFFUNDSDIALOG_H
