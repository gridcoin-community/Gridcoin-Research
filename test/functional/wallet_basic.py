#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Phase 4A wallet basics: premine, balance, spend acceptance on regtest.

Investor-mode only (no beacon/CPID).

  - premine discovery via listunspent (getbalance reports 0 for the raw premine,
    which is immature for balance accounting);
  - staking lifts getbalance above 0 (coinstake outputs mature immediately under
    IsMockableChain);
  - a raw spend of a staked coinstake output is accepted into the mempool;
  - address validation round-trip.

NOTE: confirmation is not asserted -- generatetoaddress blocks contain only the
coinbase + coinstake (mempool txs are not included on this stack). The spend
sources a staked coinstake output rather than sendtoaddress, because
sendtoaddress coin selection may pick a premine coinbase UTXO whose source tx is
not in the regtest tx index, which the wallet then rejects at commit ("-4 ...
coins already spent"). Amounts are floats (AmountFromValue rejects strings).
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

        # --- premine present via listunspent; getbalance reports 0 ---
        assert_equal(len(node.listunspent(0)), 10)
        assert_equal(node.getbalance(), 0)

        # --- staking converts premine into mature, spendable coinstake outputs ---
        node.generatetoaddress(10, node.getnewaddress())
        bal = node.getbalance()
        assert_greater_than(bal, 0)
        self.log.info("post-stake spendable balance: %s GRC", bal)

        # --- raw spend of a recent coinstake output is accepted into the mempool ---
        # (Most-recently-staked output = fewest confirmations = a coinstake, which
        # is tx-indexed; never the height-0 premine coinbase.)
        cs = min(node.listunspent(0), key=lambda u: u["confirmations"])
        dest = node.getnewaddress()
        raw = node.createrawtransaction(
            [{"txid": cs["txid"], "vout": cs["vout"]}], {dest: float(cs["amount"]) - 1})
        signed = node.signrawtransactionwithwallet(raw)
        assert signed.get("complete"), signed
        txid = node.sendrawtransaction(signed["hex"])
        assert txid in node.getrawmempool(), "coinstake spend not accepted into mempool"
        self.log.info("coinstake spend accepted into mempool: %s", txid)

        # --- address validation round-trip ---
        info = node.validateaddress(dest)
        assert info["isvalid"], info
        assert info.get("ismine", True), info
        self.log.info("validateaddress round-trip OK for %s", dest)


if __name__ == "__main__":
    WalletBasicTest().main()
