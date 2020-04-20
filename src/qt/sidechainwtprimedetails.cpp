// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/sidechainwtprimedetails.h>
#include <qt/forms/ui_sidechainwtprimedetails.h>

#include <core_io.h>
#include <primitives/transaction.h>

#include <QApplication>
#include <QClipboard>
#include <QMessageBox>

SidechainWTPrimeDetails::SidechainWTPrimeDetails(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SidechainWTPrimeDetails)
{
    ui->setupUi(this);

    strHex = "";
    strTx = "";
}

SidechainWTPrimeDetails::~SidechainWTPrimeDetails()
{
    delete ui;
}

void SidechainWTPrimeDetails::on_pushButtonCopyHex_clicked()
{
    QApplication::clipboard()->setText(QString::fromStdString(strHex));
}

void SidechainWTPrimeDetails::on_pushButtonClose_clicked()
{
    this->close();
}

void  SidechainWTPrimeDetails::SetTransaction(const CMutableTransaction& mtx)
{
    // Get & set the hex
    strHex = EncodeHexTx(mtx);

    // Get & set the JSON
    strTx = CTransaction(mtx).ToString();

    // Display
    ui->textBrowserTx->setText(QString::fromStdString(strTx));
    ui->textBrowserHex->setText(QString::fromStdString(strHex));
}
