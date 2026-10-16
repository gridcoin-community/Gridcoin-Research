#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""A poll finishes at its first block past the end, not when the wall clock passes the end."""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal

STAKE_TIMESTAMP_MASK = 15
SPACING = 96
DAY = 86400
POLL_DAYS = 7
WEIGHT_BALANCE = 1
RESPONSE_SINGLE_CHOICE = 2
EXTRA_ARGS = ["-staking=0", "-connect=0", "-listen=0", "-debug=vote"]


class PollFinishByBlockTest(GridcoinTestFramework):
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

    def run_test(self):
        node = self.nodes[0]
        node.generatetoaddress(12, node.getnewaddress())
        self.grow_utxo_universe(node)

        created = node.mock_now()
        poll_id = node.addpoll("survey", "Finish", POLL_DAYS, "Which?", "A;B",
                               WEIGHT_BALANCE, RESPONSE_SINGLE_CHOICE, "https://gridcoin.us")["id"]
        self.stake()
        node.votebyid(poll_id, 0)
        self.stake()
        result = node.getpollresults(poll_id)
        assert_equal(result["poll_expired"], False)
        assert "ending_block_height" not in result
        assert poll_id in [p["id"] for p in node.listpolls()]
        assert_equal(node.getmininginfo()["current_poll"], "Finish")

        self.log.info("the wall clock passes the end and no block does: the poll is still open")
        node.advance_mocktime_to(created + POLL_DAYS * DAY + DAY)
        result = node.getpollresults(poll_id)
        assert_equal(result["poll_expired"], False)
        assert "ending_block_height" not in result
        assert poll_id in [p["id"] for p in node.listpolls()]
        assert_equal(node.getmininginfo()["current_poll"], "Finish")

        self.log.info("the first block past the end finishes it")
        self.stake()
        result = node.getpollresults(poll_id)
        assert_equal(result["poll_expired"], True)
        assert_equal(result["votes"], 1)
        assert poll_id not in [p["id"] for p in node.listpolls()]
        assert poll_id in [p["id"] for p in node.listpolls(True)]
        assert node.getmininginfo()["current_poll"] != "Finish"


if __name__ == "__main__":
    PollFinishByBlockTest().main()
