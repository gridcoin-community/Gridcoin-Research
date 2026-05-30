#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Phase 4A mempool acceptance / rejection on regtest.

Investor-mode only. Gridcoin's merged RPC surface has no testmempoolaccept, so
this drives sendrawtransaction + getrawmempool directly against a staked
coinstake output:

  - a valid raw tx is accepted into the mempool;
  - a double-spend of the same in-mempool UTXO is rejected.

assert_raises is used (not assert_raises_rpc_error) so the rejection check does
not depend on Gridcoin's specific RPC error codes.

NOTE: mining/confirmation is not asserted here. On the current regtest stack,
generatetoaddress blocks contain only the coinbase + coinstake (mempool
transactions are not included), so a mempool tx never confirms. Re-add a
confirm-and-clear step once regtest block assembly includes mempool transactions.
"""

from test_framework.authproxy import JSONRPCException
from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal, assert_raises


class MempoolAcceptTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0", "-connect=0", "-listen=0"]]

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def _signed_spend(self, node, utxo, dest, amount):
        raw = node.createrawtransaction(
            [{"txid": utxo["txid"], "vout": utxo["vout"]}], {dest: amount})
        signed = node.signrawtransactionwithwallet(raw)
        assert signed.get("complete"), signed
        return signed["hex"]

    def run_test(self):
        node = self.nodes[0]
        assert_equal(len(node.listunspent(0)), 10)

        # stake so we have a mature, tx-indexed coinstake output to spend
        node.generatetoaddress(10, node.getnewaddress())
        # most-recently-staked output (fewest confirmations) is a coinstake,
        # never the height-0 premine coinbase.
        u = min(node.listunspent(0), key=lambda x: x["confirmations"])
        amt = float(u["amount"]) - 1  # ~1 GRC fee

        # 1. a valid tx is accepted into the mempool
        first = self._signed_spend(node, u, node.getnewaddress(), amt)
        txid = node.sendrawtransaction(first)
        assert txid in node.getrawmempool(), "valid tx not accepted into mempool"
        self.log.info("valid tx accepted into mempool: %s", txid)

        # 2. a double-spend of the same UTXO is rejected
        second = self._signed_spend(node, u, node.getnewaddress(), amt)
        assert_raises(JSONRPCException, node.sendrawtransaction, second)
        self.log.info("double-spend correctly rejected")


if __name__ == "__main__":
    MempoolAcceptTest().main()
