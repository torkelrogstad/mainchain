// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <sidechaindb.h>

#include <consensus/consensus.h>
#include <consensus/merkle.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <sidechain.h>
#include <uint256.h>
#include <util.h> // For LogPrintf TODO move LogPrintf
#include <utilstrencodings.h>

SidechainDB::SidechainDB()
{
    Reset();
}

bool SidechainDB::ApplyLDBData(const uint256& hashBlock, const SidechainBlockData& data)
{
    hashBlockLastSeen = hashBlock;
    vWTPrimeStatus = data.vWTPrimeStatus;

    // TODO verify SCDB hash matches MT hash commit for block
    return true;
}

void SidechainDB::AddRemovedBMM(const uint256& hashRemoved)
{
    vRemovedBMM.push_back(hashRemoved);
}

void SidechainDB::AddRemovedDeposit(const uint256& hashRemoved)
{
    vRemovedDeposit.push_back(hashRemoved);
}

bool SidechainDB::AddDeposits(const std::vector<CTransaction>& vtx, const uint256& hashBlock, bool fJustCheck)
{
    // Note that we aren't splitting the deposits by nSidechain yet, that will
    // be done after verifying all of the deposits
    std::vector<SidechainDeposit> vDeposit;
    for (const CTransaction& tx : vtx) {
        // Create sidechain deposit objects from transaction outputs
        // We loop through the transaction outputs and look for both the burn
        // output to the sidechain scriptPubKey and the data output which has
        // the encoded destination keyID for the sidechain.

        SidechainDeposit deposit;
        bool fBurnFound = false;
        bool fFormatChecked = false;
        for (size_t i = 0; i < tx.vout.size(); i++) {
            const CScript &scriptPubKey = tx.vout[i].scriptPubKey;

            uint8_t nSidechain;
            if (HasSidechainScript(std::vector<CScript>{scriptPubKey}, nSidechain)) {
                // We found the burn output, copy the output index & nSidechain
                deposit.nSidechain = nSidechain;
                deposit.n = i;
                fBurnFound = true;
                continue;
            }

            // Move on to looking for the encoded keyID output

            if (scriptPubKey.front() != OP_RETURN)
                continue;
            if (scriptPubKey.size() != 22 && scriptPubKey.size() != 23)
                continue;

            CScript::const_iterator pkey = scriptPubKey.begin() + 1;
            opcodetype opcode;
            std::vector<unsigned char> vch;
            if (!scriptPubKey.GetOp(pkey, opcode, vch))
                continue;
            if (vch.size() != sizeof(uint160))
                continue;

            CKeyID keyID = CKeyID(uint160(vch));
            if (keyID.IsNull())
                continue;

            deposit.tx = tx;
            deposit.keyID = keyID;
            deposit.hashBlock = hashBlock;

            fFormatChecked = true;
        }
        // TODO Confirm single burn & single keyID OP_RETURN output
        if (fBurnFound && fFormatChecked && CTransaction(deposit.tx) == tx) {
            vDeposit.push_back(deposit);
        }
    }

    // Check that deposits can be sorted
    std::vector<SidechainDeposit> vDepositSorted;
    if (!SortDeposits(vDeposit, vDepositSorted))
        return false;

    if (fJustCheck)
        return true;

    // Add deposits to cache, note that this AddDeposit call will split deposits
    // by nSidechain and sort them
    AddDeposits(vDeposit, hashBlock);

    return true;
}

void SidechainDB::AddDeposits(const std::vector<SidechainDeposit>& vDeposit, const uint256& hashBlock)
{
    // Split the deposits by nSidechain - and double check them
    std::vector<std::vector<SidechainDeposit>> vDepositSplit;
    vDepositSplit.resize(vDepositCache.size());
    for (const SidechainDeposit& d : vDeposit) {
        if (!IsSidechainNumberValid(d.nSidechain))
            continue;
        if (HaveDepositCached(d))
            continue;
        // Put deposit into vector based on nSidechain
        vDepositSplit[d.nSidechain].push_back(d);
    }

    // Add the deposits to SCDB
    for (size_t x = 0; x < vDepositSplit.size(); x++) {
        for (size_t y = 0; y < vDepositSplit[x].size(); y++) {
            vDepositCache[x].push_back(vDepositSplit[x][y]);
        }
    }

    // Sort the deposits by CTIP UTXO spend order
    // TODO check return value
    SortSCDBDeposits();

    // Finally, update the CTIP for each nSidechain and log it
    UpdateCTIP(hashBlock);
}

bool SidechainDB::AddWTPrime(uint8_t nSidechain, const uint256& hashWTPrime, int nHeight, bool fDebug)
{
    if (!IsSidechainNumberValid(nSidechain)) {
        LogPrintf("SCDB %s: Rejected WT^: %s. Invalid sidechain number: %u\n",
                __func__,
                hashWTPrime.ToString());
        return false;
    }

    if (HaveWTPrimeWorkScore(hashWTPrime, nSidechain)) {
        LogPrintf("SCDB %s: Rejected WT^: %s already known\n",
                __func__,
                hashWTPrime.ToString());
        return false;
    }

    std::vector<SidechainWTPrimeState> vWT;

    SidechainWTPrimeState wt;
    wt.nSidechain = nSidechain;

    wt.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - 1;
    wt.nWorkScore = 1;
    wt.hashWTPrime = hashWTPrime;

    vWT.push_back(wt);

    if (fDebug)
        LogPrintf("SCDB %s: Cached WT^: %s\n", __func__, hashWTPrime.ToString());

    std::map<uint8_t, uint256> mapNewWTPrime;
    mapNewWTPrime[wt.nSidechain] = hashWTPrime;

    // TODO
    // Remove fSkipDEC
    bool fUpdated = UpdateSCDBIndex(vWT, nHeight, true /* fDebug */, mapNewWTPrime, true /* fSkipDEC */);

    if (!fUpdated && fDebug)
        LogPrintf("SCDB %s: Failed to update SCDBIndex.\n", __func__);

    return fUpdated;
}

void SidechainDB::AddSpentWTPrimes(const std::vector<SidechainSpentWTPrime>& vSpent)
{
    std::map<uint256, std::vector<SidechainSpentWTPrime>>::iterator it;

    for (const SidechainSpentWTPrime& spent : vSpent) {
        it = mapSpentWTPrime.find(spent.hashBlock);
        if (it != mapSpentWTPrime.end()) {
            it->second.push_back(spent);
        } else {
            mapSpentWTPrime[spent.hashBlock] = std::vector<SidechainSpentWTPrime>{ spent };
        }
    }
}

void SidechainDB::CacheActiveSidechains(const std::vector<Sidechain>& vActiveSidechainIn)
{
    vActiveSidechain = vActiveSidechainIn;

    // Resize vWTPrimeStatus to keep track of WT^(s)
    vWTPrimeStatus.resize(vActiveSidechain.size());

    // Resize vDepositCache to keep track of deposit(s)
    vDepositCache.resize(vActiveSidechain.size());
}

bool SidechainDB::CacheCustomVotes(const std::vector<SidechainCustomVote>& vCustomVote)
{
    // Check for valid vote type and non-null WT^ hash.
    for (const SidechainCustomVote& v : vCustomVote) {
        // Check WT^ hash is not null
        if (v.hashWTPrime.IsNull())
            return false;
        // Check that vote type is valid
        if (v.vote != SCDB_UPVOTE && v.vote != SCDB_DOWNVOTE
                && v.vote != SCDB_ABSTAIN)
        {
            return false;
        }
    }

    // For each vote passed in we want to check if it is an update to an
    // existing vote. If it is, update the old vote. If it is a new vote, add
    // it to the cache. If the new vote is for a sidechain that already has a
    // WT^ vote, remove the old vote.
    for (const SidechainCustomVote& v : vCustomVote) {
        bool fFound = false;
        for (size_t i = 0; i < vCustomVoteCache.size(); i++) {
            if (vCustomVoteCache[i].hashWTPrime == v.hashWTPrime &&
                    vCustomVoteCache[i].nSidechain == v.nSidechain)
            {
                vCustomVoteCache[i].vote = v.vote;
                fFound = true;
                break;
            }
        }
        if (!fFound) {
            // Check if there's already a WT^ vote for this sidechain and remove
            for (size_t i = 0; i < vCustomVoteCache.size(); i++) {
                if (vCustomVoteCache[i].nSidechain == v.nSidechain) {
                    vCustomVoteCache[i] = vCustomVoteCache.back();
                    vCustomVoteCache.pop_back();
                    break;
                }
            }
            vCustomVoteCache.push_back(v);
        }
    }
    // TODO right now this is accepting votes for any sidechain, whether active
    // or not. The function also accepts votes for WT^(s) that do not exist yet
    // or maybe never will. I'm not sure yet whether that behavior is the best.
    // Some miner may wish to set votes for a WT^ they create on a sidechain
    // before it's even added to the SCDB, but it also might be good to give
    // an error if they do in case it was an accident?
    //
    // We can update this function so that it returns false if some or all
    // votes were not cached, and also return an error string explaining why,
    // possibly along with a list of the vote(s) that weren't cached.
    return true;
}

