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
#include <sidechaindb.h>
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

void SCDBMerkleRootHistoryDialog::UpdateOnShow()
{
    UpdateVoteTree();
    UpdateNextTree();
    UpdateHistoryTree();
}

void SCDBMerkleRootHistoryDialog::UpdateVoteTree()
{
    // Update vote tree

    ui->treeWidgetVote->setUpdatesEnabled(false);
    ui->treeWidgetVote->clear();

    std::vector<Sidechain> vSidechain = scdb.GetActiveSidechains();
    int index = 0;
    for (size_t i = 0; i < vSidechain.size(); i++) {
        std::vector<SidechainWithdrawalState> vWithdrawal;
        vWithdrawal = scdb.GetState(vSidechain[i].nSidechain);

        QTreeWidgetItem *topItem = new QTreeWidgetItem();
        topItem->setText(0, "SC #" + QString::number(vSidechain[i].nSidechain) + " " + QString::fromStdString(vSidechain[i].title));
        ui->treeWidgetVote->insertTopLevelItem(i, topItem);

        if (!vWithdrawal.size()) {
            index++;
            continue;
        }

        // Abstain
        QTreeWidgetItem *subItemAbstain = new QTreeWidgetItem();
        subItemAbstain->setText(0, "Abstain");
        subItemAbstain->setCheckState(0, Qt::Unchecked);
        subItemAbstain->setData(0, NumRole, vSidechain[i].nSidechain);
        subItemAbstain->setData(0, HashRole, "");
        topItem->addChild(subItemAbstain);

        // Alarm
        QTreeWidgetItem *subItemAlarm = new QTreeWidgetItem();
        subItemAlarm->setText(0, "Alarm");
        subItemAlarm->setCheckState(0, Qt::Unchecked);
        subItemAlarm->setData(0, NumRole, vSidechain[i].nSidechain);
        subItemAlarm->setData(0, HashRole, "");
        topItem->addChild(subItemAlarm);

        std::vector<SidechainCustomVote> vCustomVote = scdb.GetCustomVoteCache();

        // Other vote options
        bool fUpvote = false;
        for (size_t i = 0; i < vWithdrawal.size(); i++) {
            for (const SidechainCustomVote& v : vCustomVote) {
                if (v.nSidechain == vWithdrawal[i].nSidechain &&
                        v.vote == SCDB_UPVOTE &&
                        v.hash == vWithdrawal[i].hash) {
                    fUpvote = true;
                    break;
                }
            }

            QTreeWidgetItem *subItemWT = new QTreeWidgetItem();
            subItemWT->setText(0, QString::fromStdString(vWithdrawal[i].hash.ToString()));
            subItemWT->setCheckState(0, fUpvote ? Qt::Checked : Qt::Unchecked);
            subItemWT->setData(0, NumRole, vWithdrawal[i].nSidechain);
            subItemWT->setData(0, HashRole, QString::fromStdString(vWithdrawal[i].hash.ToString()));

            QTreeWidgetItem *subItemBlocks = new QTreeWidgetItem();
            subItemBlocks->setText(0, "Blocks left: " + QString::number(vWithdrawal[i].nBlocksLeft));
            subItemWT->addChild(subItemBlocks);

            QTreeWidgetItem *subItemScore = new QTreeWidgetItem();
            subItemScore->setText(0, "Work score: " + QString::number(vWithdrawal[i].nWorkScore));
            subItemWT->addChild(subItemScore);

            topItem->addChild(subItemWT);
        }

        // Update check state of abstain and alarm item based on custom votes
        bool fDownvote = false;
        if (!fUpvote) {
            // Check for downvotes and check abstain checkbox if we find none
            for (const SidechainCustomVote& v : vCustomVote) {
                if (v.nSidechain == vSidechain[i].nSidechain &&
                        v.vote == SCDB_DOWNVOTE) {
                    fDownvote = true;
                    break;
                }
            }
        }
        if (fDownvote) {
            subItemAlarm->setCheckState(0, Qt::Checked);
        }
        else
        if (!fUpvote && !fDownvote) {
            subItemAbstain->setCheckState(0, Qt::Checked);
        }

        index++;
    }
    ui->treeWidgetVote->collapseAll();
    ui->treeWidgetVote->expandToDepth(0);
    ui->treeWidgetVote->setColumnWidth(0, 600);

    ui->treeWidgetVote->setUpdatesEnabled(true);
}

