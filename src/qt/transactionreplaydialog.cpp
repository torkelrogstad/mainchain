// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/transactionreplaydialog.h>
#include <qt/forms/ui_transactionreplaydialog.h>

#include <QProgressDialog>

#include <qt/addresstablemodel.h>
#include <qt/drivenetunits.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/walletmodel.h>

#include <apiclient.h>
#include <amount.h>
#include <uint256.h>
#include <primitives/transaction.h>
#include <wallet/wallet.h>

TransactionReplayDialog::TransactionReplayDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TransactionReplayDialog)
{
    ui->setupUi(this);

    ui->tableWidgetCoins->setColumnCount(COLUMN_VOUT_INDEX + 1);
    ui->tableWidgetCoins->setHorizontalHeaderLabels(
                QStringList() << "Amount" << "Label" << "Address" << "Date"
                << "Confirmations" << "txid" << "n");

    // Resize cells (in a backwards compatible way)
#if QT_VERSION < 0x050000
    ui->tableWidgetCoins->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
#else
    ui->tableWidgetCoins->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
#endif

    ui->tableWidgetCoins->horizontalHeader()->setStretchLastSection(false);
    ui->tableWidgetCoins->verticalHeader()->setVisible(false);
}

TransactionReplayDialog::~TransactionReplayDialog()
{
    delete ui;
}

void TransactionReplayDialog::SetWalletModel(WalletModel* model)
{
    this->walletModel = model;
    Update();
}

