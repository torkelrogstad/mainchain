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

#include "test/test_drivenet.h"

#include <boost/test/unit_test.hpp>

CScript EncodeWTFees(const CAmount& amount)
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

BOOST_AUTO_TEST_CASE(sidechaindb_wtprime)
{
    // Test creating a WT^ and approving it with enough workscore
    SidechainDB scdbTest;

    BOOST_CHECK(ActivateTestSidechain(scdbTest));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);

    uint256 hashWTTest = GetRandHash();

    SidechainWTPrimeState wtTest;
    wtTest.hashWTPrime = hashWTTest;
    wtTest.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - 1;
    wtTest.nSidechain = 0;
    int nHeight = 0;
    for (int i = 1; i <= SIDECHAIN_MIN_WORKSCORE; i++) {
        wtTest.nWorkScore = i;
        BOOST_CHECK(scdbTest.UpdateSCDBIndex(std::vector<SidechainWTPrimeState>{wtTest}, nHeight));
        nHeight++;
    }

    // WT^ 0 should pass with valid workscore
    BOOST_CHECK(scdbTest.CheckWorkScore(0, hashWTTest));
}

BOOST_AUTO_TEST_CASE(sidechaindb_MultipleWTPrimes_one_expires)
{
    // Test multiple verification periods, approve multiple WT^s on the
    // same sidechain
    SidechainDB scdbTest;

    BOOST_CHECK(ActivateTestSidechain(scdbTest));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);

    // WT^ hash for first period
    uint256 hashWTTest1 = GetRandHash();

    // Verify first transaction, check work score
    SidechainWTPrimeState wt1;
    wt1.hashWTPrime = hashWTTest1;
    wt1.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - 1;
    wt1.nSidechain = 0;
    int nHeight = 0;
    int nBlocksLeft = wt1.nBlocksLeft + 1;
    for (int i = 1; i <= SIDECHAIN_MIN_WORKSCORE; i++) {
        std::vector<SidechainWTPrimeState> vWT;
        wt1.nWorkScore = i;
        BOOST_CHECK(scdbTest.UpdateSCDBIndex(std::vector<SidechainWTPrimeState> {wt1}, nHeight));
        nHeight++;
        nBlocksLeft--;
    }
    BOOST_CHECK(scdbTest.CheckWorkScore(0, hashWTTest1));

    // Keep updating until the first WT^ expires
    while (nBlocksLeft >= 0) {
        BOOST_CHECK(scdbTest.UpdateSCDBIndex(std::vector<SidechainWTPrimeState> {wt1}, nHeight, false, std::map<uint8_t, uint256>(), false, true));
        nHeight++;
        nBlocksLeft--;
    }

    // Create dummy coinbase tx
    CMutableTransaction mtx;
    mtx.nVersion = 1;
    mtx.vin.resize(1);
    mtx.vout.resize(1);
    mtx.vin[0].scriptSig = CScript() << 486604799;
    mtx.vout.push_back(CTxOut(50 * CENT, CScript() << OP_RETURN));

    // WT^ hash for second period
    uint256 hashWTTest2 = GetRandHash();

    // Add new WT^
    std::vector<SidechainWTPrimeState> vWT;
    SidechainWTPrimeState wt2;
    wt2.hashWTPrime = hashWTTest2;
    wt2.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - 1;
    wt2.nSidechain = 0;
    wt2.nWorkScore = 1;
    vWT.push_back(wt2);
    BOOST_CHECK(scdbTest.UpdateSCDBIndex(vWT, 0));
    BOOST_CHECK(!scdbTest.CheckWorkScore(0, hashWTTest2));

    // Verify that scdbTest has updated to correct WT^
    const std::vector<SidechainWTPrimeState> vState = scdbTest.GetState(0);
    BOOST_CHECK(vState.size() == 1 && vState[0].hashWTPrime == hashWTTest2);

    // Give second transaction sufficient workscore and check work score
    nHeight = 0;
    for (int i = 1; i <= SIDECHAIN_MIN_WORKSCORE; i++) {
        std::vector<SidechainWTPrimeState> vWT;
        wt2.nWorkScore = i;
        vWT.push_back(wt2);
        scdbTest.UpdateSCDBIndex(vWT, nHeight);
        nHeight++;
    }
    BOOST_CHECK(scdbTest.CheckWorkScore(0, hashWTTest2));
}

BOOST_AUTO_TEST_CASE(sidechaindb_MT_single)
{
    // Merkle tree based scdbTest update test with only scdbTest data (no LD)
    // in the tree, and a single WT^ to be updated.
    SidechainDB scdbTest;

    BOOST_CHECK(ActivateTestSidechain(scdbTest));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);

    // Create scdbTest with initial WT^
    std::vector<SidechainWTPrimeState> vWT;

    SidechainWTPrimeState wt;
    wt.hashWTPrime = GetRandHash();
    wt.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - 1;
    wt.nWorkScore = 1;
    wt.nSidechain = 0;

    vWT.push_back(wt);
    scdbTest.UpdateSCDBIndex(vWT, 0);

    // Create a copy of the scdbTest to manipulate
    SidechainDB scdbTestCopy = scdbTest;

    // Update the scdbTest copy to get a new MT hash
    vWT.clear();
    wt.nWorkScore++;
    vWT.push_back(wt);
    scdbTestCopy.UpdateSCDBIndex(vWT, 0);

    BOOST_CHECK(scdbTest.UpdateSCDBMatchMT(2, scdbTestCopy.GetSCDBHash()));
}

BOOST_AUTO_TEST_CASE(sidechaindb_MT_multipleSC)
{
    // TODO fix, does not actually have multiple sidechains
    // Merkle tree based scdbTest update test with multiple sidechains that each
    // have one WT^ to update. Only one WT^ out of the three will be updated.
    // This test ensures that nBlocksLeft is properly decremented even when a
    // WT^'s score is unchanged.
    SidechainDB scdbTest;

    BOOST_CHECK(ActivateTestSidechain(scdbTest));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);

    // Add initial WT^s to scdbTest
    SidechainWTPrimeState wtTest;
    wtTest.hashWTPrime = GetRandHash();
    wtTest.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - 1;
    wtTest.nSidechain = 0;
    wtTest.nWorkScore = 1;

    std::vector<SidechainWTPrimeState> vWT;
    vWT.push_back(wtTest);

    scdbTest.UpdateSCDBIndex(vWT, 0);

    // Create a copy of the scdbTest to manipulate
    SidechainDB scdbTestCopy = scdbTest;

    // Update the scdbTest copy to get a new MT hash
    wtTest.nWorkScore++;

    vWT.clear();
    vWT.push_back(wtTest);

    scdbTestCopy.UpdateSCDBIndex(vWT, 1);

    // Use MT hash prediction to update the original scdbTest
    BOOST_CHECK(scdbTest.UpdateSCDBMatchMT(2, scdbTestCopy.GetSCDBHash()));
}

