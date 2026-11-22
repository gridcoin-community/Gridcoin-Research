#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""The psgt message's v15 gate and its bounded read.

Two unconnected regtest nodes, each fed by scripted peers:

- node0 runs with default args, so it is pre-v15 (BlockV15Height is
  unscheduled on regtest). It must drop a psgt message before reading it.
- node1 runs with -blockv15height=0 and mines two blocks under the mock clock
  first, so OutOfSyncByAge() is false and a psgt reaches pool validation.

Legs:

- A (node1): a psgt longer than MAX_PSGT_WIRE_SIZE is refused at the read
  and logged as undecodable. A node that reads it in full and leaves the
  size check to the pool logs "invalid psgt from ...: too-large" instead.
- B (node1): a garbage blob of exactly MAX_PSGT_WIRE_SIZE bytes passes the
  read and is refused by pool validation ("invalid psgt from"), not as
  undecodable. The expected line also proves the node was in sync.
- C (node0): a psgt whose length prefix promises more bytes than follow
  logs neither line. A node that reads before its v15 check fails that read
  and logs it as undecodable; one that checks v15 first never reads it. An
  oversize psgt would not tell the two apart: read in full or not at all,
  it reaches no log line on a pre-v15 node.

The scripted peers are 127.0.0.1, which CNode::Misbehaving exempts from
scoring (it returns early for a local address), so the test pins each path
by its log line, never by banscore. Each leg pings inside its
assert_debug_log block: the pong proves the psgt was processed before the
log is read, which is what keeps the "unexpected" checks from passing
vacuously.
"""

from test_framework.messages import msg_psgt, ser_compact_size
from test_framework.p2p import P2PInterface
from test_framework.test_framework import GridcoinTestFramework

# psgt.h
MAX_PSGT_WIRE_SIZE = 100000

UNDECODABLE = "undecodable psgt message from"
INVALID = "invalid psgt from"


class msg_psgt_truncated:
    """A psgt message whose length prefix says 1000 bytes but carries 10."""
    __slots__ = ()
    command = b"psgt"

    def serialize(self):
        return ser_compact_size(1000) + b"\x00" * 10

    def __repr__(self):
        return "msg_psgt_truncated()"


class P2PPsgtReadCapTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0"], ["-staking=0", "-blockv15height=0"]]

    def setup_network(self):
        # Bring the nodes up without the base-class createwallet path (the
        # daemon rejects its named params). They stay unconnected: each leg
        # reads one node's log.
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def send_and_ping(self, node, message):
        peer = self.add_p2p_connection_spaced(node, P2PInterface())
        peer.send_message(message)
        peer.sync_with_ping()

    def run_test(self):
        node0, node1 = self.nodes[0], self.nodes[1]

        # A current chain on node1 so OutOfSyncByAge() does not end the
        # handler before the paths under test. It measures the age of the
        # tip's PARENT, so one block is not enough: with a single block the
        # parent is genesis and the node stays out of sync.
        node1.generatetoaddress(2, node1.getnewaddress())

        self.log.info("A: an oversize psgt is refused at the read on a v15 node")
        with node1.assert_debug_log(expected_msgs=[UNDECODABLE],
                                    unexpected_msgs=["too-large"]):
            self.send_and_ping(node1, msg_psgt(b"\x00" * (MAX_PSGT_WIRE_SIZE + 1)))

        self.log.info("B: a psgt of exactly the cap passes the read")
        with node1.assert_debug_log(expected_msgs=[INVALID],
                                    unexpected_msgs=[UNDECODABLE]):
            self.send_and_ping(node1, msg_psgt(b"\x00" * MAX_PSGT_WIRE_SIZE))

        self.log.info("C: a pre-v15 node drops a psgt before reading it")
        with node0.assert_debug_log(expected_msgs=[],
                                    unexpected_msgs=[UNDECODABLE, INVALID]):
            self.send_and_ping(node0, msg_psgt_truncated())


if __name__ == "__main__":
    P2PPsgtReadCapTest().main()
