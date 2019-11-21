#include <qt/wtprimevotetablemodel.h>

#include <sidechain.h>
#include <sidechaindb.h>
#include <util.h>
#include <validation.h>

#include <QIcon>
#include <QMetaType>
#include <QPushButton>
#include <QTimer>
#include <QVariant>

#include <qt/guiconstants.h>
#include <qt/guiutil.h>

Q_DECLARE_METATYPE(WTPrimeVoteTableObject)

WTPrimeVoteTableModel::WTPrimeVoteTableModel(QObject *parent) :
    QAbstractTableModel(parent)
{
    // This timer will be fired repeatedly to update the model
    pollTimer = new QTimer(this);
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(UpdateModel()));
    pollTimer->start(MODEL_UPDATE_DELAY);
}

int WTPrimeVoteTableModel::rowCount(const QModelIndex & /*parent*/) const
{
    return model.size();
}

int WTPrimeVoteTableModel::columnCount(const QModelIndex & /*parent*/) const
{
    return 3;
}

QVariant WTPrimeVoteTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return false;
    }

    int row = index.row();
    int col = index.column();

    if (!model.at(row).canConvert<WTPrimeVoteTableObject>())
        return QVariant();

    WTPrimeVoteTableObject object = model.at(row).value<WTPrimeVoteTableObject>();

    switch (role) {
    case Qt::DisplayRole:
    {
        // Vote
        if (col == 0) {
            if (object.vote == SCDB_UPVOTE)
                return "Upvote";
            else
            if (object.vote == SCDB_ABSTAIN)
                return "Abstain";
            else
            if (object.vote == SCDB_DOWNVOTE)
                return "Downvote";
            else
                return "N/A";
        }
        // nSidechain
        if (col == 1) {
            return object.nSidechain;
        }
        // Hash of WT^
        if (col == 2) {
            return object.hash;
        }
    }
    }
    return QVariant();
}

QVariant WTPrimeVoteTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Horizontal) {
            switch (section) {
            case 0:
                return QString("Vote");
            case 1:
                return QString("SC Number");
            case 2:
                return QString("WT^ Hash");
            }
        }
    }
    return QVariant();
}

