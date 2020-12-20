// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/sidechainwtprimedialog.h>
#include <qt/forms/ui_sidechainwtprimedialog.h>

#include <qt/platformstyle.h>
#include <qt/wtprimevotetablemodel.h>

#include <QMessageBox>
#include <QScrollBar>
#include <QTimer>

#include <sidechain.h>
#include <sidechaindb.h>
#include <util.h>
#include <validation.h>

enum DefaultWTPrimeVote {
    WTPRIME_UPVOTE = 0,
    WTPRIME_ABSTAIN = 1,
    WTPRIME_DOWNVOTE = 2,
};

SidechainWTPrimeDialog::SidechainWTPrimeDialog(const PlatformStyle *_platformStyle, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SidechainWTPrimeDialog),
    platformStyle(_platformStyle)
{
    ui->setupUi(this);

    wtPrimeVoteModel = new WTPrimeVoteTableModel(this);

    ui->tableViewWTPrimeVote->setModel(wtPrimeVoteModel);

    // Set resize mode for WT^ vote table
    ui->tableViewWTPrimeVote->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    // Don't stretch last cell of horizontal header
    ui->tableViewWTPrimeVote->horizontalHeader()->setStretchLastSection(false);

    // Hide vertical header
    ui->tableViewWTPrimeVote->verticalHeader()->setVisible(false);

    // Left align the horizontal header text
    ui->tableViewWTPrimeVote->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);

    // Set horizontal scroll speed to per 3 pixels (very smooth, default is awful)
    ui->tableViewWTPrimeVote->horizontalHeader()->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->tableViewWTPrimeVote->horizontalHeader()->horizontalScrollBar()->setSingleStep(3); // 3 Pixels

    // Select entire row
    ui->tableViewWTPrimeVote->setSelectionBehavior(QAbstractItemView::SelectRows);

    // If the user has WT^ vote parameters set, update the default vote combobox
    std::string strDefault = gArgs.GetArg("-defaultwtprimevote", "");
    if (strDefault == "upvote") {
        ui->comboBoxDefaultVote->setCurrentIndex(WTPRIME_UPVOTE);
    }
    else
    if (strDefault == "downvote") {
        ui->comboBoxDefaultVote->setCurrentIndex(WTPRIME_DOWNVOTE);
    }

    // Setup platform style single color icons

    ui->pushButtonUpvote->setIcon(platformStyle->SingleColorIcon(":/icons/ack"));
    ui->pushButtonDownvote->setIcon(platformStyle->SingleColorIcon(":/icons/nack"));
    ui->pushButtonAbstain->setIcon(platformStyle->SingleColorIcon(":/icons/replay_not_replayed"));
    ui->pushButtonHelp->setIcon(platformStyle->SingleColorIcon(":/icons/transaction_0"));
    ui->comboBoxDefaultVote->setItemIcon(0, platformStyle->SingleColorIcon(":/icons/ack"));
    ui->comboBoxDefaultVote->setItemIcon(1, platformStyle->SingleColorIcon(":/icons/replay_not_replayed"));
    ui->comboBoxDefaultVote->setItemIcon(2, platformStyle->SingleColorIcon(":/icons/nack"));

    // Start the poll timer to update the page
    pollTimer = new QTimer(this);
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(Update()));
    pollTimer->start(1000); // 1 second

    Update();
}

SidechainWTPrimeDialog::~SidechainWTPrimeDialog()
{
    delete ui;
}

void SidechainWTPrimeDialog::on_comboBoxDefaultVote_currentIndexChanged(const int i)
{
    if (i == WTPRIME_UPVOTE) {
        gArgs.ForceSetArg("-defaultwtprimevote", "upvote");
    }
    else
    if (i == WTPRIME_ABSTAIN) {
        gArgs.ForceSetArg("-defaultwtprimevote", "abstain");
    }
    else
    if (i == WTPRIME_DOWNVOTE) {
        gArgs.ForceSetArg("-defaultwtprimevote", "downvote");
    }
}

void SidechainWTPrimeDialog::on_pushButtonHelp_clicked()
{
    // TODO move text into static const
    QMessageBox::information(this, tr("DriveNet - information"),
        tr("Sidechain WT^ vote signalling:\n\n"
           "Use this page to set votes for WT^(s).\n\n"
           "Set Upvote to increase the work score of WT^(s) in blocks "
           "that you mine. Downvote to decrease the work score, and Abstain "
           "to ignore a WT^ and not change its workscore.\n\n"
           "You may also use the RPC command 'setwtprimevote' to set votes "
           "or 'clearwtprimevotes' to reset and erase any votes you have set."
           ),
        QMessageBox::Ok);
}

// TODO
// refactor all of the voting pushButton slots to use one function for
// setting WT^ vote type.

void SidechainWTPrimeDialog::on_pushButtonUpvote_clicked()
{
    // Set WT^ vote type

    QModelIndexList selected = ui->tableViewWTPrimeVote->selectionModel()->selectedIndexes();

    for (int i = 0; i < selected.size(); i++) {
        uint256 hash;
        unsigned int nSidechain;
        if (wtPrimeVoteModel->GetWTPrimeInfoAtRow(selected[i].row(), hash, nSidechain)) {
            SidechainCustomVote vote;
            vote.nSidechain = nSidechain;
            vote.hashWTPrime = hash;
            vote.vote = SCDB_UPVOTE;

            scdb.CacheCustomVotes(std::vector<SidechainCustomVote>{ vote });
        }
    }
}

void SidechainWTPrimeDialog::on_pushButtonDownvote_clicked()
{
    // Set WT^ vote type

    QModelIndexList selected = ui->tableViewWTPrimeVote->selectionModel()->selectedIndexes();

    for (int i = 0; i < selected.size(); i++) {
        uint256 hash;
        unsigned int nSidechain;
        if (wtPrimeVoteModel->GetWTPrimeInfoAtRow(selected[i].row(), hash, nSidechain)) {
            SidechainCustomVote vote;
            vote.nSidechain = nSidechain;
            vote.hashWTPrime = hash;
            vote.vote = SCDB_DOWNVOTE;

            scdb.CacheCustomVotes(std::vector<SidechainCustomVote>{ vote });
        }
    }
}

void SidechainWTPrimeDialog::on_pushButtonAbstain_clicked()
{
    // Set WT^ vote type

    QModelIndexList selected = ui->tableViewWTPrimeVote->selectionModel()->selectedIndexes();

    for (int i = 0; i < selected.size(); i++) {
        uint256 hash;
        unsigned int nSidechain;
        if (wtPrimeVoteModel->GetWTPrimeInfoAtRow(selected[i].row(), hash, nSidechain)) {
            SidechainCustomVote vote;
            vote.nSidechain = nSidechain;
            vote.hashWTPrime = hash;
            vote.vote = SCDB_ABSTAIN;

            scdb.CacheCustomVotes(std::vector<SidechainCustomVote>{ vote });
        }
    }
}

void SidechainWTPrimeDialog::Update()
{
    // Disable the default vote combo box if custom votes are set, enable it
    // if they are not.
    std::vector<SidechainCustomVote> vCustomVote = scdb.GetCustomVoteCache();
    bool fCustomVote = vCustomVote.size();
    ui->comboBoxDefaultVote->setEnabled(!fCustomVote);

    // Add a tip to the default vote label on how to reset them
    ui->labelClearVotes->setHidden(!fCustomVote);
}
