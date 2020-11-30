// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef HASHCALCDIALOG_H
#define HASHCALCDIALOG_H

#include <QDialog>

class PlatformStyle;

namespace Ui {
class HashCalcDialog;
}

class HashCalcDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HashCalcDialog(const PlatformStyle *platformStyle, QWidget *parent = nullptr);
    ~HashCalcDialog();

private:
    Ui::HashCalcDialog *ui;

    const PlatformStyle *platformStyle;
};

#endif // HASHCALCDIALOG_H
