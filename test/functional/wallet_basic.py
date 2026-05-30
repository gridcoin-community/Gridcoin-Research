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
  - sendtoaddress is accepted into the mempool;
  - address validation round-trip.

NOTE: transaction *confirmation* is intentionally not asserted here. On the
current regtest stack, generatetoaddress blocks contain only the coinbase +
coinstake (total size 460) — mempool transactions are not included — so a
sent/raw tx never confirms. Re-add confirmation assertions once regtest block
assembly includes mempool transactions. Amounts are floats (AmountFromValue
rejects string amounts).
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

        # --- sendtoaddress is accepted into the mempool ---
        to = node.getnewaddress()
        txid = node.sendtoaddress(to, 1.0)
        assert txid in node.getrawmempool(), "sendtoaddress not accepted into mempool"
        self.log.info("sendtoaddress 1 GRC -> %s accepted into mempool (%s)", to, txid)

        # --- address validation round-trip ---
        info = node.validateaddress(to)
        assert info["isvalid"], info
        assert info.get("ismine", True), info
        self.log.info("validateaddress round-trip OK for %s", to)


if __name__ == "__main__":
    WalletBasicTest().main()
