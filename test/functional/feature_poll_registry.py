#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""The poll registry keeps polls and votes across a reorg and a restart.

A cleared poll history (-clearpollhistory) is not exercised here: on regtest the
startup contract replay never runs (ReplayContracts returns early below height
164618 outside testnet), so nothing would rebuild a cleared registry. The unit
tests cover clearing.
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal

STAKE_TIMESTAMP_MASK = 15
SPACING = 96
POLL_DAYS = 7
WEIGHT_BALANCE = 1
RESPONSE_SINGLE_CHOICE = 2
EXTRA_ARGS = ["-staking=0", "-connect=0", "-listen=0", "-debug=vote"]


class PollRegistryTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [list(EXTRA_ARGS)]

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def stake(self):
        node = self.nodes[0]
        node.advance_mocktime_to((node.mock_now() + SPACING) & ~STAKE_TIMESTAMP_MASK)
        node.generatetoaddress(1, node.getnewaddress())

    def votes(self, poll_id):
        listed = [p for p in self.nodes[0].listpolls(True) if p["id"] == poll_id]
        assert_equal(len(listed), 1)
        counted = self.nodes[0].getpollresults(poll_id)["votes"]
        assert_equal(listed[0]["votes"], counted)
        return counted

    def run_test(self):
        node = self.nodes[0]
        node.generatetoaddress(12, node.getnewaddress())
        self.grow_utxo_universe(node)

        self.log.info("a poll and two votes, each in its own block")
        poll_id = node.addpoll("survey", "Registry", POLL_DAYS, "Which?", "A;B",
                               WEIGHT_BALANCE, RESPONSE_SINGLE_CHOICE, "https://gridcoin.us")["id"]
        self.stake()
        node.votebyid(poll_id, 0)
        self.stake()
        before_second = node.getbestblockhash()
        node.votebyid(poll_id, 1)
        self.stake()
        assert_equal(self.votes(poll_id), 2)

        self.log.info("disconnecting the second vote's block unlinks it; mining it again links it again")
        assert_equal(node.reorganize(before_second)["RollbackChain"], True)
        assert_equal(self.votes(poll_id), 1)
        self.stake()
        assert_equal(self.votes(poll_id), 2)

        self.log.info("a restart loads the same registry")
        self.restart_node(0, extra_args=list(EXTRA_ARGS))
        assert_equal(self.votes(poll_id), 2)


if __name__ == "__main__":
    PollRegistryTest().main()
