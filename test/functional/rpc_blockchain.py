#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""RPC blockchain read-only queries on regtest.

Investor-mode only. Stakes a few blocks and asserts the chain-inspection surface
is internally consistent:

  - getblockcount tracks generatetoaddress;
  - getbestblockhash == getblockhash(tip height);
  - getblock returns the matching hash/height with a tx list;
  - getblockchaininfo exposes Gridcoin's field set
    {blocks, in_sync, moneysupply, difficulty, testnet, errors};
  - getdifficulty returns the {current, target} pair.
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal


class RpcBlockchainTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0", "-connect=0", "-listen=0"]]

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def run_test(self):
        node = self.nodes[0]

        start = node.getblockcount()
        node.generatetoaddress(3, node.getnewaddress())
        height = node.getblockcount()
        assert_equal(height, start + 3)

        # --- tip hash is consistent across the lookups ---
        best = node.getbestblockhash()
        assert_equal(node.getblockhash(height), best)
        block = node.getblock(best)
        assert_equal(block["hash"], best)
        assert_equal(block["height"], height)
        assert "merkleroot" in block and "tx" in block, block
        self.log.info("getblock/getblockhash/getbestblockhash agree at height %d", height)

        # --- getblockchaininfo field set ---
        info = node.getblockchaininfo()
        for field in ("blocks", "in_sync", "moneysupply", "difficulty", "testnet", "errors"):
            assert field in info, (field, info)
        assert_equal(info["blocks"], height)
        assert_equal(info["testnet"], False)
        self.log.info("getblockchaininfo exposes the expected field set")

        # --- getdifficulty current/target pair ---
        difficulty = node.getdifficulty()
        assert "current" in difficulty and "target" in difficulty, difficulty
        self.log.info("getdifficulty returns {current, target}")


if __name__ == "__main__":
    RpcBlockchainTest().main()