BOOST_AUTO_TEST_CASE(sidechaindb_MT_multipleWT)
{
    // TODO fix, does not actually have multiple sidechains
    // Merkle tree based scdbTest update test with multiple sidechains and multiple
    // WT^(s) being updated. This tests that MT based scdbTest update will work if
    // work scores are updated for more than one sidechain per block.
    SidechainDB scdbTest;

    BOOST_CHECK(ActivateTestSidechain(scdbTest));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);

    // Add initial WT^s to scdbTest
    SidechainWTPrimeState wtTest;
    wtTest.hashWTPrime = GetRandHash();
    wtTest.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - 1;
    wtTest.nSidechain = 0;
    wtTest.nWorkScore = 1;

    std::vector<SidechainWTPrimeState> vWT;
    vWT.push_back(wtTest);

    scdbTest.UpdateSCDBIndex(vWT, 0);

    // Create a copy of the scdbTest to manipulate
    SidechainDB scdbTestCopy = scdbTest;

    // Update the scdbTest copy to get a new MT hash
    wtTest.nWorkScore++;

    vWT.clear();
    vWT.push_back(wtTest);

    scdbTestCopy.UpdateSCDBIndex(vWT, 1);

    // Use MT hash prediction to update the original scdbTest
    BOOST_CHECK(scdbTest.UpdateSCDBMatchMT(2, scdbTestCopy.GetSCDBHash()));
}

BOOST_AUTO_TEST_CASE(sidechaindb_wallet_ctip_create)
{
    // Create a deposit (and CTIP) for a single sidechain
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

    scdbTest.AddDepositsFromBlock(std::vector<CTransaction>{mtx}, GetRandHash());

    // Check if we cached it
    std::vector<SidechainDeposit> vDeposit = scdbTest.GetDeposits(0);
    BOOST_CHECK(vDeposit.size() == 1 && vDeposit.front().tx == mtx);

    // Compare with scdbTest CTIP
    SidechainCTIP ctip;
    BOOST_CHECK(scdbTest.GetCTIP(0, ctip));
    BOOST_CHECK(ctip.out.hash == mtx.GetHash());
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

    scdbTest.AddDepositsFromBlock(std::vector<CTransaction>{mtx}, GetRandHash());

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

    scdbTest.AddDepositsFromBlock(std::vector<CTransaction>{mtx2}, GetRandHash());

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

BOOST_AUTO_TEST_CASE(sidechaindb_wallet_ctip_multi_deposits_multi_sidechain)
{
    // TODO
    // Create many deposits and make sure that single valid CTIP results
    // for multiple sidechains.
    SidechainDB scdbTest;

    BOOST_CHECK(ActivateTestSidechain(scdbTest));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);
}

BOOST_AUTO_TEST_CASE(sidechaindb_wallet_ctip_spend_wtprime)
{
    // Create a deposit (and CTIP) for a single sidechain,
    // and then spend it with a WT^
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

    scdbTest.AddDepositsFromBlock(std::vector<CTransaction>{mtx}, GetRandHash());

    // Check if we cached it
    std::vector<SidechainDeposit> vDeposit = scdbTest.GetDeposits(0);
    BOOST_CHECK(vDeposit.size() == 1 && vDeposit.front().tx == mtx);

    // Compare with scdbTest CTIP
    SidechainCTIP ctip;
    BOOST_CHECK(scdbTest.GetCTIP(0, ctip));
    BOOST_CHECK(ctip.out.hash == mtx.GetHash());
    BOOST_CHECK(ctip.out.n == 1);

    // Create a WT^ that spends the CTIP
    CMutableTransaction wmtx;
    wmtx.nVersion = 2;
    wmtx.vin.push_back(CTxIn(ctip.out.hash, ctip.out.n));
    wmtx.vout.push_back(CTxOut(CAmount(0), CScript() << OP_RETURN << ParseHex(HexStr(SIDECHAIN_WTPRIME_RETURN_DEST) )));
    wmtx.vout.push_back(CTxOut(CAmount(0), EncodeWTFees(1 * CENT)));
    wmtx.vout.push_back(CTxOut(25 * CENT, GetScriptForDestination(pubkey.GetID())));
    wmtx.vout.push_back(CTxOut(24 * CENT, sidechainScript));

    // Give it sufficient work score
    SidechainWTPrimeState wt;
    uint256 hashBlind;
    BOOST_CHECK(CTransaction(wmtx).GetBWTHash(hashBlind));
    wt.hashWTPrime = hashBlind;
    wt.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - 1;
    wt.nSidechain = 0;
    int nHeight = 0;
    for (int i = 1; i <= SIDECHAIN_MIN_WORKSCORE; i++) {
        wt.nWorkScore = i;
        BOOST_CHECK(scdbTest.UpdateSCDBIndex(std::vector<SidechainWTPrimeState>{wt}, nHeight));
        nHeight++;
    }

    // WT^ 0 should pass with valid workscore
    BOOST_CHECK(scdbTest.CheckWorkScore(0, hashBlind));

    // Spend the WT^
    BOOST_CHECK(scdbTest.SpendWTPrime(0, GetRandHash(), wmtx));

    // Check that the CTIP has been updated to the return amount from the WT^
    SidechainCTIP ctipFinal;
    BOOST_CHECK(scdbTest.GetCTIP(0, ctipFinal));
    BOOST_CHECK(ctipFinal.out.hash == wmtx.GetHash());
    BOOST_CHECK(ctipFinal.out.n == 3);
}

