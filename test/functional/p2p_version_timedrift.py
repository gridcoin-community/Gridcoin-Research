#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""The VERSION handshake's clock-drift gate.

The node disconnects a peer whose version.nTime is more than 480 seconds from
its adjusted time, before that time can reach AddTimeData(). This checks both
edges of the band and one second past each, then a crafted
nTime == adjusted time + INT64_MIN. The gate used to compute
std::abs(adjusted - nTime), which is undefined behaviour for the crafted value:
the subtraction wraps to INT64_MIN, whose std::abs() is negative, and GCC builds
let the peer pass (Clang at -O2 happens to fold the check and reject it).

The node's clock is pinned with setmocktime so its adjusted time is known to
the second (inbound peers never feed AddTimeData, so the offset stays 0).
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.p2p import P2PInterface
from test_framework.util import assert_equal, p2p_port

DRIFT_LIMIT = 8 * 60
INT64_MIN = -(1 << 63)
INT64_MAX = (1 << 63) - 1

# More than the daemon's 5 s per-IP inbound rate limit, which compares
# (mock-affected) adjusted-time deltas; every scripted peer is 127.0.0.1.
CONNECT_SPACING = 6


class DriftPeer(P2PInterface):
    """Records the connection closing. The node drops a peer the gate rejects
    within milliseconds, often before add_p2p_connection() would see it
    connected, so the reject path cannot rely on observing that."""

    def __init__(self):
        super().__init__()
        self.closed = False

    def on_close(self):
        self.closed = True


class P2PVersionTimeDriftTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0"]]

    def setup_network(self):
        # Same as p2p_version_handshake.py: only the node up and listening.
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def connect(self, node, version_time):
        """Connect a peer whose version.nTime is version_time(adjusted time),
        evaluated after the clock advance; return whether the node completed
        the handshake."""
        node.advance_mocktime_to(node.mock_now() + CONNECT_SPACING)

        peer = DriftPeer()
        peer.version_time = version_time(node.mock_now())
        peer.peer_connect(dstaddr="127.0.0.1", dstport=p2p_port(node.index),
                          net=node.chain, timeout_factor=node.timeout_factor)()
        node.p2ps.append(peer)

        # A peer that passes the gate gets the node's version and verack; one
        # that fails is disconnected before the node sends anything.
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

        for label, version_time, expected in (
            ("on time", lambda now: now, True),
            ("+480 s, the upper edge", lambda now: now + DRIFT_LIMIT, True),
            ("-480 s, the lower edge", lambda now: now - DRIFT_LIMIT, True),
            ("+481 s", lambda now: now + DRIFT_LIMIT + 1, False),
            ("-481 s", lambda now: now - DRIFT_LIMIT - 1, False),
            ("INT64_MAX", lambda now: INT64_MAX, False),
            # A valid int64 because the adjusted time is positive. The old
            # gate's adjusted - nTime overflowed to INT64_MIN for it, whose
            # std::abs() is negative, so on GCC builds the peer passed.
            ("adjusted time + INT64_MIN", lambda now: now + INT64_MIN, False),
        ):
            self.log.info("version.nTime %s: expect %s", label,
                          "accepted" if expected else "disconnected")
            assert_equal(self.connect(node, version_time), expected)


if __name__ == "__main__":
    P2PVersionTimeDriftTest().main()
