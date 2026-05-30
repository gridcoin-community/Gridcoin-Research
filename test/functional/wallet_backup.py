#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Phase 4A wallet backup / key dump-import primitives on regtest.

Investor-mode only. Exercises the backup and per-key dump/import surface:

  - backupwallet(path): writes a copy of the BDB wallet file.
  - dumpprivkey(addr): exports a wallet key as WIF.
  - importprivkey(wif): re-imports the key (no-op round-trip; the address stays
    ours).
  - dumpprivkey error path on an invalid address.

A full backup -> wipe -> restore cycle needs multiwallet (createwallet /
loadwallet), which Gridcoin does not have (single default BDB wallet), so it is
out of scope here.
"""

import os

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_raises_rpc_error


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

        # --- backupwallet writes a non-empty copy of the wallet file ---
        backup_path = os.path.join(node.datadir, "wallet.backup")
        node.backupwallet(backup_path)
        assert os.path.isfile(backup_path), "backupwallet produced no file"
        size = os.path.getsize(backup_path)
        assert size > 0, "backup file is empty"
        self.log.info("backupwallet wrote %d bytes to %s", size, backup_path)

        # --- dumpprivkey exports a WIF; re-importing it is a no-op round-trip ---
        addr = node.getnewaddress()
        wif = node.dumpprivkey(addr)
        assert isinstance(wif, str) and len(wif) > 0, "dumpprivkey returned no WIF"

        # importprivkey of an already-owned key must not error; rescan=False to
        # avoid a full chain rescan. (If Gridcoin rejects re-import of a known
        # key, switch this to an externally generated key under WSL.)
        node.importprivkey(wif, "", False)
        info = node.validateaddress(addr)
        assert info["isvalid"] and info.get("ismine", True), info
        self.log.info("dumpprivkey/importprivkey round-trip OK for %s", addr)

        # --- error path: dumpprivkey on a bogus address raises (invalid addr) ---
        assert_raises_rpc_error(-5, None, node.dumpprivkey, "not_a_real_address")
        self.log.info("dumpprivkey invalid-address error path OK")


if __name__ == "__main__":
    WalletBackupTest().main()
