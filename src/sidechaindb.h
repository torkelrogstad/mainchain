// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SIDECHAINDB_H
#define BITCOIN_SIDECHAINDB_H

#include <map>
#include <memory> // Required for forward declaration of CTransactionRef typedef
#include <set>
#include <vector>

#include <amount.h>
#include <uint256.h>

class CCriticalData;
class COutPoint;
class CScript;
class CTransaction;
typedef std::shared_ptr<const CTransaction> CTransactionRef;
class CMutableTransaction;
class CTxOut;
class uint256;

struct Sidechain;
struct SidechainActivationStatus;
struct SidechainBlockData;
struct SidechainCustomVote;
struct SidechainCTIP;
struct SidechainDeposit;
struct SidechainProposal;
struct SidechainWTPrimeState;
struct SidechainSpentWTPrime;
struct SidechainFailedWTPrime;

// TODO custom operator[] or getter functions for private data members which
// will check the index and throw an error instead of going out of bounds

// TODO Refactor: remove AddWTPrime() and UpdateSCDBIndex() - handle both inside
// of SCDB::Update()

// TODO use the bitcoin core lock system or std::mutex & std::lock_guard to
// protect private data structures.

class SidechainDB
{
public:
    SidechainDB();

    bool ApplyLDBData(const uint256& hashBlockLastSeen, const SidechainBlockData& data);

    /** Add txid of BMM transaction removed from mempool to cache */
    void AddRemovedBMM(const uint256& hashRemoved);

    /** Add txid of removed sidechain deposit transaction */
    void AddRemovedDeposit(const uint256& hashRemoved);

    /** Add deposit(s) to cache - from block */
    bool AddDeposits(const std::vector<CTransaction>& vtx, const uint256& hashBlock, bool fJustCheck = false);

    /** Add deposit(s) to cache - from disk cache */
    void AddDeposits(const std::vector<SidechainDeposit>& vDeposit, const uint256& hashBlock);

    /** Add a new WT^ to SCDB */
    bool AddWTPrime(uint8_t nSidechain, const uint256& hashWTPrime, int nHeight, bool fDebug = false);

    /** Add spent WT^(s) to SCDB */
    void AddSpentWTPrimes(const std::vector<SidechainSpentWTPrime>& vSpent);

    /** Add failed WT^(s) to SCDB */
    void AddFailedWTPrimes(const std::vector<SidechainFailedWTPrime>& vFailed);

    /** Remove failed BMM request from cache once it has been abandoned */
    void BMMAbandoned(const uint256& txid);

    /** Add active sidechains to the in-memory cache */
    void CacheActiveSidechains(const std::vector<Sidechain>& vSidechainIn);

    /** Add a users custom vote to the in-memory cache */
    bool CacheCustomVotes(const std::vector<SidechainCustomVote>& vCustomVote);

    /** Add SidechainActivationStatus to the in-memory cache */
    void CacheSidechainActivationStatus(const std::vector<SidechainActivationStatus>& vActivationStatusIn);

    /** Add SidechainProposal to the in-memory cache */
    void CacheSidechainProposals(const std::vector<SidechainProposal>& vSidechainProposalIn);

    /** Add sidechain-to-be-activated hash to cache */
    void CacheSidechainHashToActivate(const uint256& u);

    /** Add WT^ to the in-memory cache */
    bool CacheWTPrime(const CTransaction& tx, const uint8_t nSidechain);

    /** Check SCDB WT^ verification status */
    bool CheckWorkScore(uint8_t nSidechain, const uint256& hashWTPrime, bool fDebug = false) const;

    /** Clear out the cached list of removed sidechain deposit transactions */
    void ClearRemovedDeposits();

    /** Return number of active sidechains */
    unsigned int GetActiveSidechainCount() const;

    /** Check if the hash of the sidechain is in our hashes of sidechains to
     * activate cache. Return true if it is, or false if not. */
    bool GetActivateSidechain(const uint256& u) const;

    /** Get list of currently active sidechains */
    std::vector<Sidechain> GetActiveSidechains() const;

    /** Get list of BMM txid that miner removed from the mempool. */
    std::set<uint256> GetRemovedBMM() const;

    /** Get list of deposit txid that were removed from the mempool. */
    std::vector<uint256> GetRemovedDeposits() const;

    /** Return the CTIP (critical transaction index pair) for nSidechain */
    bool GetCTIP(uint8_t nSidechain, SidechainCTIP& out) const;

    /** Return the CTIP (critical transaction index pair) for all sidechains */
    std::map<uint8_t, SidechainCTIP> GetCTIP() const;

    bool GetCachedWTPrime(const uint256& hashWTPrime, CMutableTransaction& mtx) const;

    /** Return vector of cached custom sidechain WT^ votes */
    std::vector<SidechainCustomVote> GetCustomVoteCache() const;

    /** Return vector of cached deposits for nSidechain. */
    std::vector<SidechainDeposit> GetDeposits(uint8_t nSidechain) const;

    /** Return vector of cached deposits for nSidechain. */
    std::vector<SidechainDeposit> GetDeposits(const std::string& sidechainPriv) const;

