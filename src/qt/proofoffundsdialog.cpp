// Copyright (c) 2023 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/proofoffundsdialog.h>
#include <qt/forms/ui_proofoffundsdialog.h>

ProofOfFundsDialog::ProofOfFundsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ProofOfFundsDialog)
{
    ui->setupUi(this);
}

ProofOfFundsDialog::~ProofOfFundsDialog()
{
    delete ui;
}

void ProofOfFundsDialog::on_pushButtonGenerate_clicked()
{

}

void ProofOfFundsDialog::on_pushButtonVerify_clicked()
{

}
