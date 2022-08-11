// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "consensus/validation.h"
#include "core_io.h"
#include "miner.h"
#include "random.h"
#include "script/script.h"
#include "script/standard.h"
#include "script/sigcache.h"
#include "sidechain.h"
#include "sidechaindb.h"
#include "uint256.h"
#include "utilstrencodings.h"
#include "validation.h"

#include "test/test_drivechain.h"

#include <boost/test/unit_test.hpp>

CScript EncodeWithdrawalFees(const CAmount& amount)
{
    CDataStream s(SER_NETWORK, PROTOCOL_VERSION);
    s << amount;

    CScript script;
    script << OP_RETURN;
    script << std::vector<unsigned char>(s.begin(), s.end());

    return script;
}

BOOST_FIXTURE_TEST_SUITE(sidechaindb_tests, TestingSetup)

bool ActivateTestSidechain(SidechainDB& scdbTest, int nHeight = 0)
{
    // Activate a test sidechain as sidechain #0

    Sidechain proposal;
    proposal.nSidechain = 0;
    proposal.nVersion = 0;
    proposal.title = "Test";
    proposal.description = "Description";
    proposal.strKeyID = "58c63096724814c3dcdf088b9bb0dc48e6e1a89c";
    proposal.strPrivKey = "91jbRcYNm4RpdJy4u99g8KyFTUsWxvXcJcYXYbQp9MU7mX1vg3K";

    std::vector<unsigned char> vch = ParseHex("76a91458c63096724814c3dcdf088b9bb0dc48e6e1a89c88ac");
    proposal.scriptPubKey = CScript(vch.begin(), vch.end());

    proposal.hashID1 = uint256S("b55d224f1fda033d930c92b1b40871f209387355557dd5e0d2b5dd9bb813c33f");
    proposal.hashID2 = uint160S("31d98584f3c570961359c308619f5cf2e9178482");

    return ActivateSidechain(scdbTest, proposal, nHeight);
}

BOOST_AUTO_TEST_CASE(sidechaindb_withdrawal)
{
    // Test creating a withdrawal and approving it with enough workscore
    SidechainDB scdbTest;

    BOOST_CHECK(ActivateTestSidechain(scdbTest));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);

    uint256 hash = GetRandHash();

    std::vector<std::string> vVote(SIDECHAIN_ACTIVATION_MAX_ACTIVE, std::string(1, SCDB_ABSTAIN));
    vVote[0] = hash.ToString();

    std::map<uint8_t, uint256> mapNewWithdrawal;
    mapNewWithdrawal[0] = hash;

    // Ack withdrawal bundle
    for (int i = 0; i < SIDECHAIN_WITHDRAWAL_MIN_WORKSCORE; i++) {
        if (i == 0)
            BOOST_CHECK(scdbTest.UpdateSCDBIndex(vVote, false, mapNewWithdrawal));
        else
            BOOST_CHECK(scdbTest.UpdateSCDBIndex(vVote));
    }

    // Withdrawal should pass with valid workscore
    BOOST_CHECK(scdbTest.CheckWorkScore(0, hash));
}

BOOST_AUTO_TEST_CASE(sidechaindb_mutli_withdraw_one_expires)
{
    // Let one withdrawal expire and then make another pay out

    SidechainDB scdbTest;

    BOOST_CHECK(ActivateTestSidechain(scdbTest));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);

    uint256 hash = GetRandHash();

    std::vector<std::string> vVote(SIDECHAIN_ACTIVATION_MAX_ACTIVE, std::string(1, SCDB_ABSTAIN));

    std::map<uint8_t, uint256> mapNewWithdrawal;
    mapNewWithdrawal[0] = hash;

    // Expire withdrawal bundle
    for (int i = 0; i < SIDECHAIN_WITHDRAWAL_VERIFICATION_PERIOD; i++) {
        if (i == 0)
            BOOST_CHECK(scdbTest.UpdateSCDBIndex(vVote, false, mapNewWithdrawal));
        else
            BOOST_CHECK(scdbTest.UpdateSCDBIndex(vVote));
    }

    // Verify withdrawal expired
    std::vector<SidechainWithdrawalState> vState = scdbTest.GetState(0);
    BOOST_CHECK(vState.size() == 0);

    // Withdrawal for second period
    hash = GetRandHash();
    mapNewWithdrawal[0] = hash;

    // Give second transaction sufficient workscore and check work score
    vVote[0] = hash.ToString();
    for (int i = 0; i < SIDECHAIN_WITHDRAWAL_MIN_WORKSCORE; i++) {
        if (i == 0)
            BOOST_CHECK(scdbTest.UpdateSCDBIndex(vVote, false, mapNewWithdrawal));
        else
            BOOST_CHECK(scdbTest.UpdateSCDBIndex(vVote));
    }
    BOOST_CHECK(scdbTest.CheckWorkScore(0, hash));
}

