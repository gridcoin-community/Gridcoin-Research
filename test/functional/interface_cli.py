#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""End-to-end test of the CLI client argument path on regtest.

The functional framework normally drives the node through authproxy.py (typed
JSON-RPC over HTTP), so it never exercises the CLI client's string->JSON
parameter conversion (RPCConvertValues / vRPCConvertParams in
src/rpc/client.cpp). A missing conversion-table entry breaks a command from the
shell while every other functional test still passes.

Gridcoin has no separate gridcoin-cli binary: gridcoinresearchd, given a
non-switch argv, acts as the RPC client (CommandLineRPC). This test spawns that
client mode (via the framework's node.cli wrapper) for a representative sample
of commands whose arguments are NOT plain strings -- int, bool, JSON array,
JSON object -- plus the deliberately dual-mode `logging <category>` plain-string
form, and asserts each call round-trips to the same value the typed RPC path
returns. It is the end-to-end complement to the static check-rpc-mappings unit
test, which cross-checks the conversion table against the RPCHelpMan arg types.
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal


class InterfaceCLITest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0", "-connect=0", "-listen=0"]]

    def skip_test_if_missing_module(self):
        self.skip_if_no_cli()
        self.skip_if_no_wallet()

    def setup_network(self):
        # Single isolated node; nothing to connect or sync.
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def run_test(self):
        node = self.nodes[0]
        cli = node.cli

        # Establish a short chain so the lookups below have real data.
        node.generatetoaddress(3, node.getnewaddress())
        height = node.getblockcount()

        self.log.info("no-argument command agrees over CLI and RPC")
        assert_equal(cli.getblockcount(), height)

        # int argument: getblockhash <height>. The client converts "3" -> 3
        # (vRPCConvertParams { "getblockhash", 0 }); without the entry the server
        # would receive a string and reject it.
        self.log.info("int argument round-trips: getblockhash")
        assert_equal(cli.getblockhash(height), node.getblockhash(height))

        # bool argument: getblock <hash> <txinfo>. Param 1 is converted string->bool
        # ({ "getblock", 1 } in the table); a missing entry would send "True" and
        # the server's get_bool() would reject it.
        self.log.info("bool argument round-trips: getblock txinfo")
        best = node.getbestblockhash()
        assert_equal(cli.getblock(best, True), node.getblock(best, True))

        # array + object arguments: createrawtransaction <inputs> <outputs>.
        # Param 0 (JSON array) and param 1 (JSON object) are both string->JSON
        # converted by the client. No funded utxo is needed -- the command only
        # serializes the values it is handed -- so a dummy input keeps the test
        # deterministic.
        self.log.info("array + object arguments round-trip: createrawtransaction")
        inputs = [{"txid": "00" * 32, "vout": 0}]
        outputs = {node.getnewaddress(): 1.0}
        # createrawtransaction stamps the transaction's nTime from the (adjusted)
        # clock, so two independent calls that straddle a 1-second boundary
        # serialize different hex. Pin the clock with setmocktime so the CLI and
        # RPC results are byte-identical, then restore the system clock.
        node.setmocktime(1593912000)
        try:
            assert_equal(cli.createrawtransaction(inputs, outputs),
                         node.createrawtransaction(inputs, outputs))
        finally:
            node.setmocktime(0)

        # int + bool arguments: getbalance "*" <minconf> <include_watchonly>.
        # Param 1 string->int, param 2 string->bool.
        self.log.info("int + bool arguments round-trip: getbalance")
        assert_equal(cli.getbalance("*", 0, True), node.getbalance("*", 0, True))

        # logging dual-mode: `logging` is deliberately absent from the conversion
        # table, so the client passes a bare category name through as a string.
        # EnableOrDisableLogCategories accepts that non-array form (see the
        # comment in src/rpc/misc.cpp); the JSON-array form only works over CURL.
        self.log.info("logging <category> round-trips as a plain string")
        baseline = cli.logging()
        assert isinstance(baseline, dict) and "net" in baseline, baseline
        enabled = cli.logging("net")
        assert_equal(enabled["net"], True)
        disabled = cli.logging("", "net")
        assert_equal(disabled["net"], False)


if __name__ == "__main__":
    InterfaceCLITest().main()
