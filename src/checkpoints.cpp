// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/assign/list_of.hpp> // for 'map_list_of()'
#include <boost/range/adaptor/reversed.hpp>

#include "checkpoints.h"

#include "txdb.h"
#include "main.h"
#include "uint256.h"


static const int nCheckpointSpan = 10;

namespace Checkpoints
{
    typedef std::map<int, uint256> MapCheckpoints;

    //
    // What makes a good checkpoint block?
    // + Is surrounded by blocks with reasonable timestamps
    //   (no blocks before with a timestamp after, none after with
    //    timestamp before)
    // + Contains no strange transactions
    //
    static MapCheckpoints mapCheckpoints =
        boost::assign::map_list_of
        ( 0,       hashGenesisBlock )
        ( 40,      uint256("0x0000002c305541bceb763e6f7fce2f111cb752acf9777e64c6f02fab5ef4778c") )
        ( 50,      uint256("0x000000415ff618b8e72eda69e87dc2f2ff8798a5032babbef36b089da0ae2278") )
        ( 6000,    uint256("0x5976ff9d0da7626badf301a9e038ec05d776e5e50839e2505357512945d53b04") )
        ( 17000,   uint256("0x92fe9bafd6c9c1acbe8565ade79460505a70180ac5c3b360489037ef7a4aed42") )
        ( 27000,   uint256("0x1521cd45d0564cb016e816581dd6e2d030f6333a1dac5b79bea71ca8b0186e8d") )
        ( 36500,   uint256("0xcf26a63e66ca95bc7c0189a5239128fd983ef978088f187bd30817aebb2c8424") )
        ( 67000,   uint256("0x429a4ed792c6270a263fa679946ff2c510e55e9a3b7234fa789d66bacd3068a0") )
        ( 70000,   uint256("0x829c215851d7cdf756e7ba9e2c8eeef61e503b15488ffa4becab77c7961d30c5") )
        ( 71000,   uint256("0x708c3319912b19c3547fd9a9a2aa6426c3a4543e84f972b3070a24200bd4fcd3") )
        ( 85000,   uint256("0x928f0669b1f036561a2d53b7588b10c5ea2fcb9e2960a9be593b508a7fcceec1") )
        ( 91000,   uint256("0x8ed361fa50f6c16fbda4f2c7454dd818f2278151987324825bc1ec1eacfa9be2") )
        (101000,   uint256("0x578efd18f7cd5191be3463a2b0db985375f48ee6e8a8940cc6b91d6847fa3614") )
        (118000,   uint256("0x8f8ea6eaeae707ab935b517f1c334e6324df75ad8e5f9fbc4d9fb3cc7aa2e69f") )
        (120000,   uint256("0xe64d19e39d564cc66e931e88c03207c19e4c9a873ca68ccef16ad712830da726") )
        (122000,   uint256("0xb35d4f385bba3c3cb3f9b6914edd6621bde8f78c8c42f58ec47131ed6aac82a9") )
        (124392,   uint256("0x1496cd55d7adad1ada774542165a04102a91f8f80c6e894c05f1d0c2ff7e5a39") )
        (145000,   uint256("0x99f5d7166ad55d6d0e1ac5c7fffaee1d1dd1ff1409738e0d4f13ac1ae38234cc") )
        (278000,   uint256("0x8066e63198c44b9840f664e795b0315d9b752773b267d6212f35593bc0e3b6f4") )
        (361800,   uint256("0x801981d8a8f5809e34a2881ea97600259e1d9d778fa21752a5f6cff4defcd08d") )
        (500000,   uint256("0x3916b53eaa0eb392ce5d7e4eaf7db4f745187743f167539ffa4dc1a30c06acbd") )
        (700000,   uint256("0x2e45c8a834675b505b96792d198c2dc970f560429c635479c347204044acc59b") )
        (770000,   uint256("0xfc13a63162bc0a5a09acc3f284cf959d6812a027bb54b342a0e1ccaaca8627ce") )
        (850000,   uint256("0xc78b15f25ad990d02256907fab92ab37301d129eaea177fd04acacd56c0cbd22") )
        (950000,   uint256("0x4be0afdb9273d232de0bc75b572d8bcfaa146d9efdbe4a4f1ab775334f175b0f") )
    ;

    // TestNet has no checkpoints
    static MapCheckpoints mapCheckpointsTestnet =
            boost::assign::map_list_of
            ( 0, hashGenesisBlockTestNet );

    bool CheckHardened(int nHeight, const uint256& hash)
    {
        MapCheckpoints& checkpoints = (fTestNet ? mapCheckpointsTestnet : mapCheckpoints);

        MapCheckpoints::const_iterator i = checkpoints.find(nHeight);
        if (i == checkpoints.end()) return true;
        return hash == i->second;
    }

    int GetTotalBlocksEstimate()
    {
        MapCheckpoints& checkpoints = (fTestNet ? mapCheckpointsTestnet : mapCheckpoints);

        if (checkpoints.empty())
            return 0;
        return checkpoints.rbegin()->first;
    }

    CBlockIndex* GetLastCheckpoint(const std::map<uint256, CBlockIndex*>& mapBlockIndex)
    {
        MapCheckpoints& checkpoints = (fTestNet ? mapCheckpointsTestnet : mapCheckpoints);

        for (auto const& i : boost::adaptors::reverse(checkpoints))
        {
            const uint256& hash = i.second;
            std::map<uint256, CBlockIndex*>::const_iterator t = mapBlockIndex.find(hash);
            if (t != mapBlockIndex.end())
                return t->second;
        }
        return NULL;
    }

    // Check against synchronized checkpoint
    bool CheckSync(int nHeight)
    {
        const CBlockIndex* pindexSync = GetLastCheckpoint(mapBlockIndex);

        if (pindexSync != NULL && nHeight <= pindexSync->nHeight)
            return false;
        return true;
    }
}