void SCDBMerkleRootHistoryDialog::UpdateNextTree()
{
    // Update the next M4 tree

    ui->treeWidgetNext->setUpdatesEnabled(false);
    ui->treeWidgetNext->clear();

    QTreeWidgetItem* topItem = new QTreeWidgetItem(ui->treeWidgetNext);
    ui->treeWidgetNext->insertTopLevelItem(0, topItem);

    std::vector<std::vector<SidechainWithdrawalState>> vState = scdb.GetState();

    // Loop through state here and add sub items for sc# & score change
    bool fDataFound = false;
    int nSidechain = 0;
    std::vector<uint256> vLeaf; // Keep track of state hashes to compute M4
    QString strM4Ser = "";
    for (const std::vector<SidechainWithdrawalState>& vScore : vState) {
        if (vScore.empty()) {
            nSidechain++;
            continue;
        }

        // Create sidechain item
        QTreeWidgetItem *subItemSC = new QTreeWidgetItem();

        // Create sidechain children items
        for (const SidechainWithdrawalState& s : vScore) {
            // Look up our vote setting
            char vote = SCDB_ABSTAIN;
            std::vector<SidechainCustomVote> vCustomVote = scdb.GetCustomVoteCache();
            for (const SidechainCustomVote& v : vCustomVote) {
                if (v.nSidechain == s.nSidechain) {
                    vote = v.vote;
                    break;
                }
            }

            QTreeWidgetItem *subItemScore = new QTreeWidgetItem();

            // Figure out next score based on vote settings
            QString strScore = "";
            int nNewScore = 0;
            if (vote == SCDB_UPVOTE) {
                strScore = " (Upvote / ACK)";
                nNewScore = s.nWorkScore + 1;
            }
            else
            if (vote == SCDB_ABSTAIN)
            {
                strScore = " (Abstain)";
                nNewScore = s.nWorkScore;
            }
            else
            if (vote == SCDB_DOWNVOTE)
            {
                strScore = " (Downvote / NACK)";
                nNewScore = s.nWorkScore - 1;
            }

            subItemScore->setText(0, "Work score: " + QString::number(s.nWorkScore) + " -> " + QString::number(nNewScore) + strScore);
            subItemSC->addChild(subItemScore);

            QTreeWidgetItem *subItemBlocks = new QTreeWidgetItem();
            subItemBlocks->setText(0, "Blocks remaining: " + QString::number(s.nBlocksLeft) + " -> " + QString::number(s.nBlocksLeft - 1));
            subItemSC->addChild(subItemBlocks);

            QTreeWidgetItem *subItemHash = new QTreeWidgetItem();
            subItemHash->setText(0, "Withdrawal bundle hash:\n" + QString::fromStdString(s.hash.ToString()));
            subItemSC->addChild(subItemHash);

            // Update with next vote state to get new serialization & hash
            SidechainWithdrawalState nextState = s;
            nextState.nBlocksLeft -= 1;
            nextState.nWorkScore = nNewScore;

            CDataStream ssSer(SER_DISK, CLIENT_VERSION);
            ssSer << nextState;

            QString strSer = QString::fromStdString(HexStr(ssSer.str()));

            QTreeWidgetItem *subItemSCSer = new QTreeWidgetItem();
            subItemSCSer->setText(0, "Serialization:\n" + strSer);
            subItemSC->addChild(subItemSCSer);

            vLeaf.push_back(nextState.GetHash());
            strM4Ser += "SC# " + QString::number(s.nSidechain) + ": " + strSer + ", ";
        }

        // Add SC item parent to tree
        subItemSC->setText(0, "Sidechain# " + QString::number(nSidechain) + " vote state");
        topItem->addChild(subItemSC);

        fDataFound = true;
        nSidechain++;
    }

    uint256 hashM4 = uint256();
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
        hashM4 = ComputeMerkleRoot(vLeaf);
        subItemMerkleHash->setText(0, QString::fromStdString(hashM4.ToString()));
        subItemMerkle->addChild(subItemMerkleHash);

        topItem->addChild(subItemSer);
        topItem->addChild(subItemTree);
        topItem->addChild(subItemMerkle);
    } else {
        QTreeWidgetItem *subItem = new QTreeWidgetItem();
        subItem->setText(0, "No score data for this block");
        topItem->addChild(subItem);
    }

    topItem->setText(0, "Block #" + QString::number(chainActive.Height() + 1) + " M4: " + QString::fromStdString(hashM4.ToString()));

    ui->treeWidgetNext->collapseAll();
    ui->treeWidgetNext->resizeColumnToContents(0);
    ui->treeWidgetNext->expandToDepth(1);

    ui->treeWidgetNext->setUpdatesEnabled(true);
}

