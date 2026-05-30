#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Phase 4A mempool acceptance / rejection on regtest.

Investor-mode only. Gridcoin's merged RPC surface has no testmempoolaccept, so
this drives sendrawtransaction + getrawmempool directly against premine UTXOs:

  - a valid raw tx is accepted into the mempool and cleared once mined;
  - resubmitting an already-mined tx is rejected;
  - a double-spend of an in-mempool UTXO is rejected.

NOTE: the rejection error codes (-27 already-in-chain, -26 rejected/conflict)
follow long-standing Bitcoin conventions; verify against Gridcoin's actual codes
in WSL and adjust if they differ.
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error


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
        utxos = node.listunspent(0)
        assert_equal(len(utxos), 10)

        # 1. a valid tx is accepted, then cleared from the mempool once mined
        rawhex = self._signed_spend(node, utxos[0], node.getnewaddress(), 99999.0)
        txid = node.sendrawtransaction(rawhex)
        assert txid in node.getrawmempool(), "valid tx not accepted into mempool"
        node.generatetoaddress(1, node.getnewaddress())
        assert txid not in node.getrawmempool(), "mined tx not cleared from mempool"
        assert_equal(node.gettransaction(txid)["confirmations"], 1)
        self.log.info("valid tx accepted then confirmed: %s", txid)

        # 2. resubmitting the now-mined tx is rejected (already in chain)
        assert_raises_rpc_error(-27, None, node.sendrawtransaction, rawhex)
        self.log.info("resubmit-of-mined-tx correctly rejected")

        # 3. double-spend: two txs spending the same fresh premine UTXO; the
        # first is accepted, the second conflicts and is rejected.
        u2 = node.listunspent(0)[0]
        amt = float(u2["amount"]) - 1  # ~1 GRC fee
        first = self._signed_spend(node, u2, node.getnewaddress(), amt)
        second = self._signed_spend(node, u2, node.getnewaddress(), amt)
        node.sendrawtransaction(first)
        assert_raises_rpc_error(-26, None, node.sendrawtransaction, second)
        self.log.info("double-spend correctly rejected")


if __name__ == "__main__":
    MempoolAcceptTest().main()
