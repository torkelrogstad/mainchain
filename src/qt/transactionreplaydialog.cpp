// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/transactionreplaydialog.h>
#include <qt/forms/ui_transactionreplaydialog.h>

TransactionReplayDialog::TransactionReplayDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TransactionReplayDialog)
{
    ui->setupUi(this);
}

TransactionReplayDialog::~TransactionReplayDialog()
{
    delete ui;
}