void SCDBMerkleRootHistoryDialog::UpdateHistoryTree()
{
    // Update history tree

    ui->treeWidgetHistory->setUpdatesEnabled(false);
    ui->treeWidgetHistory->clear();

    int nHeight = chainActive.Height();
    int nBlocksToDisplay = 6;
    if (nHeight < nBlocksToDisplay)
        nBlocksToDisplay = nHeight;

    for (int i = 0; i < nBlocksToDisplay; i++) {
        CBlockIndex *pindex = chainActive[nHeight - i];

        if (pindex->GetBlockHash() == Params().GetConsensus().hashGenesisBlock) {
            QTreeWidgetItem *subItem = new QTreeWidgetItem();
            subItem->setText(0, "Genesis block has no score data");
            AddHistoryTreeItem(i, "N/A", nHeight - i, subItem);
            continue;
        }

        SidechainBlockData data;
        if (!psidechaintree->GetBlockData(pindex->GetBlockHash(), data)) {
            QTreeWidgetItem *subItem = new QTreeWidgetItem();
            subItem->setText(0, "No score data for this block");
            AddHistoryTreeItem(i, "N/A", nHeight - i, subItem);
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
                if (nPrevScore < s.nWorkScore)
                    strScore = " (Upvote / ACK)";
                else
                if (nPrevScore > s.nWorkScore)
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
            subItemSC->setText(0, "Sidechain# " + QString::number(nSidechain) + " vote state");
            AddHistoryTreeItem(i, QString::fromStdString(data.hashMT.ToString()), nHeight - i, subItemSC);

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

            AddHistoryTreeItem(i, QString::fromStdString(data.hashMT.ToString()), nHeight - i, subItemSer);
            AddHistoryTreeItem(i, QString::fromStdString(data.hashMT.ToString()), nHeight - i, subItemTree);
            AddHistoryTreeItem(i, QString::fromStdString(data.hashMT.ToString()), nHeight - i, subItemMerkle);
        } else {
            QTreeWidgetItem *subItem = new QTreeWidgetItem();
            subItem->setText(0, "No score data for this block");
            AddHistoryTreeItem(i, "N/A", nHeight - i, subItem);
        }
    }
    ui->treeWidgetHistory->collapseAll();
    ui->treeWidgetHistory->resizeColumnToContents(0);

    ui->treeWidgetHistory->setUpdatesEnabled(true);
}

void SCDBMerkleRootHistoryDialog::AddHistoryTreeItem(int index, const QString& hashMT, const int nHeight, QTreeWidgetItem *item)
{
    if (!item || index < 0)
        return;

    QTreeWidgetItem *topItem = ui->treeWidgetHistory->topLevelItem(index);
    if (!topItem) {
        topItem = new QTreeWidgetItem(ui->treeWidgetHistory);
        topItem->setText(0, "Block #" + QString::number(nHeight) + " M4: " + hashMT);
        ui->treeWidgetHistory->insertTopLevelItem(index, topItem);
    }

    if (!topItem)
        return;

    topItem->addChild(item);
}

void SCDBMerkleRootHistoryDialog::numBlocksChanged()
{
    if (this->isVisible())
        UpdateOnShow();
}

void SCDBMerkleRootHistoryDialog::on_treeWidgetVote_itemChanged(QTreeWidgetItem *item, int column)
{
    QTreeWidgetItem* parent = item->parent();
    if (!parent)
        return;

    ui->treeWidgetVote->setUpdatesEnabled(false);

    bool fChecked = (item->checkState(0) == Qt::Checked);
    int nChildren = parent->childCount();

    if (nChildren < 2)
        return;

    if (fChecked) {
        // Uncheck other boxes when a new one is checked
        for (int i = 0; i < nChildren; i++) {
            QTreeWidgetItem *child = parent->child(i);
            if (fChecked && child != item)
                child->setCheckState(0, Qt::Unchecked);
        }
    } else {
        bool fCheckFound = false;
        for (int i = 0; i < nChildren; i++) {
            QTreeWidgetItem *child = parent->child(i);
            if (child->checkState(0) == Qt::Checked) {
                fCheckFound = true;
                break;
            }
        }

        // Switch back to abstain if nothing is checked
        if (!fCheckFound)
            parent->child(0)->setCheckState(0, Qt::Checked);
    }

    // Update users custom vote settings
    SidechainCustomVote vote;

    vote.nSidechain = item->data(0, NumRole).toUInt();

    if (parent->child(0)->checkState(0) == Qt::Checked) {
        vote.vote = SCDB_ABSTAIN;
    }
    else
    if (parent->child(1)->checkState(0) == Qt::Checked) {
        vote.vote = SCDB_DOWNVOTE;
    }
    else
    {
        vote.vote = SCDB_UPVOTE;
        vote.hash = uint256S(item->data(0, HashRole).toString().toStdString());
    }

    scdb.CacheCustomVotes(std::vector<SidechainCustomVote>{ vote });

    ui->treeWidgetVote->setUpdatesEnabled(true);

    UpdateNextTree();
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
