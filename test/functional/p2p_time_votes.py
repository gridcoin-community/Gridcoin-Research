#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Network time votes follow live outbound connections.

A node records an outbound peer's VERSION time offset as that peer's time vote
(AddTimeData) and withdraws it when the connection is deleted (RemoveTimeData,
called from the socket thread's node-deletion path). This drives both ends
between two regtest nodes and checks the NOISY log lines. Every regtest peer is
127.0.0.1, a single network group, so the votes never reach the five a median
needs and the offset stays 0.
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal


class P2PTimeVotesTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0", "-debug=noisy"]] * self.num_nodes

    def setup_network(self):
        # Same as p2p_version_handshake.py: nodes up, but not connected.
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def run_test(self):
        node = self.nodes[0]

        # node0 connects out to node1, so node0 records node1's time vote:
        # two votes, node1's and node0's own.
        self.log.info("an outbound connection casts a time vote")
        with node.assert_debug_log(["Added time data, votes 2"]):
            self.connect_nodes(0, 1)

        self.log.info("deleting the connection withdraws the vote")
        with node.assert_debug_log(["Withdrew time vote of peer"], timeout=30):
            self.disconnect_nodes(0, 1)

        assert_equal(node.getnetworkinfo()["timeoffset"], 0)


if __name__ == "__main__":
    P2PTimeVotesTest().main()
