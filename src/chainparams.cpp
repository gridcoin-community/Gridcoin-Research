// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"

#include "tinyformat.h"
#include "util/strencodings.h"

#include <assert.h>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

/* TODO: Uncomment this when CBlock is moved from main.h
static CBlock CreateGenesisBlock(const char* pszTimestamp, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.nTime = nTime;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 0 << CBigNum(42) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].SetEmpty();

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(txNew);
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = genesis.BuildMerkleTree();
    return genesis;
} */

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 *
 * CBlock(hash=00000f762f698b5962aa81e38926c3a3f1f03e0b384850caed34cd9164b7f990, ver=1,
 *        hashPrevBlock=0000000000000000000000000000000000000000000000000000000000000000,
 *        hashMerkleRoot=0bd65ac9501e8079a38b5c6f558a99aea0c1bcff478b8b3023d09451948fe841, nTime=1413149999, nBits=1e0fffff, nNonce=1572771, vtx=1, vchBlockSig=)
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion)
{
    const char* pszTimestamp = "10/11/14 Andrea Rossi Industrial Heat vindicated with LENR validation";
    return CreateGenesisBlock(pszTimestamp, nTime, nNonce, nBits, nVersion);
}  */

/**
 * Main network
 */
class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = CBaseChainParams::MAIN;
        consensus.ProtocolV2Height = 85400;
        consensus.ResearchAgeHeight = 364501;
        consensus.BlockV8Height = 1010000;
        consensus.BlockV9Height = 1144000;
        consensus.BlockV9TallyHeight = 1144120;
        consensus.BlockV10Height = 1420000;
        consensus.BlockV11Height = 2053000;
        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0x70;
        pchMessageStart[1] = 0x35;
        pchMessageStart[2] = 0x22;
        pchMessageStart[3] = 0x05;
        vAlertPubKey = ParseHex("049ac003b3318d9fe28b2830f6a95a2624ce2a69fb0c0c7ac0b513efcc1e93a6a6e8eba84481155dd82f2f1104e0ff62c69d662b0094639b7106abc5d84f948c0a");
        nDefaultPort = 32749;
        m_assumed_blockchain_size = 4;

     /* genesis = CreateGenesisBlock(1413033777, 130208, 0x1e0fffff, 1);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x000005a247b397eadfefa58e872bc967c2614797bdc8d4d0e6b09fea5c191599"));
        assert(genesis.hashMerkleRoot == uint256S("0x5109d5782a26e6a5a5eb76c7867f3e8ddae2bff026632c36afec5dc32ed8ce9f")); */


        base58Prefix[PUBKEY_ADDRESS] = 62;
        base58Prefix[SCRIPT_ADDRESS] = 85;

        m_is_test_chain = false;
        m_is_mockable_chain = false;

        checkpointData = {
            {
                { 40,      uint256S("0x0000002c305541bceb763e6f7fce2f111cb752acf9777e64c6f02fab5ef4778c")},
                { 50,      uint256S("0x000000415ff618b8e72eda69e87dc2f2ff8798a5032babbef36b089da0ae2278")},
                { 6000,    uint256S("0x5976ff9d0da7626badf301a9e038ec05d776e5e50839e2505357512945d53b04")},
                { 17000,   uint256S("0x92fe9bafd6c9c1acbe8565ade79460505a70180ac5c3b360489037ef7a4aed42")},
                { 27000,   uint256S("0x1521cd45d0564cb016e816581dd6e2d030f6333a1dac5b79bea71ca8b0186e8d")},
                { 36500,   uint256S("0xcf26a63e66ca95bc7c0189a5239128fd983ef978088f187bd30817aebb2c8424")},
                { 67000,   uint256S("0x429a4ed792c6270a263fa679946ff2c510e55e9a3b7234fa789d66bacd3068a0")},
                { 70000,   uint256S("0x829c215851d7cdf756e7ba9e2c8eeef61e503b15488ffa4becab77c7961d30c5")},
                { 71000,   uint256S("0x708c3319912b19c3547fd9a9a2aa6426c3a4543e84f972b3070a24200bd4fcd3")},
                { 85000,   uint256S("0x928f0669b1f036561a2d53b7588b10c5ea2fcb9e2960a9be593b508a7fcceec1")},
                { 91000,   uint256S("0x8ed361fa50f6c16fbda4f2c7454dd818f2278151987324825bc1ec1eacfa9be2")},
                {101000,   uint256S("0x578efd18f7cd5191be3463a2b0db985375f48ee6e8a8940cc6b91d6847fa3614")},
                {118000,   uint256S("0x8f8ea6eaeae707ab935b517f1c334e6324df75ad8e5f9fbc4d9fb3cc7aa2e69f")},
                {120000,   uint256S("0xe64d19e39d564cc66e931e88c03207c19e4c9a873ca68ccef16ad712830da726")},
                {122000,   uint256S("0xb35d4f385bba3c3cb3f9b6914edd6621bde8f78c8c42f58ec47131ed6aac82a9")},
                {124392,   uint256S("0x1496cd55d7adad1ada774542165a04102a91f8f80c6e894c05f1d0c2ff7e5a39")},
                {145000,   uint256S("0x99f5d7166ad55d6d0e1ac5c7fffaee1d1dd1ff1409738e0d4f13ac1ae38234cc")},
                {278000,   uint256S("0x8066e63198c44b9840f664e795b0315d9b752773b267d6212f35593bc0e3b6f4")},
                {361800,   uint256S("0x801981d8a8f5809e34a2881ea97600259e1d9d778fa21752a5f6cff4defcd08d")},
                {500000,   uint256S("0x3916b53eaa0eb392ce5d7e4eaf7db4f745187743f167539ffa4dc1a30c06acbd")},
                {700000,   uint256S("0x2e45c8a834675b505b96792d198c2dc970f560429c635479c347204044acc59b")},
                {770000,   uint256S("0xfc13a63162bc0a5a09acc3f284cf959d6812a027bb54b342a0e1ccaaca8627ce")},
                {850000,   uint256S("0xc78b15f25ad990d02256907fab92ab37301d129eaea177fd04acacd56c0cbd22")},
                {950000,   uint256S("0x4be0afdb9273d232de0bc75b572d8bcfaa146d9efdbe4a4f1ab775334f175b0f")},
                {1050000,  uint256S("0x0753b624cc0ab39d8745b436012ce53c087f7b2e077099e746a9557f569a80f3")},
                {1150000,  uint256S("0x0264545b51389faea32ac54bf76cd6efb65701d777e7fa1007584114897067f5")},
                {1250000,  uint256S("0x452467f2f74580176375f99dd38e9119d564985ba639fa1303718a51351823ab")},
                {1350000,  uint256S("0x813725a075bc3cc254742557ce6d3a680cb97ee863f65c5d9a386c1ac9a8e792")},
                {1450000,  uint256S("0x5744777ad775063a3e0b9b9c40ac205e8948904e340d11c3c449fb13914c962b")},
                {1550000,  uint256S("0xa37ab3260678f6e7f009b2c11fee14bef6add481b411516201cc35b397859bfb")},
                {1700000,  uint256S("0x831a655dd58599fda1815f7275194ff69ca53341694ba81f9941eede25c40885")},
                {1800000,  uint256S("0x61bb76ed90de21016de81855d3dc01bd192d17d90de4bdf62e8203c2dde675d7")},
                {1900000,  uint256S("0x352ca52f9a22fbf1d241082d3bec716ea5bef6b82811f737ae6486bd7771e1c7")},
                {2000000,  uint256S("0x2e1252a6ed6d0e7e556d4d0377b10f4b542ae5d6c9822cb08d68490a2a0bb706")},
                {2054000,  uint256S("0xfa1342b4076ca65be64abd7f9cea50cbbdb6247a6937f1f02d6e76494aab20bf")},
            }
        };
    }
};