BOOST_AUTO_TEST_CASE(sidechaindb_matchmt_single_upvote)
{
    // Test SCDB::UpdateSCDBMatchHash update with single upvote

    SidechainDB scdbTest;

    BOOST_CHECK(ActivateTestSidechain(scdbTest));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);

    uint256 hash = GetRandHash();

    std::map<uint8_t, uint256> mapNewWithdrawal;
    mapNewWithdrawal[0] = hash;

    std::vector<std::string> vVote(SIDECHAIN_ACTIVATION_MAX_ACTIVE, std::string(1, SCDB_ABSTAIN));
    scdbTest.UpdateSCDBIndex(vVote, false, mapNewWithdrawal);

    // Create a copy of the scdbTest to manipulate
    SidechainDB scdbTestCopy = scdbTest;

    // Set votes to ack withdrawal bundle
    vVote[0] = hash.ToString();

    // Updates scores of SCDB copy
    BOOST_CHECK(scdbTestCopy.UpdateSCDBIndex(vVote));

    // Make SCDB match copy via hash update
    BOOST_CHECK(scdbTest.UpdateSCDBMatchHash(scdbTestCopy.GetSCDBHash()));

    // Verify status of withdrawal
    std::vector<SidechainWithdrawalState> vState = scdbTest.GetState(0);
    BOOST_REQUIRE(vState.size() == 1);
    BOOST_CHECK(vState[0].nWorkScore == 2);
    BOOST_CHECK(vState[0].nBlocksLeft == SIDECHAIN_WITHDRAWAL_VERIFICATION_PERIOD - 2);
}

BOOST_AUTO_TEST_CASE(sidechaindb_matchmt_single_abstain)
{
    // Test SCDB::UpdateSCDBMatchHash update with abstain vote

    SidechainDB scdbTest;

    BOOST_CHECK(ActivateTestSidechain(scdbTest));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);

    uint256 hash = GetRandHash();

    std::map<uint8_t, uint256> mapNewWithdrawal;
    mapNewWithdrawal[0] = hash;

    std::vector<std::string> vVote(SIDECHAIN_ACTIVATION_MAX_ACTIVE, std::string(1, SCDB_ABSTAIN));
    scdbTest.UpdateSCDBIndex(vVote, false, mapNewWithdrawal);

    // Create a copy of the scdbTest to manipulate
    SidechainDB scdbTestCopy = scdbTest;

    // Updates scores of SCDB copy
    BOOST_CHECK(scdbTestCopy.UpdateSCDBIndex(vVote, false));

    // Make SCDB match copy via hash update
    BOOST_CHECK(scdbTest.UpdateSCDBMatchHash(scdbTestCopy.GetSCDBHash()));

    // Verify status of withdrawal
    std::vector<SidechainWithdrawalState> vState = scdbTest.GetState(0);
    BOOST_REQUIRE(vState.size() == 1);
    BOOST_CHECK(vState[0].nWorkScore == 1);
    BOOST_CHECK(vState[0].nBlocksLeft == SIDECHAIN_WITHDRAWAL_VERIFICATION_PERIOD - 2);
}

BOOST_AUTO_TEST_CASE(sidechaindb_matchmt_single_downvote)
{
    // Test SCDB::UpdateSCDBMatchHash update with downvote

    SidechainDB scdbTest;

    BOOST_CHECK(ActivateTestSidechain(scdbTest));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);

    uint256 hash = GetRandHash();

    std::map<uint8_t, uint256> mapNewWithdrawal;
    mapNewWithdrawal[0] = hash;

    std::vector<std::string> vVote(SIDECHAIN_ACTIVATION_MAX_ACTIVE, std::string(1, SCDB_DOWNVOTE));
    scdbTest.UpdateSCDBIndex(vVote, false, mapNewWithdrawal);

    // Create a copy of the scdbTest to manipulate
    SidechainDB scdbTestCopy = scdbTest;

    // Updates scores of SCDB copy
    BOOST_CHECK(scdbTestCopy.UpdateSCDBIndex(vVote, false));

    // Make SCDB match copy via hash update
    BOOST_CHECK(scdbTest.UpdateSCDBMatchHash(scdbTestCopy.GetSCDBHash()));

    // Verify status of withdrawal
    std::vector<SidechainWithdrawalState> vState = scdbTest.GetState(0);
    BOOST_REQUIRE(vState.size() == 1);
    BOOST_CHECK(vState[0].nWorkScore == 0);
}

BOOST_AUTO_TEST_CASE(sidechaindb_aprove_withdrawal_mt)
{
    // Test creating a withdrawal and approving it with enough workscore via
    // hash updates only

    SidechainDB scdbTest;

    BOOST_CHECK(ActivateTestSidechain(scdbTest));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);

    uint256 hash = GetRandHash();

    std::map<uint8_t, uint256> mapNewWithdrawal;
    mapNewWithdrawal[0] = hash;

    std::vector<std::string> vVote(SIDECHAIN_ACTIVATION_MAX_ACTIVE, std::string(1, SCDB_ABSTAIN));
    vVote[0] = hash.ToString();
    scdbTest.UpdateSCDBIndex(vVote, false, mapNewWithdrawal);

    // Check if withdrawal was added
    std::vector<SidechainWithdrawalState> vState = scdbTest.GetState(0);
    BOOST_CHECK(vState.size() == 1);

    // Create a copy of the scdbTest to manipulate
    SidechainDB scdbTestCopy = scdbTest;

    // Ack withdrawal bundle
    for (int i = 1; i < SIDECHAIN_WITHDRAWAL_MIN_WORKSCORE; i++) {
        BOOST_CHECK(scdbTestCopy.UpdateSCDBIndex(vVote));

        // Make SCDB match copy via hash update
        BOOST_CHECK(scdbTest.UpdateSCDBMatchHash(scdbTestCopy.GetSCDBHash()));
    }

    // Withdrawal should pass with valid workscore
    BOOST_CHECK(scdbTest.CheckWorkScore(0, hash));
}