BOOST_AUTO_TEST_CASE(sidechaindb_wallet_ctip_spend_wtprime_then_deposit)
{
    // Create a deposit (and CTIP) for a single sidechain, and then spend it
    // with a WT^. After doing that, create another deposit.
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

    scdbTest.AddDepositsFromBlock(std::vector<CTransaction>{mtx}, GetRandHash());

    // Check if we cached it
    std::vector<SidechainDeposit> vDeposit = scdbTest.GetDeposits(0);
    BOOST_CHECK(vDeposit.size() == 1 && vDeposit.front().tx == mtx);

    // Compare with scdbTest CTIP
    SidechainCTIP ctip;
    BOOST_CHECK(scdbTest.GetCTIP(0, ctip));
    BOOST_CHECK(ctip.out.hash == mtx.GetHash());
    BOOST_CHECK(ctip.out.n == 1);

    // Create a WT^ that spends the CTIP
    CMutableTransaction wmtx;
    wmtx.nVersion = 2;
    wmtx.vin.push_back(CTxIn(ctip.out.hash, ctip.out.n));
    wmtx.vout.push_back(CTxOut(CAmount(0), CScript() << OP_RETURN << ParseHex(HexStr(SIDECHAIN_WTPRIME_RETURN_DEST) )));
    wmtx.vout.push_back(CTxOut(CAmount(0), EncodeWTFees(1 * CENT)));
    wmtx.vout.push_back(CTxOut(25 * CENT, GetScriptForDestination(pubkey.GetID())));
    wmtx.vout.push_back(CTxOut(24 * CENT, sidechainScript));

    // Give it sufficient work score
    SidechainWTPrimeState wt;
    uint256 hashBlind;
    BOOST_CHECK(CTransaction(wmtx).GetBWTHash(hashBlind));
    wt.hashWTPrime = hashBlind;
    wt.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - 1;
    wt.nSidechain = 0;
    int nHeight = 0;
    for (int i = 1; i <= SIDECHAIN_MIN_WORKSCORE; i++) {
        wt.nWorkScore = i;
        scdbTest.UpdateSCDBIndex(std::vector<SidechainWTPrimeState>{wt}, nHeight);
        nHeight++;
    }

    // WT^ 0 should pass with valid workscore (100/100)
    BOOST_CHECK(scdbTest.CheckWorkScore(0, hashBlind));

    // Spend the WT^
    BOOST_CHECK(scdbTest.SpendWTPrime(0, GetRandHash(), wmtx));

    // Check that the CTIP has been updated to the return amount from the WT^
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

    scdbTest.AddDepositsFromBlock(std::vector<CTransaction>{mtx2}, GetRandHash());

    // Check if we cached it
    vDeposit.clear();
    vDeposit = scdbTest.GetDeposits(0);
    // Should now have 3 deposits cached (first deposit, WT^, this deposit)
    BOOST_CHECK(vDeposit.size() == 3 && vDeposit.back().tx == mtx2);

    // Compare with scdbTest CTIP
    SidechainCTIP ctip2;
    BOOST_CHECK(scdbTest.GetCTIP(0, ctip2));
    BOOST_CHECK(ctip2.out.hash == mtx2.GetHash());
    BOOST_CHECK(ctip2.out.n == 1);
}

BOOST_AUTO_TEST_CASE(IsCriticalHashCommit)
{
    // TODO
}

BOOST_AUTO_TEST_CASE(IsSCDBHashMerkleRootCommit)
{
    // TODO
}

BOOST_AUTO_TEST_CASE(IsWTPrimeHashCommit)
{
    // TODO test invalid
    // Test WT^ hash commitments for nSidechain 0-255 with random WT^ hashes
    for (unsigned int i = 0; i < 256; i++) {
        uint256 hashWTPrime = GetRandHash();
        uint8_t nSidechain = i;

        CBlock block;
        CMutableTransaction mtx;
        mtx.vin.resize(1);
        mtx.vin[0].prevout.SetNull();
        block.vtx.push_back(MakeTransactionRef(std::move(mtx)));
        GenerateWTPrimeHashCommitment(block, hashWTPrime, nSidechain, Params().GetConsensus());

        uint256 hashWTPrimeFromCommit;
        uint8_t nSidechainFromCommit;
        BOOST_CHECK(block.vtx[0]->vout[0].scriptPubKey.IsWTPrimeHashCommit(hashWTPrimeFromCommit, nSidechainFromCommit));

        BOOST_CHECK(hashWTPrime == hashWTPrimeFromCommit);
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
    GenerateSidechainActivationCommitment(block, proposal.GetHash(), Params().GetConsensus());

    uint256 hashSidechain;
    BOOST_CHECK(block.vtx[0]->vout[0].scriptPubKey.IsSidechainActivationCommit(hashSidechain));

    BOOST_CHECK(hashSidechain == proposal.GetHash());
}


BOOST_AUTO_TEST_CASE(IsSidechainUpdateBytes)
{
    CBlock block;
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    block.vtx.push_back(MakeTransactionRef(std::move(mtx)));
    CScript script;
    GenerateSCDBUpdateScript(block, script, std::vector<std::vector<SidechainWTPrimeState>>{}, std::vector<SidechainCustomVote>{}, Params().GetConsensus());

    BOOST_CHECK(block.vtx[0]->vout[0].scriptPubKey.IsSCDBUpdate());
}

BOOST_AUTO_TEST_CASE(update_helper_basic)
{
    // A test of the minimal functionality of generating and parsing an SCDB
    // update script. Two sidechains with one WT^ each. Abstain WT^ of sidechain
    // 0 and downvote WT^ of sidechain 1.
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

    // Add initial WT^s to scdbTest
    SidechainWTPrimeState wt1;
    wt1.hashWTPrime = GetRandHash();
    wt1.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - 1;
    wt1.nSidechain = 0; // For first sidechain
    wt1.nWorkScore = 1;

    SidechainWTPrimeState wt2;
    wt2.hashWTPrime = GetRandHash();
    wt2.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - 1;
    wt2.nSidechain = 1; // For second sidechain
    wt2.nWorkScore = 1;

    std::vector<SidechainWTPrimeState> vWT;
    vWT.push_back(wt1);
    vWT.push_back(wt2);

    BOOST_CHECK(scdbTest.UpdateSCDBIndex(vWT, 0));
    BOOST_CHECK(scdbTest.GetState(0).size() == 1);
    BOOST_CHECK(scdbTest.GetState(1).size() == 1);

    // Create a copy of the scdbTest to manipulate
    SidechainDB scdbTestCopy = scdbTest;

    // Update the scdbTest copy to get a new MT hash
    // No change to WT^ 1 means it will have a default abstain vote
    wt2.nWorkScore--;

    vWT.clear();
    vWT.push_back(wt1);
    vWT.push_back(wt2);

    BOOST_CHECK(scdbTestCopy.UpdateSCDBIndex(vWT, 1));

    // MT hash prediction should fail here without update script
    BOOST_CHECK(!scdbTest.UpdateSCDBMatchMT(2, scdbTestCopy.GetSCDBHash()));

    // Create custom vote for WT^ 2
    SidechainCustomVote vote;
    vote.nSidechain = 1;
    vote.hashWTPrime = wt2.hashWTPrime;
    vote.vote = SCDB_DOWNVOTE;

    // Generate an update script
    CBlock block;
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    block.vtx.push_back(MakeTransactionRef(std::move(mtx)));

    std::vector<std::vector<SidechainWTPrimeState>> vOldScores;
    for (const Sidechain& s : scdbTest.GetActiveSidechains()) {
        vOldScores.push_back(scdbTest.GetState(s.nSidechain));
    }
    CScript script;
    GenerateSCDBUpdateScript(block, script, vOldScores, std::vector<SidechainCustomVote>{vote}, Params().GetConsensus());

    BOOST_CHECK(script.IsSCDBUpdate());

    // Use ParseUpdateScript from validation to read it
    std::vector<SidechainWTPrimeState> vNew;
    std::vector<std::vector<SidechainWTPrimeState>> vOld;
    for (const Sidechain& s : scdbTest.GetActiveSidechains()) {
        vOld.push_back(scdbTest.GetState(s.nSidechain));
    }
    BOOST_CHECK(ParseSCDBUpdateScript(script, vOld, vNew));

    BOOST_CHECK(scdbTest.UpdateSCDBMatchMT(2, scdbTestCopy.GetSCDBHash(), vNew));
}

BOOST_AUTO_TEST_CASE(update_helper_basic_3_withdrawals)
{
    // A test of the minimal functionality of generating and parsing an SCDB
    // update script. One sidechain with three WT^(s). Upvote the middle WT^
    SidechainDB scdbTest;

    // Activate sidechain (default test sidechain)
    BOOST_CHECK(ActivateTestSidechain(scdbTest));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);

    // Add initial WT^s to scdbTest
    SidechainWTPrimeState wt1;
    wt1.hashWTPrime = GetRandHash();
    wt1.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - 1;
    wt1.nSidechain = 0;
    wt1.nWorkScore = 1;

    SidechainWTPrimeState wt2;
    wt2.hashWTPrime = GetRandHash();
    wt2.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - 1;
    wt2.nSidechain = 0;
    wt2.nWorkScore = 1;

    SidechainWTPrimeState wt3;
    wt3.hashWTPrime = GetRandHash();
    wt3.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - 1;
    wt3.nSidechain = 0;
    wt3.nWorkScore = 1;

    std::vector<SidechainWTPrimeState> vWT;
    vWT.push_back(wt1);
    vWT.push_back(wt2);
    vWT.push_back(wt3);

    for (const SidechainWTPrimeState& wt : vWT) {
        std::map<uint8_t, uint256> mapNewWTPrime;
        mapNewWTPrime[wt.nSidechain] = wt.hashWTPrime;
        BOOST_CHECK(scdbTest.UpdateSCDBIndex(std::vector<SidechainWTPrimeState>{ wt }, 0, false, mapNewWTPrime));
    }

    BOOST_CHECK(scdbTest.GetState(0).size() == 3);

    // Create a copy of the scdbTest to manipulate
    SidechainDB scdbTestCopy = scdbTest;

    // Update the scdbTest copy to get a new MT hash

    wt2.nWorkScore = 1;

    vWT.clear();
    vWT.push_back(wt2);

    BOOST_CHECK(scdbTestCopy.UpdateSCDBIndex(vWT, 1));

    // MT hash prediction should fail here without update script
    BOOST_CHECK(!scdbTest.UpdateSCDBMatchMT(2, scdbTestCopy.GetSCDBHash()));

    // Create custom vote for WT^ 2
    SidechainCustomVote vote;
    vote.nSidechain = 0;
    vote.hashWTPrime = wt2.hashWTPrime;
    vote.vote = SCDB_UPVOTE;

    // Generate an update script
    CBlock block;
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    block.vtx.push_back(MakeTransactionRef(std::move(mtx)));

    std::vector<std::vector<SidechainWTPrimeState>> vOldScores;
    for (const Sidechain& s : scdbTest.GetActiveSidechains()) {
        vOldScores.push_back(scdbTest.GetState(s.nSidechain));
    }
    CScript script;
    GenerateSCDBUpdateScript(block, script, vOldScores, std::vector<SidechainCustomVote>{vote}, Params().GetConsensus());

    BOOST_CHECK(script.IsSCDBUpdate());

    // Use ParseUpdateScript from validation to read it
    std::vector<SidechainWTPrimeState> vNew;
    std::vector<std::vector<SidechainWTPrimeState>> vOld;
    for (const Sidechain& s : scdbTest.GetActiveSidechains()) {
        vOld.push_back(scdbTest.GetState(s.nSidechain));
    }
    BOOST_CHECK(ParseSCDBUpdateScript(script, vOld, vNew));

    BOOST_CHECK(scdbTest.UpdateSCDBMatchMT(2, scdbTestCopy.GetSCDBHash(), vNew));
}

