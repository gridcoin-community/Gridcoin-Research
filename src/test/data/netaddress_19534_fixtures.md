# Legacy networking fixtures

Captured from c015839389734f9e85ba1756e90df6ef2a5ff39b (the unchanged supplied #19534 patch on eef7a61), before applying #19628.

Both files include mainnet network magic and the original checksum. Tests load them using CAddrDB/CBanDB, then compare the rewritten files byte for byte.

Peers: deterministic AddrMan; source SetInternal("baseline-seed"); addresses 250.7.1.1, 2001:470:abcd::1, and 5wyqrzbvrdsumnok.onion, all port 32749 and NODE_NETWORK. nTime=1700000000; first address marked Good at 1700000001.

Bans: 1.2.3.4/24, 2001:470:abcd::1/36, legacy non-contiguous 1.2.3.4/255.255.232.0, and legacy single-host 5wyqrzbvrdsumnok.onion. All manually added at 1700000000, until 1700003600. The non-contiguous mask pins loading old entries that new mask constructors intentionally reject; the Tor entry pins Gridcoin single-host ban compatibility.

netaddress_19534_peers.bin: 4372 bytes; SHA256 b152eba5fbfb8d913144af9534a0ac1784ce416a5b4dbafcf6b44941ac4aae81

netaddress_19534_banlist.bin: 253 bytes; SHA256 15ba8935b698860fe81a6ba041ea442a74feeaee0b7c2c0e1498b2a01cfeb663