void SidechainDB::CacheSidechainActivationStatus(const std::vector<SidechainActivationStatus>& vActivationStatusIn)
{
    vActivationStatus = vActivationStatusIn;
}

void SidechainDB::CacheSidechainProposals(const std::vector<SidechainProposal>& vSidechainProposalIn)
{
    for (const SidechainProposal& s : vSidechainProposalIn)
        vSidechainProposal.push_back(s);
}

void SidechainDB::CacheSidechainHashToActivate(const uint256& u)
{
    vSidechainHashActivate.push_back(u);
}

bool SidechainDB::CacheWTPrime(const CTransaction& tx)
{
    if (vActiveSidechain.empty())
        return false;
    if (HaveWTPrimeCached(tx.GetHash()))
        return false;

    vWTPrimeCache.push_back(tx);

    return true;
}

bool SidechainDB::CheckWorkScore(uint8_t nSidechain, const uint256& hashWTPrime, bool fDebug) const
{
    if (!IsSidechainNumberValid(nSidechain))
        return false;

    std::vector<SidechainWTPrimeState> vState = GetState(nSidechain);
    for (const SidechainWTPrimeState& state : vState) {
        if (state.hashWTPrime == hashWTPrime) {
            if (state.nWorkScore >= SIDECHAIN_MIN_WORKSCORE) {
                if (fDebug)
                    LogPrintf("SCDB %s: Approved: %s\n",
                            __func__,
                            hashWTPrime.ToString());
                return true;
            } else {
                if (fDebug)
                    LogPrintf("SCDB %s: Rejected: %s (insufficient work score)\n",
                            __func__,
                            hashWTPrime.ToString());
                return false;
            }
        }
    }
    if (fDebug)
        LogPrintf("SCDB %s: Rejected (WT^ state not found): %s\n",
                __func__,
                hashWTPrime.ToString());
    return false;
}

void SidechainDB::ClearRemovedBMM()
{
    vRemovedBMM.clear();
}

void SidechainDB::ClearRemovedDeposits()
{
    vRemovedDeposit.clear();
}

unsigned int SidechainDB::GetActiveSidechainCount() const
{
    return vActiveSidechain.size();
}

bool SidechainDB::GetActivateSidechain(const uint256& u) const
{
    // TODO change the container to make this more efficient
    for (const uint256& hash : vSidechainHashActivate) {
        if (u == hash) {
            return true;
        }
    }
    // Also check if we created the sidechain proposal, and ACK it
    for (const SidechainProposal& s : vSidechainProposal) {
        if (s.GetHash() == u) {
            return true;
        }
    }
    return false;
}

std::vector<Sidechain> SidechainDB::GetActiveSidechains() const
{
    return vActiveSidechain;
}

std::vector<uint256> SidechainDB::GetRemovedBMM() const
{
    return vRemovedBMM;
}

std::vector<uint256> SidechainDB::GetRemovedDeposits() const
{
    return vRemovedDeposit;
}

bool SidechainDB::GetCTIP(uint8_t nSidechain, SidechainCTIP& out) const
{
    if (!IsSidechainNumberValid(nSidechain))
        return false;

    std::map<uint8_t, SidechainCTIP>::const_iterator it = mapCTIP.find(nSidechain);
    if (it != mapCTIP.end()) {
        out = it->second;
        return true;
    }

    return false;
}

bool SidechainDB::GetCachedWTPrime(const uint256& hashWTPrime, CMutableTransaction& mtx) const
{
    // Find the WT^
    for (const CMutableTransaction& t : vWTPrimeCache) {
        if (t.GetHash() == hashWTPrime) {
            mtx = t;
            return true;
        }
    }
    return false;
}

std::map<uint8_t, SidechainCTIP> SidechainDB::GetCTIP() const
{
    return mapCTIP;
}

std::vector<SidechainCustomVote> SidechainDB::GetCustomVoteCache() const
{
    return vCustomVoteCache;
}

std::vector<SidechainDeposit> SidechainDB::GetDeposits(uint8_t nSidechain) const
{
    std::vector<SidechainDeposit> vDeposit;
    if (!IsSidechainNumberValid(nSidechain))
        return vDeposit;

    return vDepositCache[nSidechain];
}

std::vector<SidechainDeposit> SidechainDB::GetDeposits(const std::string& sidechainPriv) const
{
    // TODO refactor: only one GetDeposits function in SCDB

    // Make sure that the hash is related to an active sidechain,
    // and then return the result of the old function call.
    uint8_t nSidechain = 0;
    bool fFound = false;
    for (const Sidechain& s : vActiveSidechain) {
        if (s.sidechainPriv == sidechainPriv) {
            fFound = true;
            break;
        }
        nSidechain++;
    }

    if (!fFound)
        return std::vector<SidechainDeposit>{};

    return GetDeposits(nSidechain);
}

uint256 SidechainDB::GetHashBlockLastSeen()
{
    return hashBlockLastSeen;
}

uint256 SidechainDB::GetTotalSCDBHash() const
{
    // Note: This function is used for testing only right now, and is very noisy
    // in the log. If this function is to be used for non-testing in the future
    // the log messages should be commented out to be re-enabled for testing if
    // desired.
    std::vector<uint256> vLeaf;

    // Add mapCTIP
    std::map<uint8_t, SidechainCTIP>::const_iterator it;
    for (it = mapCTIP.begin(); it != mapCTIP.end(); it++) {
        vLeaf.push_back(it->second.GetHash());
    }

    uint256 hash = ComputeMerkleRoot(vLeaf);
    LogPrintf("%s: Hash with CTIP data: %s\n", __func__, hash.ToString());

    // Add hashBlockLastSeen
    vLeaf.push_back(hashBlockLastSeen);

    hash = ComputeMerkleRoot(vLeaf);
    LogPrintf("%s: Hash with hashBlockLastSeen data: %s\n", __func__, hash.ToString());

    // Add vActiveSidechain
    for (const Sidechain& s : vActiveSidechain) {
        vLeaf.push_back(s.GetHash());
    }

    hash = ComputeMerkleRoot(vLeaf);
    LogPrintf("%s: Hash with vActiveSidechain data: %s\n", __func__, hash.ToString());

    // Add vActivationStatus
    for (const SidechainActivationStatus& s : vActivationStatus) {
        vLeaf.push_back(s.GetHash());
    }

    hash = ComputeMerkleRoot(vLeaf);
    LogPrintf("%s: Hash with vActivationStatus data: %s\n", __func__, hash.ToString());

    // Add vDepositCache
    for (const std::vector<SidechainDeposit>& v : vDepositCache) {
        for (const SidechainDeposit& d : v) {
            vLeaf.push_back(d.GetHash());
        }
    }

    hash = ComputeMerkleRoot(vLeaf);
    LogPrintf("%s: Hash with vDepositCache data: %s\n", __func__, hash.ToString());

    // Add vSidechainHashActivate
    for (const uint256& u : vSidechainHashActivate) {
        vLeaf.push_back(u);
    }

    hash = ComputeMerkleRoot(vLeaf);
    LogPrintf("%s: Hash with vSidechainHashActivate data: %s\n", __func__, hash.ToString());

    // Add vSidechainProposal
    for (const SidechainProposal& p : vSidechainProposal) {
        vLeaf.push_back(p.GetHash());
    }

    hash = ComputeMerkleRoot(vLeaf);
    LogPrintf("%s: Hash with vSidechainProposal data: %s\n", __func__, hash.ToString());

    // Add vWTPrimeCache
    for (const CMutableTransaction& tx : vWTPrimeCache) {
        vLeaf.push_back(tx.GetHash());
    }

    hash = ComputeMerkleRoot(vLeaf);
    LogPrintf("%s: Hash with vWTPrimeCache data: %s\n", __func__, hash.ToString());

    // Add vWTPrimeStatus
    for (const Sidechain& s : vActiveSidechain) {
        std::vector<SidechainWTPrimeState> vState = GetState(s.nSidechain);
        for (const SidechainWTPrimeState& state : vState) {
            vLeaf.push_back(state.GetHash());
        }
    }

    hash = ComputeMerkleRoot(vLeaf);
    LogPrintf("%s: Hash with vWTPrimeStatus data (total hash): %s\n", __func__, hash.ToString());

    return ComputeMerkleRoot(vLeaf);
}

uint256 SidechainDB::GetSCDBHash() const
{
    if (vWTPrimeStatus.empty())
        return uint256();

    std::vector<uint256> vLeaf;
    for (const Sidechain& s : vActiveSidechain) {
        std::vector<SidechainWTPrimeState> vState = GetState(s.nSidechain);
        for (const SidechainWTPrimeState& state : vState) {
            vLeaf.push_back(state.GetHash());
        }
    }
    return ComputeMerkleRoot(vLeaf);
}

