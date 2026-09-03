#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Phase 4A network RPCs across two regtest nodes.

Investor-mode only. Two nodes are started unconnected (regtest has no
auto-peering), then linked via the framework's connect_nodes (addnode under the
hood). Exercises getconnectioncount / getpeerinfo and verifies a block mined on
node0 propagates to node1 over the daemon-to-daemon link. Then exercises
disconnectnode: argument validation, disconnect by address and by nodeid, that
a block mined while split does not cross, and that the link can be re-made.
"""

import time

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_raises_rpc_error,
    p2p_port,
)


class RpcNetTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.chain = "regtest"
        self.setup_clean_chain = True
        # No -connect=0/-listen=0 here: the nodes must be able to accept the
        # addnode link connect_nodes() establishes. They stay isolated until
        # then (no DNS seeds / addrman peers on regtest).
        self.extra_args = [["-staking=0"], ["-staking=0"]]

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def open_inbound_window(self, node):
        """Move node's clock past the inbound rate limit before re-dialing it.

        The daemon accepts at most one inbound connection per 5 s from a given
        IP and scores a faster retry as misbehaviour (net.cpp, the accept loop
        in ThreadSocketHandler), so dialing a node straight after disconnecting
        from it is silently dropped, and retrying walks the dialer toward a ban.
        Every node here is 127.0.0.1. The limit reads GetAdjustedTime, which
        setmocktime pins, so the accepting side is moved 6 s forward instead of
        sleeping. Blocks staked on the unmocked node carry real timestamps,
        which are in this node's past and accepted as such.
        """
        self.mock_time += 6
        node.setmocktime(self.mock_time)

    def run_test(self):
        node0, node1 = self.nodes
        self.mock_time = int(time.time())

        # --- start unconnected ---
        assert_equal(node0.getconnectioncount(), 0)
        assert_equal(node0.getpeerinfo(), [])
        self.log.info("both nodes start with 0 peers")

        # --- connect and verify peer state on both sides ---
        self.connect_nodes(0, 1)
        assert_greater_than(node0.getconnectioncount(), 0)
        assert_greater_than(len(node0.getpeerinfo()), 0)
        assert_greater_than(len(node1.getpeerinfo()), 0)
        self.log.info("node0<->node1 linked: %d connection(s)", node0.getconnectioncount())

        # --- a block mined on node0 propagates to node1 ---
        node0.generatetoaddress(2, node0.getnewaddress())
        self.sync_blocks([node0, node1])
        assert_equal(node1.getbestblockhash(), node0.getbestblockhash())
        assert_equal(node1.getblockcount(), 2)
        self.log.info("block propagation over P2P link OK; tips synced at height 2")

        # --- disconnectnode: argument validation, none of which touches the link ---
        both = "Only one of address and nodeid should be provided."
        assert_raises_rpc_error(-32602, both, node0.disconnectnode)
        assert_raises_rpc_error(-32602, both, node0.disconnectnode, "")
        assert_raises_rpc_error(-32602, both, node0.disconnectnode, "127.0.0.1:1", 0)
        not_found = "Node not found in connected nodes"
        assert_raises_rpc_error(-29, not_found, node0.disconnectnode, "", 999999)
        assert_raises_rpc_error(-29, not_found, node0.disconnectnode, "127.0.0.1:1")
        # Control: every reject above left the link alone.
        assert_greater_than(node0.getconnectioncount(), 0)
        assert_greater_than(node1.getconnectioncount(), 0)
        self.log.info("disconnectnode rejects malformed and unknown targets")

        # --- disconnect by address, from the side that dialed ---
        # connect_nodes(0, 1) had node0 dial node1, so node0 lists the peer
        # under the exact address it dialed.
        target = "127.0.0.1:%d" % p2p_port(1)
        assert any(peer['addr'] == target for peer in node0.getpeerinfo())
        node0.disconnectnode(target)
        self.wait_until(lambda: node0.getconnectioncount() == 0
                        and node1.getconnectioncount() == 0, timeout=10)
        self.log.info("disconnectnode by address dropped the link on both sides")

        # --- a block mined while split stays on the miner ---
        node0.generatetoaddress(1, node0.getnewaddress())
        assert_equal(node0.getblockcount(), 3)
        assert_equal(node1.getblockcount(), 2)

        # --- reconnect; the re-made link carries the block ---
        # Connect-time header sync alone does not pull a block the peer already
        # has (node1 answers the version exchange and never sends getblocks;
        # feature_reorg.py records the same stall), so stake one more block on
        # node0: the fresh announcement over the established link is what
        # propagates, and the getblocks it triggers brings block 3 with it.
        self.open_inbound_window(node1)
        self.connect_nodes(0, 1)
        node0.generatetoaddress(1, node0.getnewaddress())
        self.sync_blocks([node0, node1])
        assert_equal(node1.getblockcount(), 4)
        self.log.info("re-made link synced the block mined while split")

        # --- disconnect by nodeid through the framework helper, then once more
        #     with the roles reversed so the helper is exercised from the side
        #     that accepted the connection ---
        self.disconnect_nodes(0, 1)
        assert_equal(node0.getconnectioncount(), 0)
        assert_equal(node1.getconnectioncount(), 0)
        # node0 has never accepted an inbound connection, so no window to open.
        self.connect_nodes(1, 0)
        self.disconnect_nodes(0, 1)
        assert_equal(node0.getconnectioncount(), 0)
        assert_equal(node1.getconnectioncount(), 0)
        self.log.info("disconnect_nodes works from either side of the link")


if __name__ == "__main__":
    RpcNetTest().main()