    /** Return the hash of the last block SCDB processed */
    uint256 GetHashBlockLastSeen();

    /** For testing purposes - return the hash of everything that SCDB is
     * tracking instead of just WT^ state as GetSCDBHash() does.
     * This includes members used for consensus as well as user data like
     * which sidechain(s) they have set votes for and their own sidechain
     * proposals. */
    uint256 GetTotalSCDBHash() const;

    /** Return serialization hash of SCDB latest verification(s) */
    uint256 GetSCDBHash() const;

    /** Return what the SCDB hash would be if the updates are applied */
    uint256 GetSCDBHashIfUpdate(const std::vector<SidechainWTPrimeState>& vNewScores, int nHeight, const std::map<uint8_t, uint256>& mapNewWTPrime = {}, bool fRemoveExpired = false) const;

    /** Get the sidechain that relates to nSidechain if it exists */
    bool GetSidechain(const uint8_t nSidechain, Sidechain& sidechain) const;

    /** Get sidechain activation status */
    std::vector<SidechainActivationStatus> GetSidechainActivationStatus() const;

    /** Get the name of a sidechain */
    std::string GetSidechainName(uint8_t nSidechain) const;

    /** Get list of this node's uncommitted sidechain proposals */
    std::vector<SidechainProposal> GetSidechainProposals() const;

    /** Get the scriptPubKey that relates to nSidechain if it exists */
    bool GetSidechainScript(const uint8_t nSidechain, CScript& scriptPubKey) const;

    /** Get list of sidechains that we have set to ACK */
    std::vector<uint256> GetSidechainsToActivate() const;

    /** Get a list of WT^(s) spent in a given block */
    std::vector<SidechainSpentWTPrime> GetSpentWTPrimesForBlock(const uint256& hashBlock) const;

    /** Get status of nSidechain's WT^(s) (public for unit tests) */
    std::vector<SidechainWTPrimeState> GetState(uint8_t nSidechain) const;

    std::vector<std::vector<SidechainWTPrimeState>> GetState() const;

    /** Return cached but uncommitted WT^ transaction's hash(s) for nSidechain */
    std::vector<uint256> GetUncommittedWTPrimeCache(uint8_t nSidechain) const;

    /** Return cached but uncommitted WT^ transaction's hash(s) for nSidechain */
    std::vector<SidechainProposal> GetUncommittedSidechainProposals() const;

    /** Returns SCDB WT^ state with single vote type applied to all of the most
     * recent WT^(s) in the cache */
    std::vector<SidechainWTPrimeState> GetLatestStateWithVote(const char& vote, const std::map<uint8_t, uint256>& mapNewWTPrime) const;

    /** Return cached WT^ transaction(s) */
    std::vector<std::pair<uint8_t, CMutableTransaction>> GetWTPrimeCache() const;

    /** Return cached spent WT^(s) as a vector for dumping to disk */
    std::vector<SidechainSpentWTPrime> GetSpentWTPrimeCache() const;

    /** Return cached failed WT^(s) as a vector for dumping to disk */
    std::vector<SidechainFailedWTPrime> GetFailedWTPrimeCache() const;

    /** Is there anything being tracked by the SCDB? */
    bool HasState() const;

    /** Return true if the transaction spends sidechain inputs. Return the
     * sidechain number by reference */
    bool HasSidechainScript(const std::vector<CScript>& vScript, uint8_t& nSidechain) const;

    /** Return true if the deposit is cached */
    bool HaveDepositCached(const SidechainDeposit& deposit) const;

    /** Return true if the WT^ has been spent */
    bool HaveSpentWTPrime(const uint256& hashWTPrime, const uint8_t nSidechain) const;

    /** Return true if the WT^ failed */
    bool HaveFailedWTPrime(const uint256& hashWTPrime, const uint8_t nSidechain) const;

    /** Return true if the full WT^ CTransaction is cached */
    bool HaveWTPrimeCached(const uint256& hashWTPrime) const;

    /** Check if SCDB is tracking the work score of a WT^ */
    bool HaveWTPrimeWorkScore(const uint256& hashWTPrime, uint8_t nSidechain) const;

    /** Check if a sidechain with nSidechain exists in the DB */
    bool IsSidechainNumberValid(uint8_t nSidechain) const;

    void RemoveExpiredWTPrimes();

    /** Remove sidechain-to-be-activated hash from cache, because the user
     * changed their mind */
    void RemoveSidechainHashToActivate(const uint256& u);

    /** Reset SCDB and clear out all data tracked by SidechainDB */
    void ResetWTPrimeState();

    /** Clear out the WT^ custom vote cache */
    void ResetWTPrimeVotes();

    /** Reset everything */
    void Reset();

    /** Spend a WT^ (if we can) */
    bool SpendWTPrime(uint8_t nSidechain, const uint256& hashBlock, const CTransaction& tx, bool fJustCheck = false,  bool fDebug = false);

    /** Get SidechainDeposit from deposit CTransaction. Part of SCDB because
     * we need the list of active sidechains to find deposit outputs. */
    bool TxnToDeposit(const CTransaction& tx, const uint256& hashBlock, SidechainDeposit& deposit);

