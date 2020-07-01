// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TRANSACTIONREPLAYDIALOG_H
#define TRANSACTIONREPLAYDIALOG_H

#include <QDialog>

class ClientModel;
class CoinSplitConfirmationDialog;
class PlatformStyle;
class WalletModel;

enum
{
    COLUMN_REPLAY = 0,
    COLUMN_AMOUNT,
    COLUMN_ADDRESS,
    COLUMN_DATE,
    COLUMN_TXHASH,
    COLUMN_VOUT_INDEX,
    COLUMN_CONFIRMATIONS,
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
    void setClientModel(ClientModel *model);
    void SetPlatformStyle(const PlatformStyle* style);

private Q_SLOTS:
    void on_pushButtonCheckReplay_clicked();
    void on_pushButtonSplitCoins_clicked();
    void Update();

private:
    Ui::TransactionReplayDialog *ui;

    WalletModel* walletModel = nullptr;
    ClientModel* clientModel= nullptr;

    const PlatformStyle* platformStyle;

    CoinSplitConfirmationDialog* coinSplitConfirmationDialog;

    QIcon GetReplayIcon(int nReplayStatus) const;
};

QString FormatReplayStatus(int nReplayStatus);

#endif // TRANSACTIONREPLAYDIALOG_H

