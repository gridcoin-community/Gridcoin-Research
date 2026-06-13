#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""RPC multisig address creation on regtest.

Investor-mode only, offline (no funding). Gridcoin has no createmultisig, so this
drives addmultisigaddress (which returns a wallet-owned P2SH address string):

  - a 2-of-2 from two wallet pubkeys yields a valid, wallet-owned P2SH address;
  - members may also be given as addresses (not just hex pubkeys).
"""

from test_framework.test_framework import GridcoinTestFramework


class RpcMultisigTest(GridcoinTestFramework):
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
        addr1 = node.getnewaddress()
        addr2 = node.getnewaddress()
        pub1 = node.validateaddress(addr1)["pubkey"]
        pub2 = node.validateaddress(addr2)["pubkey"]

        # --- 2-of-2 from hex pubkeys ---
        p2sh = node.addmultisigaddress(2, [pub1, pub2])
        assert isinstance(p2sh, str) and p2sh, "empty multisig address"
        info = node.validateaddress(p2sh)
        assert info["isvalid"], info
        assert info.get("ismine", False), info
        self.log.info("addmultisigaddress(2-of-2, pubkeys) -> %s", p2sh)

        # --- members may be addresses too ---
        p2sh2 = node.addmultisigaddress(1, [addr1])
        assert node.validateaddress(p2sh2)["isvalid"], p2sh2
        self.log.info("addmultisigaddress accepts addresses as members")


if __name__ == "__main__":
    RpcMultisigTest().main()
