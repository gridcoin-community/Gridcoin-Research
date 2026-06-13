#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""RPC wallet transaction-history queries on regtest.

Investor-mode only, read-only over a staked chain:

  - listtransactions returns the staked coinstake entries (category "generate")
    with the expected per-entry fields;
  - gettransaction round-trips a known txid with amount/confirmations/details;
  - listsinceblock returns {transactions, lastblock} and lastblock is the tip.

Also checks the encrypted-wallet guards: walletpassphrase / walletlock must
reject on this unencrypted regtest wallet. The full encrypt lifecycle is not
exercised -- encryptwallet calls StartShutdown() and the in-memory mock BDB does
not persist encryption across a restart.
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than, assert_raises_rpc_error


class WalletListTransactionsTest(GridcoinTestFramework):
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
        node.generatetoaddress(5, node.getnewaddress())

        # --- listtransactions: staked coinstakes show as "generate" ---
        txs = node.listtransactions("*", 20)
        assert_greater_than(len(txs), 0)
        for field in ("txid", "amount", "category", "confirmations", "time"):
            assert field in txs[-1], (field, txs[-1])
        assert any(t.get("category") == "generate" for t in txs), txs
        self.log.info("listtransactions returned %d entries incl. coinstake 'generate'", len(txs))

        # --- gettransaction round-trips a known txid ---
        txid = txs[-1]["txid"]
        gt = node.gettransaction(txid)
        assert_equal(gt["txid"], txid)
        for field in ("amount", "confirmations", "details"):
            assert field in gt, (field, gt)
        self.log.info("gettransaction round-trip OK for %s", txid)

        # --- listsinceblock structure + lastblock is the tip ---
        since = node.listsinceblock()
        assert "transactions" in since and "lastblock" in since, since
        assert_equal(since["lastblock"], node.getbestblockhash())
        self.log.info("listsinceblock lastblock == chain tip")

        # --- encrypted-wallet guards reject on an unencrypted wallet ---
        assert_raises_rpc_error(None, None, node.walletpassphrase, "passphrase", 10)
        assert_raises_rpc_error(None, None, node.walletlock)
        self.log.info("walletpassphrase/walletlock correctly reject on an unencrypted wallet")


if __name__ == "__main__":
    WalletListTransactionsTest().main()
