#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""RPC HTLC (Hash Time-Locked Contract) construction on regtest.

Investor-mode only, no funding required. HTLC is Gridcoin-specific and has had
no functional coverage. Builds an unfunded contract and proves the redeem script
encodes the contract terms:

  - createhtlc returns a P2SH address + redeem script for a (receiver, sender,
    hash, timeout) tuple, echoing the hash/timeout back;
  - the P2SH address validates;
  - decodescript on the redeem script exposes Gridcoin's "htlc" detail object,
    whose hash/timeout/pubkeys match what createhtlc produced.

claimhtlc/refundhtlc are not exercised -- they require a funded, confirmed HTLC
output (and, for refund, an elapsed timeout), neither testable on this stack.
"""

import hashlib

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal


class RpcHtlcTest(GridcoinTestFramework):
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
        receiver = node.getnewaddress()
        sender = node.getnewaddress()
        preimage = bytes(range(32))
        digest = hashlib.sha256(preimage).hexdigest()
        timeout = node.getblockcount() + 100

        # --- createhtlc (unfunded) ---
        htlc = node.createhtlc(receiver, sender, digest, timeout)
        assert htlc.get("p2sh_address"), htlc
        assert htlc.get("redeem_script"), htlc
        assert_equal(htlc["hash"], digest)
        assert_equal(htlc["timeout"], timeout)
        self.log.info("createhtlc produced P2SH address %s", htlc["p2sh_address"])

        # --- the P2SH address is a valid Gridcoin address ---
        assert node.validateaddress(htlc["p2sh_address"])["isvalid"], htlc["p2sh_address"]

        # --- decodescript exposes the HTLC terms baked into the redeem script ---
        decoded = node.decodescript(htlc["redeem_script"])
        assert "htlc" in decoded, decoded
        terms = decoded["htlc"]
        assert_equal(terms["hash"], digest)
        assert_equal(terms["timeout"], timeout)
        assert_equal(terms["receiver_pubkey"], htlc["receiver_pubkey"])
        assert_equal(terms["sender_pubkey"], htlc["sender_pubkey"])
        self.log.info("decodescript HTLC terms match createhtlc output")


if __name__ == "__main__":
    RpcHtlcTest().main()