BOOST_AUTO_TEST_CASE(sidechaindb_aprove_withdrawal_mt_multi_sidechain)
{
    // Test creating and approving multiple withdrawals via hash updates only

    SidechainDB scdbTest;

    BOOST_CHECK(ActivateTestSidechain(scdbTest));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);

    // A second sidechain proposal
    Sidechain proposal;
    proposal.nSidechain = 86;
    proposal.nVersion = 0;
    proposal.title = "sidechain2";
    proposal.description = "test";
    proposal.strKeyID = "c37afd89181060fa69deb3b26a0b95c02986ec78";

    std::vector<unsigned char> vchPubKey = ParseHex("76a91480dca759b4ff2c9e9b65ec790703ad09fba844cd88ac");
    proposal.scriptPubKey = CScript(vchPubKey.begin(), vchPubKey.end());

    proposal.strPrivKey = "5Jf2vbdzdCccKApCrjmwL5EFc4f1cUm5Ah4L4LGimEuFyqYpa9r"; // TODO
    proposal.hashID1 = GetRandHash();
    proposal.hashID2 = uint160S("31d98584f3c570961359c308619f5cf2e9178482");

    // Activate a second sidechain
    ActivateSidechain(scdbTest, proposal, 0);


    uint256 hash = GetRandHash();

    std::map<uint8_t, uint256> mapNewWithdrawal;
    mapNewWithdrawal[0] = hash;

    std::vector<std::string> vVote(SIDECHAIN_ACTIVATION_MAX_ACTIVE, std::string(1, SCDB_ABSTAIN));
    vVote[0] = hash.ToString();
    scdbTest.UpdateSCDBIndex(vVote, false, mapNewWithdrawal);

    // Check if withdrawal was added
    std::vector<SidechainWithdrawalState> vState = scdbTest.GetState(0);
    BOOST_CHECK(vState.size() == 1);

    // Create a copy of the scdbTest to manipulate
    SidechainDB scdbTestCopy = scdbTest;

    // Ack withdrawal bundle until it has 50% of the required score
    for (int i = 1; i < SIDECHAIN_WITHDRAWAL_MIN_WORKSCORE / 2; i++) {
        BOOST_CHECK(scdbTestCopy.UpdateSCDBIndex(vVote));

        // Make SCDB match copy via hash update
        BOOST_CHECK(scdbTest.UpdateSCDBMatchHash(scdbTestCopy.GetSCDBHash()));
    }

    // Withdrawal should not pass
    BOOST_CHECK(!scdbTest.CheckWorkScore(0, hash));


    // Create second withdrawal for second sidechain
    uint256 hash2 = GetRandHash();

    std::map<uint8_t, uint256> mapNewWithdrawal2;
    mapNewWithdrawal2[86] = hash2;

    vVote[86] = hash2.ToString();
    scdbTest.UpdateSCDBIndex(vVote, false, mapNewWithdrawal2);

    // Check if withdrawal was added
    vState = scdbTest.GetState(86);
    BOOST_CHECK(vState.size() == 1);

    // Create a copy of the scdbTest to manipulate
    scdbTestCopy = scdbTest;

    // Ack second withdrawal bundle until it has the required score
    for (int i = 1; i < SIDECHAIN_WITHDRAWAL_MIN_WORKSCORE; i++) {
        BOOST_CHECK(scdbTestCopy.UpdateSCDBIndex(vVote));

        // Make SCDB match copy via hash update
        BOOST_CHECK(scdbTest.UpdateSCDBMatchHash(scdbTestCopy.GetSCDBHash()));
    }

    // Withdrawal 1 should pass
    BOOST_CHECK(scdbTest.CheckWorkScore(0, hash));

    // Withdrawal 2 should pass
    BOOST_CHECK(scdbTest.CheckWorkScore(86, hash2));
}

