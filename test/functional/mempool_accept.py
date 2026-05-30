#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Phase 4A mempool acceptance / rejection on regtest.

Investor-mode only. Gridcoin's merged RPC surface has no testmempoolaccept, so
this drives sendrawtransaction + getrawmempool directly:

  - a valid raw tx (spending a staked coinstake output) is accepted into the
    mempool and cleared once mined;
  - resubmitting an already-mined tx is rejected;
  - a double-spend of an in-mempool UTXO is rejected.

Coins are sourced from staked coinstake outputs (written to the tx index by
block connection), not the raw genesis premine coinbase (which is not tx-indexed
on this stack, so a spend of it would be accepted to the mempool but silently
dropped from blocks). assert_raises is used (not assert_raises_rpc_error) so the
rejection checks do not depend on Gridcoin's specific RPC error codes.
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

    def _recent_coinstake(self, node):
        # Most-recently-staked output: a coinstake (tx-indexed -> mineable),
        # never the height-0 premine coinbase (most confirmations).
        return min(node.listunspent(0), key=lambda u: u["confirmations"])

    def run_test(self):
        node = self.nodes[0]
        assert_equal(len(node.listunspent(0)), 10)

        # stake so we have mature, tx-indexed coinstake outputs to spend
        node.generatetoaddress(10, node.getnewaddress())

        # 1. a valid tx is accepted, then cleared from the mempool once mined
        u = self._recent_coinstake(node)
        rawhex = self._signed_spend(node, u, node.getnewaddress(), float(u["amount"]) - 1)
        txid = node.sendrawtransaction(rawhex)
        assert txid in node.getrawmempool(), "valid tx not accepted into mempool"
        node.generatetoaddress(1, node.getnewaddress())
        assert txid not in node.getrawmempool(), "mined tx not cleared from mempool"
        assert_equal(node.gettransaction(txid)["confirmations"], 1)
        self.log.info("valid tx accepted then confirmed: %s", txid)

        # 2. resubmitting the now-mined tx is rejected (already in chain)
        assert_raises(JSONRPCException, node.sendrawtransaction, rawhex)
        self.log.info("resubmit-of-mined-tx correctly rejected")

        # 3. double-spend: two txs spending the same fresh coinstake UTXO; the
        # first is accepted, the second conflicts and is rejected.
        u2 = self._recent_coinstake(node)
        amt = float(u2["amount"]) - 1
        first = self._signed_spend(node, u2, node.getnewaddress(), amt)
        second = self._signed_spend(node, u2, node.getnewaddress(), amt)
        node.sendrawtransaction(first)
        assert_raises(JSONRPCException, node.sendrawtransaction, second)
        self.log.info("double-spend correctly rejected")


if __name__ == "__main__":
    MempoolAcceptTest().main()
