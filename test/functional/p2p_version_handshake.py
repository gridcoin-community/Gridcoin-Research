#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Phase 3 P2P smoke test: version/verack handshake + ping/pong.

Connects a framework P2PInterface to a regtest gridcoinresearchd, completes the
Gridcoin version/verack handshake (the daemon sends its version as the `aries`
command), exchanges a ping/pong, confirms the node lists our subversion in
getpeerinfo, then disconnects cleanly. This is the minimal proof that
messages.py + p2p.py speak Gridcoin's wire protocol and that NetworkThread is
wired into the framework.
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.messages import MY_SUBVERSION
from test_framework.p2p import P2PInterface
from test_framework.util import assert_equal


class P2PVersionHandshakeTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        # -staking=0 keeps the chain quiet; the node still listens for our
        # inbound P2P connection (regtest -listen defaults on).
        self.extra_args = [["-staking=0"]]

    def setup_network(self):
        # Skip the base regtest setup_nodes() coinbase import path
        # (import_deterministic_coinbase_privkeys -> createwallet, an RPC
        # Gridcoin does not have). We only need the node up and listening.
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def run_test(self):
        node = self.nodes[0]

        # add_p2p_connection sends our version and waits for the node's
        # version+verack, then does a ping round-trip.
        peer = node.add_p2p_connection(P2PInterface())
        assert peer.is_connected
        assert "verack" in peer.last_message, "no verack received from node"
        assert "version" in peer.last_message, "no version received from node"
        self.log.info("handshake complete; node version=%d subver=%s",
                      peer.last_message["version"].nVersion,
                      peer.last_message["version"].strSubVer)

        # The node should now count us as a connected peer with our subversion.
        assert_equal(node.num_test_p2p_connections(), 1)
        assert any(p["subver"] == MY_SUBVERSION for p in node.getpeerinfo())

        # A second ping/pong confirms the message pump is healthy both ways.
        peer.sync_with_ping()

        node.disconnect_p2ps()
        assert_equal(node.num_test_p2p_connections(), 0)
        self.log.info("p2p_version_handshake: connect / handshake / ping / disconnect OK")


if __name__ == "__main__":
    P2PVersionHandshakeTest().main()
