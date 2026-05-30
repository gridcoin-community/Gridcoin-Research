#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Phase 4A chain reorganization across two regtest nodes.

Investor-mode only (no beacon/CPID). Gridcoin's RPC surface has no
disconnectnode, so instead of split-then-reconnect we build divergent chains on
two nodes that start ISOLATED, then connect them and assert the node on the
shorter/lower-trust chain reorganizes onto the longer one.

With trivial regtest difficulty, more PoS blocks means more cumulative chain
trust, so node0's height-3 chain outweighs node1's height-1 chain.
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

        # --- build divergent chains: node0 longer (h=3), node1 shorter (h=1) ---
        node0.generatetoaddress(3, node0.getnewaddress())
        node1.generatetoaddress(1, node1.getnewaddress())
        tip0 = node0.getbestblockhash()
        assert node0.getbestblockhash() != node1.getbestblockhash(), "chains did not diverge"
        self.log.info("divergent chains: node0 h=%d, node1 h=%d",
                      node0.getblockcount(), node1.getblockcount())

        # --- connect; node1 reorgs onto node0's higher-trust chain ---
        self.connect_nodes(0, 1)
        self.sync_blocks([node0, node1])
        assert_equal(node1.getbestblockhash(), tip0)
        assert_equal(node1.getblockcount(), 3)
        assert_equal(node0.getbestblockhash(), tip0)  # node0 unchanged (it was longer)
        self.log.info("node1 reorganized onto node0's chain; common tip=%s", tip0)


if __name__ == "__main__":
    ReorgTest().main()