/**
 * Testnet
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        strNetworkID = CBaseChainParams::TESTNET;
        consensus.ProtocolV2Height = 2060;
        consensus.ResearchAgeHeight = 36501;
        consensus.BlockV8Height = 311999;
        consensus.BlockV9Height = 399000;
        consensus.BlockV9TallyHeight = 399120;
        consensus.BlockV10Height = 629409;
        consensus.BlockV11Height = 1301500;

        pchMessageStart[0] = 0xcd;
        pchMessageStart[1] = 0xf2;
        pchMessageStart[2] = 0xc0;
        pchMessageStart[3] = 0xef;
        vAlertPubKey = ParseHex("0471dc165db490094d35cde15b1f5d755fa6ad6f2b5ed0f340e3f17f57389c3c2af113a8cbcc885bde73305a553b5640c83021128008ddf882e856336269080496");
        // TestNet alerts private key
        // "308201130201010420b665cff1884e53da26376fd1b433812c9a5a8a4d5221533b15b9629789bb7e42a081a53081a2020101302c06072a8648ce3d0101022100fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f300604010004010704410479be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8022100fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141020101a1440342000471dc165db490094d35cde15b1f5d755fa6ad6f2b5ed0f340e3f17f57389c3c2af113a8cbcc885bde73305a553b5640c83021128008ddf882e856336269080496"
        nDefaultPort = 32748;
        m_assumed_blockchain_size = 2;

     /* genesis = CreateGenesisBlock(1406674534, 22436, 0x1f00ffff, 1);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x00006e037d7b84104208ecf2a8638d23149d712ea810da604ee2f2cb39bae713"));
        assert(genesis.hashMerkleRoot == uint256S("0x5109d5782a26e6a5a5eb76c7867f3e8ddae2bff026632c36afec5dc32ed8ce9f")); */

        base58Prefix[PUBKEY_ADDRESS] = 111;
        base58Prefix[SCRIPT_ADDRESS] = 196;

        m_is_test_chain = true;
        m_is_mockable_chain = false;

        checkpointData = {
            {
            }
        };
    }
};

static std::unique_ptr<const CChainParams> globalChainParams;

const CChainParams &Params() {
    assert(globalChainParams);
    return *globalChainParams;
}

std::unique_ptr<const CChainParams> CreateChainParams(const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN)
        return std::unique_ptr<CChainParams>(new CMainParams());
    else if (chain == CBaseChainParams::TESTNET)
        return std::unique_ptr<CChainParams>(new CTestNetParams());
    throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    globalChainParams = CreateChainParams(network);
}