uint256 SidechainDB::GetSCDBHashIfUpdate(const std::vector<SidechainWTPrimeState>& vNewScores, int nHeight, const std::map<uint8_t, uint256>& mapNewWTPrime, bool fRemoveExpired) const
{
    SidechainDB scdbCopy = (*this);
    if (!scdbCopy.UpdateSCDBIndex(vNewScores, nHeight, false /* fDebug */, mapNewWTPrime, false, fRemoveExpired))
    {
        LogPrintf("%s: SCDB failed to get updated hash at height: %i\n", __func__, nHeight);
        return uint256();
    }
    return (scdbCopy.GetSCDBHash());
}

bool SidechainDB::GetSidechain(const uint8_t nSidechain, Sidechain& sidechain) const
{
    if (!IsSidechainNumberValid(nSidechain))
        return false;

    for (const Sidechain& s : vActiveSidechain) {
        if (s.nSidechain == nSidechain) {
            sidechain = s;
            return true;
        }
    }
    return false;
}

std::vector<SidechainActivationStatus> SidechainDB::GetSidechainActivationStatus() const
{
    return vActivationStatus;
}

std::string SidechainDB::GetSidechainName(uint8_t nSidechain) const
{
    std::string str = "UnknownSidechain";

    Sidechain sidechain;
    if (GetSidechain(nSidechain, sidechain))
        return sidechain.title;

    return str;
}

std::vector<SidechainProposal> SidechainDB::GetSidechainProposals() const
{
    return vSidechainProposal;
}

bool SidechainDB::GetSidechainScript(const uint8_t nSidechain, CScript& scriptPubKey) const
{
    Sidechain sidechain;
    if (!GetSidechain(nSidechain, sidechain))
        return false;

    std::vector<unsigned char> vch(ParseHex(sidechain.sidechainHex));
    scriptPubKey = CScript(vch.begin(), vch.end());

    return true;
}

std::vector<uint256> SidechainDB::GetSidechainsToActivate() const
{
    return vSidechainHashActivate;
}

std::vector<SidechainSpentWTPrime> SidechainDB::GetSpentWTPrimesForBlock(const uint256& hashBlock) const
{
    std::map<uint256, std::vector<SidechainSpentWTPrime>>::const_iterator it;
    it = mapSpentWTPrime.find(hashBlock);

    if (it != mapSpentWTPrime.end())
        return it->second;

    return std::vector<SidechainSpentWTPrime>{};
}

std::vector<SidechainWTPrimeState> SidechainDB::GetState(uint8_t nSidechain) const
{
    if (!HasState() || !IsSidechainNumberValid(nSidechain))
        return std::vector<SidechainWTPrimeState>();

    // TODO See comment in UpdateSCDBIndex about accessing vector by nSidechain
    return vWTPrimeStatus[nSidechain];
}

std::vector<std::vector<SidechainWTPrimeState>> SidechainDB::GetState() const
{
    return vWTPrimeStatus;
}

std::vector<uint256> SidechainDB::GetUncommittedWTPrimeCache(uint8_t nSidechain) const
{
    std::vector<uint256> vHash;
    // TODO update the container of WT^ cache, and only loop through the
    // correct sidechain's (based on nSidechain) WT^(s).
    for (const CTransaction& t : vWTPrimeCache) {
        uint256 txid = t.GetHash();
        if (!HaveWTPrimeWorkScore(txid, nSidechain)) {
            vHash.push_back(t.GetHash());
        }
    }
    return vHash;
}

std::vector<SidechainWTPrimeState> SidechainDB::GetLatestStateWithVote(const char& vote, const std::map<uint8_t, uint256>& mapNewWTPrime) const
{
    std::vector<SidechainWTPrimeState> vNew;
    for (const Sidechain& s : vActiveSidechain) {
        std::vector<SidechainWTPrimeState> vOld = GetState(s.nSidechain);

        if (!vOld.size())
            continue;

        // If there's a new WT^ for this sidechain we don't want to make any
        // votes as adding a new WT^ is a vote (they start with 1 workscore)
        std::map<uint8_t, uint256>::const_iterator it = mapNewWTPrime.find(s.nSidechain);
        if (it != mapNewWTPrime.end())
            continue;

        // Get the latest WT^ to apply vote to
        SidechainWTPrimeState latest = vOld.back();

        if (vote == SCDB_UPVOTE)
            latest.nWorkScore++;
        else
        if (vote == SCDB_DOWNVOTE && latest.nWorkScore > 0)
            latest.nWorkScore--;

        vNew.push_back(latest);
    }
    return vNew;
}

std::vector<CMutableTransaction> SidechainDB::GetWTPrimeCache() const
{
    return vWTPrimeCache;
}

std::vector<SidechainSpentWTPrime> SidechainDB::GetSpentWTPrimeCache() const
{
    std::vector<SidechainSpentWTPrime> vSpent;
    for (auto const& it : mapSpentWTPrime) {
        for (const SidechainSpentWTPrime& s : it.second)
            vSpent.push_back(s);
    }
    return vSpent;
}

bool SidechainDB::HasState() const
{
    // Make sure that SCDB is actually initialized
    if (vWTPrimeStatus.empty() || !GetActiveSidechainCount())
        return false;

    // Check if we have WT^ state
    for (auto i : vWTPrimeStatus) {
        if (!i.empty())
            return true;
    }

    if (vWTPrimeCache.size())
        return true;

    return false;
}

bool SidechainDB::HasSidechainScript(const std::vector<CScript>& vScript, uint8_t& nSidechain) const
{
    // Check if scriptPubKey is the deposit script of any active sidechains
    for (const CScript& scriptPubKey : vScript) {
        for (const Sidechain& s : vActiveSidechain) {
            if (HexStr(scriptPubKey) == s.sidechainHex) {
                nSidechain = s.nSidechain;
                return true;
            }
        }
    }
    return false;
}

bool SidechainDB::HaveDepositCached(const SidechainDeposit &deposit) const
{
    if (!IsSidechainNumberValid(deposit.nSidechain))
        return false;

    for (const SidechainDeposit& d : vDepositCache[deposit.nSidechain]) {
        if (d == deposit)
            return true;
    }
    return false;
}

bool SidechainDB::HaveWTPrimeCached(const uint256& hashWTPrime) const
{
    for (const CMutableTransaction& tx : vWTPrimeCache) {
        if (tx.GetHash() == hashWTPrime)
            return true;
    }
    return false;
}

bool SidechainDB::HaveWTPrimeWorkScore(const uint256& hashWTPrime, uint8_t nSidechain) const
{
    if (!IsSidechainNumberValid(nSidechain))
        return false;

    std::vector<SidechainWTPrimeState> vState = GetState(nSidechain);
    for (const SidechainWTPrimeState& state : vState) {
        if (state.hashWTPrime == hashWTPrime)
            return true;
    }
    return false;
}

bool SidechainDB::IsSidechainNumberValid(uint8_t nSidechain) const
{
    if (nSidechain >= vActiveSidechain.size())
        return false;

    if (nSidechain >= vWTPrimeStatus.size())
        return false;

    if (nSidechain >= vDepositCache.size())
        return false;

    for (const Sidechain& s : vActiveSidechain) {
        if (s.nSidechain == nSidechain)
            return true;
    }

    return false;
}

void SidechainDB::RemoveExpiredWTPrimes()
{
    for (size_t x = 0; x < vWTPrimeStatus.size(); x++) {
        vWTPrimeStatus[x].erase(std::remove_if(
                    vWTPrimeStatus[x].begin(), vWTPrimeStatus[x].end(),
                    [](const SidechainWTPrimeState& state)
                    {
                        if (state.nBlocksLeft == 0) {
                            LogPrintf("SCDB RemoveExpiredWTPrimes: Erasing expired WT^: %s\n",
                                    state.ToString());
                            return true;
                        } else {
                            return false;
                        }
                    }),
                    vWTPrimeStatus[x].end());
    }
}

void SidechainDB::RemoveSidechainHashToActivate(const uint256& u)
{
    // TODO change container to make this efficient
    for (size_t i = 0; i < vSidechainHashActivate.size(); i++) {
        if (vSidechainHashActivate[i] == u) {
            vSidechainHashActivate[i] = vSidechainHashActivate.back();
            vSidechainHashActivate.pop_back();
        }
    }
}

void SidechainDB::ResetWTPrimeState()
{
    // Clear out WT^ state
    vWTPrimeStatus.clear();
    vWTPrimeStatus.resize(vActiveSidechain.size());
}

void SidechainDB::ResetWTPrimeVotes()
{
    vCustomVoteCache.clear();
}

