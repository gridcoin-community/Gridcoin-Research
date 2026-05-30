#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Phase 4A wallet key-dump primitives on regtest.

Investor-mode only. Exercises Gridcoin's private-key export surface:

  - dumpprivkey(addr): exports a wallet key as WIF;
  - dumpprivkey error path on an invalid address.

NOTE: backupwallet is intentionally not exercised here. On regtest the wallet is
an in-memory mock BDB (fMockDb), so there is no wallet.dat file to copy:
backupwallet returns "Backup wallet success": false and writes
"wallet.dat: No such file or directory" to stderr (which also trips the
framework's clean-shutdown stderr check). Backup coverage needs a file-backed
wallet. A full backup -> wipe -> restore cycle additionally needs multiwallet
(createwallet/loadwallet), which Gridcoin does not have.
"""

from test_framework.authproxy import JSONRPCException
from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_raises


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
