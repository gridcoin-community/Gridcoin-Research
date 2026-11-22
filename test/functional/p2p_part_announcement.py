#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""A peer's announcement of a scraper part is answered from the part store.

A node fetches the parts of a scraper manifest it holds. It asks for a part
only while a manifest references the part and the part's data has not
arrived. A peer that announces a part no manifest here references gets no
request for it.

The test's node holds no manifest, so every part is unreferenced. As a control,
the same peer announces a transaction the node does not have, and the node
asks for that one: the announcement was read and requests are being sent.
"""

from test_framework.messages import CInv, MSG_PART, MSG_TX, msg_inv
from test_framework.p2p import P2PInterface
from test_framework.test_framework import GridcoinTestFramework

PART_HASH = 0x7061727400000000000000000000000000000000000000000000000000000001
TX_HASH = 0x7478000000000000000000000000000000000000000000000000000000000002


class Announcer(P2PInterface):
    def __init__(self):
        super().__init__()
        self.requested = []

    def on_getdata(self, message):
        self.requested.extend((inv.type, inv.hash) for inv in message.inv)


class P2PPartAnnouncementTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0"]]

    def setup_network(self):
        # Start the node without the base class's coinbase key import.
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def run_test(self):
        peer = self.nodes[0].add_p2p_connection(Announcer())

        # The part first, then the transaction: the node works through an inv in
        # order and sends its requests in the order it queued them, so once the
        # request for the transaction has arrived, one for the part would have too.
        peer.send_message(msg_inv([CInv(MSG_PART, PART_HASH), CInv(MSG_TX, TX_HASH)]))

        self.wait_until(lambda: (MSG_TX, TX_HASH) in peer.requested, timeout=30)
        peer.sync_with_ping()

        assert (MSG_PART, PART_HASH) not in peer.requested, \
            "the node asked for a part that no manifest references"
        self.log.info("unreferenced part announced: not requested; unknown transaction announced: requested")

        self.nodes[0].disconnect_p2ps()


if __name__ == "__main__":
    P2PPartAnnouncementTest().main()
