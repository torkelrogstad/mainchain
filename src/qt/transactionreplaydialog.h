// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TRANSACTIONREPLAYDIALOG_H
#define TRANSACTIONREPLAYDIALOG_H

#include <QDialog>

class WalletModel;

enum
{
    COLUMN_AMOUNT = 0,
    COLUMN_LABEL,
    COLUMN_ADDRESS,
    COLUMN_DATE,
    COLUMN_CONFIRMATIONS,
    COLUMN_TXHASH,
    COLUMN_VOUT_INDEX,
};

namespace Ui {
class TransactionReplayDialog;
}

class TransactionReplayDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TransactionReplayDialog(QWidget *parent = 0);
    ~TransactionReplayDialog();

    void SetWalletModel(WalletModel* model);

private Q_SLOTS:
    void on_pushButtonCheckReplay_clicked();
    void on_pushButtonSplitCoins_clicked();

private:
    Ui::TransactionReplayDialog *ui;

    WalletModel* walletModel;

    void Update();
};

#endif // TRANSACTIONREPLAYDIALOG_H

