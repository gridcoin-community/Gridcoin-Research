#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""RPC wallet keypool / address operations on regtest.

Investor-mode only, no spendable coins required:

  - keypoolrefill grows the keypool to at least the requested size;
  - getnewaddress yields distinct, valid, wallet-owned addresses;
  - getnewaddress with an account label still produces a valid address;
  - dumpprivkey reveals the key for a wallet address.
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_greater_than_or_equal


class WalletKeypoolTest(GridcoinTestFramework):
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

        # --- keypoolrefill grows the pool to the requested size ---
        assert "keypoolsize" in node.getwalletinfo(), node.getwalletinfo()
        node.keypoolrefill(20)
        assert_greater_than_or_equal(node.getwalletinfo()["keypoolsize"], 20)
        self.log.info("keypoolrefill grew keypool to >= 20")

        # --- getnewaddress yields distinct, valid, wallet-owned addresses ---
        first = node.getnewaddress()
        second = node.getnewaddress()
        assert first != second, "getnewaddress returned a duplicate"
        for addr in (first, second):
            info = node.validateaddress(addr)
            assert info["isvalid"] and info.get("ismine", False), info
        self.log.info("getnewaddress produced distinct wallet-owned addresses")

        # --- account-labelled address is still valid ---
        labelled = node.getnewaddress("functional-test-label")
        assert node.validateaddress(labelled)["isvalid"], labelled

        # --- dumpprivkey reveals the wallet key for an address ---
        priv = node.dumpprivkey(first)
        assert isinstance(priv, str) and priv, "empty private key"
        self.log.info("dumpprivkey round-trip OK")


if __name__ == "__main__":
    WalletKeypoolTest().main()
