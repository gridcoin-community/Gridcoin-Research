#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Two superblocks disconnected by two reorganizations.

From block version 15, a superblock disconnect that finds the beacon
registry's superblock state not current rebuilds the registry. The second
disconnect here is one. One node runs with block version 15 active and one
below it, where the behaviour is unchanged.
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal

CPID = "00010203040506070809101112131415"
STAKE_TIMESTAMP_MASK = 15
REBUILD = "Rebuilding beacon registry in-line"
# GetSuperblockAgeSpacing on regtest: a superblock needs this long since the last one.
SUPERBLOCK_SPACING = 43200


class BeaconSuperblockDisconnectsTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [
            ["-staking=0", "-connect=0", "-listen=0", "-blockv15height=1"],
            ["-staking=0", "-connect=0", "-listen=0"],
        ]

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def advance_to_slot(self, node, seconds=128):
        tip_time = node.getblock(node.getbestblockhash())["time"]
        slot = (tip_time + seconds) & ~STAKE_TIMESTAMP_MASK
        node.setmocktime(slot)

    def two_superblocks(self, node):
        """Mine two superblocks; return the blocks just before each."""
        node.generatetoaddress(5, node.getnewaddress())
        self.advance_to_slot(node)
        before_first = node.getbestblockhash()
        node.generatesuperblock({CPID: 100})
        node.generatetoaddress(2, node.getnewaddress())
        self.advance_to_slot(node, SUPERBLOCK_SPACING + 600)
        before_second = node.getbestblockhash()
        node.generatesuperblock({CPID: 100})
        node.generatetoaddress(2, node.getnewaddress())
        return before_first, before_second

    def run_test(self):
        for node, v15 in ((self.nodes[0], True), (self.nodes[1], False)):
            self.log.info(f"two superblock disconnects, v15 {'on' if v15 else 'off'}")
            before_first, before_second = self.two_superblocks(node)

            with node.assert_debug_log([], unexpected_msgs=[REBUILD]):
                assert_equal(node.reorganize(before_second)["RollbackChain"], True)

            if v15:
                with node.assert_debug_log([REBUILD]):
                    assert_equal(node.reorganize(before_first)["RollbackChain"], True)
            else:
                with node.assert_debug_log([], unexpected_msgs=[REBUILD]):
                    assert_equal(node.reorganize(before_first)["RollbackChain"], True)


if __name__ == "__main__":
    BeaconSuperblockDisconnectsTest().main()
