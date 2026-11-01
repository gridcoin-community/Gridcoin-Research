#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""RPC message signing / address validation on regtest.

Investor-mode only, fully offline (no chain state, beacon, or spendable coins
needed): exercises the RPCHelpMan surface of signmessage / verifymessage /
validateaddress.

  - sign a message with a wallet address, verify the signature round-trips;
  - a tampered message fails verification;
  - the same signature checked against a different address fails;
  - validateaddress reports valid/mine for a wallet address and invalid for junk.
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal


class RpcSignMessageTest(GridcoinTestFramework):
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
        addr = node.getnewaddress()
        message = "Gridcoin functional-test message"

        # --- sign / verify round-trip ---
        sig = node.signmessage(addr, message)
        assert isinstance(sig, str) and sig, "empty signature"
        assert_equal(node.verifymessage(addr, sig, message), True)
        self.log.info("signmessage/verifymessage round-trip OK")

        # --- a tampered message must not verify ---
        assert_equal(node.verifymessage(addr, sig, message + " tampered"), False)

        # --- the signature must not verify against a different address ---
        other = node.getnewaddress()
        assert other != addr
        assert_equal(node.verifymessage(other, sig, message), False)
        self.log.info("verifymessage rejects tampered message and wrong address")

        # --- validateaddress: wallet address vs junk ---
        info = node.validateaddress(addr)
        assert info["isvalid"], info
        assert info.get("ismine", False), info
        bad = node.validateaddress("not-a-real-gridcoin-address")
        assert_equal(bad["isvalid"], False)
        self.log.info("validateaddress distinguishes valid wallet address from junk")


if __name__ == "__main__":
    RpcSignMessageTest().main()
