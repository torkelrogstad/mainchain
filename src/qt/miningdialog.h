// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MININGDIALOG_H
#define MININGDIALOG_H

#include <QDialog>

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

namespace Ui {
class MiningDialog;
}

class MiningDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MiningDialog(QWidget *parent = 0);
    ~MiningDialog();

public Q_SLOTS:
    void on_pushButtonAbandonBMM_clicked();

    void on_pushButtonStartMining_clicked();

    void on_pushButtonStopMining_clicked();

private Q_SLOTS:
    void Update();

    void UpdateMiningOutput();

private:
    Ui::MiningDialog *ui;

    void AbandonFailedBMM();

    QTimer* pollTimer;
    QTimer* miningOutputTimer;
};

#endif // MININGDIALOG_H
