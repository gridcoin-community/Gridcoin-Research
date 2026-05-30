#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Phase 4A network RPCs across two regtest nodes.

Investor-mode only. Two nodes are started unconnected (regtest has no
auto-peering), then linked via the framework's connect_nodes (addnode under the
hood). Exercises getconnectioncount / getpeerinfo and verifies a block mined on
node0 propagates to node1 over the daemon-to-daemon link.
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than


class RpcNetTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.chain = "regtest"
        self.setup_clean_chain = True
        # No -connect=0/-listen=0 here: the nodes must be able to accept the
        # addnode link connect_nodes() establishes. They stay isolated until
        # then (no DNS seeds / addrman peers on regtest).
        self.extra_args = [["-staking=0"], ["-staking=0"]]

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def run_test(self):
        node0, node1 = self.nodes

        # --- start unconnected ---
        assert_equal(node0.getconnectioncount(), 0)
        assert_equal(node0.getpeerinfo(), [])
        self.log.info("both nodes start with 0 peers")

        # --- connect and verify peer state on both sides ---
        self.connect_nodes(0, 1)
        assert_greater_than(node0.getconnectioncount(), 0)
        assert_greater_than(len(node0.getpeerinfo()), 0)
        assert_greater_than(len(node1.getpeerinfo()), 0)
        self.log.info("node0<->node1 linked: %d connection(s)", node0.getconnectioncount())

        # --- a block mined on node0 propagates to node1 ---
        node0.generatetoaddress(2, node0.getnewaddress())
        self.sync_blocks([node0, node1])
        assert_equal(node1.getbestblockhash(), node0.getbestblockhash())
        assert_equal(node1.getblockcount(), 2)
        self.log.info("block propagation over P2P link OK; tips synced at height 2")


if __name__ == "__main__":
    RpcNetTest().main()
