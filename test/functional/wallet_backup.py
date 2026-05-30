#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Phase 4A wallet backup / key dump primitives on regtest.

Investor-mode only. Exercises Gridcoin's backup and key-export surface:

  - backupwallet(): NO arguments (unlike Bitcoin's backupwallet "dest"); it backs
    up the wallet/config/settings to Gridcoin's managed backup directory and
    returns a result object whose "Backup wallet success" flag we assert.
  - dumpprivkey(addr): exports a wallet key as WIF.
  - dumpprivkey error path on an invalid address.

A full backup -> wipe -> restore cycle and importprivkey round-trip need
multiwallet (createwallet/loadwallet) and a regtest WIF helper, which are out of
scope here (Gridcoin has a single default BDB wallet).
"""

from test_framework.authproxy import JSONRPCException
from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal, assert_raises


class WalletBackupTest(GridcoinTestFramework):
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

        # --- backupwallet() takes no args and reports per-file success flags ---
        result = node.backupwallet()
        assert_equal(result["Backup wallet success"], True)
        self.log.info("backupwallet OK: %s", result)

        # --- dumpprivkey exports a WIF for a wallet address ---
        addr = node.getnewaddress()
        wif = node.dumpprivkey(addr)
        assert isinstance(wif, str) and len(wif) > 0, "dumpprivkey returned no WIF"
        self.log.info("dumpprivkey returned a WIF for %s", addr)

        # --- error path: dumpprivkey on a bogus address raises ---
        assert_raises(JSONRPCException, node.dumpprivkey, "not_a_real_address")
        self.log.info("dumpprivkey invalid-address error path OK")


if __name__ == "__main__":
    WalletBackupTest().main()