void SidechainDB::Reset()
{
    // Clear out CTIP data
    mapCTIP.clear();

    // Reset hashBlockLastSeen
    hashBlockLastSeen.SetNull();

    // Clear out active sidechains
    vActiveSidechain.clear();

    // Clear out sidechain activation status
    vActivationStatus.clear();

    // Clear out our cache of sidechain deposits
    vDepositCache.clear();

    // Clear out list of sidechain (hashes) we want to ACK
    vSidechainHashActivate.clear();

    // Clear out our cache of sidechain proposals
    vSidechainProposal.clear();

    // Clear out cached WT^ serializations
    vWTPrimeCache.clear();

    // Clear out WT^ state
    ResetWTPrimeState();

    // Clear out custom vote cache
    vCustomVoteCache.clear();

    // Clear out spent WT^ cache
    mapSpentWTPrime.clear();
}

bool SidechainDB::SpendWTPrime(uint8_t nSidechain, const uint256& hashBlock, const CTransaction& tx, bool fJustCheck, bool fDebug)
{
    if (!IsSidechainNumberValid(nSidechain)) {
        if (fDebug) {
            LogPrintf("SCDB %s: Cannot spend WT^ (txid): %s for sidechain number: %u.\n Invalid sidechain number.\n",
                __func__,
                tx.GetHash().ToString(),
                nSidechain);
        }
        return false;
    }

    uint256 hashBlind;
    if (!tx.GetBWTHash(hashBlind)) {
        if (fDebug) {
            LogPrintf("SCDB %s: Cannot spend WT^ (txid): %s for sidechain number: %u.\n Cannot get blind hash.\n",
                __func__,
                tx.GetHash().ToString(),
                nSidechain);
        }
        return false;
    }

    if (!CheckWorkScore(nSidechain, hashBlind, fDebug)) {
        if (fDebug) {
            LogPrintf("SCDB %s: Cannot spend WT^: %s for sidechain number: %u. CheckWorkScore() failed.\n",
                __func__,
                hashBlind.ToString(),
                nSidechain);
        }
        return false;
    }

    // Find the required single output returning to the sidechain script
    bool fReturnFound = false;
    uint32_t n = 0;
    uint8_t nSidechainScript;
    for (size_t i = 0; i < tx.vout.size(); i++) {
        const CScript &scriptPubKey = tx.vout[i].scriptPubKey;
        if (HasSidechainScript(std::vector<CScript>{scriptPubKey}, nSidechainScript)) {
            if (fReturnFound) {
                // We already found a sidechain script output. This second
                // sidechain output makes the WT^ invalid.
                if (fDebug) {
                    LogPrintf("SCDB %s: Cannot spend WT^: %s for sidechain number: %u. Multiple sidechain return outputs in WT^.\n",
                        __func__,
                        hashBlind.ToString(),
                        nSidechain);
                }
                return false;
            }

            // Copy output index of deposit burn and move on
            n = i;
            fReturnFound = true;
            continue;
        }
    }

    // Make sure that the sidechain output was found
    if (!fReturnFound) {
        if (fDebug) {
            LogPrintf("SCDB %s: Cannot spend WT^: %s for sidechain number: %u. No sidechain return output in WT^.\n",
                __func__,
                hashBlind.ToString(),
                nSidechain);
        }
        return false;
    }

    // Make sure that the sidechain output is to the correct sidechain
    if (nSidechainScript != nSidechain) {
        if (fDebug) {
            LogPrintf("SCDB %s: Cannot spend WT^: %s for sidechain number: %u. Return output to incorrect nSidechain: %u in WT^.\n",
                __func__,
                hashBlind.ToString(),
                nSidechain,
                nSidechainScript);
        }
        return false;
    }

    if (nSidechain >= vWTPrimeStatus.size()) {
        if (fDebug) {
            LogPrintf("SCDB %s: Cannot spend WT^: %s for sidechain number: %u. WT^ status for sidechain not found.\n",
                __func__,
                hashBlind.ToString(),
                nSidechain);
        }
        return false;
    }

    if (fJustCheck)
        return true;

    // Create a sidechain deposit object for the return amount
    SidechainDeposit deposit;
    deposit.nSidechain = nSidechain;
    deposit.keyID = CKeyID(uint160(ParseHex("1111111111111111111111111111111111111111")));
    deposit.tx = tx;
    deposit.n = n;
    deposit.hashBlock = hashBlock;

    // This will also update the SCDB CTIP
    AddDeposits(std::vector<SidechainDeposit>{deposit}, hashBlock);

    // TODO In the event that the block which spent a WT^ is disconnected, a
    // miner will no longer have the raw WT^ transaction to create a WT^ payout
    // in the block that replaces the disconnected block. They will either have
    // to get it from the sidechain again, or this code can be changed to not
    // removed spent WT^(s) until some number of blocks after it has been spent
    // to avoid this issue. Another option is to cache all received WT^ raw txns
    // until the miner manually clears them out with an RPC command or similar.
    //
    // Find the cached transaction for the WT^ we spent and remove it
    for (size_t i = 0; i < vWTPrimeCache.size(); i++) {
        if (vWTPrimeCache[i].GetHash() == hashBlind) {
            vWTPrimeCache[i] = vWTPrimeCache.back();
            vWTPrimeCache.pop_back();
        }
    }

    SidechainSpentWTPrime spent;
    spent.nSidechain = nSidechain;
    spent.hashWTPrime = hashBlind;
    spent.hashBlock = hashBlock;

    // Track the spent WT^
    AddSpentWTPrimes(std::vector<SidechainSpentWTPrime>{ spent });

    // The WT^ will be removed from SCDB when SCDB::Update() is called now that
    // it has been marked as spent.

    LogPrintf("%s WT^ spent: %s for sidechain number: %u.\n", __func__, hashBlind.ToString(), nSidechain);

    return true;
}

std::string SidechainDB::ToString() const
{
    std::string str;
    str += "SidechainDB:\n";

    str += "Hash of block last seen: " + hashBlockLastSeen.ToString() + "\n";

    str += "Active sidechains: ";
    str += std::to_string(vActiveSidechain.size());
    str += "\n";
    for (const Sidechain& s : vActiveSidechain) {
        // Print sidechain name
        str += "Sidechain: " + s.GetSidechainName() + "\n";

        // Print sidechain WT^ workscore(s)
        std::vector<SidechainWTPrimeState> vState = GetState(s.nSidechain);
        str += "WT^(s): ";
        str += std::to_string(vState.size());
        str += "\n";
        for (const SidechainWTPrimeState& state : vState) {
            str += "WT^:\n";
            str += state.ToString();
        }
        str += "\n";

        // Print CTIP
        SidechainCTIP ctip;
        str += "CTIP:\n";
        if (GetCTIP(s.nSidechain, ctip)) {
            str += "txid: " + ctip.out.hash.ToString() + "\n";
            str += "n: " + std::to_string(ctip.out.n) + "\n";
            str += "amount: " + std::to_string(ctip.amount) + "\n";
        } else {
            str += "No CTIP found for sidechain.\n";
        }
        str += "\n";
    }

    str += "Sidechain proposal activation status:\n";

    if (!vActivationStatus.size())
        str += "No sidechain proposal status.\n";
    for (const SidechainActivationStatus& s : vActivationStatus) {
        str += s.proposal.ToString();
        str += "age: " + std::to_string(s.nAge) += "\n";
        str += "fails: " + std::to_string(s.nFail) += "\n";
    }
    str += "\n";

    return str;
}

// TODO remove bool fResync

bool SidechainDB::Update(int nHeight, const uint256& hashBlock, const uint256& hashPrevBlock, const std::vector<CTxOut>& vout, bool fJustCheck, bool fDebug, bool fResync)
{
    // Make a copy of SCDB to test update
    SidechainDB scdbCopy = (*this);
    if (scdbCopy.ApplyUpdate(nHeight, hashBlock, hashPrevBlock, vout, fJustCheck, fDebug, fResync)) {
        return ApplyUpdate(nHeight, hashBlock, hashPrevBlock, vout, fJustCheck, fDebug, fResync);
    } else {
        return false;
    }
}