void TransactionReplayDialog::Update()
{
    if (!walletModel || !walletModel->getOptionsModel()
            || !walletModel->getAddressTableModel())
        return;

    ui->tableWidgetCoins->setRowCount(0);

    int nDisplayUnit = walletModel->getOptionsModel()->getDisplayUnit();

    std::map<QString, std::vector<COutput> > mapCoins;
    walletModel->listCoins(mapCoins);

    int nRow = 0;
    for (const std::pair<QString, std::vector<COutput>>& coins : mapCoins) {
        QString sWalletAddress = coins.first;
        QString sWalletLabel = walletModel->getAddressTableModel()->labelForAddress(sWalletAddress);
        if (sWalletLabel.isEmpty())
            sWalletLabel = tr("(no label)");

        CAmount nSum = 0;
        int nChildren = 0;
        for (const COutput& out : coins.second) {
            ui->tableWidgetCoins->insertRow(nRow);
            nSum += out.tx->tx->vout[out.i].nValue;
            nChildren++;

            uint256 txhash = out.tx->GetHash();

            // Check if the coin is locked - if it is we will create the
            // item with disabled status
            bool fLocked = walletModel->isLockedCoin(txhash, out.i);

            // address
            CTxDestination outputAddress;
            QString sAddress = "";
            if(ExtractDestination(out.tx->tx->vout[out.i].scriptPubKey, outputAddress))
            {
                sAddress = QString::fromStdString(EncodeDestination(outputAddress));
            }
            QTableWidgetItem *itemAddress = new QTableWidgetItem();
            itemAddress->setText(sAddress);
            itemAddress->setFlags(itemAddress->flags() & ~Qt::ItemIsEditable);
            if (fLocked)
                itemAddress->setFlags(itemAddress->flags() & ~Qt::ItemIsEnabled);
            ui->tableWidgetCoins->setItem(nRow, COLUMN_ADDRESS, itemAddress);

            // label
            QTableWidgetItem *itemLabel = new QTableWidgetItem();
            if (!(sAddress == sWalletAddress)) // change
            {
                // tooltip from where the change comes from
                itemLabel->setToolTip(tr("change from %1 (%2)").arg(sWalletLabel).arg(sWalletAddress));
                itemLabel->setText(tr("(change)"));
            }
            else
            {
                QString sLabel = walletModel->getAddressTableModel()->labelForAddress(sAddress);
                if (sLabel.isEmpty())
                    sLabel = tr("(no label)");
                itemLabel->setText(sLabel);
            }
            itemLabel->setFlags(itemLabel->flags() & ~Qt::ItemIsEditable);
            if (fLocked)
                itemLabel->setFlags(itemLabel->flags() & ~Qt::ItemIsEnabled);
            ui->tableWidgetCoins->setItem(nRow, COLUMN_LABEL, itemLabel);

            // amount
            QTableWidgetItem *itemAmount = new QTableWidgetItem();
            itemAmount->setText(BitcoinUnits::format(nDisplayUnit, out.tx->tx->vout[out.i].nValue));
            itemAmount->setFlags(itemAmount->flags() & ~Qt::ItemIsEditable);
            if (fLocked)
                itemAmount->setFlags(itemAmount->flags() & ~Qt::ItemIsEnabled);
            ui->tableWidgetCoins->setItem(nRow, COLUMN_AMOUNT, itemAmount);

            // date
            QTableWidgetItem *itemDate = new QTableWidgetItem();
            itemDate->setText(GUIUtil::dateTimeStr(out.tx->GetTxTime()));
            itemDate->setFlags(itemDate->flags() & ~Qt::ItemIsEditable);
            if (fLocked)
                itemDate->setFlags(itemDate->flags() & ~Qt::ItemIsEnabled);
            ui->tableWidgetCoins->setItem(nRow, COLUMN_DATE, itemDate);

            // confirmations
            QTableWidgetItem *itemConf = new QTableWidgetItem();
            itemConf->setText(QString::number(out.nDepth));
            itemConf->setFlags(itemConf->flags() & ~Qt::ItemIsEditable);
            if (fLocked)
                itemConf->setFlags(itemConf->flags() & ~Qt::ItemIsEnabled);
            ui->tableWidgetCoins->setItem(nRow, COLUMN_CONFIRMATIONS, itemConf);

            // txid
            QTableWidgetItem *itemTXID = new QTableWidgetItem();
            itemTXID->setText(QString::fromStdString(txhash.GetHex()));
            itemTXID->setFlags(itemTXID->flags() & ~Qt::ItemIsEditable);
            if (fLocked)
                itemTXID->setFlags(itemTXID->flags() & ~Qt::ItemIsEnabled);
            ui->tableWidgetCoins->setItem(nRow, COLUMN_TXHASH, itemTXID);

            // vout n
            QTableWidgetItem *itemN = new QTableWidgetItem();
            itemN->setText(QString::number(out.i));
            itemN->setFlags(itemN->flags() & ~Qt::ItemIsEditable);
            if (fLocked)
                itemN->setFlags(itemN->flags() & ~Qt::ItemIsEnabled);
            ui->tableWidgetCoins->setItem(nRow, COLUMN_VOUT_INDEX, itemN);

            nRow++;
        }
    }

    // TODO display loaded coins?
//    // Add loaded coins to the view
//    std::vector<LoadedCoin> vLoadedCoin;
//    vLoadedCoin = walletModel->getMyLoadedCoins();
//    for (const LoadedCoin& c : vLoadedCoin) {
//        if (walletModel->isSpent(c.out))
//            continue;

//        CCoinControlWidgetItem *itemOutput = new CCoinControlWidgetItem(ui->treeWidget);
//        itemOutput->setFlags(flgCheckbox);
//        itemOutput->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);
//        CTxDestination outputAddress;
//        QString sAddress = "";
//        if (ExtractDestination(c.coin.out.scriptPubKey, outputAddress)) {
//            sAddress = QString::fromStdString(EncodeDestination(outputAddress));
//            // TODO use scriptPubKey from LoadedCoin class so that we can
//            // guess what the address might have been...?
//            //itemOutput->setText(COLUMN_ADDRESS, sAddress);
//            itemOutput->setText(COLUMN_ADDRESS, tr("Loaded Coin (unknown address)"));
//        }
//        itemOutput->setText(COLUMN_LABEL, tr("(LOADED)"));
//        // amount
//        itemOutput->setText(COLUMN_AMOUNT, BitcoinUnits::format(nDisplayUnit, c.coin.out.nValue));
//        itemOutput->setData(COLUMN_AMOUNT, Qt::UserRole, QVariant((qlonglong)c.coin.out.nValue));
//        // TODO show other data that coincontroldialog displays
//        // transaction hash
//        itemOutput->setText(COLUMN_TXHASH, QString::fromStdString(c.out.hash.ToString()));
//        // vout index
//        itemOutput->setText(COLUMN_VOUT_INDEX, QString::number(c.out.n));
//        // set checkbox
//        if (coinControl()->IsSelected(c.out))
//            itemOutput->setCheckState(COLUMN_CHECKBOX, Qt::Checked);
//    }

    // TODO ?
    // sort view
    // sortView(sortColumn, sortOrder);
}