BOOST_AUTO_TEST_CASE(sidechaindb_wallet_ctip_create)
{
    // Create a deposit (and CTIP) for a single sidechain
    SidechainDB scdbTest;

    BOOST_CHECK(ActivateTestSidechain(scdbTest));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);

    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();

    CKey key;
    CPubKey pubkey;

    key.MakeNewKey(true);
    pubkey = key.GetPubKey();

    // User deposit data script
    CScript dataScript = CScript() << OP_RETURN << ToByteVector(pubkey.GetID());

    mtx.vout.push_back(CTxOut(CAmount(0), dataScript));

    CScript sidechainScript;
    BOOST_CHECK(scdbTest.GetSidechainScript(0, sidechainScript));

    // Add deposit output
    mtx.vout.push_back(CTxOut(50 * CENT, sidechainScript));

    SidechainDeposit deposit;
    deposit.nSidechain = 0;
    deposit.strDest = "";
    deposit.tx = mtx;
    deposit.nBurnIndex = 1;
    deposit.nTx = 1;
    deposit.hashBlock = GetRandHash();

    scdbTest.AddDeposits(std::vector<SidechainDeposit>{ deposit });

    // Check if deposit was cached
    std::vector<SidechainDeposit> vDeposit = scdbTest.GetDeposits(0);
    BOOST_CHECK(vDeposit.size() == 1 && vDeposit.front() == deposit);

    // Check if CTIP was updated
    SidechainCTIP ctip;
    BOOST_CHECK(scdbTest.GetCTIP(0, ctip));
    BOOST_CHECK(ctip.out.hash == deposit.tx.GetHash());
    BOOST_CHECK(ctip.out.n == 1);
}

BOOST_AUTO_TEST_CASE(sidechaindb_wallet_ctip_multi_sidechain)
{
    // Create a deposit (and CTIP) for multiple sidechains
    SidechainDB scdbTest;

    BOOST_CHECK(ActivateTestSidechain(scdbTest));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);
}

BOOST_AUTO_TEST_CASE(sidechaindb_wallet_ctip_multi_deposits)
{
    // Create many deposits and make sure that single valid CTIP results
    SidechainDB scdbTest;

    BOOST_CHECK(ActivateTestSidechain(scdbTest));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);

    // TODO use the wallet function
    // Create deposit
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();

    CKey key;
    CPubKey pubkey;

    key.MakeNewKey(true);
    pubkey = key.GetPubKey();

    // User deposit data script
    CScript dataScript = CScript() << OP_RETURN << ToByteVector(pubkey.GetID());

    mtx.vout.push_back(CTxOut(CAmount(0), dataScript));

    Sidechain sidechain;
    BOOST_CHECK(scdbTest.GetSidechain(0, sidechain));

    CScript sidechainScript;
    BOOST_CHECK(scdbTest.GetSidechainScript(0, sidechainScript));

    // Add deposit output
    mtx.vout.push_back(CTxOut(50 * CENT, sidechainScript));

    SidechainDeposit deposit;
    deposit.nSidechain = 0;
    deposit.strDest = "";
    deposit.tx = mtx;
    deposit.nBurnIndex = 1;
    deposit.nTx = 1;

    scdbTest.AddDeposits(std::vector<SidechainDeposit>{ deposit });

    // Check if we cached it
    std::vector<SidechainDeposit> vDeposit = scdbTest.GetDeposits(0);
    BOOST_CHECK(vDeposit.size() == 1 && vDeposit.front().tx == mtx);

    // Compare with scdbTest CTIP
    SidechainCTIP ctip;
    BOOST_CHECK(scdbTest.GetCTIP(0, ctip));
    BOOST_CHECK(ctip.out.hash == mtx.GetHash());
    BOOST_CHECK(ctip.out.n == 1);

    // Create another deposit
    CMutableTransaction mtx2;
    mtx2.vin.resize(1);
    mtx2.vin[0].prevout.SetNull();

    CKey key2;
    CPubKey pubkey2;

    key2.MakeNewKey(true);
    pubkey2 = key2.GetPubKey();

    // User deposit data script
    CScript dataScript2 = CScript() << OP_RETURN << ToByteVector(pubkey2.GetID());

    mtx2.vout.push_back(CTxOut(CAmount(0), dataScript2));

    // Add deposit output
    mtx2.vout.push_back(CTxOut(25 * CENT, sidechainScript));

    deposit.tx = mtx2;

    scdbTest.AddDeposits(std::vector<SidechainDeposit>{ deposit });

    // Check if we cached it
    vDeposit.clear();
    vDeposit = scdbTest.GetDeposits(0);
    BOOST_CHECK(vDeposit.size() == 2 && vDeposit.back().tx == mtx2);

    // Compare with scdbTest CTIP
    SidechainCTIP ctip2;
    BOOST_CHECK(scdbTest.GetCTIP(0, ctip2));
    BOOST_CHECK(ctip2.out.hash == mtx2.GetHash());
    BOOST_CHECK(ctip2.out.n == 1);
}

