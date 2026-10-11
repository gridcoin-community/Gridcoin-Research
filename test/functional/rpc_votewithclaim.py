#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""votewithclaim builds a vote whose claim lists exactly the outputs given.

Regtest-only; the tally's functional tests use it to choose a vote's claim.
"""

from decimal import Decimal

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than, assert_raises_rpc_error


class VoteWithClaimTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0", "-connect=0", "-listen=0", "-blockv15height=1"]] * 2

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def run_test(self):
        node = self.nodes[0]
        node.generatetoaddress(12, node.getnewaddress())
        # The premine is a few large outputs and each mined block churns one;
        # fan one out so the few blocks below always find a mature stake kernel.
        self.grow_utxo_universe(node)

        utxo, amount = self.newest_coinstake_utxo(node)
        addr = node.getnewaddress()
        raw = node.createrawtransaction([{"txid": utxo["txid"], "vout": utxo["vout"]}], {addr: amount})
        node.sendrawtransaction(node.signrawtransactionwithwallet(raw)["hex"])
        node.generatetoaddress(1, node.getnewaddress())
        coin = [u for u in node.listunspent(0) if u["address"] == addr][0]

        poll = node.addpoll("survey", "explicit claim", 7, "Which?", "A;B", 1, 2, "https://gridcoin.us")
        node.generatetoaddress(1, node.getnewaddress())

        outpoint = {"txid": coin["txid"], "vout": coin["vout"]}
        vote = node.votewithclaim(poll["id"], [1], [outpoint])
        claim = node.getvotingclaim(vote["vote_txid"])
        listed = [(o["txid"], o["offset"]) for a in claim["balance_claim"] for o in a["outpoints"]]
        assert_equal(listed, [(coin["txid"], coin["vout"])])

        # An output of the wallet's own send to someone else is in mapWallet but
        # pays an address the wallet holds no key for: refuse it, never drop it.
        # (node1 is only a source of a foreign address; it is never connected.)
        foreign = self.nodes[1].getnewaddress()
        spare = [u for u in node.listunspent(1) if u["txid"] != coin["txid"]]
        pay = spare[0]
        raw = node.createrawtransaction([{"txid": pay["txid"], "vout": pay["vout"]}],
                                        {foreign: pay["amount"] - Decimal("1.0")})
        sent = node.sendrawtransaction(node.signrawtransactionwithwallet(raw)["hex"])
        foreign_out = {"txid": sent, "vout": 0}
        tx_out = node.getrawtransaction(sent, True)["vout"][0]
        assert_equal(tx_out["scriptPubKey"]["addresses"], [foreign])
        assert_raises_rpc_error(-8, "holds no key", node.votewithclaim,
                                poll["id"], [0], [outpoint, foreign_out])

        # A fee outpoint that does not parse is a parameter error.
        for bad in ["zz:0", "nocolon", coin["txid"] + ":x"]:
            assert_raises_rpc_error(-8, "fee_outpoint", node.votewithclaim,
                                    poll["id"], [0], [outpoint], bad)

        # A pinned fee outpoint is the vote transaction's only input. The
        # smallest coin that can pay is pinned, and the wallet holds a larger
        # one.
        eligible = sorted((u for u in node.listunspent(1)
                           if u["txid"] not in (coin["txid"], sent) and u["amount"] > 2),
                          key=lambda u: u["amount"])
        fee_in = eligible[0]
        assert_greater_than(max(u["amount"] for u in node.listunspent(0)), fee_in["amount"])
        pinned = node.votewithclaim(poll["id"], [0], [outpoint],
                                    "%s:%d" % (fee_in["txid"], fee_in["vout"]))
        vins = [(i["txid"], i["vout"]) for i in node.getrawtransaction(pinned["vote_txid"], True)["vin"]]
        assert_equal(vins, [(fee_in["txid"], fee_in["vout"])])

        # A negative output number and a choice id outside 0..255 are parameter
        # errors.
        assert_raises_rpc_error(-8, "vout must be non-negative", node.votewithclaim,
                                poll["id"], [0], [{"txid": coin["txid"], "vout": -1}])
        for choice in (256, -1):
            assert_raises_rpc_error(-8, "out of range", node.votewithclaim,
                                    poll["id"], [choice], [outpoint])

        assert_raises_rpc_error(-8, "not a wallet output", node.votewithclaim,
                                poll["id"], [0], [{"txid": "00" * 32, "vout": 0}])


if __name__ == "__main__":
    VoteWithClaimTest().main()