void TransactionReplayDialog::on_pushButtonCheckReplay_clicked()
{
    if (!walletModel || !walletModel->getOptionsModel()) {
        return;
    }

    QMessageBox messageBox;
    QModelIndexList selection = ui->tableWidgetCoins->selectionModel()->selectedRows(COLUMN_TXHASH);
    if (!selection.size()) {
        messageBox.setWindowTitle("Please select transaction(s)!");
        QString str = QString("<p>You must select one or more transactions to check the replay status of!</p>");
        messageBox.setText(str);
        messageBox.setIcon(QMessageBox::Information);
        messageBox.setStandardButtons(QMessageBox::Ok);
        messageBox.exec();
        return;
    }

    messageBox.setWindowTitle("Are you sure?");
    QString warning = "Privacy Warning:\n\n";
    warning += "Using this feature will send requests over the internet ";
    warning += "which include information about your wallet's transactions.";
    warning += "\n\n";
    warning += "Checking the replay status of your wallet's transactions ";
    warning += "will require sending the same data over the internet as ";
    warning += "if you had vistied a block explorer yourself.\n";
    messageBox.setText(warning);
    messageBox.setIcon(QMessageBox::Warning);
    messageBox.setStandardButtons(QMessageBox::Abort | QMessageBox::Ok);
    messageBox.setDefaultButton(QMessageBox::Abort);
    int ret = messageBox.exec();
    if (ret != QMessageBox::Ok) {
        return;
    }

    QString strProgress = "Checking transaction replay status...\n\n";
    strProgress += "Contacting block explorer API to check if selected ";
    strProgress += "transaction(s) have been replayed.\n\n";

    // Progress dialog with abort button. If user selects many transactions
    // to check, the operation can take a while.
    QProgressDialog progress(strProgress, "Abort", 0, selection.size(), this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setWindowTitle("Replay status");
    progress.setMinimumDuration(0);
    progress.setMinimumSize(500, 100);

    QFont font;
    font.setStyleHint(QFont::Monospace);
    font.setFamily("noto");
    progress.setFont(font);

    progress.setValue(0);

    // Check replay status & display progress
    APIClient client;
    for (int i = 0; i < selection.size(); i++) {
        progress.setValue(i);

        if (progress.wasCanceled())
            break;

        QVariant data = selection[i].data();
        uint256 txid = uint256S(data.toString().toStdString());

        QString strStatus = strProgress;
        strStatus += "Checking: ";
        strStatus += QString::fromStdString(txid.ToString());
        strStatus += "\n";

        progress.setLabelText(strStatus);

        // TODO
        // if replay status is currently ReplayLoaded, skip
        // TODO
        // if replay status is already ReplayTrue, skip
        // TODO handle request failure and keep set to unknown...
        if (client.IsTxReplayed(txid)) {
            // TODO wallet->wtx->updateReplayStatus
        } else {
            // TODO wallet->wtx->updateReplayStatus
        }
    }
    progress.setValue(selection.size());
}

void TransactionReplayDialog::on_pushButtonSplitCoins_clicked()
{

}
