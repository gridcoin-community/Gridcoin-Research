#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""A mempool message is not answered: the peer stays connected and the
request is logged under the net category.
"""

from test_framework.messages import msg_mempool
from test_framework.p2p import P2PInterface
from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal


class P2PMempoolRequestTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0", "-debug=net"]]

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def run_test(self):
        node = self.nodes[0]
        peer = node.add_p2p_connection(P2PInterface())

        self.log.info("Send mempool; it is not answered and the peer stays connected")
        with node.assert_debug_log(expected_msgs=["mempool request from"]):
            peer.send_and_ping(msg_mempool())
        assert peer.is_connected
        assert_equal(len(node.getpeerinfo()), 1)

        node.disconnect_p2ps()


if __name__ == "__main__":
    P2PMempoolRequestTest().main()