BOOST_AUTO_TEST_CASE(sidechaindb_wallet_ctip_spend_withdrawal)
{
    // Create deposit / CTIP for sidechain then withdraw and deposit again.

    SidechainDB scdbTest;

    BOOST_CHECK(ActivateTestSidechain(scdbTest));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);

    // TODO use the wallet function
    // Create deposit
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();

    CKey key;
    CPubKey pubkey;

    key.MakeNewKey(true);
    pubkey = key.GetPubKey();

    // User deposit data script
    CScript dataScript = CScript() << OP_RETURN << ToByteVector(pubkey.GetID());

    mtx.vout.push_back(CTxOut(CAmount(0), dataScript));

    Sidechain sidechain;
    BOOST_CHECK(scdbTest.GetSidechain(0, sidechain));

    CScript sidechainScript;
    BOOST_CHECK(scdbTest.GetSidechainScript(0, sidechainScript));

    // Add deposit output
    mtx.vout.push_back(CTxOut(50 * CENT, sidechainScript));

    SidechainDeposit deposit;
    deposit.nSidechain = 0;
    deposit.strDest = "";
    deposit.tx = mtx;
    deposit.nBurnIndex = 1;
    deposit.nTx = 1;

    scdbTest.AddDeposits(std::vector<SidechainDeposit>{ deposit });

    // Check if we cached it
    std::vector<SidechainDeposit> vDeposit = scdbTest.GetDeposits(0);
    BOOST_CHECK(vDeposit.size() == 1 && vDeposit.front().tx == mtx);

    // Compare with scdbTest CTIP
    SidechainCTIP ctip;
    BOOST_CHECK(scdbTest.GetCTIP(0, ctip));
    BOOST_CHECK(ctip.out.hash == mtx.GetHash());
    BOOST_CHECK(ctip.out.n == 1);

    // Create a withdrawal that spends the CTIP
    CMutableTransaction wmtx;
    wmtx.nVersion = 2;
    wmtx.vin.push_back(CTxIn(ctip.out.hash, ctip.out.n));
    wmtx.vout.push_back(CTxOut(CAmount(0), CScript() << OP_RETURN << ParseHex(HexStr(SIDECHAIN_WITHDRAWAL_RETURN_DEST) )));
    wmtx.vout.push_back(CTxOut(CAmount(0), EncodeWithdrawalFees(1 * CENT)));
    wmtx.vout.push_back(CTxOut(25 * CENT, GetScriptForDestination(pubkey.GetID())));
    wmtx.vout.push_back(CTxOut(24 * CENT, sidechainScript));

    // Give it sufficient work score
    SidechainWithdrawalState state;
    uint256 hashBlind;
    BOOST_CHECK(CTransaction(wmtx).GetBlindHash(hashBlind));

    // Add withdrawal bundle
    scdbTest.AddWithdrawal(0, hashBlind, 0);

    // Check if withdrawal was added
    std::vector<SidechainWithdrawalState> vState = scdbTest.GetState(0);
    BOOST_CHECK(vState.size() == 1);

    // Create upvote vector
    std::vector<std::string> vVote(SIDECHAIN_ACTIVATION_MAX_ACTIVE, std::string(1, SCDB_ABSTAIN));
    vVote[0] = hashBlind.ToString();

    // Ack withdrawal bundle
    for (int i = 1; i < SIDECHAIN_WITHDRAWAL_MIN_WORKSCORE; i++)
        BOOST_CHECK(scdbTest.UpdateSCDBIndex(vVote));

    // The withdrawal should have valid workscore
    BOOST_CHECK(scdbTest.CheckWorkScore(0, hashBlind));

    // Spend the withdrawal
    BOOST_CHECK(scdbTest.SpendWithdrawal(0, GetRandHash(), wmtx, 1));

    // Check that the CTIP has been updated to the change amount
    SidechainCTIP ctipFinal;
    BOOST_CHECK(scdbTest.GetCTIP(0, ctipFinal));
    BOOST_CHECK(ctipFinal.out.hash == wmtx.GetHash());
    BOOST_CHECK(ctipFinal.out.n == 3);

    // Create another deposit
    CMutableTransaction mtx2;
    mtx2.vin.resize(1);
    mtx2.vin[0].prevout.SetNull();

    CKey key2;
    CPubKey pubkey2;

    key2.MakeNewKey(true);
    pubkey2 = key2.GetPubKey();

    // User deposit data script
    CScript dataScript2 = CScript() << OP_RETURN << ToByteVector(pubkey2.GetID());

    mtx2.vout.push_back(CTxOut(CAmount(0), dataScript2));

    // Add deposit output
    mtx2.vout.push_back(CTxOut(25 * CENT, sidechainScript));

    deposit.tx = mtx2;

    scdbTest.AddDeposits(std::vector<SidechainDeposit>{ deposit });

    // Check if we cached it
    vDeposit.clear();
    vDeposit = scdbTest.GetDeposits(0);
    // Should now have 3 deposits cached (first deposit, withdrawal change,
    // this deposit)
    BOOST_CHECK(vDeposit.size() == 3 && vDeposit.back().tx == mtx2);

    // Compare with scdbTest CTIP
    SidechainCTIP ctip2;
    BOOST_CHECK(scdbTest.GetCTIP(0, ctip2));
    BOOST_CHECK(ctip2.out.hash == mtx2.GetHash());
    BOOST_CHECK(ctip2.out.n == 1);
}

