#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Phase 2A/2B regtest functional test: premine, block production, stakelimit.

Exercises the regtest chain mode added by the Phase 2A skeleton plus the
Phase 2B.1/2B.5 block-production RPCs:

  - Premine discovery: the genesis premine (10 x 100,000 GRC = 1,000,000 GRC)
    pays the deterministic secp256k1 privkey=1 P2PKH. init.cpp plants that key
    into the wallet under IsMockableChain and rescans from before genesis, so a
    freshly started regtest node already sees the full premine balance.
  - Deterministic block production: the regtest-only `generatetoaddress` RPC
    mints blocks via TryMineRegtestBlock. Gridcoin has no proof-of-work path,
    so every produced block is proof-of-stake by construction; a +N height
    delta is therefore proof the PoS assembly pipeline ran N times.
  - stakelimit RPC contract: get/set/disable round-trip.

Determinism: the background ThreadStakeMiner is disabled with -staking=0 so the
chain advances only via explicit generate calls. TryMineRegtestBlock is gated
solely on IsMockableChain (not on -staking), so direct generation still works.

Wallet note: Gridcoin loads a single default BDB wallet at startup and has no
`createwallet` RPC, so the base class's regtest import_deterministic_coinbase_
privkeys() path (which calls createwallet) is bypassed here via a setup_network
override. The premine key is planted by the daemon itself; addresses are minted
with getnewaddress.
"""

from decimal import Decimal

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal


# 10 genesis-coinbase outputs of 100,000 GRC each (src/main.cpp regtest genesis
# construction; matching key planted in src/init.cpp under IsMockableChain).
EXPECTED_PREMINE = Decimal("1000000")


class RegtestStakingTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        # -staking=0 disables the background ThreadStakeMiner so the chain only
        # advances via explicit generate calls (deterministic height). -connect=0
        # / -listen=0 keep the single node isolated from any network.
        self.extra_args = [["-staking=0", "-connect=0", "-listen=0"]]

    def setup_network(self):
        # Single isolated regtest node. Bypass the base setup_nodes() regtest
        # branch: it calls import_deterministic_coinbase_privkeys() ->
        # createwallet, an RPC Gridcoin does not have (single default wallet,
        # no multiwallet). The regtest premine key is planted by the daemon
        # itself (init.cpp, under IsMockableChain), so no coinbase-key import
        # is needed.
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def run_test(self):
        node = self.nodes[0]

        # 1. Premine discovery. The genesis-coinbase premine is spendable and
        # stakeable on regtest, but getbalance() reports 0 because it treats
        # coinbase outputs as immature independently of the regtest maturity
        # shortcut. listunspent surfaces the 10 premine UTXOs directly.
        utxos = node.listunspent(0)
        assert_equal(len(utxos), 10)
        premine_total = sum(u["amount"] for u in utxos)
        assert_equal(premine_total, EXPECTED_PREMINE)
        self.log.info("regtest premine discovered: %d UTXOs totalling %s GRC",
                      len(utxos), premine_total)

        # 2. Deterministic block production. The TestNode.generate() wrapper
        # uses named RPC args plus a maxtries kwarg that Gridcoin's RPC parser
        # rejects ("Params must be an array"), so call the daemon's positional
        # `generatetoaddress <nblocks> <address>` directly. The address is
        # shape-checked only; the PoS coinstake reward goes to wallet keys.
        height0 = node.getblockcount()
        addr = node.getnewaddress()
        hashes = node.generatetoaddress(5, addr)
        assert_equal(len(hashes), 5)
        assert_equal(node.getblockcount(), height0 + 5)
        self.log.info("generated 5 PoS blocks: height %d -> %d",
                      height0, node.getblockcount())

        # 3. stakelimit RPC contract (regtest-only): no-arg reports the current
        # cap, a positional arg sets it, 0 disables.
        assert_equal(node.stakelimit()["height"], 0)
        assert_equal(node.stakelimit(10)["height"], 10)
        assert_equal(node.stakelimit(0)["height"], 0)
        self.log.info("stakelimit get/set/disable contract OK")


if __name__ == "__main__":
    RegtestStakingTest().main()