bool SidechainDB::ApplyUpdate(int nHeight, const uint256& hashBlock, const uint256& hashPrevBlock, const std::vector<CTxOut>& vout, bool fJustCheck, bool fDebug, bool fResync)
{
    if (hashBlock.IsNull()) {
        if (fDebug)
            LogPrintf("SCDB %s: Failed: block hash is null at height: %u\n",
                    __func__,
                    nHeight);
        return false;
    }

    if (!hashBlockLastSeen.IsNull() && hashPrevBlock.IsNull())
    {
        if (fDebug)
            LogPrintf("SCDB %s: Failed: previous block hash null at height: %u\n",
                    __func__,
                    nHeight);
        return false;
    }

    if (!vout.size())
    {
        if (fDebug)
            LogPrintf("SCDB %s: Failed: empty coinbase transaction at height: %u\n",
                    __func__,
                    nHeight);
        return false;
    }

    if (!fResync && !hashBlockLastSeen.IsNull() && hashPrevBlock != hashBlockLastSeen) {
        if (fDebug)
            LogPrintf("SCDB %s: Failed: previous block hash: %s does not match hashBlockLastSeen: %s at height: %u\n",
                    __func__,
                    hashPrevBlock.ToString(),
                    hashBlockLastSeen.ToString(),
                    nHeight);
        return false;
    }

    // Scan for updated SCDB MT hash commit
    std::vector<CScript> vMTHashScript;
    for (const CTxOut& out : vout) {
        const CScript& scriptPubKey = out.scriptPubKey;
        if (scriptPubKey.IsSCDBHashMerkleRootCommit())
            vMTHashScript.push_back(scriptPubKey);
    }

    // Verify that there is only one MT hash commit if any
    if (vMTHashScript.size() > 1) {
        if (fDebug) {
            LogPrintf("SCDB %s: Error: Multiple MT commits at height: %u\n",
                __func__,
                nHeight);
        }
        return false;
    }

    // TODO IsSCDBHashMerkleRootCommit should return the MT hash
    uint256 hashMerkleRoot;
    if (vMTHashScript.size()) {
        // Get MT hash from script
        const CScript& scriptPubKey = vMTHashScript.front();
        hashMerkleRoot = uint256(std::vector<unsigned char>(scriptPubKey.begin() + 6, scriptPubKey.begin() + 38));
    }

    // If there's a MT hash commit in this block, it must be different than
    // the current SCDB hash (WT^ blocks remaining should have at least
    // been updated if nothing else)
    if (!hashMerkleRoot.IsNull() && GetSCDBHash() == hashMerkleRoot) {
        if (fDebug)
            LogPrintf("SCDB %s: Invalid (equal) merkle root hash: %s at height: %u\n",
                    __func__,
                    hashMerkleRoot.ToString(),
                    nHeight);
        return false;
    }

    /*
     * Look for data relevant to SCDB in this block's coinbase.
     *
     * Scan for new WT^(s) and start tracking them.
     *
     * Scan for updated SCDB MT hash, and perform MT hash based SCDB update.
     *
     * Scan for sidechain proposals & sidechain activation commitments.
     *
     * Update hashBlockLastSeen.
     */

    // Scan for sidechain proposal commitments
    std::vector<SidechainProposal> vProposal;
    if (!fResync && !fJustCheck) {
        for (const CTxOut& out : vout) {
            const CScript& scriptPubKey = out.scriptPubKey;

            if (!scriptPubKey.IsSidechainProposalCommit())
                continue;

            SidechainProposal proposal;
            if (!proposal.DeserializeFromScript(scriptPubKey))
                continue;

            // Check for duplicate
            bool fDuplicate = false;
            for (const SidechainActivationStatus& s : vActivationStatus) {
                if (s.proposal == proposal) {
                    fDuplicate = true;
                    break;
                }
            }
            if (fDuplicate)
                continue;

            vProposal.push_back(proposal);
        }
    }
    if (!fJustCheck && !fResync && vProposal.size() == 1) {
        SidechainActivationStatus status;
        status.nFail = 0;
        status.nAge = 0;
        status.proposal = vProposal.front();

        // Make sure that the proposal is unique,
        bool fUnique = true;

        // check the activation status cache
        for (const SidechainActivationStatus& s : vActivationStatus) {
            if (s.proposal == status.proposal) {
                fUnique = false;
                break;
            }
        }
        // check the active sidechain list
        for (const Sidechain& s : vActiveSidechain) {
            // Note that we are comparing a Sidechain to a SidechainProposal.
            // There is a custom operator== for this purpose.
            if (s == status.proposal) {
                fUnique = false;
                break;
            }
        }

        if (fUnique) {
            LogPrintf("SCDB %s: Tracking new sidechain proposal:\n%s\n",
                    __func__,
                    status.proposal.ToString());

            // Start tracking the new sidechain proposal
            vActivationStatus.push_back(status);
        }
    }

    // Scan for sidechain activation commitments
    std::vector<uint256> vActivationHash;
    if (!fResync) {
        for (const CTxOut& out : vout) {
            const CScript& scriptPubKey = out.scriptPubKey;
            uint256 hashSidechain;
            if (!scriptPubKey.IsSidechainActivationCommit(hashSidechain))
                continue;
            if (hashSidechain.IsNull())
                continue;

            vActivationHash.push_back(hashSidechain);
        }
    }
    if (!fJustCheck && !fResync)
        UpdateActivationStatus(vActivationHash);

    // Scan for new WT^(s) and start tracking them
    std::map<uint8_t, uint256> mapNewWTPrime;
    for (const CTxOut& out : vout) {
        const CScript& scriptPubKey = out.scriptPubKey;
        uint8_t nSidechain;
        uint256 hashWTPrime;
        if (scriptPubKey.IsWTPrimeHashCommit(hashWTPrime, nSidechain)) {
            if (!IsSidechainNumberValid(nSidechain)) {
                if (fDebug)
                    LogPrintf("SCDB %s: Skipping new WT^: %s, invalid sidechain number: %u\n",
                            __func__,
                            hashWTPrime.ToString(),
                            nSidechain);
                continue;
            }

            if (!fJustCheck && !AddWTPrime(nSidechain, hashWTPrime, nHeight, fDebug)) {
                if (fDebug) {
                    LogPrintf("SCDB %s: Failed to cache WT^: %s for sidechain number: %u at height: %u\n",
                            __func__,
                            hashWTPrime.ToString(),
                            nSidechain,
                            nHeight);
                }
                return false;
            }

            // Check that there is only 1 new WT^ per sidechain per block
            std::map<uint8_t, uint256>::const_iterator it = mapNewWTPrime.find(nSidechain);
            if (it == mapNewWTPrime.end()) {
                mapNewWTPrime[nSidechain] = hashWTPrime;
            } else {
                if (fDebug) {
                    LogPrintf("SCDB %s: Multiple new WT^ for sidechain number: %u at height: %u\n",
                            __func__,
                            nSidechain,
                            nHeight);
                }
                return false;
            }
        }
    }

    // Update SCDB to match new SCDB MT (hashMerkleRoot) from block
    if (!fJustCheck && !hashMerkleRoot.IsNull()) {

        // Check if there are update bytes
        std::vector<CScript> vUpdateBytes;
        for (const CTxOut& out : vout) {
            const CScript scriptPubKey = out.scriptPubKey;
            if (scriptPubKey.IsSCDBUpdate())
                vUpdateBytes.push_back(scriptPubKey);
        }
        // There is a maximum of 1 update bytes script
        if (vUpdateBytes.size() > 1) {
            if (fDebug)
                LogPrintf("SCDB %s: Error: multiple update byte scripts at height: %u\n",
                       __func__,
                       nHeight);
            return false;
        }

        std::vector<SidechainWTPrimeState> vNewScores;
        if (vUpdateBytes.size()) {
            // Get old (current) state
            std::vector<std::vector<SidechainWTPrimeState>> vOldState;
            for (const Sidechain& s : vActiveSidechain) {
                vOldState.push_back(GetState(s.nSidechain));
            }

            // Parse SCDB update bytes for new WT^ scores
            if (!ParseSCDBUpdateScript(vUpdateBytes.front(), vOldState, vNewScores)) {
                if (fDebug)
                    LogPrintf("SCDB %s: Error: Failed to parse update bytes at height: %u\n",
                            __func__,
                            nHeight);
                return false;
            }
            if (fDebug)
                LogPrintf("SCDB %s: Parsed update bytes at height: %u\n",
                        __func__,
                        nHeight);
        }

        bool fUpdated = UpdateSCDBMatchMT(nHeight, hashMerkleRoot, vNewScores, mapNewWTPrime);
        if (!fUpdated) {
            if (fDebug)
                LogPrintf("SCDB %s: Failed to match MT: %s at height: %u\n",
                        __func__,
                        hashMerkleRoot.ToString(),
                        nHeight);
            return false;
        }
    }

    if (!fJustCheck && hashMerkleRoot.IsNull()) {
        if (fDebug)
            LogPrintf("SCDB %s: hashMerkleRoot is null - applying default update!\n",
                    __func__);

        ApplyDefaultUpdate();
    }

    // Remove any WT^(s) that were spent in this block. This can happen when a
    // new block is connected, re-connected, or during SCDB resync.
    std::vector<SidechainSpentWTPrime> vSpent;
    vSpent = GetSpentWTPrimesForBlock(hashBlock);
    for (const SidechainSpentWTPrime& s : vSpent) {
        if (!IsSidechainNumberValid(s.nSidechain)) {
            if (fDebug) {
                LogPrintf("SCDB %s: Spent WT^ has invalid sidechain number: %u at height: %u\n",
                        __func__,
                        s.nSidechain,
                        nHeight);
            }
            return false;
        }
        bool fRemoved = false;
        for (size_t i = 0; i < vWTPrimeStatus[s.nSidechain].size(); i++) {
            const SidechainWTPrimeState wt = vWTPrimeStatus[s.nSidechain][i];
            if (wt.nSidechain == s.nSidechain &&
                    wt.hashWTPrime == s.hashWTPrime) {

                if (fDebug && !fJustCheck) {
                    LogPrintf("SCDB %s: Removing spent WT^: %s for nSidechain: %u in block %s.\n",
                            __func__,
                            wt.hashWTPrime.ToString(),
                            wt.nSidechain,
                            hashBlock.ToString());
                }

                fRemoved = true;
                if (fJustCheck)
                    break;

                // Remove the spent WT^
                vWTPrimeStatus[s.nSidechain][i] = vWTPrimeStatus[s.nSidechain].back();
                vWTPrimeStatus[s.nSidechain].pop_back();

                break;
            }
        }
        if (!fRemoved) {
            if (fDebug) {
                LogPrintf("SCDB %s: Failed to remove spent WT^: %s for sidechain: %u at height: %u\n",
                        __func__,
                        s.hashWTPrime.ToString(),
                        s.nSidechain,
                        nHeight);
            }
            return false;
        }
    }


    if (fDebug && !fJustCheck) {
        LogPrintf("SCDB: %s: Updated from block %s to block %s.\n",
                __func__,
                hashBlockLastSeen.ToString(),
                hashBlock.ToString());
    }

    // Update hashBLockLastSeen
    if (!fJustCheck)
        hashBlockLastSeen = hashBlock;

    return true;
}

