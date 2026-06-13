#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""RPC PSGT (Partially Signed Gridcoin Transaction) round-trip on regtest.

Investor-mode only. PSGT is Gridcoin's PSBT analogue and has had no functional
coverage. Exercises the surface against a staked coinstake output (tx-indexed):

  - createpsgt builds an unsigned PSGT from a real UTXO;
  - decodepsgt round-trips the embedded transaction;
  - converttopsgt turns an equivalent raw tx into the same PSGT input;
  - combinepsgt merges duplicates without losing the transaction;
  - utxoupdatepsgt fills in the input UTXO from the tx index, then
    walletprocesspsgt signs with wallet keys and finalizepsgt extracts the
    completed transaction.

Amounts are floats (AmountFromValue rejects strings).
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal


class RpcPsgtTest(GridcoinTestFramework):
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

        # stake so we have a mature, tx-indexed coinstake output to spend
        node.generatetoaddress(10, node.getnewaddress())
        utxo = min(node.listunspent(0), key=lambda u: u["confirmations"])
        inputs = [{"txid": utxo["txid"], "vout": utxo["vout"]}]
        dest = node.getnewaddress()
        outputs = {dest: float(utxo["amount"]) - 1}

        # --- createpsgt + decodepsgt round-trip ---
        psgt = node.createpsgt(inputs, outputs)
        assert isinstance(psgt, str) and psgt, "empty PSGT"
        decoded = node.decodepsgt(psgt)
        assert_equal(decoded["tx"]["vin"][0]["txid"], utxo["txid"])
        assert_equal(len(decoded["tx"]["vout"]), 1)
        self.log.info("createpsgt/decodepsgt round-trip OK")

        # --- converttopsgt from an equivalent raw tx yields the same input ---
        raw = node.createrawtransaction(inputs, outputs)
        converted = node.converttopsgt(raw)
        assert_equal(node.decodepsgt(converted)["tx"]["vin"][0]["txid"], utxo["txid"])

        # --- combinepsgt of duplicates keeps the transaction intact ---
        combined = node.combinepsgt([psgt, psgt])
        assert_equal(node.decodepsgt(combined)["tx"]["vin"][0]["txid"], utxo["txid"])
        self.log.info("converttopsgt/combinepsgt round-trips OK")

        # --- fill UTXO data, sign with the wallet, then finalize ---
        updated = node.utxoupdatepsgt(psgt)
        processed = node.walletprocesspsgt(updated)
        assert "psgt" in processed and "complete" in processed, processed
        final = node.finalizepsgt(processed["psgt"])
        # The input is a wallet-owned UTXO signed by walletprocesspsgt, so finalize
        # must complete; assert unconditionally so a signing regression can't pass
        # silently.
        assert final.get("complete"), final
        assert final.get("hex"), "complete PSGT missing extracted hex"
        assert_equal(
            node.decoderawtransaction(final["hex"])["vin"][0]["txid"], utxo["txid"])
        self.log.info("walletprocesspsgt/finalizepsgt completed and extracted OK")


if __name__ == "__main__":
    RpcPsgtTest().main()
