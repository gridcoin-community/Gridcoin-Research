#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""The VERSION handshake's protocol floor.

The node always accepts a peer on the previous release's protocol version
(PROTOCOL_VERSION - 1) and always disconnects anything older. A peer on the
previous version is disconnected only once the grace period past the
activation height of the fork the current PROTOCOL_VERSION carries has
elapsed. That fork, v15, is unscheduled on regtest, so this checks the floor
itself: PROTOCOL_VERSION and PROTOCOL_VERSION - 1 complete the handshake, and
PROTOCOL_VERSION - 2 and an ancient version are dropped.

The floor used to be a literal that was not bumped with PROTOCOL_VERSION. On
regtest, where the v14 grace period has not elapsed at height 0,
PROTOCOL_VERSION - 2 then passed.
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.messages import MY_VERSION
from test_framework.p2p import P2PInterface
from test_framework.util import assert_equal, p2p_port

# More than the daemon's 5 s per-IP inbound rate limit, which compares
# (mock-affected) adjusted-time deltas; every scripted peer is 127.0.0.1.
CONNECT_SPACING = 6


class VersionPeer(P2PInterface):
    """Advertises a chosen protocol version and records the connection closing.
    The node drops a rejected peer within milliseconds, often before
    add_p2p_connection() would see it connected."""

    def __init__(self, version):
        super().__init__()
        self.version = version
        self.closed = False

    def peer_connect_send_version(self):
        super().peer_connect_send_version()
        self.on_connection_send_msg.nVersion = self.version

    def on_close(self):
        self.closed = True


class P2PVersionFloorTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0"]]

    def setup_network(self):
        # Same as p2p_version_handshake.py: only the node up and listening.
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def connect(self, node, version):
        """Connect a peer advertising version; return whether the node
        completed the handshake."""
        node.advance_mocktime_to(node.mock_now() + CONNECT_SPACING)

        peer = VersionPeer(version)
        peer.version_time = node.mock_now()
        peer.peer_connect(dstaddr="127.0.0.1", dstport=p2p_port(node.index),
                          net=node.chain, timeout_factor=node.timeout_factor)()
        node.p2ps.append(peer)

        # A peer above the floor gets the node's version and verack; one below
        # it is disconnected before the node sends anything.
        peer.wait_until(lambda: "verack" in peer.last_message or peer.closed,
                        check_connected=False)
        accepted = "verack" in peer.last_message and not peer.closed

        if accepted:
            # Still connected after a full round trip, not dropped late.
            peer.sync_with_ping()

        node.disconnect_p2ps()
        assert_equal(node.num_test_p2p_connections(), 0)

        return accepted

    def run_test(self):
        node = self.nodes[0]
        node.setmocktime(node.mock_now())

        for label, version, expected in (
            ("PROTOCOL_VERSION", MY_VERSION, True),
            ("PROTOCOL_VERSION - 1, the previous release", MY_VERSION - 1, True),
            ("PROTOCOL_VERSION - 2", MY_VERSION - 2, False),
            ("an ancient version", 180300, False),
        ):
            self.log.info("peer on %s (%d): expect %s", label, version,
                          "accepted" if expected else "disconnected")
            assert_equal(self.connect(node, version), expected)


if __name__ == "__main__":
    P2PVersionFloorTest().main()
