#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Phase 4A wallet basics: spend, balance, confirmations on regtest.

Investor-mode only (no beacon/CPID). The genesis premine (10 x 100,000 GRC) is
the coin source. Two spend paths are exercised:

  - raw tx (createrawtransaction -> signrawtransactionwithwallet ->
    sendrawtransaction): the primitive proven by the Phase 3 p2p test; spends a
    listunspent premine UTXO directly.
  - sendtoaddress: the high-level wallet send. It needs spendable wallet
    balance, so we stake a few blocks first; coinbase/coinstake maturity is
    gated to 0 under IsMockableChain, so the staked rewards become spendable
    immediately and lift getbalance above 0.

getbalance note: a freshly started node reports getbalance() == 0 even though
listunspent shows the premine, because the premine coinbase is treated as
immature for *balance accounting*. So premine spends go through raw tx (which
selects a specific listunspent UTXO), not getbalance-driven sendtoaddress.

Amounts are passed as floats: Gridcoin's AmountFromValue rejects string-encoded
amounts (which is what a Decimal becomes when JSON-serialized).
"""

from decimal import Decimal

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
        premine = node.listunspent(0)
        assert_equal(len(premine), 10)

        # --- raw-tx spend of a premine UTXO to a fresh wallet address ---
        u = premine[0]
        dest = node.getnewaddress()
        raw = node.createrawtransaction(
            [{"txid": u["txid"], "vout": u["vout"]}], {dest: 99999.0})  # ~1 GRC fee
        signed = node.signrawtransactionwithwallet(raw)
        assert signed.get("complete"), signed
        txid = node.sendrawtransaction(signed["hex"])
        assert txid in node.getrawmempool()

        # confirm it: gettransaction confirmations advance to 1 after one block
        sink = node.getnewaddress()
        node.generatetoaddress(1, sink)
        assert_equal(node.gettransaction(txid)["confirmations"], 1)
        dest_utxos = [x for x in node.listunspent(0) if x.get("address") == dest]
        assert_equal(len(dest_utxos), 1)
        assert_equal(dest_utxos[0]["amount"], Decimal("99999"))
        self.log.info("raw spend confirmed; %s holds 99999 GRC", dest)

        # --- address validation round-trip ---
        info = node.validateaddress(dest)
        assert info["isvalid"], info
        assert info.get("ismine", True), info

        # --- sendtoaddress: stake first so the wallet has matured spendable
        # balance (the premine alone reports getbalance 0). ---
        node.generatetoaddress(5, node.getnewaddress())
        bal = node.getbalance()
        assert_greater_than(bal, 0)
        self.log.info("post-stake spendable balance: %s GRC", bal)

        to = node.getnewaddress()
        send_txid = node.sendtoaddress(to, 1.0)
        assert send_txid in node.getrawmempool()
        node.generatetoaddress(1, sink)
        assert_equal(node.gettransaction(send_txid)["confirmations"], 1)
        recv = [x for x in node.listunspent(0) if x.get("address") == to]
        assert_equal(sum(x["amount"] for x in recv), Decimal("1"))
        self.log.info("sendtoaddress 1 GRC -> %s confirmed", to)


if __name__ == "__main__":
    WalletBasicTest().main()