BOOST_AUTO_TEST_CASE(update_helper_basic_four_withdrawals)
{
    // A test of the minimal functionality of generating and parsing an SCDB
    // update script. One sidechain with four WT^(s). Upvote the third WT^.
    SidechainDB scdbTest;

    // Activate sidechain (default test sidechain)
    BOOST_CHECK(ActivateTestSidechain(scdbTest));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);

    // Add initial WT^s to scdbTest
    SidechainWTPrimeState wt1;
    wt1.hashWTPrime = GetRandHash();
    wt1.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - 1;
    wt1.nSidechain = 0;
    wt1.nWorkScore = 1;

    SidechainWTPrimeState wt2;
    wt2.hashWTPrime = GetRandHash();
    wt2.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - 1;
    wt2.nSidechain = 0;
    wt2.nWorkScore = 1;

    SidechainWTPrimeState wt3;
    wt3.hashWTPrime = GetRandHash();
    wt3.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - 1;
    wt3.nSidechain = 0;
    wt3.nWorkScore = 1;

    SidechainWTPrimeState wt4;
    wt4.hashWTPrime = GetRandHash();
    wt4.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - 1;
    wt4.nSidechain = 0;
    wt4.nWorkScore = 1;

    std::vector<SidechainWTPrimeState> vWT;
    vWT.push_back(wt1);
    vWT.push_back(wt2);
    vWT.push_back(wt3);
    vWT.push_back(wt4);

    for (const SidechainWTPrimeState& wt : vWT) {
        std::map<uint8_t, uint256> mapNewWTPrime;
        mapNewWTPrime[wt.nSidechain] = wt.hashWTPrime;
        BOOST_CHECK(scdbTest.UpdateSCDBIndex(std::vector<SidechainWTPrimeState>{ wt }, 0, false, mapNewWTPrime));
    }

    BOOST_CHECK(scdbTest.GetState(0).size() == 4);

    // Create a copy of the scdbTest to manipulate
    SidechainDB scdbTestCopy = scdbTest;

    // Update the scdbTest copy to get a new MT hash

    wt3.nWorkScore = 1;

    vWT.clear();
    vWT.push_back(wt3);

    BOOST_CHECK(scdbTestCopy.UpdateSCDBIndex(vWT, 1));

    // MT hash prediction should fail here without update script
    BOOST_CHECK(!scdbTest.UpdateSCDBMatchMT(2, scdbTestCopy.GetSCDBHash()));

    // Create custom vote for WT^ 2
    SidechainCustomVote vote;
    vote.nSidechain = 0;
    vote.hashWTPrime = wt3.hashWTPrime;
    vote.vote = SCDB_UPVOTE;

    // Generate an update script
    CBlock block;
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    block.vtx.push_back(MakeTransactionRef(std::move(mtx)));

    std::vector<std::vector<SidechainWTPrimeState>> vOldScores;
    for (const Sidechain& s : scdbTest.GetActiveSidechains()) {
        vOldScores.push_back(scdbTest.GetState(s.nSidechain));
    }
    CScript script;
    GenerateSCDBUpdateScript(block, script, vOldScores, std::vector<SidechainCustomVote>{vote}, Params().GetConsensus());

    BOOST_CHECK(script.IsSCDBUpdate());

    // Use ParseUpdateScript from validation to read it
    std::vector<SidechainWTPrimeState> vNew;
    std::vector<std::vector<SidechainWTPrimeState>> vOld;
    for (const Sidechain& s : scdbTest.GetActiveSidechains()) {
        vOld.push_back(scdbTest.GetState(s.nSidechain));
    }
    BOOST_CHECK(ParseSCDBUpdateScript(script, vOld, vNew));

    BOOST_CHECK(scdbTest.UpdateSCDBMatchMT(2, scdbTestCopy.GetSCDBHash(), vNew));
}

