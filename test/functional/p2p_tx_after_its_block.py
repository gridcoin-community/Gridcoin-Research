#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""A transaction that reaches a node after its block is ignored.

A transaction can reach a node after the block that holds it: the block and
the transaction travel separately, and the block can arrive first. Such a
transaction is not a candidate for the pool. The node reads the message, logs
the transaction as already in a block and leaves the pool as it was.
"""

from test_framework.p2p import P2PInterface
from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal


class msg_raw_tx:
    """A tx message carrying a transaction's bytes as the node serialized them."""
    __slots__ = ("raw",)
    command = b"tx"

    def __init__(self, rawhex):
        self.raw = bytes.fromhex(rawhex)

    def serialize(self):
        return self.raw

    def __repr__(self):
        return "msg_raw_tx(%d bytes)" % len(self.raw)


class P2PTxAfterItsBlockTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0", "-debug=net", "-debug=noisy"]]

    def setup_network(self):
        # Start the node without the base class's coinbase key import.
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def run_test(self):
        node = self.nodes[0]
        sink = node.getnewaddress()
        node.generatetoaddress(12, sink)

        txid = node.sendtoaddress(sink, 10.0)
        raw = node.getrawtransaction(txid)

        # Move the clock on so that the miner includes the transaction.
        node.setmocktime(node.mock_now() + 64)
        node.generatetoaddress(1, sink)
        assert_equal(node.getrawmempool(), [])
        assert node.gettransaction(txid)["confirmations"] >= 1

        peer = node.add_p2p_connection(P2PInterface())

        # The first line shows that the node read the message.
        with node.assert_debug_log(expected_msgs=["received: tx from",
                                                  "ignored tx %s from" % txid,
                                                  "already in a block"]):
            peer.send_message(msg_raw_tx(raw))
            peer.sync_with_ping()

        assert_equal(node.getrawmempool(), [])

        node.disconnect_p2ps()


if __name__ == "__main__":
    P2PTxAfterItsBlockTest().main()
