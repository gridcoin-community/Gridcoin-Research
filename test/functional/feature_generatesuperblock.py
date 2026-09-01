#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""generatesuperblock: explicit superblock attach on regtest.

Scrapers are disabled under -regtest, so Quorum::CreateSuperblock() has nothing
to build from and the miner suppresses auto-attach. generatesuperblock is the
only path: it stages a caller-specified superblock and the next block the miner
assembles consumes it.

The block still goes through the ordinary consensus path.
Quorum::ValidateSuperblockClaim requires the superblock to be needed, the claim
quorum hash to match, and Quorum::ValidateSuperblock to not return INVALID --
which on a node with no local manifest data resolves to UNKNOWN ("waiting for
manifest data"), i.e. accepted. That is what makes a synthetic superblock
viable on regtest without a consensus bypass.
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error

CPID_A = "00010203040506070809101112131415"
CPID_B = "1f1e1d1c1b1a19181716151413121110"


class GenerateSuperblockTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        # -staking=0 disables the background ThreadStakeMiner so the chain only
        # advances via explicit generate calls; -connect=0 / -listen=0 keep the
        # node isolated.
        self.extra_args = [["-staking=0", "-connect=0", "-listen=0"]]

    def setup_network(self):
        # Bypass the base setup_nodes() regtest branch: it calls
        # import_deterministic_coinbase_privkeys() -> createwallet, which
        # Gridcoin does not have.
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def run_test(self):
        node = self.nodes[0]

        # A few ordinary blocks first: they must NOT pick up a superblock,
        # which is the auto-attach suppression this RPC works around.
        node.generatetoaddress(3, node.getnewaddress())
        assert_equal(node.superblocks(100), [])

        self.log.info("generatesuperblock attaches a staged superblock")
        height_before = node.getblockcount()
        hashes = node.generatesuperblock({CPID_A: 100, CPID_B: 50.25})
        assert_equal(len(hashes), 1)
        assert_equal(node.getblockcount(), height_before + 1)

        # The block was accepted onto the chain, carrying the contract.
        assert_equal(node.getbestblockhash(), hashes[0])
        landed = node.superblocks(100)
        assert len(landed) == 1, f"expected exactly one superblock, got {landed}"

        self.log.info("the staged superblock is consumed exactly once")
        # The next ordinary block must not re-attach it.
        node.generatetoaddress(1, node.getnewaddress())
        assert_equal(len(node.superblocks(100)), 1)

        self.log.info("input validation")
        assert_raises_rpc_error(
            -8, "at least one CPID magnitude is required",
            node.generatesuperblock, {})
        assert_raises_rpc_error(
            -8, "invalid CPID", node.generatesuperblock, {"not-a-cpid": 1})
        assert_raises_rpc_error(
            -8, "negative magnitude", node.generatesuperblock, {CPID_A: -1})
        assert_raises_rpc_error(
            -8, "must be a number", node.generatesuperblock, {CPID_A: "high"})
        assert_raises_rpc_error(
            -8, "at least one name", node.generatesuperblock, {CPID_A: 100}, [])
        assert_raises_rpc_error(
            -8, "non-empty string", node.generatesuperblock, {CPID_A: 100}, [1])

        # A failed call must not leave anything staged behind it.
        node.generatetoaddress(1, node.getnewaddress())
        assert_equal(len(node.superblocks(100)), 1)


if __name__ == "__main__":
    GenerateSuperblockTest().main()
