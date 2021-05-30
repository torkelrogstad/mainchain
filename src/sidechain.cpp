// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <sidechain.h>

#include <clientversion.h>
#include <core_io.h>
#include <hash.h>
#include <script/script.h>
#include <streams.h>
#include <utilstrencodings.h>

#include <sstream>

bool Sidechain::operator==(const Sidechain& s) const
{
    return (strPrivKey == s.strPrivKey &&
            scriptPubKey == s.scriptPubKey &&
            strKeyID == s.strKeyID &&
            title == s.title &&
            description == s.description &&
            hashID1 == s.hashID1 &&
            hashID2 == s.hashID2 &&
            nVersion == s.nVersion &&
            nSidechain == s.nSidechain);
}

std::string Sidechain::GetSidechainName() const
{
    return title;
}

std::string Sidechain::ToString() const
{
    std::stringstream ss;
    ss << "fActive=" << fActive << std::endl;
    ss << "nSidechain=" << (unsigned int)nSidechain << std::endl;
    ss << "nVersion=" << nVersion << std::endl;
    ss << "strPrivKey=" << strPrivKey << std::endl;
    ss << "scriptPubKey=" << ScriptToAsmStr(scriptPubKey) << std::endl;
    ss << "strKeyID=" << strKeyID << std::endl;
    ss << "title=" << title << std::endl;
    ss << "description=" << description << std::endl;
    ss << "hashID1=" << hashID1.ToString() << std::endl;
    ss << "hashID2=" << hashID2.ToString() << std::endl;
    return ss.str();
}

bool SidechainDeposit::operator==(const SidechainDeposit& a) const
{
    return (a.nSidechain == nSidechain &&
            a.strDest == strDest &&
            a.tx == tx &&
            a.n == n &&
            a.hashBlock == hashBlock);
}

std::string SidechainDeposit::ToString() const
{
    std::stringstream ss;
    ss << "nsidechain=" << (unsigned int)nSidechain << std::endl;
    ss << "strDest=" << strDest << std::endl;
    ss << "txid=" << tx.GetHash().ToString() << std::endl;
    ss << "n=" << n << std::endl;
    ss << "hashblock=" << hashBlock.ToString() << std::endl;
    return ss.str();
}

std::string SidechainCTIP::ToString() const
{
    std::stringstream ss;
    ss << "outpoint=" << out.ToString() << std::endl;
    ss << "amount=" << amount << std::endl;
    return ss.str();
}

bool SidechainWTPrimeState::IsNull() const
{
    return (hashWTPrime.IsNull());
}

bool SidechainWTPrimeState::operator==(const SidechainWTPrimeState& a) const
{
    return (a.nSidechain == nSidechain &&
            a.hashWTPrime == hashWTPrime);
}

std::string SidechainWTPrimeState::ToString() const
{
    std::stringstream ss;
    ss << "hash=" << GetHash().ToString() << std::endl;
    ss << "nsidechain=" << (unsigned int)nSidechain << std::endl;
    ss << "nBlocksLeft=" << (unsigned int)nBlocksLeft << std::endl;
    ss << "nWorkScore=" << (unsigned int)nWorkScore << std::endl;
    ss << "hashWTPrime=" << hashWTPrime.ToString() << std::endl;
    return ss.str();
}

bool Sidechain::DeserializeFromProposalScript(const CScript& script)
{
    if (!script.IsSidechainProposalCommit())
        return false;

    CScript::const_iterator pc = script.begin() + 5;
    std::vector<unsigned char> vch;

    opcodetype opcode;
    if (!script.GetOp(pc, opcode, vch))
        return false;
    if (vch.empty())
        return false;

    const char *vch0 = (const char *) &vch.begin()[0];
    CDataStream ds(vch0, vch0+vch.size(), SER_DISK, CLIENT_VERSION);

    Sidechain sidechain;
    sidechain.DeserializeProposal(ds);

    fActive = false;
    nSidechain = sidechain.nSidechain;
    nVersion = sidechain.nVersion;
    title = sidechain.title;
    description = sidechain.description;
    strKeyID = sidechain.strKeyID;
    scriptPubKey = sidechain.scriptPubKey;
    strPrivKey = sidechain.strPrivKey;
    hashID1 = sidechain.hashID1;
    hashID2 = sidechain.hashID2;

    return true;
}