BOOST_AUTO_TEST_CASE(update_helper_multi_custom)
{
    // SCDB update script test with custom votes for more than one WT^ and
    // three active sidechains but still only one WT^ per sidechain.
    SidechainDB scdbTest;

    // Activate first sidechain (default test sidechain)
    BOOST_CHECK(ActivateTestSidechain(scdbTest));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);

    // A second sidechain proposal
    Sidechain proposal;
    proposal.nSidechain = 1;
    proposal.nVersion = 0;
    proposal.title = "sidechain2";
    proposal.description = "test 2";
    proposal.strKeyID = "c37afd89181060fa69deb3b26a0b95c02986ec78";

    std::vector<unsigned char> vchPubKey = ParseHex("76a91480dca759b4ff2c9e9b65ec790703ad09fba844cd88ac");
    proposal.scriptPubKey = CScript(vchPubKey.begin(), vchPubKey.end());

    proposal.strPrivKey = "5Jf2vbdzdCccKApCrjmwL5EFc4f1cUm5Ah4L4LGimEuFyqYpa9r"; // TODO
    proposal.hashID1 = GetRandHash();
    proposal.hashID2 = uint160S("31d98584f3c570961359c308619f5cf2e9178482");

    // Activate second sidechain
    BOOST_CHECK(ActivateSidechain(scdbTest, proposal, 0));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 2);

    // A third sidechain proposal
    Sidechain proposal2;
    proposal2.nSidechain = 2;
    proposal2.nVersion = 0;
    proposal2.title = "sidechain3";
    proposal2.description = "test 3";
    proposal2.hashID1 = GetRandHash();
    proposal2.hashID2 = uint160S("31d98584f3c570961359c308619f5cf2e9178482");

    // Activate second sidechain
    BOOST_CHECK(ActivateSidechain(scdbTest, proposal2, 0, true));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 3);

    // Add initial WT^s to scdbTest
    SidechainWTPrimeState wt1;
    wt1.hashWTPrime = GetRandHash();
    wt1.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - 1;
    wt1.nSidechain = 0; // For first sidechain
    wt1.nWorkScore = 1;

    SidechainWTPrimeState wt2;
    wt2.hashWTPrime = GetRandHash();
    wt2.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - 1;
    wt2.nSidechain = 1; // For second sidechain
    wt2.nWorkScore = 1;

    SidechainWTPrimeState wt3;
    wt3.hashWTPrime = GetRandHash();
    wt3.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - 1;
    wt3.nSidechain = 2; // For third sidechain
    wt3.nWorkScore = 1;

    std::vector<SidechainWTPrimeState> vWT;
    vWT.push_back(wt1);
    vWT.push_back(wt2);
    vWT.push_back(wt3);

    BOOST_CHECK(scdbTest.UpdateSCDBIndex(vWT, 0));
    BOOST_CHECK(scdbTest.GetState(0).size() == 1);
    BOOST_CHECK(scdbTest.GetState(1).size() == 1);
    BOOST_CHECK(scdbTest.GetState(2).size() == 1);

    // Create a copy of the scdbTest to manipulate
    SidechainDB scdbTestCopy = scdbTest;

    // Update the scdbTest copy to get a new MT hash
    // No change to WT^ 1 means it will have a default abstain vote

    wt2.nWorkScore--;
    wt3.nWorkScore++;

    vWT.clear();
    vWT.push_back(wt1);
    vWT.push_back(wt2);
    vWT.push_back(wt3);

    BOOST_CHECK(scdbTestCopy.UpdateSCDBIndex(vWT, 1));

    // MT hash prediction should fail here without update script
    BOOST_CHECK(!scdbTest.UpdateSCDBMatchMT(2, scdbTestCopy.GetSCDBHash()));

    // Create custom vote for WT^ 2
    SidechainCustomVote vote;
    vote.nSidechain = 1;
    vote.hashWTPrime = wt2.hashWTPrime;
    vote.vote = SCDB_DOWNVOTE;

    // Create custom vote for WT^ 3
    SidechainCustomVote vote2;
    vote2.nSidechain = 2;
    vote2.hashWTPrime = wt3.hashWTPrime;
    vote2.vote = SCDB_UPVOTE;

    // Generate an update script
    CBlock block;
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    block.vtx.push_back(MakeTransactionRef(std::move(mtx)));

    std::vector<std::vector<SidechainWTPrimeState>> vOldScores;
    for (const Sidechain& s : scdbTest.GetActiveSidechains()) {
        vOldScores.push_back(scdbTest.GetState(s.nSidechain));
    }
    CScript script;
    GenerateSCDBUpdateScript(block, script, vOldScores, std::vector<SidechainCustomVote>{vote, vote2}, Params().GetConsensus());

    BOOST_CHECK(script.IsSCDBUpdate());

    // Use ParseUpdateScript from validation to read it
    std::vector<SidechainWTPrimeState> vNew;
    std::vector<std::vector<SidechainWTPrimeState>> vOld;
    for (const Sidechain& s : scdbTest.GetActiveSidechains()) {
        vOld.push_back(scdbTest.GetState(s.nSidechain));
    }
    BOOST_CHECK(vOld.size() == 3);
    BOOST_CHECK(ParseSCDBUpdateScript(script, vOld, vNew));
    BOOST_CHECK(vNew.size() == 2);

    BOOST_CHECK(scdbTest.UpdateSCDBMatchMT(2, scdbTestCopy.GetSCDBHash(), vNew));
}

