#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""disconnect_nodes must not be held open by a peer that arrives during it.

The framework's disconnect_nodes identifies each link from the side that
dialed it and waits for the accepting side by elimination against a snapshot
of the peers it had before the disconnect (the count-baseline race recorded
in the #3330 review). Three nodes: node 0 dials node 1, then node 1 dials
node 2 in the window between that snapshot and the wait. The disconnect of
0-1 completes and the helper has to return while 1-2 is up.

The arrival is injected deterministically rather than raced: the helper's
disconnectnode call on node 0 is wrapped so that node 1 connects to node 2
first, then the real RPC runs. A helper that bounded the acceptor by its
connection count falling from that snapshot never saw the drop it was waiting
for and timed out here.

Only the arriving-peer arm is pinned. A pre-existing peer of the acceptor
leaving mid-wait can still satisfy the helper early; that arm is unchanged
and not deterministically reproducible, so it is not asserted.

Connection directions are chosen so that no node accepts an inbound link
twice: the daemon drops a second inbound from the same IP within 5 s, and
every node here is 127.0.0.1. A fourth link would re-arm that limit.
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import (
    assert_equal,
    p2p_port,
)


class DisconnectNodesChurnTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 3
        self.chain = "regtest"
        self.setup_clean_chain = True
        # Nodes must be able to accept the addnode links connect_nodes makes;
        # regtest has no auto-peering, so they stay isolated until then.
        self.extra_args = [["-staking=0"]] * 3

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def run_test(self):
        node0, node1, node2 = self.nodes

        # Control first, so that on a helper that fails the pin below this
        # half is still on record: a plain disconnect with nothing arriving.
        # Node 1 dials node 0 here (node 0's one inbound accept).
        self.connect_nodes(1, 0)
        assert_equal(node0.getconnectioncount(), 1)
        self.disconnect_nodes(0, 1)
        assert_equal(node0.getconnectioncount(), 0)
        assert_equal(node1.getconnectioncount(), 0)
        self.log.info("plain disconnect_nodes drops the link on both sides")

        # Node 0 dials node 1 (node 1's one inbound accept).
        self.connect_nodes(0, 1)
        assert_equal(node1.getconnectioncount(), 1)

        # Wrap the RPC the helper issues on the dialing side. TestNode routes
        # unknown attributes to the RPC connection through __getattr__, which
        # only runs for attributes the instance does not have, so an instance
        # attribute shadows the RPC for exactly as long as it is set.
        real_disconnectnode = node0.disconnectnode
        injected = []

        def disconnectnode_with_arrival(*args, **kwargs):
            # Between the helper's snapshot of node 1's peers and its wait:
            # node 1 gains a peer by dialing node 2 (node 2's one inbound
            # accept). connect_nodes waits for the handshake, so the arrival
            # is complete before the disconnect is issued.
            self.connect_nodes(1, 2)
            assert_equal(node1.getconnectioncount(), 2)
            injected.append(True)
            return real_disconnectnode(*args, **kwargs)

        node0.disconnectnode = disconnectnode_with_arrival
        try:
            self.disconnect_nodes(0, 1)
        finally:
            del node0.disconnectnode
        assert_equal(len(injected), 1)
        self.log.info("disconnect_nodes returned with a peer that arrived mid-wait")

        # 0-1 is gone on both sides; 1-2 is untouched.
        assert_equal(node0.getconnectioncount(), 0)
        assert_equal([p['addr'] for p in node1.getpeerinfo()],
                     ["127.0.0.1:" + str(p2p_port(2))])
        assert_equal(node2.getconnectioncount(), 1)


if __name__ == "__main__":
    DisconnectNodesChurnTest().main()
