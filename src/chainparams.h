// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "chainparamsbase.h"
#include "consensus/params.h"
#include "protocol.h"

#include <memory>
#include <vector>


typedef std::map<int, uint256> MapCheckpoints;

struct CCheckpointData {
    MapCheckpoints mapCheckpoints;
};

/**
 * CChainParams defines various tweakable parameters of a given instance of the
 * Gridcoin system. There are two: the main network on which people trade goods
 * and services and the public test network
 */
class CChainParams
{
public:
    enum Base58Type {
        PUBKEY_ADDRESS,
        SCRIPT_ADDRESS,

        MAX_BASE58_TYPES
    };

    const Consensus::Params& GetConsensus() const { return consensus; }
    const CMessageHeader::MessageStartChars& MessageStart() const { return pchMessageStart; }
    const std::vector<unsigned char>& AlertKey() const { return vAlertPubKey; }
    int GetDefaultPort() const { return nDefaultPort; }

    // const CBlock& GenesisBlock() const { return genesis; }
    /** If this chain is exclusively used for testing */
    bool IsTestChain() const { return m_is_test_chain; }
    /** If this chain allows time to be mocked */
    bool IsMockableChain() const { return m_is_mockable_chain; }
    /** Minimum free space (in GB) needed for data directory */
    uint64_t AssumedBlockchainSize() const { return m_assumed_blockchain_size; }
    /** Return the network string */
    std::string NetworkIDString() const { return strNetworkID; }
    const unsigned char& Base58Prefix(Base58Type type) const { return base58Prefix[type]; }
    const CCheckpointData& Checkpoints() const { return checkpointData; }
protected:
    CChainParams() {}

    Consensus::Params consensus;
    CMessageHeader::MessageStartChars pchMessageStart;
    std::vector<unsigned char> vAlertPubKey;
    int nDefaultPort;
    uint64_t m_assumed_blockchain_size;
    unsigned char base58Prefix[MAX_BASE58_TYPES];
    std::string strNetworkID;
    // CBlock genesis;
    bool m_is_test_chain;
    bool m_is_mockable_chain;
    CCheckpointData checkpointData;
};

/**
 * Creates and returns a std::unique_ptr<CChainParams> of the chosen chain.
 * @returns a CChainParams* of the chosen chain.
 * @throws a std::runtime_error if the chain is not supported.
 */
std::unique_ptr<const CChainParams> CreateChainParams(const std::string& chain);

/**
 * Return the currently selected parameters. This won't change after app
 * startup, except for unit tests.
 */
const CChainParams &Params();

/**
 * Sets the params returned by Params() to those for the given chain name.
 * @throws std::runtime_error when the chain is not supported.
 */
void SelectParams(const std::string& chain);

inline bool IsProtocolV2(int nHeight)
{
    return nHeight > Params().GetConsensus().ProtocolV2Height;
}

inline bool IsResearchAgeEnabled(int nHeight)
{
    return nHeight >= Params().GetConsensus().ResearchAgeHeight;
}

inline bool IsV8Enabled(int nHeight)
{
    // Start creating V8 blocks after these heights.
    // In testnet the first V8 block was created on block height 320000.
    return nHeight > Params().GetConsensus().BlockV8Height;
}

inline bool IsV9Enabled(int nHeight)
{
    return nHeight >= Params().GetConsensus().BlockV9Height;
}

inline bool IsV9Enabled_Tally(int nHeight)
{
    return nHeight >= Params().GetConsensus().BlockV9TallyHeight;
}

inline bool IsV10Enabled(int nHeight)
{
    // Testnet used a controlled switch by injecting a v10 block
    // using a modified client and different miner trigger rules,
    // hence the odd height.
    return nHeight >= Params().GetConsensus().BlockV10Height;
}

inline bool IsV11Enabled(int nHeight)
{
    return nHeight >= Params().GetConsensus().BlockV11Height;
}

inline int GetSuperblockAgeSpacing(int nHeight)
{
    return (fTestNet ? 86400 : (nHeight > 364500) ? 86400 : 43200);
}

inline int GetOrigNewbieSnapshotFixHeight()
{
    // This is the original hard fork point for the newbie accrual fix that didn't work.
    return fTestNet ? 1393000 : 2104000;
}

inline int GetNewbieSnapshotFixHeight()
{
    return fTestNet ? 1480000 : 2197000;
}