BOOST_AUTO_TEST_CASE(IsWithdrawalHashCommit)
{
    // TODO test invalid
    // Test hash commitments for nSidechain 0-255 with random withdrawal hashes
    for (unsigned int i = 0; i < 256; i++) {
        uint256 hash = GetRandHash();
        uint8_t nSidechain = i;

        CBlock block;
        CMutableTransaction mtx;
        mtx.vin.resize(1);
        mtx.vin[0].prevout.SetNull();
        block.vtx.push_back(MakeTransactionRef(std::move(mtx)));
        GenerateWithdrawalHashCommitment(block, hash, nSidechain);

        uint256 hashFromCommit;
        uint8_t nSidechainFromCommit;
        BOOST_CHECK(block.vtx[0]->vout[0].scriptPubKey.IsWithdrawalHashCommit(hashFromCommit, nSidechainFromCommit));

        BOOST_CHECK(hash == hashFromCommit);
        BOOST_CHECK(nSidechain == nSidechainFromCommit);
    }
}

BOOST_AUTO_TEST_CASE(IsSidechainProposalCommit)
{
    // TODO test more proposals with different data, versions etc
    // TODO test invalid

    // Create sidechain proposal
    Sidechain proposal;
    proposal.nVersion = 0;
    proposal.title = "Test";
    proposal.description = "Description";
    proposal.strKeyID = "80dca759b4ff2c9e9b65ec790703ad09fba844cd";

    std::vector<unsigned char> vchPubKey = ParseHex("76a91480dca759b4ff2c9e9b65ec790703ad09fba844cd88ac");
    proposal.scriptPubKey = CScript(vchPubKey.begin(), vchPubKey.end());

    proposal.strPrivKey = "5Jf2vbdzdCccKApCrjmwL5EFc4f1cUm5Ah4L4LGimEuFyqYpa9r";
    proposal.hashID1 = uint256S("b55d224f1fda033d930c92b1b40871f209387355557dd5e0d2b5dd9bb813c33f");
    proposal.hashID2 = uint160S("31d98584f3c570961359c308619f5cf2e9178482");

    // Create transaction output with sidechain proposal
    CTxOut out;
    out.scriptPubKey = proposal.GetProposalScript();
    out.nValue = 50 * CENT;

    BOOST_CHECK(out.scriptPubKey.IsSidechainProposalCommit());
}

BOOST_AUTO_TEST_CASE(IsSidechainActivationCommit)
{
    // TODO test more proposals with different data, versions etc
    // TODO test invalid

    // Create sidechain proposal
    Sidechain proposal;
    proposal.nVersion = 0;
    proposal.title = "Test";
    proposal.description = "Description";
    proposal.strKeyID = "80dca759b4ff2c9e9b65ec790703ad09fba844cd";

    std::vector<unsigned char> vchPubKey = ParseHex("76a91480dca759b4ff2c9e9b65ec790703ad09fba844cd88ac");
    proposal.scriptPubKey = CScript(vchPubKey.begin(), vchPubKey.end());

    proposal.strPrivKey = "5Jf2vbdzdCccKApCrjmwL5EFc4f1cUm5Ah4L4LGimEuFyqYpa9r";
    proposal.hashID1 = uint256S("b55d224f1fda033d930c92b1b40871f209387355557dd5e0d2b5dd9bb813c33f");
    proposal.hashID2 = uint160S("31d98584f3c570961359c308619f5cf2e9178482");

    // Use the function from validation to generate the commit, and then
    // copy it from the block.
    CBlock block;
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    block.vtx.push_back(MakeTransactionRef(std::move(mtx)));
    GenerateSidechainActivationCommitment(block, proposal.GetSerHash());

    uint256 hashSidechain;
    BOOST_CHECK(block.vtx[0]->vout[0].scriptPubKey.IsSidechainActivationCommit(hashSidechain));

    BOOST_CHECK(hashSidechain == proposal.GetSerHash());
}

BOOST_AUTO_TEST_CASE(IsSidechainUpdateBytes)
{
    CBlock block;
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    block.vtx.push_back(MakeTransactionRef(std::move(mtx)));
    CScript script;
    GenerateSCDBUpdateScript(block, script, std::vector<std::vector<SidechainWithdrawalState>>{}, std::vector<std::string>(256, std::string(1, SCDB_ABSTAIN)));

    BOOST_CHECK(block.vtx[0]->vout[0].scriptPubKey.IsSCDBUpdate());
}

