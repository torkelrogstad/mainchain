// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/createnewsdialog.h>
#include <qt/forms/ui_createnewsdialog.h>

#include <amount.h>
#include <wallet/wallet.h>
#include <validation.h>

#include <qt/drivenetunits.h>
#include <qt/newstablemodel.h> // TODO move enum NewsFilters

#include <QMessageBox>

CreateNewsDialog::CreateNewsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CreateNewsDialog)
{
    ui->setupUi(this);
    ui->feeAmount->setValue(0);

    ui->comboBoxCategory->addItem("General OP_RETURN data");
    ui->comboBoxCategory->addItem("Tokyo daily news");
    ui->comboBoxCategory->addItem("US daily news");
}

CreateNewsDialog::~CreateNewsDialog()
{
    delete ui;
}

void CreateNewsDialog::on_pushButtonCreate_clicked()
{
    QMessageBox messageBox;

    const CAmount& nFee = ui->feeAmount->value();
    std::string strText = ui->plainTextEdit->toPlainText().toStdString();

    // Format strings for confirmation dialog
    QString strFee = BitcoinUnits::formatWithUnit(BitcoinUnit::BTC, nFee, false, BitcoinUnits::separatorAlways);

    // Show confirmation dialog
    // TODO

#ifdef ENABLE_WALLET
    if (vpwallets.empty()) {
        messageBox.setWindowTitle("Wallet Error!");
        messageBox.setText("No active wallets to create the transaction.");
        messageBox.exec();
        return;
    }

    if (vpwallets[0]->IsLocked()) {
        // Locked wallet message box
        messageBox.setWindowTitle("Wallet locked!");
        messageBox.setText("Wallet must be unlocked to create transactions.");
        messageBox.exec();
        return;
    }

    // Block until the wallet has been updated with the latest chain tip
    vpwallets[0]->BlockUntilSyncedToCurrentChain();

    // Create news OP_RETURN script
    CScript script;

    if (ui->comboBoxCategory->currentIndex() == COIN_NEWS_ALL) {
        script << OP_RETURN;
    }
    else
    if (ui->comboBoxCategory->currentIndex() == COIN_NEWS_TOKYO_DAY){
        script = GetNewsTokyoDailyHeader();
    }
    else
    if (ui->comboBoxCategory->currentIndex() == COIN_NEWS_US_DAY){
        script = GetNewsUSDailyHeader();
    }

    // TODO Should script include the pushdata size added by << operator?
    std::string strHex = HexStr(strText.begin(), strText.end());
    std::vector<unsigned char> vBytes = ParseHex(strHex);
    script << vBytes;

    CTransactionRef tx;
    std::string strFail = "";
    if (!vpwallets[0]->CreateOPReturnTransaction(tx, strFail, nFee, script))
    {
        messageBox.setWindowTitle("Creating transaction failed!");
        QString createError = "Error creating transaction!\n\n";
        createError += QString::fromStdString(strFail);
        messageBox.setText(createError);
        messageBox.exec();
        return;
    }

    // Success message box
    messageBox.setWindowTitle("Transaction created!");
    QString result = "txid: " + QString::fromStdString(tx->GetHash().ToString());
    result += "\n";
    messageBox.setText(result);
    messageBox.exec();
#endif
}

void CreateNewsDialog::on_pushButtonHelp_clicked()
{
    QMessageBox messageBox;
    messageBox.setWindowTitle("Help!");
    messageBox.setText("help");
    messageBox.exec();
}
