#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""RPC raw-transaction build / decode / sign on regtest.

Investor-mode only. Drives the offline raw-transaction surface against a staked
coinstake output (tx-indexed, unlike the height-0 premine coinbase):

  - createrawtransaction builds an unsigned tx from a real UTXO;
  - decoderawtransaction round-trips the input/outputs and exposes Gridcoin's
    transaction-level nTime ("time") field;
  - decodescript decodes the output scriptPubKey;
  - signrawtransactionwithwallet completes the signature.

NOTE: the signed tx is not broadcast/mined here -- confirmation is covered by
mempool_accept.py, and generatetoaddress blocks on this stack exclude mempool
txs. Amounts are floats (AmountFromValue rejects strings).
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal


class RpcRawTransactionTest(GridcoinTestFramework):
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
        dest = node.getnewaddress()
        amount = float(utxo["amount"]) - 1  # ~1 GRC fee

        # --- createrawtransaction ---
        raw = node.createrawtransaction(
            [{"txid": utxo["txid"], "vout": utxo["vout"]}], {dest: amount})
        assert isinstance(raw, str) and raw, "empty raw tx"

        # --- decoderawtransaction round-trips inputs/outputs ---
        decoded = node.decoderawtransaction(raw)
        assert_equal(decoded["vin"][0]["txid"], utxo["txid"])
        assert_equal(decoded["vin"][0]["vout"], utxo["vout"])
        assert_equal(len(decoded["vout"]), 1)
        assert "time" in decoded, "Gridcoin tx nTime field absent from decode"
        self.log.info("createrawtransaction/decoderawtransaction round-trip OK")

        # --- decodescript on the output scriptPubKey ---
        spk_hex = decoded["vout"][0]["scriptPubKey"]["hex"]
        script = node.decodescript(spk_hex)
        assert script.get("type"), script
        assert dest in script.get("addresses", []), script
        self.log.info("decodescript resolved output to destination address")

        # --- signrawtransactionwithwallet completes the signature ---
        signed = node.signrawtransactionwithwallet(raw)
        assert_equal(signed["complete"], True)
        self.log.info("signrawtransactionwithwallet completed the signature")

        # --- signrawtransactionwithkey: offline sign with an explicit key ---
        # listunspent exposes the UTXO's address + scriptPubKey, so the input can
        # be signed with just its private key and prevout metadata (no wallet
        # context). This is the offline-signing path.
        priv = node.dumpprivkey(utxo["address"])
        prevtxs = [{"txid": utxo["txid"], "vout": utxo["vout"],
                    "scriptPubKey": utxo["scriptPubKey"]}]
        signed_key = node.signrawtransactionwithkey(raw, [priv], prevtxs)
        assert_equal(signed_key["complete"], True)
        self.log.info("signrawtransactionwithkey completed with an explicit key")


if __name__ == "__main__":
    RpcRawTransactionTest().main()
