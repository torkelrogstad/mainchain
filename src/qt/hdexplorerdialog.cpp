// Copyright (c) 2023 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/hdexplorerdialog.h>
#include <qt/forms/ui_hdexplorerdialog.h>

#include <qt/platformstyle.h>

HDExplorerDialog::HDExplorerDialog(const PlatformStyle *_platformStyle, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::HDExplorerDialog),
    platformStyle(_platformStyle)
{
    ui->setupUi(this);
}

HDExplorerDialog::~HDExplorerDialog()
{
    delete ui;
}

void HDExplorerDialog::on_lineEditWords_textChanged(QString text)
{
    std::string strWords = text.toStdString();
}
