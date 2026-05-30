#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Phase 4A chain reorganization across two regtest nodes.

Investor-mode only (no beacon/CPID). Gridcoin's RPC surface has no
disconnectnode, so instead of split-then-reconnect we build divergent chains on
two nodes that start ISOLATED, then connect them and assert the node on the
shorter/lower-trust chain reorganizes onto the longer one.

Both chains are built to completion BEFORE connecting, so node0's tip is stable
when node1 starts syncing and there is no post-connect block-announcement race to
miss. node1 (the downloader) dials node0 (the block source) via connect_nodes(1,
0), matching the framework's default fPreferredDownload topology, so node1
actively header-syncs from node0 and reorganizes rather than passively waiting
for a push. With trivial regtest difficulty, more PoS blocks means more
cumulative chain trust, so node0's chain outweighs node1's.
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal


class ReorgTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0"], ["-staking=0"]]

    def setup_network(self):
        # Start unconnected so each node builds its own chain in isolation.
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def run_test(self):
        node0, node1 = self.nodes
        assert_equal(node0.getblockcount(), 0)
        assert_equal(node1.getblockcount(), 0)

        # --- build divergent chains in isolation, fully, before connecting ---
        # node0 longer (h=4), node1 shorter (h=1). Building both chains before
        # connecting means node0's tip is stable when node1 starts syncing, so
        # there is no post-connect block-announcement race to miss.
        node0.generatetoaddress(4, node0.getnewaddress())
        node1_tip = node1.generatetoaddress(1, node1.getnewaddress())[0]
        tip0 = node0.getbestblockhash()  # node0 at height 4
        assert tip0 != node1_tip, "chains did not diverge"
        self.log.info("divergent chains: node0 h=%d, node1 h=%d",
                      node0.getblockcount(), node1.getblockcount())

        # --- connect downloader -> source so node1 actively header-syncs ---
        # connect_nodes(a, b) dials from a to b; the framework's default topology
        # connects each downloader toward the block source (fPreferredDownload).
        # node1 (shorter) therefore dials node0 (longer) and reorganizes onto it.
        self.connect_nodes(1, 0)
        self.sync_blocks([node0, node1], timeout=120)
        assert_equal(node1.getbestblockhash(), tip0)
        assert_equal(node1.getblockcount(), 4)
        assert node1.getbestblockhash() != node1_tip, "node1 did not abandon its original tip"
        self.log.info("node1 reorganized onto node0's chain; common tip=%s", tip0)


if __name__ == "__main__":
    ReorgTest().main()