bool SidechainDB::Undo(int nHeight, const uint256& hashBlock, const uint256& hashPrevBlock, const std::vector<CTransactionRef>& vtx, bool fDebug)
{
    // WT^ workscore is recalculated by ResyncSCDB in validation - not here

    if (!vtx.size()) {
        LogPrintf("%s: SCDB undo failed for block: %s - vtx is empty!\n", __func__, hashBlock.ToString());
        return false;
    }

    // Remove cached WT^ spends from the block that was disconnected
    std::map<uint256, std::vector<SidechainSpentWTPrime>>::const_iterator it;
    it = mapSpentWTPrime.find(hashBlock);

    if (it != mapSpentWTPrime.end())
        mapSpentWTPrime.erase(it);

    // Undo deposits
    // Loop through the transactions in the block being disconnected, and if
    // they match a transaction in our deposit cache remove it.
    bool fDepositRemoved = false;
    for (const CTransactionRef& tx : vtx) {
        for (size_t x = 0; x < vDepositCache.size(); x++) {
            for (size_t y = 0; y < vDepositCache[x].size(); y++) {
                if (*tx == CTransaction(vDepositCache[x][y].tx)) {
                    vDepositCache[x][y] = vDepositCache[x].back();
                    vDepositCache[x].pop_back();
                    fDepositRemoved = true;
                }
            }
        }
    }

    // If any deposits were removed re-sort deposits and update CTIP
    if (fDepositRemoved) {
        // TODO check return value
        SortSCDBDeposits();
        UpdateCTIP(hashBlock);
    }

    // TODO
    // Undo sidechain activation & de-activate a sidechain if it was activated
    // in the disconnected block. If a sidechain was de-activated then we will
    // also need to add it back to vActivationStatus and restore it's score

    // Remove sidechain proposals that were committed in the disconnected block
    for (const CTxOut& out : vtx[0]->vout) {
        const CScript& scriptPubKey = out.scriptPubKey;

        if (!scriptPubKey.IsSidechainProposalCommit())
            continue;

        SidechainProposal proposal;
        if (!proposal.DeserializeFromScript(scriptPubKey))
            continue;

        bool fRemoved = false;

        // Remove from SCDB
        for (size_t i = 0; i < vActivationStatus.size(); i++) {
            if (vActivationStatus[i].proposal == proposal) {
                vActivationStatus[i] = vActivationStatus.back();
                vActivationStatus.pop_back();
                fRemoved = true;
            }
        }

        // TODO If we are disconnecting a block that had a proposal we should
        // probably actually return an error here if vActivationStatus does
        // not contain the proposal.
        if (!fRemoved && vActivationStatus.size()) {
            LogPrintf("%s: SCDB failed to remove sidechain proposal from block: %s.\n", __func__, hashBlock.ToString());
            LogPrintf("%s: vActivationStatus size: %u", __func__, vActivationStatus.size());
            return false;
        }
    }

    // Undo hashBlockLastSeen
    hashBlockLastSeen = hashPrevBlock;

    LogPrintf("%s: SCDB undo for block: %s complete!\n", __func__, hashBlock.ToString());

    return true;
}

// TODO remove unused nHeight
bool SidechainDB::UpdateSCDBIndex(const std::vector<SidechainWTPrimeState>& vNewScores, int nHeight, bool fDebug, const std::map<uint8_t, uint256>& mapNewWTPrime, bool fSkipDec, bool fRemoveExpired)
{
    if (vWTPrimeStatus.empty()) {
        if (fDebug)
            LogPrintf("SCDB %s: Update failed: vWTPrimeStatus is empty!\n",
                    __func__);
        return false;
    }

    // First check that sidechain numbers are valid
    for (const SidechainWTPrimeState& s : vNewScores) {
        if (!IsSidechainNumberValid(s.nSidechain)) {
            if (fDebug)
                LogPrintf("SCDB %s: Update failed! Invalid sidechain number: %u\n",
                        __func__,
                        s.nSidechain);
            return false;
        }
    }

    // Decrement nBlocksLeft of existing WT^(s) -- don't mess with new WT^(s)
    // x = nsidechain y = WT^
    if (!fSkipDec)
    {
        // Remove expired WT^(s) if fRemoveExpired is set (used by the miner)
        if (fRemoveExpired)
            RemoveExpiredWTPrimes();

        for (size_t x = 0; x < vWTPrimeStatus.size(); x++) {
            std::map<uint8_t, uint256>::const_iterator it = mapNewWTPrime.find(x);
            uint256 hashNewWTPrime;
            if (it != mapNewWTPrime.end())
                hashNewWTPrime = it->second;

            for (size_t y = 0; y < vWTPrimeStatus[x].size(); y++) {
                if (vWTPrimeStatus[x][y].hashWTPrime != hashNewWTPrime) {
                    if (vWTPrimeStatus[x][y].nBlocksLeft > 0) {
                        vWTPrimeStatus[x][y].nBlocksLeft--;
                    }
                }
            }
        }
    }

    // Keep track of which (if any) WT^ was upvoted for each sidechain. Later
    // we will downvote all of the other WT^(s) for a sidechain if any WT^ for
    // that sidechain was upvoted. Upvoting 1 WT^ also means downvoting all of
    // the rest. The vector is the size of vWTPrimeState - the number of active
    // sidechains.
    std::vector<uint256> vWTPrimeUpvoted;
    vWTPrimeUpvoted.resize(vWTPrimeStatus.size());

    // Apply new work scores / add new WT^(s)
    for (const SidechainWTPrimeState& s : vNewScores) {

        // TODO
        // Refactor this and any other sidechain related code that access
        // vectors with the [] operator based on nSidechain. nSidechain has been
        // checked but this could still be improved.

        size_t x = s.nSidechain;

        // Check nSidechain again
        if (!IsSidechainNumberValid(x)) {
            if (fDebug)
                LogPrintf("SCDB %s: Update failed! Invalid sidechain number (double check): %u\n",
                        __func__,
                        s.nSidechain);
            return false;
        }

        // Track whether we already have a score for the WT^ specified. If not
        // then cache the new WT^ if it is valid.
        bool fFound = false;

        // If a new WT^ was added for this sidechain, that is the WT^ being
        // upvoted and no other scores matter (or should exist)
        std::map<uint8_t, uint256>::const_iterator it = mapNewWTPrime.find(x);

        // If no new WT^ for this sidechain was found, apply new scores
        if (it == mapNewWTPrime.end()) {
            for (size_t y = 0; y < vWTPrimeStatus[x].size(); y++) {
                const SidechainWTPrimeState state = vWTPrimeStatus[x][y];

                if (state.hashWTPrime == s.hashWTPrime) {
                    // We have received an update for an existing WT^ in SCDB
                    fFound = true;
                    // Make sure the score increment / decrement is valid.
                    // The score can only change by 1 point per block.
                    if ((state.nWorkScore == s.nWorkScore) ||
                            (s.nWorkScore == (state.nWorkScore + 1)) ||
                            (s.nWorkScore == (state.nWorkScore - 1)))
                    {
                        // TODO We shouldn't add any new scores until we have
                        // first verified all of the updates. Don't apply the
                        // updates as we loop.

                        if (s.nWorkScore == state.nWorkScore + 1) {
                            if (!vWTPrimeUpvoted[x].IsNull()) {
                                if (fDebug)
                                    LogPrintf("SCDB %s: Error: multiple WT^ upvotes for one sidechain!\n", __func__);
                                return false;
                            }
                            vWTPrimeUpvoted[x] = state.hashWTPrime;
                        }

                        // Too noisy but can be re-enabled for debugging
                        //if (fDebug)
                        //    LogPrintf("SCDB %s: WT^ work  score updated: %s %u->%u\n",
                        //            __func__,
                        //            state.hashWTPrime.ToString(),
                        //            vWTPrimeStatus[x][y].nWorkScore,
                        //            s.nWorkScore);
                        vWTPrimeStatus[x][y].nWorkScore = s.nWorkScore;
                    }
                }
            }
        }

        // If the WT^ wasn't found, check if it is a valid new WT^ and cache it
        if (!fFound) {
            if (s.nWorkScore != 1) {
                if (fDebug)
                    LogPrintf("SCDB %s: Rejected new WT^: %s. Invalid initial workscore (not 1): %u\n",
                            __func__,
                            s.hashWTPrime.ToString(),
                            s.nWorkScore);
                continue;
            }

            if (s.nBlocksLeft != SIDECHAIN_VERIFICATION_PERIOD - 1) {
                if (fDebug)
                    LogPrintf("SCDB %s: Rejected new WT^: %s. Invalid initial nBlocksLeft (not %u): %u\n",
                            __func__,
                            s.hashWTPrime.ToString(),
                            SIDECHAIN_VERIFICATION_PERIOD,
                            s.nBlocksLeft);
                continue;
            }

            // Check a third time...
            if (!IsSidechainNumberValid(x)) {
                if (fDebug)
                    LogPrintf("SCDB %s: Rejected new WT^: %s. Invalid sidechain number: %u\n",
                            __func__,
                            s.hashWTPrime.ToString(),
                            s.nSidechain);
                continue;
            }

            // Make sure that if a new WT^ is being added, no upvotes for the
            // same sidechain were set
            if (!vWTPrimeUpvoted[x].IsNull()) {
                if (fDebug)
                    LogPrintf("SCDB %s: Error: Adding new WT^ when upvotes are also added for the same sidechain!\n", __func__);
                return false;
            }
            vWTPrimeUpvoted[x] = s.hashWTPrime;

            vWTPrimeStatus[x].push_back(s);

            if (fDebug)
                LogPrintf("SCDB %s: Cached new WT^: %s\n",
                        __func__,
                        s.hashWTPrime.ToString());
        }
    }

    // For sidechains that had a WT^ upvoted, downvote all of the other WT^(s)
    for (size_t x = 0; x < vWTPrimeStatus.size(); x++) {
        if (vWTPrimeUpvoted[x].IsNull())
            continue;

        for (size_t y = 0; y < vWTPrimeStatus[x].size(); y++) {
            if (vWTPrimeStatus[x][y].hashWTPrime != vWTPrimeUpvoted[x]) {
                if (vWTPrimeStatus[x][y].nWorkScore > 0)
                    vWTPrimeStatus[x][y].nWorkScore--;
            }
        }
    }

    // Too noisy but can be re-enabled for debugging
    //if (fDebug)
    //    LogPrintf("SCDB %s: Finished updating at height: %u with %u WT^ updates.\n",
    //            __func__,
    //            nHeight,
    //            vNewScores.size());

    return true;
}

