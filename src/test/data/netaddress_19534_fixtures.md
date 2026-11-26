# Legacy networking fixtures

Captured from upstream gridcoin-community/Gridcoin-Research development at 7b1138525244bcb51759d2f270cb029ad7af9936 (v5.5.1.8, the last development commit before 0946e5a79 bumped the version), with no source changes. This predates #19628. Both files reproduce byte for byte from that commit.

Both files include mainnet network magic and the original checksum. Tests load them using CAddrDB/CBanDB, then compare the rewritten files byte for byte.

Peers: deterministic AddrMan; source SetInternal("baseline-seed"); addresses 250.7.1.1, 2001:470:abcd::1, and 5wyqrzbvrdsumnok.onion, all port 32749 and NODE_NETWORK. nTime=1700000000; first address marked Good at 1700000001.

Bans: 1.2.3.4/24, 2001:470:abcd::1/36, legacy non-contiguous 1.2.3.4/255.255.232.0, and legacy single-host 5wyqrzbvrdsumnok.onion. All manually added at 1700000000, until 1700003600. The non-contiguous mask pins loading old entries that new mask constructors intentionally reject; the Tor entry pins Gridcoin single-host ban compatibility.

netaddress_19534_peers.bin: 4372 bytes; SHA256 b152eba5fbfb8d913144af9534a0ac1784ce416a5b4dbafcf6b44941ac4aae81

netaddress_19534_banlist.bin: 253 bytes; SHA256 15ba8935b698860fe81a6ba041ea442a74feeaee0b7c2c0e1498b2a01cfeb663

## Reproducing

1. Check out 7b1138525244bcb51759d2f270cb029ad7af9936 in a separate worktree.
2. Add the test below as `src/test/capture_19534_tests.cpp` and list it in `add_executable(test_gridcoin ...)` in `src/test/CMakeLists.txt`. Change nothing else.
3. Configure and build: `cmake -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_TESTS=ON -DENABLE_GUI=OFF && cmake --build build --target test_gridcoin`.
4. Run: `mkdir -p out && CAPTURE_DIR=$PWD/out build/src/test/test_gridcoin --run_test=capture_19534_tests`.
5. Compare `out/*.bin` with the files here using `cmp` or the SHA256 sums above.

Capturing from a later build gives the same banlist. In peers.dat, only the client-version field of each address record changes (byte offsets 51, 113 and 175), plus the 32-byte checksum at the end of the file.

```cpp
#include "addrdb.h"
#include "addrman.h"
#include "netbase.h"
#include "protocol.h"
#include "util.h"
#include <boost/test/unit_test.hpp>
#include <fstream>
BOOST_AUTO_TEST_SUITE(capture_19534_tests)
BOOST_AUTO_TEST_CASE(capture)
{
    const char* outdir = std::getenv("CAPTURE_DIR");
    BOOST_REQUIRE(outdir);
    AddrMan peers{true};
    CNetAddr source;
    BOOST_REQUIRE(source.SetInternal("baseline-seed"));
    for (const char* name : {"250.7.1.1", "2001:470:abcd::1", "5wyqrzbvrdsumnok.onion"}) {
        CService service;
        BOOST_REQUIRE(Lookup(name, service, 32749, false));
        CAddress address(service, NODE_NETWORK);
        address.nTime = 1700000000;
        BOOST_REQUIRE(peers.Add(address, source));
    }
    CService tried;
    BOOST_REQUIRE(Lookup("250.7.1.1", tried, 32749, false));
    peers.Good(tried, 1700000001);
    CAddrDB adb;
    BOOST_REQUIRE(adb.Write(peers));
    { std::ifstream i((GetDataDir() / "peers.dat").string(), std::ios::binary); std::ofstream o(std::string(outdir) + "/netaddress_19534_peers.bin", std::ios::binary); o << i.rdbuf(); }

    banmap_t bans;
    CNetAddr a, b, m, t;
    BOOST_REQUIRE(LookupHost("1.2.3.4", a, false));
    BOOST_REQUIRE(LookupHost("2001:470:abcd::1", b, false));
    BOOST_REQUIRE(LookupHost("255.255.232.0", m, false));
    BOOST_REQUIRE(t.SetSpecial("5wyqrzbvrdsumnok.onion"));
    for (const CSubNet& s : {CSubNet(a, 24), CSubNet(b, 36), CSubNet(a, m), CSubNet(t)}) {
        CBanEntry e(1700000000, BanReasonManuallyAdded);
        e.nBanUntil = 1700003600;
        bans[s] = e;
    }
    BOOST_REQUIRE_EQUAL(bans.size(), 4U);
    CBanDB bdb(fs::path(outdir) / "netaddress_19534_banlist.bin");
    BOOST_REQUIRE(bdb.Write(bans));
}
BOOST_AUTO_TEST_SUITE_END()
```
