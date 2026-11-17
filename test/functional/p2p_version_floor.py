#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""The VERSION handshake's protocol floor.

The node always accepts a peer on MIN_PEER_PROTO_VERSION, the previous
mandatory release's protocol version, and always disconnects anything older. A
peer from that version up to PROTOCOL_VERSION - 1 is disconnected only once
the grace period past the activation height of the fork the current
PROTOCOL_VERSION carries has elapsed.

PROTOCOL_VERSION is 180331, the block v15 mandatory. The floor is 180329, the
block v14 mandatory. 180330 lies between them: the releases that carry the v15
code with its height unset use it, so the floor is two below PROTOCOL_VERSION
and both older versions stay connected until the v15 grace period has elapsed.

v15 is unscheduled on regtest, so node0 checks the floor itself: 180331,
180330 and 180329 complete the handshake, and 180328 and an old version are
dropped -- by the floor check, which each dropped case asserts from the node's
log, not by some earlier check.

node1 and node2 check the post-grace rung at height 0, which a v15 height of
minus the grace period puts at its boundary (the regtest grace is 6,300 blocks;
the disconnect needs the tip above the v15 height plus the grace). node1's v15
height is one block further back, so the grace has elapsed and it drops 180330
and 180329 too. node2's is exactly at the boundary, so it still accepts both.
node2 then mines one block. Its tip is past the boundary after that and it
drops both, which shows that the node judges the grace against its own tip.

The old version is 180325, the oldest one that sends the current VERSION
layout. At 180324 and below the node reads five legacy strings before
nServices, and this framework always sends the current layout, so such a peer
would be misparsed and dropped by the time-drift check instead.

Each node's getnetworkinfo is checked against the framework's MY_VERSION first,
so the versions the cases are built from are the node's own.
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.messages import MIN_PEER_PROTO_VERSION, MY_VERSION
from test_framework.p2p import P2PInterface
from test_framework.util import assert_equal, p2p_port

# More than the daemon's 5 s per-IP inbound rate limit, which compares
# (mock-affected) adjusted-time deltas; every scripted peer is 127.0.0.1.
CONNECT_SPACING = 6

# Consensus::Params::ProtocolVersionGracePeriod on regtest (900 * 7).
REGTEST_GRACE = 6300


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
        self.num_nodes = 3
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [
            ["-staking=0"],
            ["-staking=0", "-blockv15height=%d" % -(REGTEST_GRACE + 1)],
            ["-staking=0", "-blockv15height=%d" % -REGTEST_GRACE],
        ]

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

    def check(self, node, cases):
        node.setmocktime(node.mock_now())

        for label, version, expected in cases:
            self.log.info("node%d, peer on %s (%d): expect %s", node.index, label, version,
                          "accepted" if expected else "disconnected")
            # The floor check names the version it drops. A drop for any other
            # reason, such as a misparsed VERSION failing the time-drift check,
            # must not pass as a floor rejection.
            floor_msg = "using obsolete version %d; disconnecting" % version
            drift_msg = "so far off our adjusted time"
            if expected:
                log_check = node.assert_debug_log(expected_msgs=[],
                                                  unexpected_msgs=[floor_msg, drift_msg])
            else:
                log_check = node.assert_debug_log(expected_msgs=[floor_msg],
                                                  unexpected_msgs=[drift_msg])
            with log_check:
                assert_equal(self.connect(node, version), expected)

    def run_test(self):
        # The cases below name the versions outright, so a change to either
        # constant has to come here and say what each version then is.
        assert_equal(MY_VERSION, 180331)
        assert_equal(MIN_PEER_PROTO_VERSION, 180329)
        for node in self.nodes:
            assert_equal(node.getnetworkinfo()["protocolversion"], MY_VERSION)

        current = ("PROTOCOL_VERSION, the v15 mandatory", 180331)
        between = ("the releases with v15 unset", 180330)
        floor = ("MIN_PEER_PROTO_VERSION, the v14 mandatory", 180329)
        below = ("MIN_PEER_PROTO_VERSION - 1", 180328)
        oldest = ("the oldest version with the current VERSION layout", 180325)

        self.log.info("v15 unscheduled: the floor")
        self.check(self.nodes[0], ((*current, True), (*between, True), (*floor, True),
                                   (*below, False), (*oldest, False)))

        self.log.info("v15 grace elapsed at height 0: everything below PROTOCOL_VERSION is dropped")
        self.check(self.nodes[1], ((*current, True), (*between, False), (*floor, False), (*below, False)))

        self.log.info("height 0 at the end of the v15 grace: the older versions are still accepted")
        self.check(self.nodes[2], ((*current, True), (*between, True), (*floor, True), (*below, False)))

        self.log.info("one block later the v15 grace has elapsed on that node")
        self.nodes[2].generatetoaddress(1, self.nodes[2].getnewaddress())
        assert_equal(self.nodes[2].getblockcount(), 1)
        self.check(self.nodes[2], ((*current, True), (*between, False), (*floor, False)))


if __name__ == "__main__":
    P2PVersionFloorTest().main()
