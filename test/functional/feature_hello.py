#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Phase 1 smoke test for the functional-test framework.

Starts gridcoinresearchd in -testnet mode with no outbound peers, calls
getblockchaininfo over RPC, and asserts the daemon reports chain=="test".
The point is to prove the Python framework can start the binary, authenticate,
issue an RPC, parse the response, and shut the binary down cleanly.

Phase 2A introduces CRegTestParams; after that lands, additional tests can run
against -regtest and exercise generate/staking/contract code paths. For Phase
1, testnet is the only available chain, so this test deliberately avoids any
RPC that requires the node to be in sync, have peers, or have mined blocks.
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal


class FeatureHelloTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "test"
        self.setup_clean_chain = True
        # -connect=0 prevents the daemon from reaching out to testnet seed
        # nodes; the test only needs the RPC interface to come up.
        # -listen=0 stops the daemon from binding an inbound listener.
        self.extra_args = [["-connect=0", "-listen=0"]]

    def setup_network(self):
        # Single node; nothing to connect or sync. Skip the base class's
        # connect_nodes/sync_all loop.
        self.setup_nodes()

    def run_test(self):
        node = self.nodes[0]
        info = node.getblockchaininfo()
        assert_equal(info["chain"], "test")
        self.log.info("feature_hello: getblockchaininfo OK (chain=%s, blocks=%d)",
                      info["chain"], info["blocks"])


if __name__ == "__main__":
    FeatureHelloTest().main()