BOOST_AUTO_TEST_CASE(update_helper_basic)
{
    // A test of the minimal functionality of generating and parsing an SCDB
    // update script. Two sidechains with one withdrawal each.
    // Abstain withdrawal of sidechain 0 and downvote for sidechain 1.
    SidechainDB scdbTest;

    // Activate first sidechain (default test sidechain)
    BOOST_CHECK(ActivateTestSidechain(scdbTest));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);

    // A second sidechain proposal
    Sidechain proposal;
    proposal.nSidechain = 1;
    proposal.nVersion = 0;
    proposal.title = "sidechain2";
    proposal.description = "test";
    proposal.strKeyID = "c37afd89181060fa69deb3b26a0b95c02986ec78";

    std::vector<unsigned char> vchPubKey = ParseHex("76a91480dca759b4ff2c9e9b65ec790703ad09fba844cd88ac");
    proposal.scriptPubKey = CScript(vchPubKey.begin(), vchPubKey.end());

    proposal.strPrivKey = "5Jf2vbdzdCccKApCrjmwL5EFc4f1cUm5Ah4L4LGimEuFyqYpa9r"; // TODO
    proposal.hashID1 = GetRandHash();
    proposal.hashID2 = uint160S("31d98584f3c570961359c308619f5cf2e9178482");

    // Activate second sidechain
    BOOST_CHECK(ActivateSidechain(scdbTest, proposal, 0));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 2);

    uint256 hash1 = GetRandHash();
    uint256 hash2 = GetRandHash();

    // Add withdrawal bundles
    scdbTest.AddWithdrawal(0, hash1, 0);
    scdbTest.AddWithdrawal(1, hash2, 0);

    // Check if withdrawals were added
    std::vector<SidechainWithdrawalState> vState1 = scdbTest.GetState(0);
    BOOST_CHECK(vState1.size() == 1);

    std::vector<SidechainWithdrawalState> vState2 = scdbTest.GetState(1);
    BOOST_CHECK(vState2.size() == 1);

    // Create a copy of the scdbTest to manipulate
    SidechainDB scdbTestCopy = scdbTest;

    // Update the scdbTest copy to get a new hash. No change to withdrawal
    // SC 0 withdrawal means it will have a default abstain vote.
    std::vector<std::string> vVote(SIDECHAIN_ACTIVATION_MAX_ACTIVE, std::string(1, SCDB_ABSTAIN));
    vVote[1] = SCDB_DOWNVOTE; // Downvote withdrawals of SC # 1

    BOOST_CHECK(scdbTestCopy.UpdateSCDBIndex(vVote));

    // Hash prediction should fail here without update script
    BOOST_CHECK(!scdbTest.UpdateSCDBMatchHash(scdbTestCopy.GetSCDBHash()));

    // Generate an update script
    CBlock block;
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    block.vtx.push_back(MakeTransactionRef(std::move(mtx)));

    std::vector<std::vector<SidechainWithdrawalState>> vOldScores;
    for (const Sidechain& s : scdbTest.GetActiveSidechains()) {
        vOldScores.push_back(scdbTest.GetState(s.nSidechain));
    }
    CScript script;
    GenerateSCDBUpdateScript(block, script, vOldScores, vVote);

    BOOST_CHECK(script.IsSCDBUpdate());

    // Use ParseUpdateScript from validation to read it
    std::vector<std::vector<SidechainWithdrawalState>> vOld;
    for (const Sidechain& s : scdbTest.GetActiveSidechains()) {
        vOld.push_back(scdbTest.GetState(s.nSidechain));
    }

    std::vector<std::string> vParsedVote;
    BOOST_CHECK(ParseSCDBUpdateScript(script, vOld, vParsedVote));

    BOOST_CHECK(scdbTest.UpdateSCDBMatchHash(scdbTestCopy.GetSCDBHash(), vParsedVote));
}

BOOST_AUTO_TEST_CASE(update_helper_max_active)
{
    // Test parsing update bytes with maximum active sidechains

    SidechainDB scdbTest;

    Sidechain proposal;
    proposal.nVersion = 0;
    proposal.title = "sidechain";

    // Activate the maximum number of sidechains allowed
    for (int i = 0; i < SIDECHAIN_ACTIVATION_MAX_ACTIVE; i++) {
        proposal.nSidechain = i;
        proposal.title = "sidechain" + std::to_string(i);

        ActivateSidechain(scdbTest, proposal, 0);
    }

    // Check that the maximum number have been activated
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == SIDECHAIN_ACTIVATION_MAX_ACTIVE);

    // Add a withdrawal to each sidechain

    std::map<uint8_t, uint256> mapNewWithdrawal;
    for (size_t i = 0; i < SIDECHAIN_ACTIVATION_MAX_ACTIVE; i++)
        mapNewWithdrawal[i] = GetRandHash();

    SidechainDB scdbTestCopy = scdbTest;

    std::vector<std::string> vVote(SIDECHAIN_ACTIVATION_MAX_ACTIVE, std::string(1, SCDB_ABSTAIN));
    BOOST_CHECK(scdbTestCopy.UpdateSCDBIndex(vVote, false, mapNewWithdrawal));

    // This update should work without update helper bytes
    BOOST_CHECK(scdbTest.UpdateSCDBMatchHash(scdbTestCopy.GetSCDBHash(), vVote, mapNewWithdrawal));

    // Now test updates that won't work without update helper bytes

    scdbTestCopy = scdbTest;

    vVote[0] = SCDB_DOWNVOTE;
    vVote[5] = mapNewWithdrawal[5].ToString();
    vVote[10] = SCDB_DOWNVOTE;
    vVote[205] = SCDB_DOWNVOTE;
    vVote[245] = SCDB_DOWNVOTE;
    BOOST_CHECK(scdbTestCopy.UpdateSCDBIndex(vVote));

    // This update should not work without update helper bytes
    BOOST_CHECK(!scdbTest.UpdateSCDBMatchHash(scdbTestCopy.GetSCDBHash()));

    // Generate update script

    std::vector<std::vector<SidechainWithdrawalState>> vOldScores;
    for (const Sidechain& s : scdbTest.GetActiveSidechains()) {
        vOldScores.push_back(scdbTest.GetState(s.nSidechain));
    }

    CBlock block;
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    block.vtx.push_back(MakeTransactionRef(std::move(mtx)));
    CScript script;
    GenerateSCDBUpdateScript(block, script, vOldScores, vVote);

    BOOST_CHECK(block.vtx[0]->vout[0].scriptPubKey.IsSCDBUpdate());

    // Read update script
    std::vector<std::string> vParsedVote;
    BOOST_CHECK(ParseSCDBUpdateScript(script, vOldScores, vParsedVote));

    // Compare parsed votes to vote settings
    BOOST_CHECK(vVote == vParsedVote);
    BOOST_CHECK(vVote[245] == vParsedVote[245]);

    // Update copy of SCDB based on update helper bytes
    BOOST_CHECK(scdbTest.UpdateSCDBMatchHash(scdbTestCopy.GetSCDBHash(), vParsedVote));
}