bool SidechainDB::UpdateSCDBMatchMT(int nHeight, const uint256& hashMerkleRoot, const std::vector<SidechainWTPrimeState>& vScores, const std::map<uint8_t, uint256>& mapNewWTPrime)
{
    // Note: vScores is an optional vector of scores that we have parsed from
    // an update script, the network or otherwise.

    // Try testing out most likely updates
    std::vector<SidechainWTPrimeState> vUpvote = GetLatestStateWithVote(SCDB_UPVOTE, mapNewWTPrime);
    if (GetSCDBHashIfUpdate(vUpvote, nHeight, mapNewWTPrime, true /* fRemoveExpired */) == hashMerkleRoot) {
        UpdateSCDBIndex(vUpvote, nHeight, true /* fDebug */, mapNewWTPrime, false /* fSkipDec */, true /* fRemoveExpired */);
        return (GetSCDBHash() == hashMerkleRoot);
    }

    std::vector<SidechainWTPrimeState> vAbstain = GetLatestStateWithVote(SCDB_ABSTAIN, mapNewWTPrime);
    if (GetSCDBHashIfUpdate(vAbstain, nHeight, mapNewWTPrime, true /* fRemoveExpired */) == hashMerkleRoot) {
        UpdateSCDBIndex(vAbstain, nHeight, true /* fDebug */, mapNewWTPrime, false /* fSkipDec */, true /* fRemoveExpired */);
        return (GetSCDBHash() == hashMerkleRoot);
    }

    std::vector<SidechainWTPrimeState> vDownvote = GetLatestStateWithVote(SCDB_DOWNVOTE, mapNewWTPrime);
    if (GetSCDBHashIfUpdate(vDownvote, nHeight, mapNewWTPrime, true /* fRemoveExpired */) == hashMerkleRoot) {
        UpdateSCDBIndex(vDownvote, nHeight, true /* fDebug */, mapNewWTPrime, false /* fSkipDec */, true /* fRemoveExpired */);
        return (GetSCDBHash() == hashMerkleRoot);
    }

    // Try using new scores (optionally passed in) from update bytes
    if (vScores.size()) {
        if (GetSCDBHashIfUpdate(vScores, nHeight, mapNewWTPrime, true /* fRemoveExpired */) == hashMerkleRoot) {
            UpdateSCDBIndex(vScores, nHeight, true /* fDebug */, mapNewWTPrime, false /* fSkipDec */, true /* fRemoveExpired */);
            return (GetSCDBHash() == hashMerkleRoot);
        }
    }
    return false;
}

void SidechainDB::ApplyDefaultUpdate()
{
    if (!HasState())
        return;

    // Decrement nBlocksLeft, nothing else changes
    for (size_t x = 0; x < vWTPrimeStatus.size(); x++) {
        for (size_t y = 0; y < vWTPrimeStatus[x].size(); y++) {
            if (vWTPrimeStatus[x][y].nBlocksLeft > 0)
                vWTPrimeStatus[x][y].nBlocksLeft--;
        }
    }

    // Remove expired WT^(s)
    RemoveExpiredWTPrimes();
}

void SidechainDB::UpdateActivationStatus(const std::vector<uint256>& vHash)
{
    // Increment the age of all sidechain proposals, remove expired.
    for (size_t i = 0; i < vActivationStatus.size(); i++) {
        vActivationStatus[i].nAge++;
        if (vActivationStatus[i].nAge > SIDECHAIN_ACTIVATION_MAX_AGE) {
            LogPrintf("SCDB %s: Sidechain proposal expired:\n%s\n",
                    __func__,
                    vActivationStatus[i].proposal.ToString());

            vActivationStatus[i] = vActivationStatus.back();
            vActivationStatus.pop_back();
        }
    }

    // Calculate failures. Sidechain proposals with activation status will have
    // their activation failure count increased by 1 if a activation commitment
    // for them is not found in the block.
    for (size_t i = 0; i < vActivationStatus.size(); i++) {
        bool fFound = false;
        for (const uint256& u : vHash) {
            if (u == vActivationStatus[i].proposal.GetHash()) {
                fFound = true;
                break;
            }
        }
        if (!fFound)
            vActivationStatus[i].nFail++;
    }

    // Remove sidechain proposals with too many failures to activate
    std::vector<std::vector<SidechainActivationStatus>::const_iterator> vFail;
    for (size_t i = 0; i < vActivationStatus.size(); i++) {
        if (vActivationStatus[i].nFail >= SIDECHAIN_ACTIVATION_MAX_FAILURES) {
            LogPrintf("SCDB %s: Sidechain proposal rejected:\n%s\n",
                    __func__,
                    vActivationStatus[i].proposal.ToString());

            vActivationStatus[i] = vActivationStatus.back();
            vActivationStatus.pop_back();
        }
    }

    // TODO this needs to be replaced
    // Don't activate any more sidechains if we have reached the max
    if (vActiveSidechain.size() >= SIDECHAIN_ACTIVATION_MAX_ACTIVE)
        return;

    // Move activated sidechains to vActivatedSidechain
    for (size_t i = 0; i < vActivationStatus.size(); i++) {
        if (vActivationStatus[i].nAge == SIDECHAIN_ACTIVATION_MAX_AGE) {
            // Create sidechain object
            Sidechain sidechain;
            sidechain.nVersion = vActivationStatus[i].proposal.nVersion;
            sidechain.hashID1 = vActivationStatus[i].proposal.hashID1;
            sidechain.hashID2 = vActivationStatus[i].proposal.hashID2;
            // TODO Get nSidechain in a smarter way
            sidechain.nSidechain = vActiveSidechain.size();
            sidechain.sidechainPriv = vActivationStatus[i].proposal.sidechainPriv;
            sidechain.sidechainHex = vActivationStatus[i].proposal.sidechainHex;
            sidechain.sidechainKeyID = vActivationStatus[i].proposal.sidechainKeyID;
            sidechain.title = vActivationStatus[i].proposal.title;
            sidechain.description = vActivationStatus[i].proposal.description;

            vActiveSidechain.push_back(sidechain);

            // Save proposal for later
            SidechainProposal proposal = vActivationStatus[i].proposal;

            vActivationStatus[i] = vActivationStatus.back();
            vActivationStatus.pop_back();

            // Add blank vector to track this sidechain's WT^(s)
            vWTPrimeStatus.push_back(std::vector<SidechainWTPrimeState>{});

            // Add a blank vector to track this sidechain's deposit(s)
            vDepositCache.push_back(std::vector<SidechainDeposit>{});

            // Remove proposal from our cache if it has activated
            for (size_t j = 0; j < vSidechainProposal.size(); j++) {
                if (proposal == vSidechainProposal[j]) {
                    vSidechainProposal[j] = vSidechainProposal.back();
                    vSidechainProposal.pop_back();
                }
            }

            LogPrintf("SCDB %s: Sidechain activated:\n%s\n",
                    __func__,
                    vActivationStatus[i].proposal.ToString());
        }
    }
}

