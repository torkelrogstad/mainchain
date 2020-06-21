// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/miningdialog.h>
#include <qt/forms/ui_miningdialog.h>

MiningDialog::MiningDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MiningDialog)
{
    ui->setupUi(this);
}

MiningDialog::~MiningDialog()
{
    delete ui;
}