uint256 SidechainActivationStatus::GetHash() const
{
    return SerializeHash(*this);
}

uint256 SidechainDeposit::GetHash() const
{
    return SerializeHash(*this);
}

uint256 Sidechain::GetHash() const
{
    return SerializeHash(*this);
}

uint256 SidechainWTPrimeState::GetHash() const
{
    return SerializeHash(*this);
}

uint256 SidechainCTIP::GetHash() const
{
    return SerializeHash(*this);
}

CScript Sidechain::GetProposalScript() const
{
    CDataStream ds(SER_DISK, CLIENT_VERSION);
    ((Sidechain *) this)->SerializeProposal(ds);
    std::vector<unsigned char> vch(ds.begin(), ds.end());

    CScript script;
    script.resize(5);
    script[0] = OP_RETURN;
    script[1] = 0xD5;
    script[2] = 0xE0;
    script[3] = 0xC4;
    script[4] = 0xAF;
    script << vch;

    return script;
}

uint256 SidechainObj::GetHash(void) const
{
    uint256 ret;
    if (sidechainop == DB_SIDECHAIN_BLOCK_OP)
        ret = SerializeHash(*(SidechainBlockData *) this);

    return ret;
}

CScript SidechainObj::GetScript(void) const
{
    CDataStream ds (SER_DISK, CLIENT_VERSION);
    if (sidechainop == DB_SIDECHAIN_BLOCK_OP)
        ((SidechainBlockData *) this)->Serialize(ds);

    CScript script;
    script << std::vector<unsigned char>(ds.begin(), ds.end()) << OP_SIDECHAIN;
    return script;
}

std::string SidechainObj::ToString(void) const
{
    std::stringstream str;
    str << "sidechainop=" << sidechainop << std::endl;
    return str.str();
}

std::string SidechainBlockData::ToString() const
{
    std::stringstream str;
    str << "sidechainop=" << sidechainop << std::endl;
    return str.str();
}

bool ParseDepositAddress(const std::string& strAddressIn, std::string& strAddressOut, unsigned int& nSidechainOut)
{
    if (strAddressIn.empty())
        return false;

    // First character should be 's'
    if (strAddressIn.front() != 's')
        return false;

    unsigned int delim1 = strAddressIn.find_first_of("_") + 1;
    unsigned int delim2 = strAddressIn.find_last_of("_");

    if (delim1 == std::string::npos || delim2 == std::string::npos)
        return false;
    if (delim1 >= strAddressIn.size() || delim2 + 1 >= strAddressIn.size())
        return false;

    std::string strSidechain = strAddressIn.substr(1, delim1);
    if (strSidechain.empty())
        return false;

    // Get sidechain number
    try {
        nSidechainOut = std::stoul(strSidechain);
    } catch (...) {
        return false;
    }

    // Check sidechain number is within range
    if (nSidechainOut > 255)
        return false;

    // Get substring without prefix or suffix
    strAddressOut = "";
    strAddressOut = strAddressIn.substr(delim1, delim2 - delim1);
    if (strAddressOut.empty())
        return false;

    // Get substring without checksum (for generating our checksum)
    std::string strNoCheck = strAddressIn.substr(0, delim2 + 1);
    if (strNoCheck.empty())
        return false;

    // Generate our own checksum of the address - checksum
    std::vector<unsigned char> vch;
    vch.resize(CSHA256::OUTPUT_SIZE);
    CSHA256().Write((unsigned char*)&strNoCheck[0], strNoCheck.size()).Finalize(&vch[0]);
    std::string strHash = HexStr(vch.begin(), vch.end());

    if (strHash.size() != 64)
        return false;

    // Get checksum from address string
    std::string strCheck = strAddressIn.substr(delim2 + 1, strAddressIn.size());
    if (strCheck.size() != 6)
        return false;

    // Compare address checksum with our checksum
    if (strCheck != strHash.substr(0, 6))
        return false;

    return true;
}