bool SidechainDB::SortSCDBDeposits()
{
    std::vector<std::vector<SidechainDeposit>> vDepositSorted;

    // Loop through deposits and sort the vector for each sidechain
    for (const std::vector<SidechainDeposit>& v : vDepositCache) {
        std::vector<SidechainDeposit> vDeposit;
        if (!SortDeposits(v, vDeposit)) {
            LogPrintf("%s: Error: Failed to sort deposits!\n", __func__);
            return false;
        }
        vDepositSorted.push_back(vDeposit);
    }

    // TODO check the result
    // - Make sure that all deposits were sorted
    // - ...

    // Update deposit cache with sorted list
    vDepositCache = vDepositSorted;

    return true;
}

void SidechainDB::UpdateCTIP(const uint256& hashBlock)
{
    for (size_t x = 0; x < vDepositCache.size(); x++) {
        if (vDepositCache[x].size()) {
            const SidechainDeposit& d = vDepositCache[x].back();

            const COutPoint out(d.tx.GetHash(), d.n);
            const CAmount amount = d.tx.vout[d.n].nValue;

            SidechainCTIP ctip;
            ctip.out = out;
            ctip.amount = amount;

            mapCTIP[d.nSidechain] = ctip;

            // Log the update
            // If hash block is null - that means we loaded deposits from disk
            if (!hashBlock.IsNull()) {
                LogPrintf("SCDB %s: Updated sidechain CTIP for nSidechain: %u. CTIP output: %s CTIP amount: %i hashBlock: %s.\n",
                    __func__,
                    d.nSidechain,
                    out.ToString(),
                    amount,
                    hashBlock.ToString());
            } else {
                LogPrintf("SCDB %s: Updated sidechain CTIP for nSidechain: %u. CTIP output: %s CTIP amount: %i. (Loaded from disk).\n",
                    __func__,
                    d.nSidechain,
                    out.ToString(),
                    amount);
            }
        } else {
            // If there are no deposits now, remove CTIP for nSidechain
            std::map<uint8_t, SidechainCTIP>::const_iterator it;
            it = mapCTIP.find(x);
            mapCTIP.erase(it);

            LogPrintf("SCDB %s: Removed sidechain CTIP.\n",
                    __func__);
        }
    }
}

bool ParseSCDBUpdateScript(const CScript& script, const std::vector<std::vector<SidechainWTPrimeState>>& vOldScores, std::vector<SidechainWTPrimeState>& vNewScores)
{
    if (!script.IsSCDBUpdate()) {
        LogPrintf("SCDB %s: Error: script not SCDB update bytes!\n", __func__);
        return false;
    }

    if (vOldScores.empty()) {
        LogPrintf("SCDB %s: Error: no old scores!\n", __func__);
        return false;
    }

    CScript bytes = CScript(script.begin() + 5, script.end());

    size_t x = 0; // vOldScores outer vector (sidechains)
    for (CScript::const_iterator it = bytes.begin(); it < bytes.end(); it++) {
        const unsigned char c = *it;
        if (c == SC_OP_UPVOTE || c == SC_OP_DOWNVOTE) {
            // Figure out which WT^ is being upvoted
            if (vOldScores.size() <= x) {
                LogPrintf("SCDB %s: Error: Sidechain missing from old scores!\n", __func__);
                return false;
            }

            // Read which WT^ we are voting on from the bytes and set
            size_t y = 0; // vOldScores inner vector (WT^(s) per sidechain)
            if (bytes.end() - it > 2) {
                CScript::const_iterator itWT = it + 1;
                const unsigned char cNext = *itWT;
                if (cNext != SC_OP_DELIM) {
                    if (cNext == 0x01)
                    {
                        if (!(bytes.end() - itWT >= 1)) {
                            LogPrintf("SCDB %s: Error: Invalid WT^ index A\n", __func__);
                            return false;
                        }

                        const CScript::const_iterator it1 = itWT + 1;
                        y = CScriptNum(std::vector<unsigned char>{*it1}, false).getint();
                    }
                    else
                    if (cNext == 0x02)
                    {
                        if (!(bytes.end() - itWT >= 2)) {
                            LogPrintf("SCDB %s: Error: Invalid WT^ index B\n", __func__);
                            return false;
                        }

                        const CScript::const_iterator it1 = itWT + 1;
                        const CScript::const_iterator it2 = itWT + 2;
                        y = CScriptNum(std::vector<unsigned char>{*it1, *it2}, false).getint();
                    }
                    else
                    {
                        // TODO support WT^ indexes requiring more than 2 bytes?
                        return false;
                    }
                }
            }

            if (vOldScores[x].size() <= y) {
                LogPrintf("SCDB %s: Error: WT^ missing from old scores!\n", __func__);
                return false;
            }

            SidechainWTPrimeState newScore = vOldScores[x][y];

            if (c == SC_OP_UPVOTE)
                newScore.nWorkScore++;
            else
            if (newScore.nWorkScore > 0)
                newScore.nWorkScore--;

            vNewScores.push_back(newScore);
        }
        else
        if (c == SC_OP_DELIM) {
            // Moving on to the next sidechain
            x++;
            continue;
        }
    }

    return true;
}

bool SortDeposits(const std::vector<SidechainDeposit>& vDeposit, std::vector<SidechainDeposit>& vDepositSorted)
{
    if (vDeposit.empty())
        return true;

    if (vDeposit.size() == 1) {
        vDepositSorted = vDeposit;
        return true;
    }

    // Find the first deposit in the list by looking for the deposit which
    // spends a CTIP not in the list. There can only be one. We are also going
    // to check that there is only one missing CTIP input here.
    int nMissingCTIP = 0;
    for (size_t x = 0; x < vDeposit.size(); x++) {
        const SidechainDeposit dx = vDeposit[x];

        // Look for the input of this deposit
        bool fFound = false;
        for (size_t y = 0; y < vDeposit.size(); y++) {
            const SidechainDeposit dy = vDeposit[y];

            // The CTIP output of the deposit that might be the input
            const COutPoint prevout(dy.tx.GetHash(), dy.n);

            // Look for the CTIP output
            for (const CTxIn& in : dx.tx.vin) {
                if (in.prevout == prevout) {
                    fFound = true;
                    break;
                }
            }
            if (fFound)
                break;
        }

        // If we didn't find the CTIP input, this should be the first and only
        // deposit without one.
        if (!fFound) {
            nMissingCTIP++;
            if (nMissingCTIP > 1) {
                LogPrintf("%s: Error: Multiple missing CTIP!\n", __func__);
                return false;
            }
            // Add the first deposit to the result
            vDepositSorted.push_back(dx);
            // We found the first deposit but do not stop the loop here
            // because we are also checking to make sure there aren't any
            // other deposits missing a CTIP input from the list.
        }
    }

    if (vDepositSorted.empty()) {
        LogPrintf("%s: Error: Could not find first deposit in list!\n", __func__);
        return false;
    }

    // Now that we know which deposit is first in the list we can add the rest
    // in CTIP spend order.

    // Track the CTIP output of the latest deposit we have sorted
    COutPoint prevout(vDepositSorted.back().tx.GetHash(), vDepositSorted.back().n);

    // Look for the deposit that spends the last sorted CTIP output and sort it.
    // If we cannot find a deposit spending the CTIP, that should mean we
    // reached the end of sorting.
    std::vector<SidechainDeposit>::const_iterator it = vDeposit.begin();
    while (it != vDeposit.end()) {
        bool fFound = false;
        for (const CTxIn& in : it->tx.vin) {
            if (in.prevout == prevout) {
                // Add the sorted deposit to the list
                vDepositSorted.push_back(*it);

                // Update the CTIP output we are looking for
                const SidechainDeposit deposit = vDepositSorted.back();
                prevout = COutPoint(deposit.tx.GetHash(), deposit.n);

                // Start from begin() again
                fFound = true;
                it = vDeposit.begin();

                break;
            }
        }
        if (!fFound)
            it++;
    }

    if (vDeposit.size() != vDepositSorted.size()) {
        LogPrintf("%s: Error: Invalid result size! In: %u Out: %u\n", __func__,
                vDeposit.size(), vDepositSorted.size());
        return false;
    }

    return true;
}

