#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Phase 4A wallet basics: spend, balance, confirmations on regtest.

Investor-mode only (no beacon/CPID). Coins are sourced from *staked coinstake
outputs*, not the raw genesis premine:

  - The genesis premine coinbase is spendable into the mempool, but the regtest
    chain does not (yet) write the genesis coinbase to the transaction index, so
    block assembly (CreateRestOfTheBlock -> ReadTxFromDisk) cannot find that
    input and silently drops the spending tx from the block. Staked coinstake
    outputs are written to the tx index by normal block connection, so spends of
    them confirm normally.
  - We therefore stake a handful of blocks first (converting premine UTXOs into
    mature coinstake outputs), then spend a coinstake output and confirm it.

getbalance reports 0 for the raw premine (immature for balance accounting) but
becomes positive after staking, since coinstake outputs are mature under
IsMockableChain. Amounts are floats (AmountFromValue rejects string amounts).
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than


class WalletBasicTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0", "-connect=0", "-listen=0"]]

    def setup_network(self):
        # Single isolated regtest node; bypass the base regtest createwallet path
        # (Gridcoin has one default BDB wallet, no multiwallet).
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def run_test(self):
        node = self.nodes[0]

        # --- premine present via listunspent (getbalance reports 0) ---
        assert_equal(len(node.listunspent(0)), 10)
        assert_equal(node.getbalance(), 0)

        # --- stake blocks: premine UTXOs -> mature, tx-indexed coinstakes ---
        node.generatetoaddress(10, node.getnewaddress())
        bal = node.getbalance()
        assert_greater_than(bal, 0)
        self.log.info("post-stake spendable balance: %s GRC", bal)

        # --- spend a recent coinstake UTXO via raw tx and confirm it ---
        # The most-recently-staked output has the fewest confirmations and is a
        # coinstake (the genesis premine sits at height 0 with the most), so this
        # avoids spending the not-tx-indexed premine coinbase.
        cs = min(node.listunspent(0), key=lambda u: u["confirmations"])
        dest = node.getnewaddress()
        amount = float(cs["amount"]) - 1  # ~1 GRC fee
        raw = node.createrawtransaction(
            [{"txid": cs["txid"], "vout": cs["vout"]}], {dest: amount})
        signed = node.signrawtransactionwithwallet(raw)
        assert signed.get("complete"), signed
        txid = node.sendrawtransaction(signed["hex"])
        assert txid in node.getrawmempool()

        node.generatetoaddress(1, node.getnewaddress())
        assert txid not in node.getrawmempool(), "spend was not mined"
        assert_equal(node.gettransaction(txid)["confirmations"], 1)
        recv = [x for x in node.listunspent(0) if x.get("address") == dest]
        assert_equal(len(recv), 1)
        self.log.info("coinstake spend confirmed; %s holds %s GRC", dest, recv[0]["amount"])

        # --- address validation round-trip ---
        info = node.validateaddress(dest)
        assert info["isvalid"], info
        assert info.get("ismine", True), info

        # --- high-level sendtoaddress reaches the mempool ---
        # (We assert acceptance, not confirmation: sendtoaddress coin selection
        # may pick a premine coinbase UTXO, which is mempool-valid but not
        # mineable on this stack until the genesis coinbase is tx-indexed.)
        to = node.getnewaddress()
        send_txid = node.sendtoaddress(to, 1.0)
        assert send_txid in node.getrawmempool()
        self.log.info("sendtoaddress 1 GRC -> %s accepted into mempool", to)


if __name__ == "__main__":
    WalletBasicTest().main()
