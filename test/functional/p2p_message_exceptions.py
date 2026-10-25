#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""A peer's malformed message is logged, not raised as a node warning.

When a peer's message fails to deserialize, ProcessMessage() throws and
ProcessMessages() catches it. The peer chose those bytes, so the exception is
an ordinary rejection: it goes to debug.log, the connection stays up, and the
node's warning string (getnetworkinfo "errors", the GUI status bar) and stderr
are left alone. Each case throws a different deserialization error.

The node runs with the net category excluded, so the test also checks that
the line is logged on a node without -debug=net.
"""

import struct

from test_framework.messages import MAX_LOCATOR_SZ, ser_compact_size
from test_framework.p2p import P2PInterface
from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal


class msg_raw:
    """A message with a given command and payload, for bytes the typed message
    classes will not produce."""
    __slots__ = ("command", "payload")

    def __init__(self, command, payload):
        self.command = command
        self.payload = payload

    def serialize(self):
        return self.payload

    def __repr__(self):
        return "msg_raw(%s, %d bytes)" % (self.command.decode(), len(self.payload))


# (description, command, payload, start of the exception's what())
CASES = [
    ("an inv count with no entries after it", b"inv",
     ser_compact_size(1),
     "CDataStream::read(): end of data"),
    ("an inv count above MAX_SIZE", b"inv",
     b"\xfe" + struct.pack("<I", 0x04000000),
     "ReadCompactSize(): size too large"),
    ("a getblocks locator above MAX_LOCATOR_SZ", b"getblocks",
     struct.pack("<i", 0) + ser_compact_size(MAX_LOCATOR_SZ + 1),
     "Vector length limit exceeded"),
    ("an inv count in a non-canonical CompactSize", b"inv",
     b"\xfd\x01\x00",
     "non-canonical ReadCompactSize()"),
]


class P2PMessageExceptionsTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0", "-debugexclude=net"]]

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def run_test(self):
        node = self.nodes[0]
        errors_before = node.getnetworkinfo()["errors"]
        peer = node.add_p2p_connection(P2PInterface())

        for description, command, payload, what in CASES:
            self.log.info("Send %s", description)
            # The line names the command and payload size, then the
            # exception's what(). The type name after it is the compiler's
            # (mangled) spelling, so it is not matched.
            expected = "ProcessMessages(%s, %d bytes): Exception '%s" % (
                command.decode(), len(payload), what)
            with node.assert_debug_log(expected_msgs=[expected]):
                # The pong proves the node read past the bad message and kept
                # the connection.
                peer.send_and_ping(msg_raw(command, payload))
            assert_equal(node.getnetworkinfo()["errors"], errors_before)

        node.disconnect_p2ps()
        # stop_node() also fails the test on any stderr output.


if __name__ == "__main__":
    P2PMessageExceptionsTest().main()