BOOST_AUTO_TEST_CASE(update_helper_multi_custom_multi_wtprime)
{
    // SCDB update script test with custom votes for more than one WT^ and
    // three active sidechains with multiple WT^(s) per sidechain.
    SidechainDB scdbTest;

    // Activate first sidechain (default test sidechain)
    BOOST_CHECK(ActivateTestSidechain(scdbTest));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);

    // A second sidechain proposal
    Sidechain proposal;
    proposal.nSidechain = 1;
    proposal.nVersion = 0;
    proposal.title = "sidechain2";
    proposal.description = "test 2";
    proposal.strKeyID = "c37afd89181060fa69deb3b26a0b95c02986ec78";

    std::vector<unsigned char> vchPubKey = ParseHex("76a91480dca759b4ff2c9e9b65ec790703ad09fba844cd88ac");
    proposal.scriptPubKey = CScript(vchPubKey.begin(), vchPubKey.end());

    proposal.strPrivKey = "5Jf2vbdzdCccKApCrjmwL5EFc4f1cUm5Ah4L4LGimEuFyqYpa9r"; // TODO
    proposal.hashID1 = GetRandHash();
    proposal.hashID2 = uint160S("31d98584f3c570961359c308619f5cf2e9178482");

    // Activate second sidechain
    BOOST_CHECK(ActivateSidechain(scdbTest, proposal, 0));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 2);

    // A third sidechain proposal
    Sidechain proposal2;
    proposal2.nSidechain = 2;
    proposal2.nVersion = 0;
    proposal2.title = "sidechain3";
    proposal2.description = "test 3";
    proposal2.hashID1 = GetRandHash();
    proposal2.hashID2 = uint160S("31d98584f3c570961359c308619f5cf2e9178482");

    // Activate third sidechain
    BOOST_CHECK(ActivateSidechain(scdbTest, proposal2, 0, true));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 3);

    // Add initial WT^s to scdbTest
    SidechainWTPrimeState wt1a;
    wt1a.hashWTPrime = GetRandHash();
    wt1a.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - 1;
    wt1a.nSidechain = 0; // For first sidechain
    wt1a.nWorkScore = 1;

    SidechainWTPrimeState wt2a;
    wt2a.hashWTPrime = GetRandHash();
    wt2a.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - 1;
    wt2a.nSidechain = 1; // For second sidechain
    wt2a.nWorkScore = 1;

    SidechainWTPrimeState wt3a;
    wt3a.hashWTPrime = GetRandHash();
    wt3a.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - 1;
    wt3a.nSidechain = 2; // For third sidechain
    wt3a.nWorkScore = 1;

    std::vector<SidechainWTPrimeState> vWT;
    vWT.push_back(wt1a);
    vWT.push_back(wt2a);
    vWT.push_back(wt3a);

    BOOST_CHECK(scdbTest.UpdateSCDBIndex(vWT, 0));
    BOOST_CHECK(scdbTest.GetState(0).size() == 1);
    BOOST_CHECK(scdbTest.GetState(1).size() == 1);
    BOOST_CHECK(scdbTest.GetState(2).size() == 1);

    // Create a copy of the scdbTest to manipulate
    SidechainDB scdbTestCopy = scdbTest;

    // Update the scdbTest copy to get a new MT hash
    wt3a.nWorkScore++;

    vWT.clear();
    vWT.push_back(wt3a);

    BOOST_CHECK(scdbTestCopy.UpdateSCDBIndex(vWT, 1));

    // MT hash prediction should fail here without update script
    BOOST_CHECK(!scdbTest.UpdateSCDBMatchMT(2, scdbTestCopy.GetSCDBHash()));

    // Create custom vote for WT^ 1
    SidechainCustomVote vote;
    vote.nSidechain = 0;
    vote.hashWTPrime = wt1a.hashWTPrime;
    vote.vote = SCDB_DOWNVOTE;

    // Create custom vote for WT^ 2
    SidechainCustomVote vote1;
    vote1.nSidechain = 1;
    vote1.hashWTPrime = wt2a.hashWTPrime;
    vote1.vote = SCDB_DOWNVOTE;

    // Create custom vote for WT^ 3
    SidechainCustomVote vote2;
    vote2.nSidechain = 2;
    vote2.hashWTPrime = wt3a.hashWTPrime;
    vote2.vote = SCDB_UPVOTE;

    // Generate an update script
    CBlock block;
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    block.vtx.push_back(MakeTransactionRef(std::move(mtx)));

    std::vector<std::vector<SidechainWTPrimeState>> vOldScores;
    for (const Sidechain& s : scdbTest.GetActiveSidechains()) {
        vOldScores.push_back(scdbTest.GetState(s.nSidechain));
    }
    CScript script;
    GenerateSCDBUpdateScript(block, script, vOldScores, std::vector<SidechainCustomVote>{vote2}, Params().GetConsensus());

    BOOST_CHECK(script.IsSCDBUpdate());

    // Use ParseUpdateScript from validation to read it
    std::vector<SidechainWTPrimeState> vNew;
    std::vector<std::vector<SidechainWTPrimeState>> vOld;
    for (const Sidechain& s : scdbTest.GetActiveSidechains()) {
        vOld.push_back(scdbTest.GetState(s.nSidechain));
    }
    BOOST_CHECK(vOld.size() == 3);
    BOOST_CHECK(ParseSCDBUpdateScript(script, vOld, vNew));
    BOOST_CHECK(vNew.size() == 1);

    BOOST_CHECK(scdbTest.UpdateSCDBMatchMT(2, scdbTestCopy.GetSCDBHash(), vNew));

    // Now add more WT^(s) to the existing sidechains
    SidechainWTPrimeState wt1b;
    wt1b.hashWTPrime = GetRandHash();
    wt1b.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - 1;
    wt1b.nSidechain = 0; // For first sidechain
    wt1b.nWorkScore = 1;

    SidechainWTPrimeState wt2b;
    wt2b.hashWTPrime = GetRandHash();
    wt2b.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - 1;
    wt2b.nSidechain = 1; // For second sidechain
    wt2b.nWorkScore = 1;

    SidechainWTPrimeState wt3b;
    wt3b.hashWTPrime = GetRandHash();
    wt3b.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - 1;
    wt3b.nSidechain = 2; // For third sidechain
    wt3b.nWorkScore = 1;

    vWT.clear();
    vWT.push_back(wt1b);
    vWT.push_back(wt2b);
    vWT.push_back(wt3b);

    BOOST_CHECK(scdbTest.UpdateSCDBIndex(vWT, 3));
    BOOST_CHECK(scdbTest.GetState(0).size() == 2);
    BOOST_CHECK(scdbTest.GetState(1).size() == 2);
    BOOST_CHECK(scdbTest.GetState(2).size() == 2);

    // Create a copy of the scdbTest to manipulate
    SidechainDB scdbTestCopy2 = scdbTest;

    wt3b.nWorkScore++;

    vWT.clear();
    vWT.push_back(wt3b);

    BOOST_CHECK(scdbTestCopy2.UpdateSCDBIndex(vWT, 4));

    // MT hash prediction should fail here without update script
    BOOST_CHECK(!scdbTest.UpdateSCDBMatchMT(4, scdbTestCopy2.GetSCDBHash()));

    // Create custom votes for WT^s
    SidechainCustomVote vote3;
    vote3.nSidechain = 0;
    vote3.hashWTPrime = wt1a.hashWTPrime;
    vote3.vote = SCDB_DOWNVOTE;

    SidechainCustomVote vote4;
    vote4.nSidechain = 1;
    vote4.hashWTPrime = wt2a.hashWTPrime;
    vote4.vote = SCDB_DOWNVOTE;

    SidechainCustomVote vote5;
    vote5.nSidechain = 2;
    vote5.hashWTPrime = wt3b.hashWTPrime;
    vote5.vote = SCDB_UPVOTE;

    // Generate an update script
    CBlock block2;
    CMutableTransaction mtx2;
    mtx2.vin.resize(1);
    mtx2.vin[0].prevout.SetNull();
    block2.vtx.push_back(MakeTransactionRef(std::move(mtx2)));

    vOldScores.clear();
    for (const Sidechain& s : scdbTest.GetActiveSidechains()) {
        vOldScores.push_back(scdbTest.GetState(s.nSidechain));
    }
    CScript script2;
    GenerateSCDBUpdateScript(block2, script2, vOldScores, std::vector<SidechainCustomVote>{vote3, vote4, vote5}, Params().GetConsensus());

    BOOST_CHECK(script2.IsSCDBUpdate());

    // Use ParseUpdateScript from validation to read it
    vNew.clear();
    vOld.clear();
    for (const Sidechain& s : scdbTest.GetActiveSidechains()) {
        vOld.push_back(scdbTest.GetState(s.nSidechain));
    }
    BOOST_CHECK(vOld.size() == 3);
    BOOST_CHECK(ParseSCDBUpdateScript(script2, vOld, vNew));
    BOOST_CHECK(vNew.size() == 3);

    BOOST_CHECK(scdbTest.UpdateSCDBMatchMT(4, scdbTestCopy2.GetSCDBHash(), vNew));
}

