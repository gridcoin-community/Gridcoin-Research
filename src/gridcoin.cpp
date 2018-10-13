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
        uint256("129ae6779d620ec189f8e5148e205efca2dfe31d9f88004b918da3342157b7ff"), //T407024
        uint256("c3f85818ba5290aaea1bcbd25b4e136f83acc93999942942bbb25aee2c655f7a"), //T407068
        uint256("cf7f1316f92547f611852cf738fc7a4a643a2bb5b9290a33cd2f9425f44cc3f9"), //T407099
        uint256("b47085beb075672c6f20d059633d0cad4dba9c5c20f1853d35455b75dc5d54a9"), //T407117
        uint256("5a7d437d15bccc41ee8e39143e77960781f3dcf08697a888fa8c4af8a4965682"), //T407161
        uint256("aeb3c24277ae1047bda548975a515e9d353d6e12a2952fb733da03f92438fb0f"), //T407181
        uint256("fc584b18239f3e3ea78afbbd33af7c6a29bb518b8299f01c1ed4b52d19413d4f"), //T407214
        uint256("d5441f7c35eb9ea1b786bbbed820b7f327504301ae70ef2ac3ca3cbc7106236b"), //T479114
        uint256("18e2ed822c4c5686c0189f60ffbe828fa51e90873cc1ed3e534a257d2f09c360"), //T479282
        uint256("7cb49bc17ecdb7b3a56e1290496d4866330fc9c7974c65b2034e1c6b951ea0fe"), //T511839
        uint256("c1aa0511add3bed3f2e366d38b954285a7602cae10a7244e7fe35e4002e90cd5"), //T629408
        uint256("639756cf39bf12a4a0ab4ea5ec10938fd0f463cc7bc1bd2916529a445ceba2ab"), //T680406
    };
}

std::set<uint256> GetBadBlocks()
{
    return fTestNet
            ? bad_blocks_testnet
            : bad_blocks;
}
