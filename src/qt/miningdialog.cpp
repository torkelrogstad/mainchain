// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/miningdialog.h>
#include <qt/forms/ui_miningdialog.h>

#include <sidechaindb.h>
#include <txmempool.h>
#include <validation.h>

#ifdef ENABLE_WALLET
#include <wallet/wallet.h>
#endif

MiningDialog::MiningDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MiningDialog)
{
    ui->setupUi(this);
}

MiningDialog::~MiningDialog()
{
    delete ui;
}

void MiningDialog::AbandonFailedBMM()
{
    if (vpwallets.empty())
        return; // TODO error message

    if (vpwallets[0]->IsLocked()) {
        return; // TODO error message
    }

    std::vector<uint256> vHashRemoved;
    mempool.SelectBMMRequests(vHashRemoved);
    mempool.RemoveExpiredCriticalRequests(vHashRemoved);

    // Also try to abandon cached BMM txid previously removed from our mempool
    std::vector<uint256> vCached = scdb.GetRemovedBMM();
    vHashRemoved.reserve(vCached.size());
    vHashRemoved.insert(vHashRemoved.end(), vCached.begin(), vCached.end());

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    vpwallets[0]->BlockUntilSyncedToCurrentChain();

    LOCK2(cs_main, vpwallets[0]->cs_wallet);

    for (const uint256& u : vHashRemoved) {

        if (!vpwallets[0]->mapWallet.count(u)) {
//            entry.push_back(Pair("not-in-wallet", u.ToString()));
//            results.push_back(entry);
            continue;
        }
        std::string strReason = "";
        if (!vpwallets[0]->AbandonTransaction(u, &strReason)) {
//            entry.push_back(Pair(strprintf("cannot-abandon: %s", strReason), u.ToString()));
//            results.push_back(entry);
            continue;
        }
//        entry.push_back(Pair("abandoned", u.ToString()));
//        results.push_back(entry);
    }
    scdb.ClearRemovedBMM();
}

void MiningDialog::on_pushButtonAbandonBMM_clicked()
{
    AbandonFailedBMM();
}