void WTPrimeVoteTableModel::UpdateModel()
{
    // TODO there are many ways to improve the efficiency of this

    // Get all of the current WT^(s) into one vector
    std::vector<Sidechain> vSidechain = scdb.GetActiveSidechains();
    std::vector<SidechainWTPrimeState> vWTPrime;
    for (const Sidechain& s : vSidechain) {
        std::vector<SidechainWTPrimeState> vState = scdb.GetState(s.nSidechain);
        vWTPrime.insert(vWTPrime.end(), vState.begin(), vState.end());
    }

    // Get users votes
    std::vector<SidechainCustomVote> vCustomVote = scdb.GetCustomVoteCache();

    bool fCustomVotes = vCustomVote.size();
    std::string strDefaultVote = gArgs.GetArg("-defaultwtprimevote", "abstain");

    char defaultVote;
    if (strDefaultVote == "upvote")
        defaultVote = SCDB_UPVOTE;
    else
    if (strDefaultVote == "downvote")
        defaultVote = SCDB_DOWNVOTE;
    else
        defaultVote = SCDB_ABSTAIN;

    // Look for updates to WT^(s) & their vote already cached by the model and
    // update our model / view.
    //
    // Also look for WT^(s) which have been removed, and remove them from our
    // model / view.
    std::vector<WTPrimeVoteTableObject> vRemoved;
    for (int i = 0; i < model.size(); i++) {
        if (!model[i].canConvert<WTPrimeVoteTableObject>())
            return;

        WTPrimeVoteTableObject object = model[i].value<WTPrimeVoteTableObject>();

        bool fFound = false;

        // Check if the WT^ should still be in the table and make sure we set
        // it with the current vote
        for (const SidechainWTPrimeState& s : vWTPrime) {
            // Check if we need to update the vote type
            if (s.hashWTPrime == uint256S(object.hash.toStdString())
                        && s.nSidechain == object.nSidechain) {
                fFound = true;
                if (fCustomVotes) {
                    // Check for updates to custom votes
                    for (const SidechainCustomVote& v : vCustomVote) {
                        if (v.nSidechain == s.nSidechain && v.hashWTPrime == s.hashWTPrime) {
                            if (object.vote != v.vote) {
                                // Update the vote type
                                object.vote = v.vote;

                                QModelIndex topLeft = index(i, 0);
                                QModelIndex topRight = index(i, columnCount() - 1);
                                Q_EMIT QAbstractItemModel::dataChanged(topLeft, topRight, {Qt::DecorationRole});

                                model[i] = QVariant::fromValue(object);
                            }
                        }
                    }
                } else {
                    // Check for updates to default vote
                    if (object.vote != defaultVote) {
                        // Update the vote type
                        object.vote = defaultVote;

                        QModelIndex topLeft = index(i, 0);
                        QModelIndex topRight = index(i, columnCount() - 1);
                        Q_EMIT QAbstractItemModel::dataChanged(topLeft, topRight, {Qt::DecorationRole});

                        model[i] = QVariant::fromValue(object);
                    }
                }
            }
        }

        // Add to vector of votes to be removed from model / view
        if (!fFound) {
            vRemoved.push_back(object);
        }
    }

    // Loop through the model and remove deleted votes
    for (int i = 0; i < model.size(); i++) {
        if (!model[i].canConvert<WTPrimeVoteTableObject>())
            return;

        WTPrimeVoteTableObject object = model[i].value<WTPrimeVoteTableObject>();

        for (const WTPrimeVoteTableObject& v : vRemoved) {
            if (v.hash == object.hash && v.nSidechain == object.nSidechain) {
                beginRemoveRows(QModelIndex(), i, i);
                model[i] = model.back();
                model.pop_back();
                endRemoveRows();
            }
        }
    }

    // Check for new WT^(s)
    std::vector<SidechainWTPrimeState> vNew;
    for (const SidechainWTPrimeState& s : vWTPrime) {
        bool fFound = false;

        for (const QVariant& qv : model) {
            if (!qv.canConvert<WTPrimeVoteTableObject>())
                return;

            WTPrimeVoteTableObject object = qv.value<WTPrimeVoteTableObject>();

            if (s.hashWTPrime == uint256S(object.hash.toStdString())
                    && s.nSidechain == object.nSidechain)
                fFound = true;
        }
        if (!fFound)
            vNew.push_back(s);
    }

    if (vNew.empty())
        return;

    // Add new WT^(s) if we need to - with correct vote type
    beginInsertRows(QModelIndex(), model.size(), model.size() + vNew.size() - 1);
    for (const SidechainWTPrimeState& s : vNew) {
        WTPrimeVoteTableObject object;

        // If custom votes are set, check to see if one is set for this WT^ and
        // if not set SCDB_ABSTAIN. If custom votes are not set, use the current
        // default vote
        if (fCustomVotes) {
            // Set vote to default abstain, and then look for a custom vote and
            // update the vote type if foudn
            object.vote = SCDB_ABSTAIN;
            for (const SidechainCustomVote& v : vCustomVote) {
                if (v.hashWTPrime == s.hashWTPrime && v.nSidechain == s.nSidechain) {
                    object.vote = v.vote;
                }
            }
        } else {
            object.vote = defaultVote;
        }

        // Insert new WT^ into table
        object.hash = QString::fromStdString(s.hashWTPrime.ToString());
        object.nSidechain = s.nSidechain;
        model.append(QVariant::fromValue(object));
    }
    endInsertRows();
}

bool WTPrimeVoteTableModel::GetWTPrimeInfoAtRow(int row, uint256& hash, unsigned int& nSidechain) const
{
    if (row >= model.size())
        return false;

    if (!model[row].canConvert<WTPrimeVoteTableObject>())
        return false;

    WTPrimeVoteTableObject object = model[row].value<WTPrimeVoteTableObject>();

    hash = uint256S(object.hash.toStdString());
    nSidechain = object.nSidechain;

    return true;
}