BOOST_AUTO_TEST_CASE(custom_vote_cache)
{
    SidechainDB scdbTest;
    std::vector<std::string> vVote;

    // Incorrect size of vote vector
    BOOST_CHECK(!scdbTest.CacheCustomVotes(vVote));

    // Empty votes
    vVote.resize(SIDECHAIN_ACTIVATION_MAX_ACTIVE);
    BOOST_CHECK(!scdbTest.CacheCustomVotes(vVote));

    // Empty vote
    vVote = std::vector<std::string>(SIDECHAIN_ACTIVATION_MAX_ACTIVE, std::string(1, SCDB_ABSTAIN));
    vVote[86] = "";
    BOOST_CHECK(!scdbTest.CacheCustomVotes(vVote));

    // Invalid vote size
    vVote[86] = std::string(86, '8');
    BOOST_CHECK(!scdbTest.CacheCustomVotes(vVote));

    // Invalid vote
    vVote[86] = "x";
    BOOST_CHECK(!scdbTest.CacheCustomVotes(vVote));

    // Valid votes
    vVote[86] = SCDB_ABSTAIN;
    BOOST_CHECK(scdbTest.CacheCustomVotes(vVote));
}

BOOST_AUTO_TEST_CASE(has_sidechain_script)
{
    // Test checking if a script is an active sidechain deposit script
    SidechainDB scdbTest;

    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 0);
    BOOST_CHECK(ActivateTestSidechain(scdbTest, 0));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);

    Sidechain sidechain;
    BOOST_CHECK(scdbTest.GetSidechain(0, sidechain));

    CScript scriptPubKey = sidechain.scriptPubKey;

    CScript scriptFromDB;
    BOOST_CHECK(scdbTest.GetSidechainScript(0, scriptFromDB));
    BOOST_CHECK(scriptFromDB == scriptPubKey);

    uint8_t nSidechain;
    BOOST_CHECK(scdbTest.HasSidechainScript(std::vector<CScript>{scriptPubKey}, nSidechain));
    BOOST_CHECK(nSidechain == 0);
    BOOST_CHECK(nSidechain == sidechain.nSidechain);

    CScript scriptInvalid = CScript() << 0x01 << 0x02 << 0x03 << 0x04;
    BOOST_CHECK(!scdbTest.HasSidechainScript(std::vector<CScript>{scriptInvalid}, nSidechain));
}

BOOST_AUTO_TEST_CASE(txn_to_deposit)
{
    // Test of the TxnToDeposit function. This is used by the memory pool and
    // connectBlock to easily decode a SidechainDeposit from a deposit transaction.

    // Activate test sidechain

    SidechainDB scdbTest;

    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 0);
    BOOST_CHECK(ActivateTestSidechain(scdbTest, 0));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);

    // Serialized transaction
    std::string strTx1 = "02000000011aeb87c9c992ddc8a39e7659eae88b4160980978fc03dbfa35328c07278d4de600000000484730440220417b0d700a06d205fafa9762876889cf68bcf5e4c01afd289cd9d343c022440c0220725a1a3766dc4021641ae7652788f48bded8554fc216b40ae5c68423ea82416c01ffffffff0380f69f0b010000001976a914a27085ec6c1dba30b631c6e197373b626773837388ac0000000000000000066a04616263640065cd1d000000001976a914cea73972efbfee83fbaba9021c6a0d88d3adf34a88ac00000000";

    // Deserialize
    CMutableTransaction mtx;
    BOOST_CHECK(DecodeHexTx(mtx, strTx1));

    // TxnToDeposit
    SidechainDeposit deposit;
    BOOST_CHECK(scdbTest.TxnToDeposit(mtx, 0, {}, deposit));
}

BOOST_AUTO_TEST_SUITE_END()
