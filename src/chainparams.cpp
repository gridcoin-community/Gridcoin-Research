// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Copyright (c) 2014-2025 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "chainparams.h"

#include "amount.h"
#include "arith_uint256.h"
#include "consensus/merkle.h"
#include <key.h>
#include "primitives/block.h"
#include "script.h"
#include "tinyformat.h"
#include "util.h"
#include "util/strencodings.h"
#include "util/system.h"

#include <assert.h>

#include <cstring>
#include <limits>
#include <stdexcept>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

// Construct the genesis block for the active network (main/testnet/regtest).
// Moved out of LoadBlockIndex (issue #3125, workstream C5), fulfilling the
// old "uncomment when CBlock is moved from main.h" TODO that sat here --
// CBlock moved to primitives/block.h in #3060. Behavior-preserving: note the
// mainnet/testnet coinbase nTime (1413033777) deliberately differs from the
// testnet block nTime (1406674534), exactly as the original code had it.
CBlock CreateGenesisBlock()
{
    // Genesis block - Genesis2
    // MainNet - Official New Genesis Block:
    ////////////////////////////////////////
    /*
         21:58:24 block.nTime = 1413149999
        10/12/14 21:58:24 block.nNonce = 1572771
        10/12/14 21:58:24 block.GetHash = 00000f762f698b5962aa81e38926c3a3f1f03e0b384850caed34cd9164b7f990
        10/12/14 21:58:24 CBlock(hash=00000f762f698b5962aa81e38926c3a3f1f03e0b384850caed34cd9164b7f990, ver=1,
        hashPrevBlock=0000000000000000000000000000000000000000000000000000000000000000,
        hashMerkleRoot=0bd65ac9501e8079a38b5c6f558a99aea0c1bcff478b8b3023d09451948fe841, nTime=1413149999, nBits=1e0fffff, nNonce=1572771, vtx=1, vchBlockSig=)
        10/12/14 21:58:24   Coinbase(hash=0bd65ac950, nTime=1413149999, ver=1, vin.size=1, vout.size=1, nLockTime=0)
        CTxIn(COutPoint(0000000000, 4294967295), coinbase 00012a4531302f31312f313420416e6472656120526f73736920496e647573747269616c20486561742076696e646963617465642077697468204c454e522076616c69646174696f6e)
        CTxOut(empty)
        vMerkleTree: 0bd65ac950

    */

    const char* pszTimestamp = "10/11/14 Andrea Rossi Industrial Heat vindicated with LENR validation";

    CMutableTransaction txNew;
    //GENESIS TIME
    txNew.nVersion = 1;
    txNew.nTime = Params().IsMockableChain() ? 1296688602u : 1413033777u;
    txNew.vin.resize(1);
    txNew.vin[0].scriptSig = CScript() << 0 << CScriptNum(42) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));

    if (Params().IsMockableChain()) {
        // Regtest premine. Pays 10 UTXOs of 100,000 GRC each (1M total) to
        // the well-known secp256k1 privkey=1 P2PKH address. The matching
        // private key is `01..01` (32 bytes of 0x01... no, scalar value 1)
        // and is imported into the wallet by init.cpp under -regtest. UTXOs
        // are marked immediately mature by GetBlocksToMaturity's regtest
        // shortcut so the staker can use them at height 1.
        //
        // Master plan calls for 10M GRC across 100 UTXOs (final amount/dist
        // needs maintainer signoff — flagged in PR description). Tonight's
        // smaller layout is enough to fund Phase 2B.2 generate tests.
        const std::vector<unsigned char> kRegtestPubKeyHash = {
            0x75, 0x1e, 0x76, 0xe8, 0x19, 0x91, 0x96, 0xd4, 0x54, 0x94,
            0x1c, 0x45, 0xd1, 0xb3, 0xa3, 0x23, 0xf1, 0x43, 0x3b, 0xd6
        };
        const CScript premine_script = CScript()
            << OP_DUP << OP_HASH160
            << kRegtestPubKeyHash
            << OP_EQUALVERIFY << OP_CHECKSIG;
        txNew.vout.resize(10);
        for (size_t i = 0; i < txNew.vout.size(); ++i) {
            txNew.vout[i].nValue = 100000 * COIN;
            txNew.vout[i].scriptPubKey = premine_script;
        }
    } else {
        txNew.vout.resize(1);
        txNew.vout[0].SetEmpty();
    }
    CBlock block;
    block.vtx.push_back(CTransaction(txNew));
    block.hashPrevBlock.SetNull();
    block.hashMerkleRoot = BlockMerkleRoot(block);
    const bool fRegTest = Params().IsMockableChain();
    // Regtest genesis announces a modern block version so the staker
    // walking back to it via GetProofOfStakeReward() sees pindexPrev->nVersion
    // >= 10 and uses GetConstantBlockReward(); with nVersion=1 the call
    // falls into the coin-age formula with our nCoinAge=0 argument and
    // returns a zero subsidy, which fails Claim::WellFormed.
    block.nVersion = fRegTest ? 14 : 1;
    //R&D - Testers Wanted Thread:
    block.nTime    = fRegTest ? 1296688602 : !OnTestnet() ? 1413033777 : 1406674534;
    //Official Launch time:
    block.nBits    = UintToArith256(Params().GetConsensus().powLimit).GetCompact();
    block.nNonce = fRegTest ? 0 : !OnTestnet() ? 130208 : 22436;
    LogPrintf("starting Genesis Check...");
    // If genesis block hash does not match, then generate new genesis hash.
    if (block.GetHash(true) != (fRegTest ? hashGenesisBlockRegTest : !OnTestnet() ? hashGenesisBlock : hashGenesisBlockTestNet))
    {
        LogPrintf("Searching for genesis block...");
        // This will figure out a valid hash and Nonce if you're
        // creating a different genesis block: 00000000000000000000000000000000000000000000000000000000000000000000000000000000000000xFFF
        arith_uint256 hashTarget = arith_uint256().SetCompact(block.nBits);
        arith_uint256 thash;
        while (true)
        {
            thash = UintToArith256(block.GetHash(true));
            if (thash <= hashTarget)
                break;
            if ((block.nNonce & 0xFFF) == 0)
            {
                LogPrintf("nonce %08X: hash = %s (target = %s)", block.nNonce, thash.ToString(), hashTarget.ToString());
            }
            ++block.nNonce;
            if (block.nNonce == 0)
            {
                LogPrintf("NONCE WRAPPED, incrementing time");
                ++block.nTime;
            }
        }
        LogPrintf("block.nTime = %u ", block.nTime);
        LogPrintf("block.nNonce = %u ", block.nNonce);
        LogPrintf("block.GetHash = %s", block.GetHash(true).ToString());
    }


    block.print();

    //// debug print

    //GENESIS3: Official Merkle Root
    if (!fRegTest) {
        uint256 merkle_root = uint256S("0x5109d5782a26e6a5a5eb76c7867f3e8ddae2bff026632c36afec5dc32ed8ce9f");
        assert(block.hashMerkleRoot == merkle_root);
        assert(block.GetHash(true) == (!OnTestnet() ? hashGenesisBlock : hashGenesisBlockTestNet));
    } else {
        // Regtest genesis is deterministic (fixed nVersion/nTime/nNonce and a
        // fixed premine coinbase), so its hash is stable and assertable. Mirror
        // the main/testnet assert so a future premine-layout change can't leave
        // hashGenesisBlockRegTest (chainparams.h) silently stale.
        LogPrintf("regtest genesis hash = %s merkle_root = %s nNonce = %u nTime = %u",
                  block.GetHash(true).ToString(),
                  block.hashMerkleRoot.ToString(),
                  block.nNonce, block.nTime);
        assert(block.GetHash(true) == hashGenesisBlockRegTest);
    }

    return block;
}

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
        consensus.BlockV12Height = 2671700;
        consensus.BlockV13Height = 3989800;
        consensus.BlockV14Height = 3990000;
        consensus.BlockV15Height = std::numeric_limits<int>::max();
        consensus.PendingPoolRetention = 28800; // ~30 days at ~90s spacing (issue #1783)
        consensus.ProtocolVersionGracePeriod = 900 * 7; // ~6.5 days
        consensus.PollV3Height = 2671700;
        consensus.ProjectV2Height = 2671700;
        consensus.PollMultiAddressHeight = std::numeric_limits<int>::max();
        consensus.AutoGreylistAuditHeight = 3989800;
        // TBD: set coincident with BlockV15Height when v15 is scheduled. numeric_limits max keeps
        // the deep-copy overlay fix inactive on mainnet until then (mirrors PollMultiAddressHeight).
        consensus.AutoGreylistDeepCopyHeight = std::numeric_limits<int>::max();
        // TBD: set coincident with BlockV15Height when v15 is scheduled. numeric_limits max keeps the
        // scraper no_records total-credit fix inactive on mainnet until then (mirrors the deep-copy fix).
        consensus.AutoGreylistTotalCreditFixHeight = std::numeric_limits<int>::max();
        consensus.DefaultConstantBlockReward = 10 * COIN;
        consensus.ConstantBlockRewardFloor = 0;
        consensus.ConstantBlockRewardCeiling = 500 * COIN;
        consensus.ProjectV4Height = 3989800;
        consensus.SuperblockV3Height = 3989800;
        // Immediately post zero payment interval fees 40% for mainnet
        consensus.InitialMRCFeeFractionPostZeroInterval = Fraction(2, 5);
        consensus.MRCZeroPaymentInterval = 14 * 24 * 60 * 60;
        consensus.MaxMandatorySideStakeTotalAlloc = Fraction(1, 4);
        consensus.DefaultMagnitudeUnit = Fraction(1, 4);
        consensus.MaxMagnitudeUnit = Fraction(5, 1);
        consensus.MinMagnitudeWeightFactor = Fraction(1, 10);
        consensus.DefaultMagnitudeWeightFactor = Fraction(100, 567);
        consensus.MaxMagnitudeWeightFactor = Fraction(1);
        consensus.StandardContractReplayLookback = 180 * 24 * 60 * 60;
        consensus.powLimit = uint256S("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0x70;
        pchMessageStart[1] = 0x35;
        pchMessageStart[2] = 0x22;
        pchMessageStart[3] = 0x05;
        nDefaultPort = 32749;
        m_assumed_blockchain_size = 4;

     /* genesis = CreateGenesisBlock(1413033777, 130208, 0x1e0fffff, 1);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x000005a247b397eadfefa58e872bc967c2614797bdc8d4d0e6b09fea5c191599"));
        assert(genesis.hashMerkleRoot == uint256S("0x5109d5782a26e6a5a5eb76c7867f3e8ddae2bff026632c36afec5dc32ed8ce9f")); */


        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,62);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,85);
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1,190);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};

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
                {2200000,  uint256S("0x6e834d0f49cc8c2a76452db9cf72961d44d86a80c6d604aad4a720f38673a93e")},
                {2400000,  uint256S("0xf67b595e02e22a02498dfab853e2fabe6e74298a8d83ddc6115c37eaa5808bf6")},
                {2600000,  uint256S("0xc86624a7f4dde5046d9b62aa2e177a46c60845684702ea3fd49ff40f4f2418f6")},
                {2800000,  uint256S("0x09af79c7da8880f7aa56687baf59f35a9d489037f3271938f85b3317d08a8476")},
                {3000000,  uint256S("0xc991e619995b65af5af5fe13bb8d61d60c8bb0f4eb2f1472abc058c7aac28a13")},
                {3200000,  uint256S("0xbf56ac3d117b7e24ca9d2082961a53bb4f77cd43eaa2a036628c97d2e1bc0cd2")},
            }
        };

        // Master and alert keys other than the original are shorter because they are compressed.

        // The original alert key was the same as the "master" (administrative contract) key For
        // before Kermit's Mom (< 5.3.3.12, < 5.4.0.0)
        // TestNet alerts public key for Kermit's Mom and beyond (>= 5.3.3.12, >= 5.4.0.0):
        vAlertPubKey = ParseHex("0352063cf6cf0317cc848ae24f3ed8b525334d2f059f242d27975f8c3a2e91b446");

        masterkeys = {
            {0,       ParseHex("049ac003b3318d9fe28b2830f6a95a2624ce2a69fb0c0c7ac0b513efcc1e93a6a6e8eba84481155dd82f2f1104e0ff62c69d662b0094639b7106abc5d84f948c0a")},
            {2671700, ParseHex("0288b33697c4c752f922764bf1a5075fa96bad46aaf4f0579bf7d19ab048e200f0")}
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
        consensus.BlockV12Height = 1871830;
        consensus.BlockV13Height = 2870000;
        consensus.BlockV14Height = 3126500;
        consensus.BlockV15Height = std::numeric_limits<int>::max();
        consensus.PendingPoolRetention = 28800; // identical to mainnet (~30 days at ~90s); override via -pendingpoolretention for isolated-testnet runs (issue #1783)
        consensus.ProtocolVersionGracePeriod = 900 * 21; // ~19.6 days — extended because v14 fork preceded deployment
        consensus.PollV3Height = 1944820;
        consensus.ProjectV2Height = 1944820;
        consensus.PollMultiAddressHeight = std::numeric_limits<int>::max();
        consensus.AutoGreylistAuditHeight = 3111000;
        // Inactive by default; activated on the isolated mesh / public testnet via the
        // -autogreylistdeepcopyheight override ahead of v15.
        consensus.AutoGreylistDeepCopyHeight = std::numeric_limits<int>::max();
        // Inactive by default; activated on the isolated mesh / public testnet via the
        // -autogreylisttotalcreditfixheight override ahead of v15.
        consensus.AutoGreylistTotalCreditFixHeight = std::numeric_limits<int>::max();
        consensus.DefaultConstantBlockReward = 10 * COIN;
        consensus.ConstantBlockRewardFloor = 0;
        consensus.ConstantBlockRewardCeiling = 500 * COIN;
        consensus.ProjectV4Height = 2870000;
        consensus.SuperblockV3Height = 2870000;
        // Immediately post zero payment interval fees 40% for testnet, the same as mainnet
        consensus.InitialMRCFeeFractionPostZeroInterval = Fraction(2, 5);
        consensus.MRCZeroPaymentInterval = 10 * 60;
        consensus.MaxMandatorySideStakeTotalAlloc = Fraction(1, 4);
        consensus.DefaultMagnitudeUnit = Fraction(1, 4);
        consensus.MaxMagnitudeUnit = Fraction(5, 1);
        consensus.MinMagnitudeWeightFactor = Fraction(1, 10);
        consensus.DefaultMagnitudeWeightFactor = Fraction(100, 567);
        consensus.MaxMagnitudeWeightFactor = Fraction(1);
        consensus.StandardContractReplayLookback = 180 * 24 * 60 * 60;
        consensus.powLimit = uint256S("0000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");

        pchMessageStart[0] = 0xcd;
        pchMessageStart[1] = 0xf2;
        pchMessageStart[2] = 0xc0;
        pchMessageStart[3] = 0xef;
        nDefaultPort = 32748;
        m_assumed_blockchain_size = 2;

     /* genesis = CreateGenesisBlock(1406674534, 22436, 0x1f00ffff, 1);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x00006e037d7b84104208ecf2a8638d23149d712ea810da604ee2f2cb39bae713"));
        assert(genesis.hashMerkleRoot == uint256S("0x5109d5782a26e6a5a5eb76c7867f3e8ddae2bff026632c36afec5dc32ed8ce9f")); */

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        m_is_test_chain = true;
        m_is_mockable_chain = false;

        checkpointData = {
            {
                {0,       uint256S("0x00006e037d7b84104208ecf2a8638d23149d712ea810da604ee2f2cb39bae713")},
                {2400000, uint256S("0x962b7607f8ffceb5c77951d242caed3f94f465f8529d924338700895ff8ed458")},
                {2800000, uint256S("0x038f6a3bdea036e11f6793e5ec0d66434c7889c1fdb340e32136ec0d5bc4cd18")}
            }
        };


        // Master and keys other than the original are shorter because they are compressed.

        // TestNet alerts private key for before Kermit's Mom (< 5.3.3.12, < 5.4.0.0):
        // "308201130201010420b665cff1884e53da26376fd1b433812c9a5a8a4d5221533b15b9629789bb7e42a081a53081a2020101302c06072a8648ce3d0101022100fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f300604010004010704410479be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8022100fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141020101a1440342000471dc165db490094d35cde15b1f5d755fa6ad6f2b5ed0f340e3f17f57389c3c2af113a8cbcc885bde73305a553b5640c83021128008ddf882e856336269080496"
        // TestNet alerts private key for Kermit's Mom and beyond (>= 5.3.3.12, >= 5.4.0.0):
        // "925ekjvCRKuwEzu2WuqifVFE2T3r755rwBNN3ck7Fr8esTdQdrA"
        vAlertPubKey = ParseHex("02bf4aa6330f525ab91a25cd5c1362481d16d8c039b3d27cb48ac0870176202462");

        masterkeys = {
            {0,       ParseHex("049ac003b3318d9fe28b2830f6a95a2624ce2a69fb0c0c7ac0b513efcc1e93a6a6e8eba84481155dd82f2f1104e0ff62c69d662b0094639b7106abc5d84f948c0a")},
            {1964600, ParseHex("031886a6776699cbd6362df7641c5d128146afabc769dfa36f1630889c706ce730")}
        };
    }
};

