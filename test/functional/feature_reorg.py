#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Chain reorganization across two regtest nodes, driven deterministically.

Two nodes share a chain, are split with disconnectnode, build competing
branches while apart, and are reconnected; the node on the shorter branch
must reorganize onto the longer one. With trivial regtest difficulty, more
proof-of-stake blocks means more cumulative chain trust, so node0's longer
branch outweighs node1's.

Why this is deterministic where the previous version was not. Both nodes
stake from the same premine, so two blocks staked at the same 16-second slot
can draw the same coinstake kernel; the duplicate-proof-of-stake guard then
makes each node reject the other's block and the shorter node never
reorganizes (about 13% of runs). The kernel hash includes the masked block
time, so node1's clock is moved two slots past node0's tip before it stakes:
every block on node0's branch is older than node1's, no two proofs can
collide, and the retry slots the miner may step through keep node1's block
within the 128-second future limit node0 enforces on a peer's block.

Reconnecting needs two things the framework documents: the accepting node's
clock is moved past the inbound rate limit, since a re-dial within 5 s of the
last accept from the same IP is dropped; and a fresh block is staked on node0
after the link is up, because connect-time sync alone does not pull blocks
the peer already has. Following that announcement brings node0's whole branch
and triggers the reorganize.
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal

STAKE_SLOT = 16


class ReorgTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0"], ["-staking=0"]]

    def setup_network(self):
        # node1 dials node0: the downloader toward the block source, matching
        # the framework's default topology, so node1 pulls actively.
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()
        self.connect_nodes(1, 0)

    def wait_for_link(self, node0, node1):
        self.wait_until(lambda: node0.getconnectioncount() > 0
                        and node1.getconnectioncount() > 0)

    def run_test(self):
        node0, node1 = self.nodes
        self.wait_for_link(node0, node1)

        self.log.info("shared chain: node0 stakes two blocks, node1 syncs them")
        node0.generatetoaddress(2, node0.getnewaddress())
        self.sync_blocks([node0, node1])
        fork_point = node0.getbestblockhash()
        assert_equal(node1.getblockcount(), 2)

        self.log.info("split, and build competing branches")
        self.disconnect_nodes(1, 0)
        assert_equal(node0.getconnectioncount(), 0)
        assert_equal(node1.getconnectioncount(), 0)

        node0.generatetoaddress(3, node0.getnewaddress())
        assert_equal(node0.getblockcount(), 5)
        node0_tip_time = node0.getblock(node0.getbestblockhash())["time"]

        # Two slots past everything node0 staked: no kernel can collide, and
        # the miner's retry slots stay under node0's 128 s future limit.
        node1.advance_mocktime_to(node0_tip_time + 2 * STAKE_SLOT)
        node1_tip = node1.generatetoaddress(1, node1.getnewaddress())[0]
        assert_equal(node1.getblockcount(), 3)
        assert_equal(node1.getblock(node1_tip)["previousblockhash"], fork_point)
        assert node1_tip != node0.getblockhash(3), "branches did not diverge"

        self.log.info("reconnect; node1 reorganizes onto node0's longer branch")
        # node0 accepted node1's dial before, so open its inbound window.
        node0.advance_mocktime_to(node0.mock_now() + 6)
        self.connect_nodes(1, 0)
        self.wait_for_link(node0, node1)

        node0.generatetoaddress(1, node0.getnewaddress())
        tip0 = node0.getbestblockhash()
        assert_equal(node0.getblockcount(), 6)
        self.sync_blocks([node0, node1], timeout=120)

        assert_equal(node1.getbestblockhash(), tip0)
        assert_equal(node1.getblockcount(), 6)
        assert_equal(node1.getblockhash(2), fork_point)
        assert_equal(node1.getblockhash(3), node0.getblockhash(3))
        # node1's own block is still known, but off the main chain.
        assert_equal(node1.getblock(node1_tip)["confirmations"], -1)
        # node0 never left its branch.
        assert node0.getblockhash(3) != node1_tip
        self.log.info("node1 reorganized onto node0's chain; tip=%s", tip0)


if __name__ == "__main__":
    ReorgTest().main()
