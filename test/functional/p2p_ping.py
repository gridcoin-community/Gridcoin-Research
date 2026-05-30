#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Phase 3 P2P ping/pong keepalive over the Gridcoin wire protocol.

Builds on p2p_version_handshake by exercising the ping/pong path explicitly: the
node must answer each of our `ping` messages with a `pong` echoing the same
nonce. sync_with_ping returns only once the matching-nonce pong arrives, so a
clean sequence of round-trips (and the growing pong count) proves the message
pump stays healthy after the handshake.
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.p2p import P2PInterface
from test_framework.util import assert_equal, assert_greater_than_or_equal


class P2PPingTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        # -staking=0 keeps the chain quiet; the node still listens for our
        # inbound P2P connection (regtest -listen defaults on).
        self.extra_args = [["-staking=0"]]

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def run_test(self):
        node = self.nodes[0]
        peer = node.add_p2p_connection(P2PInterface())
        assert peer.is_connected

        # Each sync_with_ping sends a uniquely-nonced ping and blocks until the
        # node returns a pong carrying that exact nonce.
        before = peer.message_count["pong"]
        rounds = 3
        for _ in range(rounds):
            peer.sync_with_ping()
        assert_greater_than_or_equal(peer.message_count["pong"], before + rounds)
        self.log.info("node answered %d ping/pong round-trips", rounds)

        node.disconnect_p2ps()
        assert_equal(node.num_test_p2p_connections(), 0)
        self.log.info("p2p_ping: handshake / ping-pong / disconnect OK")


if __name__ == "__main__":
    P2PPingTest().main()