BOOST_AUTO_TEST_CASE(update_helper_max_active)
{
    // Do a test where the maximum number of sidechains are active and we have
    // some custom votes
    SidechainDB scdbTest;

    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 0);

    Sidechain proposal;
    proposal.nVersion = 0;
    proposal.title = "sidechain";
    proposal.description = "test";
    proposal.hashID1 = GetRandHash();
    proposal.hashID2 = uint160S("31d98584f3c570961359c308619f5cf2e9178482");

    // Activate the maximum number of sidechains allowed
    unsigned int nSidechains = 0;
    for (int i = 0; i < SIDECHAIN_ACTIVATION_MAX_ACTIVE; i++) {
        proposal.nSidechain = i;
        proposal.title = "sidechain" + std::to_string(i);

        BOOST_CHECK(ActivateSidechain(scdbTest, proposal, 0, true));

        nSidechains++;

        BOOST_CHECK(scdbTest.GetActiveSidechainCount() == nSidechains);
    }

    // Check that the maximum number have been activated
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == SIDECHAIN_ACTIVATION_MAX_ACTIVE);

    // Add one WT^ to SCDB for each sidechain
    int nBlock = 0;
    std::vector<SidechainWTPrimeState> vWT;
    for (const Sidechain& s : scdbTest.GetActiveSidechains()) {
        // Create WT^
        SidechainWTPrimeState wt;
        wt.hashWTPrime = GetRandHash();
        wt.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - 1;
        wt.nSidechain = s.nSidechain;
        wt.nWorkScore = 1;

        vWT.push_back(wt);
    }
    // Check that all of the WT^s are added
    BOOST_CHECK(scdbTest.UpdateSCDBIndex(vWT, nBlock));
    for (const Sidechain& s : scdbTest.GetActiveSidechains()) {
        BOOST_CHECK(scdbTest.GetState(s.nSidechain).size() == 1);
    }
    nBlock++;

    // Create a copy of SCDB that we can modify the scores of. Use update helper
    // script to make scdbTest match scdbTestCopy
    SidechainDB scdbTestCopy = scdbTest;

    // Get the current scores of all WT^(s) and then create new votes for them
    std::vector<SidechainWTPrimeState> vNewScores;
    std::vector<SidechainCustomVote> vUserVotes;
    int i = 0;
    for (const Sidechain& s : scdbTestCopy.GetActiveSidechains()) {
        // Get the current WT^ score for this sidechain
        std::vector<SidechainWTPrimeState> vOldScores = scdbTestCopy.GetState(s.nSidechain);

        // There should be one score
        BOOST_CHECK(vOldScores.size() == 1);

        SidechainWTPrimeState wt = vOldScores.front();

        // Create custom vote for WT^
        SidechainCustomVote vote;
        vote.nSidechain = s.nSidechain;
        vote.hashWTPrime = wt.hashWTPrime;

        // If i is an even number set downvote otherwise upvote
        if (i % 2 == 0) {
            wt.nWorkScore--;
            vote.vote = SCDB_DOWNVOTE;
        } else {
            wt.nWorkScore++;
            vote.vote = SCDB_UPVOTE;
        }

        vNewScores.push_back(wt);
        vUserVotes.push_back(vote);
        i++;
    }
    BOOST_CHECK(scdbTestCopy.UpdateSCDBIndex(vNewScores, nBlock));

    // MT hash prediction should fail here without update script
    BOOST_CHECK(!scdbTest.UpdateSCDBMatchMT(nBlock, scdbTestCopy.GetSCDBHash()));

    // Generate an update script
    CBlock block;
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    block.vtx.push_back(MakeTransactionRef(std::move(mtx)));

    std::vector<std::vector<SidechainWTPrimeState>> vOldScores;
    for (const Sidechain& s : scdbTestCopy.GetActiveSidechains()) {
        vOldScores.push_back(scdbTestCopy.GetState(s.nSidechain));
    }
    CScript script;
    GenerateSCDBUpdateScript(block, script, vOldScores, vUserVotes, Params().GetConsensus());

    BOOST_CHECK(script.IsSCDBUpdate());

    // Use ParseUpdateScript from validation to read it
    vNewScores.clear();
    vOldScores.clear();
    for (const Sidechain& s : scdbTest.GetActiveSidechains()) {
        vOldScores.push_back(scdbTest.GetState(s.nSidechain));
    }
    BOOST_CHECK(vOldScores.size() == 256);
    BOOST_CHECK(ParseSCDBUpdateScript(script, vOldScores, vNewScores));
    BOOST_CHECK(vNewScores.size() == 256);

    BOOST_CHECK(scdbTest.UpdateSCDBMatchMT(nBlock, scdbTestCopy.GetSCDBHash(), vNewScores));
    BOOST_CHECK(scdbTest.GetSCDBHash() == scdbTestCopy.GetSCDBHash());
}