/**
 * Regression test
 *
 * Local development chain in which blocks can be deterministically generated for
 * functional testing. All version-gate heights are set to 0 so the chain runs at
 * the latest consensus rules from genesis. `powLimit` is the most permissive
 * 256-bit value so the genesis nonce and any subsequent kernel-target checks
 * pass trivially. The `m_is_mockable_chain` flag is the gate consulted by
 * regtest-only code paths (zero stake-age, no scraper threads, no auto-superblock
 * attach). Magic bytes match Bitcoin Core's regtest for ergonomic compatibility
 * with downstream tooling.
 */
class CRegTestParams : public CChainParams {
public:
    CRegTestParams() {
        strNetworkID = CBaseChainParams::REGTEST;
        consensus.ProtocolV2Height = 0;
        consensus.ResearchAgeHeight = 0;
        consensus.BlockV8Height = 0;
        consensus.BlockV9Height = 0;
        consensus.BlockV9TallyHeight = 0;
        consensus.BlockV10Height = 0;
        consensus.BlockV11Height = 0;
        consensus.BlockV12Height = 0;
        consensus.BlockV13Height = 0;
        consensus.BlockV14Height = 0;
        // V15 stays inert on regtest by default (matching CMainParams /
        // CTestNetParams) so the functional-test suite, which mines v14 blocks,
        // is accepted. Without this the member is left at 0 and IsV15Enabled
        // would be true from genesis, rejecting every v14 block with
        // "AcceptBlock: reject too old nVersion = 14". Activate early in an
        // isolated regtest run with -blockv15height=N to exercise POOL
        // contracts (issue #1783).
        consensus.BlockV15Height = std::numeric_limits<int>::max();
        consensus.PendingPoolRetention = 28800; // shorten via -pendingpoolretention for POOL regtest runs (issue #1783)
        consensus.ProtocolVersionGracePeriod = 900 * 7;
        consensus.PollV3Height = 0;
        consensus.ProjectV2Height = 0;
        consensus.PollMultiAddressHeight = 0;
        consensus.AutoGreylistAuditHeight = 0;
        consensus.AutoGreylistDeepCopyHeight = 0;
        consensus.AutoGreylistTotalCreditFixHeight = 0;
        consensus.DefaultConstantBlockReward = 10 * COIN;
        consensus.ConstantBlockRewardFloor = 0;
        consensus.ConstantBlockRewardCeiling = 500 * COIN;
        consensus.ProjectV4Height = 0;
        consensus.SuperblockV3Height = 0;
        consensus.InitialMRCFeeFractionPostZeroInterval = Fraction(2, 5);
        consensus.MRCZeroPaymentInterval = 10 * 60;
        consensus.MaxMandatorySideStakeTotalAlloc = Fraction(1, 4);
        consensus.DefaultMagnitudeUnit = Fraction(1, 4);
        consensus.MaxMagnitudeUnit = Fraction(5, 1);
        consensus.MinMagnitudeWeightFactor = Fraction(1, 10);
        consensus.DefaultMagnitudeWeightFactor = Fraction(100, 567);
        consensus.MaxMagnitudeWeightFactor = Fraction(1);
        consensus.StandardContractReplayLookback = 180 * 24 * 60 * 60;
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");

        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xbf;
        pchMessageStart[2] = 0xb5;
        pchMessageStart[3] = 0xda;
        nDefaultPort = 32747;
        m_assumed_blockchain_size = 1;

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        m_is_test_chain = true;
        m_is_mockable_chain = true;

        checkpointData = { {} };

        vAlertPubKey = ParseHex("02bf4aa6330f525ab91a25cd5c1362481d16d8c039b3d27cb48ac0870176202462");

        masterkeys = {
            {0, ParseHex("049ac003b3318d9fe28b2830f6a95a2624ce2a69fb0c0c7ac0b513efcc1e93a6a6e8eba84481155dd82f2f1104e0ff62c69d662b0094639b7106abc5d84f948c0a")}
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
    else if (chain == CBaseChainParams::REGTEST)
        return std::unique_ptr<CChainParams>(new CRegTestParams());
    throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    globalChainParams = CreateChainParams(network);
}

int GetBlockV15Height()
{
    // Hidden `-blockv15height` arg lets isolated testnet / regtest activate
    // POOL contracts at a low height for end-to-end exercise. Defaults to the
    // chainparams value (std::numeric_limits<int>::max() until pinned).
    return gArgs.GetArg("-blockv15height", Params().GetConsensus().BlockV15Height);
}

int GetPendingPoolRetention()
{
    // Hidden `-pendingpoolretention` arg shortens the PENDING / OPEN
    // expiration window so isolated-testnet / regtest runs can exercise
    // expiration boundaries without waiting ~30 days of mainnet-paced
    // blocks. Consensus-affecting on shared networks: nodes with different
    // values will disagree on POOL_REGISTER admission across expiration
    // boundaries and fork. Defaults to the chainparams value (28800).
    return gArgs.GetArg("-pendingpoolretention", Params().GetConsensus().PendingPoolRetention);
}
