// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/miningdialog.h>
#include <qt/forms/ui_miningdialog.h>

#include <QTimer>

#include <rpc/blockchain.h> // For GetDifficulty()

#include <chainparams.h>
#include <miner.h>
#include <sidechaindb.h>
#include <txmempool.h>
#include <validation.h>
#include <warnings.h>

#ifdef ENABLE_WALLET
#include <wallet/wallet.h>
#endif

MiningDialog::MiningDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MiningDialog)
{
    ui->setupUi(this);

    pollTimer = new QTimer(this);
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(Update()));

    miningOutputTimer = new QTimer(this);
    connect(miningOutputTimer, SIGNAL(timeout()), this, SLOT(UpdateMiningOutput()));

    pollTimer->start(30 * 1000); // 30 seconds

    Update();

    ui->pushButtonStopMining->setEnabled(false);

    ui->frameMiningOutput->setEnabled(false);
    ui->frameMiningOutput->setVisible(false);
    ui->labelMinerOutput->setVisible(false);
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

    // TODO display results in a popup message

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

void MiningDialog::on_pushButtonStartMining_clicked()
{
    int nGenProcLimit = ui->spinBoxThreads->value();
    GenerateBitcoins(true, nGenProcLimit, Params());

    ui->pushButtonStartMining->setEnabled(false);
    ui->pushButtonStopMining->setEnabled(true);

    Update();

    // TODO use signals instead of making the UI update faster to
    // keep up with changes while mining.
    miningOutputTimer->start(100);

    ui->frameMiningOutput->setEnabled(true);
    ui->frameMiningOutput->setVisible(true);

    ui->spinBoxThreads->setEnabled(false);

    ui->labelMinerOutput->setVisible(true);
}

void MiningDialog::on_pushButtonStopMining_clicked()
{
    GenerateBitcoins(false, 0, Params());

    miningOutputTimer->stop();

    ui->pushButtonStartMining->setEnabled(true);
    ui->pushButtonStopMining->setEnabled(false);

    ui->frameMiningOutput->setEnabled(false);
    ui->frameMiningOutput->setVisible(false);

    ui->labelMinerOutput->setVisible(false);

    ui->spinBoxThreads->setEnabled(true);
}

void MiningDialog::Update()
{
    // Update things that don't change quickly while mining

    QString height = "Current block height: ";
    height += QString::number(chainActive.Height() + 1);
    ui->labelHeight->setText(height);

    QString weight = "Current block weight: ";
    weight += QString::number(nLastBlockWeight);
    ui->labelWeight->setText(weight);

    QString txns = "Current block txns: ";
    txns += QString::number(nLastBlockTx);
    ui->labelTxns->setText(txns);

    QString diff = "Difficulty: ";
    diff += QString::number(GetDifficulty());
    ui->labelDiff->setText(diff);

    QString hashps = "Network hashps: ";
    hashps += QString::number(GetNetworkHashPerSecond(120, -1), 'f');
    ui->labelHashps->setText(hashps);

    QString pooledtx = "Pooled txns: ";
    pooledtx += QString::number(mempool.size());
    ui->labelPooled->setText(pooledtx);

    QString warnings = "Warnings: ";
    warnings += QString::fromStdString(GetWarnings("statusbar"));
    ui->labelWarnings->setText(warnings);
}

void MiningDialog::UpdateMiningOutput()
{
    // Update things that will change very quickly while mining

    QString height = "Current block height: ";
    height += QString::number(chainActive.Height());
    ui->labelHeight->setText(height);

    QString target = "Target hash: ";
    target += QString::fromStdString(hashTarget.ToString());
    ui->labelHashTarget->setText(target);

    QString best = "Lowest hash: ";
    best += QString::fromStdString(hashBest.ToString());
    ui->labelHashBest->setText(best);

    QString nonce = "Nonce: ";
    nonce += QString::number(nMiningNonce);
    ui->labelNonce->setText(nonce);
}

void MiningDialog::on_pushButtonManage_clicked()
{
    Q_EMIT ManagePageRequested();
}