BOOST_AUTO_TEST_CASE(custom_vote_cache)
{
    // Test the functionality of the custom vote cache

    unsigned int nMaxSidechain = 256;

    // Test that we can add a vote for every possible sidechain number

    std::vector<SidechainCustomVote> vVoteIn;
    for (size_t i = 0; i < nMaxSidechain; i++) {
        SidechainCustomVote vote;
        vote.nSidechain = i;
        vote.hashWTPrime = GetRandHash();
        vote.vote = SCDB_UPVOTE;

        vVoteIn.push_back(vote);
    }
    BOOST_CHECK(scdb.CacheCustomVotes(vVoteIn));

    std::vector<SidechainCustomVote> vVoteOut = scdb.GetCustomVoteCache();
    BOOST_CHECK(vVoteOut.size() == nMaxSidechain);

    // Test that new WT^ votes replace old votes for the same sidechain

    // Add a new vote for each sidechain and check that they have replaced all
    // of the old votes
    vVoteIn.clear();
    for (size_t i = 0; i < nMaxSidechain; i++) {
        SidechainCustomVote vote;
        vote.nSidechain = i;
        vote.hashWTPrime = GetRandHash();
        vote.vote = SCDB_ABSTAIN;

        vVoteIn.push_back(vote);
    }
    BOOST_CHECK(scdb.CacheCustomVotes(vVoteIn));

    vVoteOut.clear();
    vVoteOut = scdb.GetCustomVoteCache();
    BOOST_CHECK(vVoteOut.size() == nMaxSidechain);
    // Check that all of the new votes replaced the old ones (the votes were
    // set to abstain so this is easy to check)
    for (const SidechainCustomVote& v : vVoteOut) {
        BOOST_CHECK(v.vote == SCDB_ABSTAIN);
    }

    // Test that changing vote type updates the current WT^ vote

    // Pass in the same votes as currently in the cache, but with their vote
    // type changed to SCDB_DOWNVOTE and make sure they were all changed
    for (size_t i = 0; i < vVoteOut.size(); i++) {
        vVoteOut[i].vote = SCDB_DOWNVOTE;
        BOOST_CHECK(scdb.CacheCustomVotes(std::vector<SidechainCustomVote> { vVoteOut[i] }));
    }
    vVoteOut.clear();
    vVoteOut = scdb.GetCustomVoteCache();
    BOOST_CHECK(vVoteOut.size() == nMaxSidechain);
    for (const SidechainCustomVote& v : vVoteOut) {
        BOOST_CHECK(v.vote == SCDB_DOWNVOTE);
    }

    scdb.Reset();
    // Check that custom vote cache was cleared
    vVoteOut = scdb.GetCustomVoteCache();
    BOOST_CHECK(vVoteOut.empty());

    // Test adding each vote type and check that it is set correctly
    SidechainCustomVote upvote;
    upvote.nSidechain = 0;
    upvote.hashWTPrime = GetRandHash();
    upvote.vote = SCDB_UPVOTE;

    SidechainCustomVote abstain;
    abstain.nSidechain = 1;
    abstain.hashWTPrime = GetRandHash();
    abstain.vote = SCDB_ABSTAIN;

    SidechainCustomVote downvote;
    downvote.nSidechain = 2;
    downvote.hashWTPrime = GetRandHash();
    downvote.vote = SCDB_DOWNVOTE;

    BOOST_CHECK(scdb.CacheCustomVotes(std::vector<SidechainCustomVote> { upvote, abstain, downvote }));

    vVoteOut = scdb.GetCustomVoteCache();
    BOOST_REQUIRE(vVoteOut.size() == 3);
    BOOST_CHECK(vVoteOut[0] == upvote);
    BOOST_CHECK(vVoteOut[1] == abstain);
    BOOST_CHECK(vVoteOut[2] == downvote);

    scdb.Reset();
    // Check that custom vote cache was cleared
    vVoteOut = scdb.GetCustomVoteCache();
    BOOST_CHECK(vVoteOut.empty());

    // Check that invalid vote types are rejected the current cache size is 3
    SidechainCustomVote invalidVote;
    invalidVote.nSidechain = 2;
    invalidVote.hashWTPrime = GetRandHash();
    invalidVote.vote = 'z';

    BOOST_CHECK(!scdb.CacheCustomVotes(std::vector<SidechainCustomVote>{ invalidVote }));
    vVoteOut = scdb.GetCustomVoteCache();
    BOOST_CHECK(vVoteOut.empty());

    invalidVote.vote = ' ';

    BOOST_CHECK(!scdb.CacheCustomVotes(std::vector<SidechainCustomVote>{ invalidVote }));
    vVoteOut = scdb.GetCustomVoteCache();
    BOOST_CHECK(vVoteOut.empty());

    SidechainCustomVote nullHashWTPrime;
    nullHashWTPrime.nSidechain = 2;
    nullHashWTPrime.hashWTPrime.SetNull();
    nullHashWTPrime.vote = SCDB_DOWNVOTE;

    BOOST_CHECK(!scdb.CacheCustomVotes(std::vector<SidechainCustomVote>{ nullHashWTPrime }));
    vVoteOut = scdb.GetCustomVoteCache();
    BOOST_CHECK(vVoteOut.empty());
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

    // TODO add deposit serialization and check that deposit deserialized from
    // txn matches deposit example.

    // Serialized transaction
    std::string strTx1 = "0200000001021cfe01d1bbc1fdaa99126c0baba3573689fbd5f932a014b08612800b1329c40000000049483045022100a58e545a71f2c9cb03e06c0d8aff1a62f6bc204480db8650eeb0a3908d332aaf022038f9ae490fd3ed1825c1397c9ae41cd3aed7711b4e2d99a3a10e380506539da101ffffffff03807a7723010000001976a91470a3e11a039059d01bbf463af74c79c22a6270fd88ac0000000000000000246a227367596b444665487a745544583171384a4e726d614631435165723179527142507700e1f505000000001976a91458c63096724814c3dcdf088b9bb0dc48e6e1a89c88ac00000000";

    // Deserialize
    CMutableTransaction mtx;
    BOOST_CHECK(DecodeHexTx(mtx, strTx1));

    // TxnToDeposit
    SidechainDeposit deposit;
    BOOST_CHECK(scdbTest.TxnToDeposit(mtx, {}, deposit));
}

BOOST_AUTO_TEST_SUITE_END()
