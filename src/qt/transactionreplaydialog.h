// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TRANSACTIONREPLAYDIALOG_H
#define TRANSACTIONREPLAYDIALOG_H

#include <QDialog>

namespace Ui {
class TransactionReplayDialog;
}

class TransactionReplayDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TransactionReplayDialog(QWidget *parent = 0);
    ~TransactionReplayDialog();

private:
    Ui::TransactionReplayDialog *ui;
};

#endif // TRANSACTIONREPLAYDIALOG_H

// Deleted code from old txview replay check button

/*
 * //    if (!model || !model->getOptionsModel()) {
//        return;
//    }
//    QMessageBox messageBox;
//    // Refresh transaction replay status
//    if(!transactionView || !transactionView->selectionModel())
//        return;
//    QModelIndexList selection = transactionView->selectionModel()->selectedRows(0);
//    if (selection.size() != 1) {
//        messageBox.setWindowTitle("Please select transaction(s)!");
//        QString str = QString("<p>You must select a transaction to check the replay status of!</p>" \
//                              "<p><b>Please highlight one, and no more than one transaction.</b></p>");
//        messageBox.setText(str);
//        messageBox.setIcon(QMessageBox::Information);
//        messageBox.setStandardButtons(QMessageBox::Ok);
//        messageBox.exec();
//        return;
//    }
//    messageBox.setWindowTitle("Are you sure?");
//    QString warning = "Privacy Warning:\n\n";
//    warning += "Using this feature will send requests over the internet ";
//    warning += "which include information about your wallet's transactions.";
//    warning += "\n\n";
//    warning += "Checking the replay status of your wallet's transactions ";
//    warning += "will require sending the same data over the internet as ";
//    warning += "if you had vistied a block explorer yourself.\n";
//    messageBox.setText(warning);
//    messageBox.setIcon(QMessageBox::Warning);
//    messageBox.setStandardButtons(QMessageBox::Abort | QMessageBox::Ok);
//    messageBox.setDefaultButton(QMessageBox::Abort);
//    int ret = messageBox.exec();
//    if (ret != QMessageBox::Ok) {
//        return;
//    }

//    // TODO Make asynchronus and then allow the user to select more than one
//    // transaction to update the status of at once.
//    APIClient client;
//    for (const QModelIndex& i : selection) {
//        uint256 hash;
//        QString hashQStr = i.data(TransactionTableModel::TxHashRole).toString();
//        hash.SetHex(hashQStr.toStdString());
//        // TODO
//        // if replay status is currently ReplayLoaded, skip
//        // TODO
//        // if replay status is already ReplayTrue, skip
//        // TODO handle request failure and keep set to unknown...
//        if (client.IsTxReplayed(hash)) {
//            model->getTransactionTableModel()->updateReplayStatus(hashQStr, TransactionStatus::ReplayTrue);
//            model->getTransactionTableModel()->updateTransaction(hashQStr, CT_UPDATED, true);
//        } else {
//            model->getTransactionTableModel()->updateReplayStatus(hashQStr, TransactionStatus::ReplayFalse);
//            model->getTransactionTableModel()->updateTransaction(hashQStr, CT_UPDATED, true);
//        }
//    }
*/
