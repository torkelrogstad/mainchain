// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/hashcalcdialog.h>
#include <qt/forms/ui_hashcalcdialog.h>

HashCalcDialog::HashCalcDialog(const PlatformStyle *_platformStyle, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::HashCalcDialog),
    platformStyle(_platformStyle)
{
    ui->setupUi(this);
}

HashCalcDialog::~HashCalcDialog()
{
    delete ui;
}
