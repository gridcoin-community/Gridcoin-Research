#include "gridcoin.h"

extern bool fTestNet;

namespace
{
    static const std::set<uint256> bad_blocks =
    {
        uint256("58b2d6d0ff7e3ebcaca1058be7574a87efadd4b7f5c661f9e14255f851a6185e"), //P1144550 S
        uint256("471292b59e5f3ad94c39b3784a9a3f7a8324b9b56ff0ad00bd48c31658537c30"), //P1146939 S
        uint256("5b63d4edbdec06ddc2182703ce45a3ced70db0d813e329070e83bf37347a6c2c"), //P1152917 S
        uint256("e9035d821668a0563b632e9c84bc5af73f53eafcca1e053ac6da53907c7f6940"), //P1154121 S
        uint256("1d30c6d4dce377d69c037f1a725aabbc6bafa72a95456dbe2b2538bc1da115bd"), //P1168122 S
        uint256("934c6291209d90bb5d3987885b413c18e39f0e28430e8d302f20888d2a35e725"), //P1168193 S
        uint256("58282559939ced7ebed7d390559c7ac821932958f8f2399ad40d1188eb0a57f9"), //P1170167 S
        uint256("946996f693a33fa1334c1f068574238a463d438b1a3d2cd6d1dd51404a99c73d") //P1176436 S
    };

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
