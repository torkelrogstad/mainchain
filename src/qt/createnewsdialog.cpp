// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/createnewsdialog.h>
#include <qt/forms/ui_createnewsdialog.h>

#include <amount.h>
#include <wallet/wallet.h>
#include <txdb.h>
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

    ui->labelCharsRemaining->setText(QString::number(NEWS_HEADLINE_CHARS));

    updateTypes();
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
    } else {
        // Figure out the script header for this type
        std::vector<CustomNewsType> vCustom;
        popreturndb->GetCustomTypes(vCustom);

        size_t nFilter = ui->comboBoxCategory->currentIndex();

        // TODO figure out a better way to handle custom type lookup.
        // Perhaps all types should be in ldb and lookup up like this.
        size_t nBuiltInTypes = 3;
        nFilter -= nBuiltInTypes;

        if (nFilter >= vCustom.size()) {
            messageBox.setWindowTitle("Invalid custom type!");
            messageBox.setText("Couldn't find custom type.");
            messageBox.exec();
            return;
        }
        script = vCustom[nFilter].header;
    }

    // TODO Should script include the pushdata size added by << operator?
    std::string strHex = HexStr(strText.begin(), strText.end());
    std::vector<unsigned char> vBytes = ParseHex(strHex);
    script << vBytes;

    std::string strDecode;
    for (size_t i = 0; i < script.size(); i++)
        strDecode += script[i];

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

void CreateNewsDialog::on_plainTextEdit_textChanged()
{
    QString currentText = ui->plainTextEdit->toPlainText();
    if (currentText == cacheText)
        return;

    cacheText = currentText;
    std::string strText = currentText.toStdString();

    // Reset highlights
    QTextCursor cursor(ui->plainTextEdit->document());
    cursor.setPosition(0, QTextCursor::MoveAnchor);
    cursor.setPosition(strText.size(), QTextCursor::KeepAnchor);
    cursor.setCharFormat(QTextCharFormat());

    // Update the number of characters remaining label
    if (strText.size() >= NEWS_HEADLINE_CHARS)
        ui->labelCharsRemaining->setText(QString::number(0));
    else
        ui->labelCharsRemaining->setText(QString::number(NEWS_HEADLINE_CHARS - strText.size()));

    // Highlight characters when there are too many to fit in the headline
    // or after a newline is added.

    // Highlight characters if we've gone over the limit
    if (strText.size() > NEWS_HEADLINE_CHARS) {
        QTextCursor cursor(ui->plainTextEdit->document());

        QTextCharFormat highlight;
        highlight.setBackground(Qt::red);

        cursor.setPosition(NEWS_HEADLINE_CHARS, QTextCursor::MoveAnchor);
        cursor.setPosition(strText.size(), QTextCursor::KeepAnchor);
        cursor.setCharFormat(highlight);
    }

    // Check for any newlines and if we find one before the character
    // limit then highlight
    for (size_t i = 0; i < strText.size(); i++) {
        if (strText[i] == '\n' || strText[i] == '\r') {
            QTextCursor cursor(ui->plainTextEdit->document());

            QTextCharFormat highlight;
            highlight.setBackground(Qt::red);

            cursor.setPosition(i, QTextCursor::MoveAnchor);
            cursor.setPosition(strText.size(), QTextCursor::KeepAnchor);
            cursor.setCharFormat(highlight);

            ui->labelCharsRemaining->setText(QString::number(0));
        }
    }
}

void CreateNewsDialog::updateTypes()
{
    ui->comboBoxCategory->clear();

    ui->comboBoxCategory->addItem("All OP_RETURN data");
    ui->comboBoxCategory->addItem("Tokyo Daily News");
    ui->comboBoxCategory->addItem("US Daily News");

    std::vector<CustomNewsType> vCustom;
    popreturndb->GetCustomTypes(vCustom);

    for (const CustomNewsType c : vCustom)
        ui->comboBoxCategory->addItem(QString::fromStdString(c.title));
}
