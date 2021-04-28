// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/txdetails.h>
#include <qt/forms/ui_txdetails.h>

#include <core_io.h>
#include <primitives/transaction.h>

#include <QApplication>
#include <QClipboard>
#include <QMessageBox>

TxDetails::TxDetails(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TxDetails)
{
    ui->setupUi(this);

    strHex = "";
    strTx = "";
}

TxDetails::~TxDetails()
{
    delete ui;
}

void TxDetails::on_pushButtonCopyHex_clicked()
{
    QApplication::clipboard()->setText(QString::fromStdString(strHex));
}

void TxDetails::on_pushButtonClose_clicked()
{
    this->close();
}

void  TxDetails::SetTransaction(const CMutableTransaction& mtx)
{
    // Get & set the hex
    strHex = EncodeHexTx(mtx);

    // Get & set the JSON
    strTx = CTransaction(mtx).ToString();

    // Display
    ui->textBrowserTx->setText(QString::fromStdString(strTx));
    ui->textBrowserHex->setText(QString::fromStdString(strHex));
}
