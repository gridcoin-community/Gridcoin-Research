#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Phase 4A chain reorganization across two regtest nodes.

KNOWN-FLAKY / OPT-IN: this test is in EXTENDED_SCRIPTS, not the default suite.
Both nodes share the same deterministic, stakeable premine, so their
independently-staked block-1s can draw the same coinstake kernel; when the
proof-of-stake hashes collide (~13% of runs) the duplicate-proof-of-stake guard
makes each node reject the other's block and the shorter node never reorganizes.
It cannot be made deterministic from Python without a setmocktime RPC,
invalidateblock, or disconnectnode (none currently exposed). See the note in
test_runner.py's EXTENDED_SCRIPTS. The logic below is the best-effort reference
implementation for when one of those primitives lands.

Investor-mode only (no beacon/CPID). Gridcoin's RPC surface has no
disconnectnode, so instead of split-then-reconnect we build divergent chains on
two nodes that start ISOLATED, then connect them and assert the node on the
shorter/lower-trust chain reorganizes onto the longer one.

Two things make the reorg propagate reliably (relying on connect-time header sync
alone intermittently stalls on a loaded CI runner):

  1. node1 (the downloader) dials node0 (the block source) via connect_nodes(1,
     0), matching the framework's default fPreferredDownload topology, so node1
     actively pulls from node0 instead of passively waiting for a push.
  2. After the link is fully established (both nodes report the connection),
     node0 stakes one more block. A fresh post-connect block announcement over an
     established link is the propagation path that reliably works here (the same
     pattern rpc_net.py depends on); node1 follows it and, in doing so, pulls
     node0's whole chain and reorganizes off its own shorter tip.

With trivial regtest difficulty, more PoS blocks means more cumulative chain
trust, so node0's chain outweighs node1's.
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal


class ReorgTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0"], ["-staking=0"]]

    def setup_network(self):
        # Start unconnected so each node builds its own chain in isolation.
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def run_test(self):
        node0, node1 = self.nodes
        assert_equal(node0.getblockcount(), 0)
        assert_equal(node1.getblockcount(), 0)

        # --- build divergent chains in isolation, before connecting ---
        # node0 longer (h=4), node1 shorter (h=1).
        node0.generatetoaddress(4, node0.getnewaddress())
        node1_tip = node1.generatetoaddress(1, node1.getnewaddress())[0]
        assert node0.getbestblockhash() != node1_tip, "chains did not diverge"
        self.log.info("divergent chains: node0 h=%d, node1 h=%d",
                      node0.getblockcount(), node1.getblockcount())

        # --- connect downloader (node1) -> source (node0) ---
        # connect_nodes(a, b) dials from a to b; the framework's default topology
        # connects each downloader toward the block source (fPreferredDownload),
        # so node1 (shorter) dials node0 (longer) and pulls from it actively.
        self.connect_nodes(1, 0)
        # Wait for the link to be established in BOTH directions before relying
        # on it; node1 dialed node0, so node0 must also register the inbound peer.
        self.wait_until(
            lambda: node0.getconnectioncount() > 0 and node1.getconnectioncount() > 0)

        # --- stake a fresh block on node0 to trigger reliable propagation ---
        # A new block announced over the now-established link reliably reaches
        # node1 (cf. rpc_net.py); following it pulls node0's whole chain, so
        # node1 reorganizes off its height-1 tip onto node0's chain.
        node0.generatetoaddress(1, node0.getnewaddress())
        tip0 = node0.getbestblockhash()  # node0 at height 5
        self.sync_blocks([node0, node1], timeout=120)
        assert_equal(node1.getbestblockhash(), tip0)
        assert_equal(node1.getblockcount(), 5)
        assert node1.getbestblockhash() != node1_tip, "node1 did not abandon its original tip"
        self.log.info("node1 reorganized onto node0's chain; common tip=%s", tip0)


if __name__ == "__main__":
    ReorgTest().main()
