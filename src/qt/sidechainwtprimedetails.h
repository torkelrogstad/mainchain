// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SIDECHAINWTPRIMEDETAILS_H
#define BITCOIN_SIDECHAINWTPRIMEDETAILS_H

#include <QDialog>

#include <string>

class CMutableTransaction;

namespace Ui {
class SidechainWTPrimeDetails;
}

class SidechainWTPrimeDetails : public QDialog
{
    Q_OBJECT

public:
    explicit SidechainWTPrimeDetails(QWidget *parent = 0);
    ~SidechainWTPrimeDetails();

    void SetTransaction(const CMutableTransaction& mtx);

private:
    Ui::SidechainWTPrimeDetails *ui;
    std::string strHex;
    std::string strTx;

private Q_SLOTS:
    void on_pushButtonCopyHex_clicked();
    void on_pushButtonClose_clicked();
};

#endif // BITCOIN_SIDECHAINWTPRIMEDETAILS_H