    /** Print SCDB WT^ verification status */
    std::string ToString() const;

    /** Check the updates in a block and then apply them */
    bool Update(int nHeight, const uint256& hashBlock, const uint256& hashPrevBlock, const std::vector<CTxOut>& vout, bool fJustCheck = false, bool fDebug = false);

    /** Undo the changes to SCDB of a block - for block is disconnection */
    bool Undo(int nHeight, const uint256& hashBlock, const uint256& hashPrevBlock, const std::vector<CTransactionRef>& vtx, bool fDebug = false);

    /** Update / add multiple SCDB WT^(s) to SCDB */
    bool UpdateSCDBIndex(const std::vector<SidechainWTPrimeState>& vNewScores, int nHeight, bool fDebug = false, const std::map<uint8_t, uint256>& mapNewWTPrime = {}, bool fSkipDec = false, bool fRemoveExpired = false);

    /** Read the SCDB hash in a new block and try to synchronize our SCDB by
     * testing possible work score updates until the SCDB hash of our SCDB
     * matches the one from the new block. Return false if no match found. */
    bool UpdateSCDBMatchMT(int nHeight, const uint256& hashMerkleRoot, const std::vector<SidechainWTPrimeState>& vScores = {}, const std::map<uint8_t, uint256>& mapNewWTPrime = {});

private:
    /**
     * Submit default vote for all sidechain WT^(s). Used when a new block does
     * not contain a valid update. */
    void ApplyDefaultUpdate();

    /** Apply the changes in a block to SCDB */
    bool ApplyUpdate(int nHeight, const uint256& hashBlock, const uint256& hashPrevBlock, const std::vector<CTxOut>& vout, bool fJustCheck = false, bool fDebug = false);

    /** Takes a list of sidechain hashes to upvote */
    void UpdateActivationStatus(const std::vector<uint256>& vHash);

    /*
     * The CTIP of nSidechain up to the latest connected block (does not include
     * mempool txns). */
    std::map<uint8_t, SidechainCTIP> mapCTIP;

    /** The most recent block that SCDB has processed */
    uint256 hashBlockLastSeen;

    /** Sidechains which are currently active */
    std::vector<Sidechain> vActiveSidechain;

    /** Activation status of proposed sidechains */
    std::vector<SidechainActivationStatus> vActivationStatus;

    /** Cache of votes set by the user. These can be set via GUI on the
     * sidechain manage page, or command line params / config file */
    std::vector<SidechainCustomVote> vCustomVoteCache;

    /** Cache of deposits for each sidechain. TODO optimize with caching
     * so that we don't have to keep all of these in memory.
     * x = nSidechain
     * y = list of deposits for nSidechain */
    std::vector<std::vector<SidechainDeposit>> vDepositCache;

    /** Cache of sidechain hashes, for sidechains which this node has been
     * configured to activate by the user */
    std::vector<uint256> vSidechainHashActivate;

    /** Cache of proposals created by this node, which should be included in the
     * next block that this node mines. */
    std::vector<SidechainProposal> vSidechainProposal;

    /** Cache of potential WT^ transactions
     * TODO consider refactoring to use CTransactionRef */
    std::vector<std::pair<uint8_t, CMutableTransaction>> vWTPrimeCache;

    /** Tracks verification status of WT^(s)
     * x = nSidechain
     * y = state of WT^(s) for nSidechain */
    std::vector<std::vector<SidechainWTPrimeState>> vWTPrimeStatus;

    /** Map of spent WT^(s) key: block hash value: Spent WT^(s) from block */
    std::map<uint256, std::vector<SidechainSpentWTPrime>> mapSpentWTPrime;

    /** Map of failed WT^(s) key: WT^ hash value: Spent WT^ data **/
    std::map<uint256, SidechainFailedWTPrime> mapFailedWTPrime;

    /** List of BMM request txid that the miner removed from the mempool. */
    std::set<uint256> setRemovedBMM;

    /** List of sidechain deposits that were removed from the mempool for one
     * of a few reasons. The deposit could have been replaced by another deposit
     * that made it to the mempool first, spending the same CTIP. Or the deposit
     * may have been in the mempool when a WT^ payout was created, spending the
     * same CTIP as the deposit. */
    std::vector<uint256> vRemovedDeposit;

    /** Calls SortDeposits for all of SCDB's deposit cache */
    bool SortSCDBDeposits();

    /** Update CTIP to match the deposit cache - called after sorting / undo */
    void UpdateCTIP(const uint256& hashBlock);
};

/** Read encoded sum of WT fees from WT^ output script */
bool DecodeWTFees(const CScript& script, CAmount& amount);

/** Read an SCDB update script and return new scores by reference if valid */
bool ParseSCDBUpdateScript(const CScript& script, const std::vector<std::vector<SidechainWTPrimeState>>& vOldScores, std::vector<SidechainWTPrimeState>& vNewScores);

/** Sort deposits by CTIP UTXO spending order */
bool SortDeposits(const std::vector<SidechainDeposit>& vDeposit, std::vector<SidechainDeposit>& vDepositSorted);

#endif // BITCOIN_SIDECHAINDB_H
