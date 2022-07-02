// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/scdbmerkleroothistorydialog.h>
#include <qt/forms/ui_scdbmerkleroothistorydialog.h>

#include <qt/clientmodel.h>
#include <qt/platformstyle.h>

#include <chain.h>
#include <chainparams.h>
#include <consensus/merkle.h>
#include <sidechain.h>
#include <streams.h>
#include <txdb.h>
#include <validation.h>

SCDBMerkleRootHistoryDialog::SCDBMerkleRootHistoryDialog(const PlatformStyle *_platformStyle, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SCDBMerkleRootHistoryDialog),
    platformStyle(_platformStyle)
{
    ui->setupUi(this);
}

SCDBMerkleRootHistoryDialog::~SCDBMerkleRootHistoryDialog()
{
    delete ui;
}

void SCDBMerkleRootHistoryDialog::Update()
{
    ui->treeWidget->clear();

    int nHeight = chainActive.Height();
    int nBlocksToDisplay = 6;
    if (nHeight < nBlocksToDisplay)
        nBlocksToDisplay = nHeight;

    for (int i = 0; i < nBlocksToDisplay; i++) {
        CBlockIndex *pindex = chainActive[nHeight - i];

        if (pindex->GetBlockHash() == Params().GetConsensus().hashGenesisBlock) {
            QTreeWidgetItem *subItem = new QTreeWidgetItem();
            subItem->setText(0, "Genesis block has no score data");
            AddTreeItem(i, "N/A", nHeight - i, subItem);
            continue;
        }

        SidechainBlockData data;
        if (!psidechaintree->GetBlockData(pindex->GetBlockHash(), data)) {
            QTreeWidgetItem *subItem = new QTreeWidgetItem();
            subItem->setText(0, "No score data for this block");
            AddTreeItem(i, "N/A", nHeight - i, subItem);
            continue;
        }

        // Loop through state here and add sub items for sc# & score change
        bool fDataFound = false;
        int nSidechain = 0;
        std::vector<uint256> vLeaf; // Keep track of state hashes to compute M4
        QString strM4Ser = "";
        for (const std::vector<SidechainWithdrawalState>& vScore : data.vWithdrawalStatus) {

            if (vScore.empty()) {
                nSidechain++;
                continue;
            }

            // Create sidechain item
            QTreeWidgetItem *subItemSC = new QTreeWidgetItem();

            // Load prev block data to check old score
            SidechainBlockData prevData;
            if (pindex->pprev)
                psidechaintree->GetBlockData(pindex->pprev->GetBlockHash(), prevData);

            // Create sidechain children items
            for (const SidechainWithdrawalState& s : vScore) {

                // Look up old work score
                uint16_t nPrevScore = 0;
                if (prevData.vWithdrawalStatus.size() > s.nSidechain) {
                    for (const SidechainWithdrawalState& prevState : prevData.vWithdrawalStatus[s.nSidechain]) {
                        if (prevState.hash == s.hash)
                            nPrevScore = prevState.nWorkScore;
                    }
                }

                QTreeWidgetItem *subItemScore = new QTreeWidgetItem();
                QString strScore = " (Abstain)";
                if (nPrevScore > s.nWorkScore)
                    strScore = " (Upvote / ACK)";
                else
                if (nPrevScore < s.nWorkScore)
                    strScore = " (Downvote / NACK)";

                subItemScore->setText(0, "Work score: " + QString::number(nPrevScore) + " -> " + QString::number(s.nWorkScore) + strScore);
                subItemSC->addChild(subItemScore);

                QTreeWidgetItem *subItemBlocks = new QTreeWidgetItem();
                subItemBlocks->setText(0, "Blocks remaining: " + QString::number(s.nBlocksLeft + 1) + " -> " + QString::number(s.nBlocksLeft));
                subItemSC->addChild(subItemBlocks);

                QTreeWidgetItem *subItemHash = new QTreeWidgetItem();
                subItemHash->setText(0, "Withdrawal bundle hash:\n" + QString::fromStdString(s.hash.ToString()));
                subItemSC->addChild(subItemHash);

                CDataStream ssSer(SER_DISK, CLIENT_VERSION);
                ssSer << s;

                QString strSer = QString::fromStdString(HexStr(ssSer.str()));

                QTreeWidgetItem *subItemSCSer = new QTreeWidgetItem();
                subItemSCSer->setText(0, "Serialization:\n" + strSer);
                subItemSC->addChild(subItemSCSer);

                vLeaf.push_back(s.GetHash());
                strM4Ser += "SC# " + QString::number(s.nSidechain) + ": " + strSer + ", ";
            }

            // Add SC item parent to tree
            subItemSC->setText(0, "Sidechain# " + QString::number(nSidechain));
            AddTreeItem(i, QString::fromStdString(data.hashMT.ToString()), nHeight - i, subItemSC);

            fDataFound = true;
            nSidechain++;
        }

        if (fDataFound) {
            // Create M4 tree object
            QTreeWidgetItem *subItemTree = new QTreeWidgetItem();
            subItemTree->setText(0, "M4 merkle root leaf nodes (SHA256D(serialization))");

            // Create M4 raw serialization object
            QTreeWidgetItem *subItemSer = new QTreeWidgetItem();
            subItemSer->setText(0, "M4 serialization data");

            QTreeWidgetItem *subItemSerData = new QTreeWidgetItem();
            subItemSerData->setText(0, strM4Ser);
            subItemSer->addChild(subItemSerData);

            QString strLeaves = "";
            for (const uint256& u : vLeaf)
                strLeaves += QString::fromStdString(u.ToString()) + " ";

            // Create M4 tree leaf nodes object
            QTreeWidgetItem *subItemLeaves = new QTreeWidgetItem();
            subItemLeaves->setText(0, strLeaves);
            subItemTree->addChild(subItemLeaves);

            // Create M4 merkle root object
            QTreeWidgetItem *subItemMerkle = new QTreeWidgetItem();
            subItemMerkle->setText(0, "M4 merkle root hash");

            QTreeWidgetItem *subItemMerkleHash = new QTreeWidgetItem();
            uint256 hashM4 = ComputeMerkleRoot(vLeaf);
            subItemMerkleHash->setText(0, QString::fromStdString(hashM4.ToString()));
            subItemMerkle->addChild(subItemMerkleHash);

            AddTreeItem(i, QString::fromStdString(data.hashMT.ToString()), nHeight - i, subItemSer);
            AddTreeItem(i, QString::fromStdString(data.hashMT.ToString()), nHeight - i, subItemTree);
            AddTreeItem(i, QString::fromStdString(data.hashMT.ToString()), nHeight - i, subItemMerkle);
        } else {
            QTreeWidgetItem *subItem = new QTreeWidgetItem();
            subItem->setText(0, "No score data for this block");
            AddTreeItem(i, "N/A", nHeight - i, subItem);
        }
    }
    ui->treeWidget->collapseAll();
    ui->treeWidget->resizeColumnToContents(0);
}

void SCDBMerkleRootHistoryDialog::AddTreeItem(int index, const QString& hashMT, const int nHeight, QTreeWidgetItem *item)
{
    if (!item || index < 0)
        return;

    QTreeWidgetItem *topItem = ui->treeWidget->topLevelItem(index);
    if (!topItem) {
        topItem = new QTreeWidgetItem(ui->treeWidget);
        topItem->setText(0, "Block #" + QString::number(nHeight) + " M4: " + hashMT);
        ui->treeWidget->insertTopLevelItem(index, topItem);
    }

    if (!topItem)
        return;

    topItem->addChild(item);
}

void SCDBMerkleRootHistoryDialog::numBlocksChanged()
{
    if (this->isVisible())
        Update();
}

void SCDBMerkleRootHistoryDialog::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if(model)
    {
        connect(model, SIGNAL(numBlocksChanged(int,QDateTime,double,bool)),
                this, SLOT(numBlocksChanged()));
    }
}
