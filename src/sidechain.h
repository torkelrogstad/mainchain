// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SIDECHAIN_H
#define BITCOIN_SIDECHAIN_H

#include <primitives/transaction.h>
#include <pubkey.h>

#include <array>

// These are the temporary values to speed things up during testing
static const int SIDECHAIN_VERIFICATION_PERIOD = 300;
static const int SIDECHAIN_MIN_WORKSCORE = 140;

// These are the values that will be used in the final release
//static const int SIDECHAIN_VERIFICATION_PERIOD = 26298;
//static const int SIDECHAIN_MIN_WORKSCORE = 13140;

//! Max number of failures (blocks without commits) for a sidechain to activate
static const int SIDECHAIN_ACTIVATION_MAX_FAILURES = 32;
//! The amount of time a sidechain has to activate
static const int SIDECHAIN_ACTIVATION_MAX_AGE = 64;
//! The number of sidechains which may be signaled for activation at once
static const int SIDECHAIN_ACTIVATION_MAX_SIGNALS = 32;
//! The number of sidechains which may be active at once
static const int SIDECHAIN_ACTIVATION_MAX_ACTIVE = 256;

//! The current sidechain version
static const int SIDECHAIN_VERSION_CURRENT = 0;
//! The max supported sidechain version
static const int SIDECHAIN_VERSION_MAX = 0;

//! The key for sidechain block data in ldb
static const char DB_SIDECHAIN_BLOCK_OP = 'S';

//! The destination string for the change of a WT^
static const std::string SIDECHAIN_WTPRIME_RETURN_DEST = "D";

struct SidechainProposal {
    int32_t nVersion = SIDECHAIN_VERSION_CURRENT;
    std::string title;
    std::string description;
    std::string sidechainKeyID;
    std::string sidechainHex;
    std::string sidechainPriv;
    uint256 hashID1;
    uint160 hashID2;

    bool DeserializeFromScript(const CScript& script);

    std::vector<unsigned char> GetBytes() const;
    CScript GetScript() const;
    uint256 GetHash() const;
    bool operator==(const SidechainProposal& proposal) const;
    std::string ToString() const;

    ADD_SERIALIZE_METHODS

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(nVersion);
        READWRITE(title);
        READWRITE(description);
        READWRITE(sidechainKeyID);
        READWRITE(sidechainHex);
        READWRITE(sidechainPriv);
        READWRITE(hashID1);
        READWRITE(hashID2);
    }
};

struct SidechainActivationStatus
{
    int nAge;
    int nFail;
    SidechainProposal proposal;

    uint256 GetHash() const;

    ADD_SERIALIZE_METHODS

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(nAge);
        READWRITE(nFail);
        READWRITE(proposal);
    }
};

struct Sidechain {
    bool fActive;
    uint8_t nSidechain;
    int32_t nVersion = SIDECHAIN_VERSION_CURRENT;
    std::string sidechainKeyID;
    std::string sidechainPriv;
    std::string sidechainHex;
    std::string title;
    std::string description;
    uint256 hashID1;
    uint160 hashID2;

    std::string GetSidechainName() const;
    bool operator==(const SidechainProposal& a) const;
    std::string ToString() const;
    uint256 GetHash() const;

    ADD_SERIALIZE_METHODS

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(fActive);
        READWRITE(nSidechain);
        READWRITE(nVersion);
        READWRITE(sidechainKeyID);
        READWRITE(sidechainPriv);
        READWRITE(sidechainHex);
        READWRITE(title);
        READWRITE(description);
        READWRITE(hashID1);
        READWRITE(hashID2);
    }
};

struct SidechainDeposit {
    uint8_t nSidechain;
    std::string strDest;
    CMutableTransaction tx;
    uint32_t n;
    uint256 hashBlock;

    bool operator==(const SidechainDeposit& a) const;
    std::string ToString() const;
    uint256 GetHash() const;

    ADD_SERIALIZE_METHODS

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(nSidechain);
        READWRITE(strDest);
        READWRITE(tx);
        READWRITE(n);
        READWRITE(hashBlock);
    }
};

static const char SCDB_UPVOTE = 'u';
static const char SCDB_DOWNVOTE = 'd';
static const char SCDB_ABSTAIN = 'a';

// A vote set by the user to specify custom votes for particular WT^(s) - Used
// by the miner to pass minimal data GenerateSCDBUpdateScript()
struct SidechainCustomVote
{
    char vote; // SCDB_UPVOTE, SCDB_DOWNVOTE or SCDB_ABSTAIN
    uint8_t nSidechain;
    uint256 hashWTPrime;

    bool operator==(const SidechainCustomVote& v) const
    {
        return (vote == v.vote && nSidechain == v.nSidechain
                && hashWTPrime == v.hashWTPrime);
    }

    ADD_SERIALIZE_METHODS

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(vote);
        READWRITE(nSidechain);
        READWRITE(hashWTPrime);
    }
};

struct SidechainWTPrimeState {
    uint8_t nSidechain;
    uint16_t nBlocksLeft;
    uint16_t nWorkScore;
    uint256 hashWTPrime;

    bool IsNull() const;
    uint256 GetHash() const;
    bool operator==(const SidechainWTPrimeState& a) const;
    std::string ToString() const;

    // For hash calculation
    ADD_SERIALIZE_METHODS

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(nSidechain);
        READWRITE(nBlocksLeft);
        READWRITE(nWorkScore);
        READWRITE(hashWTPrime);
    }
};

struct SidechainSpentWTPrime {
    uint8_t nSidechain;
    uint256 hashWTPrime;
    uint256 hashBlock;

    ADD_SERIALIZE_METHODS

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(nSidechain);
        READWRITE(hashWTPrime);
        READWRITE(hashBlock);
    }
};

struct SidechainFailedWTPrime {
    uint8_t nSidechain;
    uint256 hashWTPrime;

    ADD_SERIALIZE_METHODS

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(nSidechain);
        READWRITE(hashWTPrime);
    }
};

struct SidechainCTIP {
    COutPoint out;
    CAmount amount;

    uint256 GetHash() const;
    std::string ToString() const;

    // For hash calculation
    ADD_SERIALIZE_METHODS

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(out);
        READWRITE(amount);
    }
};

/**
 * Base object for sidechain related database entries
 */
struct SidechainObj {
    char sidechainop;

    SidechainObj(void) { }
    virtual ~SidechainObj(void) { }

    uint256 GetHash(void) const;
    CScript GetScript(void) const;
    virtual std::string ToString(void) const;
};

/**
 * SCDB data for a block - database object
 */
struct SidechainBlockData: public SidechainObj {
    std::vector<std::vector<SidechainWTPrimeState>> vWTPrimeStatus;
    std::vector<SidechainSpentWTPrime> vSpentWTPrime;
    uint256 hashMT;

    SidechainBlockData(void) : SidechainObj() { sidechainop = DB_SIDECHAIN_BLOCK_OP; }
    virtual ~SidechainBlockData(void) { }

    ADD_SERIALIZE_METHODS

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(sidechainop);
        READWRITE(vWTPrimeStatus);
        READWRITE(vSpentWTPrime);
        READWRITE(hashMT);
    }

    std::string ToString(void) const;

    uint256 GetID() const {
        return GetHash();
    }
};

bool ParseDepositAddress(const std::string& strAddressIn, std::string& strAddressOut, unsigned int& nSidechainOut);

#endif // BITCOIN_SIDECHAIN_H
