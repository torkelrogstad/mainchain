// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COINSPLITCONFIRMATIONDIALOG_H
#define COINSPLITCONFIRMATIONDIALOG_H

#include <amount.h>
#include <uint256.h>
#include <script/standard.h>

#include <QDialog>
#include <QString>
#include <QWidget>

namespace Ui {
class CoinSplitConfirmationDialog;
}

class CoinSplitConfirmationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CoinSplitConfirmationDialog(QWidget *parent = 0);
    ~CoinSplitConfirmationDialog();

    bool GetConfirmed();
    void SetInfo(const CAmount& amountIn, QString txidIn, QString addressIn, int indexIn);

public Q_SLOTS:
    void on_buttonBox_accepted();

    void on_buttonBox_rejected();

private:
    Ui::CoinSplitConfirmationDialog *ui;

    CAmount amount;
    uint256 txid;
    int index;
    std::string strNewAddress;

    void Reset();

    bool fConfirmed = false;
};

#endif // COINSPLITCONFIRMATIONDIALOG_H
