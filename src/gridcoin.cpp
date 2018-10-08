#include "gridcoin.h"

extern bool fTestNet;

namespace
{
    static const std::set<uint256> bad_blocks =
    {};

    static const std::set<uint256> bad_blocks_testnet
    {
        // Invalid claims
        uint256("c1aa0511add3bed3f2e366d38b954285a7602cae10a7244e7fe35e4002e90cd5") // 629408
    };
}

std::set<uint256> GetBadBlocks()
{
    return fTestNet
            ? bad_blocks_testnet
            : bad_blocks;
}
