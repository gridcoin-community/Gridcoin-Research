#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""From v15 a poll starts at its block and ends at its closing block, the block before its first block past the end.

A cleared poll history (-clearpollhistory) is not exercised here: on regtest the startup contract replay never runs,
so nothing would rebuild a cleared registry (see feature_poll_registry.py).
"""

import time

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal

STAKE_TIMESTAMP_MASK = 15
SPACING = 96
DAY = 86400
POLL_DAYS = 7
WEIGHT_BALANCE = 1
RESPONSE_SINGLE_CHOICE = 2
EXTRA_ARGS = ["-staking=0", "-connect=0", "-listen=0", "-debug=vote", "-blockv15height=1"]


def hr_date(timestamp):
    """A timestamp in TimestampToHRDate's format."""
    return time.strftime("%m-%d-%Y %H:%M:%S", time.gmtime(timestamp))


class PollV15WindowTest(GridcoinTestFramework):
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

    def listed(self, poll_id):
        return [p for p in self.nodes[0].listpolls(True) if p["id"] == poll_id][0]

    def run_test(self):
        node = self.nodes[0]
        node.generatetoaddress(12, node.getnewaddress())
        # A poll's claim counts only outputs 10 blocks deep, and the coinstakes keep restaking the node's ten large
        # outputs. One fan of the three largest (each 100,000 GRC or more: a coinstake returns what it spends plus
        # the reward) into 144, buried by the next 11 blocks, keeps at least 133 of them, about 277,000 GRC,
        # claimable however the kernels fall (a coinstake spends one output).
        assert self.fan_largest_outputs(node, sources=3, count=144) > 2000
        for _ in range(11):
            self.stake()

        created = node.mock_now()
        poll_id = node.addpoll("survey", "Window", POLL_DAYS, "Which?", "A;B",
                               WEIGHT_BALANCE, RESPONSE_SINGLE_CHOICE, "https://gridcoin.us")["id"]
        self.stake()
        poll_time = node.getblock(node.getbestblockhash())["time"]
        assert poll_time > created

        self.log.info("the poll starts at its block")
        entry = self.listed(poll_id)
        assert_equal(entry["timestamp"], hr_date(poll_time))
        assert_equal(entry["expiration"], hr_date(poll_time + POLL_DAYS * DAY))

        node.votebyid(poll_id, 0)
        self.stake()

        self.log.info("a block past the end counted from the poll's transaction, not from its block: still open")
        node.advance_mocktime_to(poll_time + POLL_DAYS * DAY - SPACING)
        self.stake()
        assert_equal(node.getblock(node.getbestblockhash())["time"], poll_time + POLL_DAYS * DAY)
        result = node.getpollresults(poll_id)
        assert_equal(result["poll_expired"], False)
        assert "ending_block_height" not in result

        self.log.info("the first block past the end closes it, and its range ends at the closing block before it")
        self.stake()
        result = node.getpollresults(poll_id)
        assert_equal(result["poll_expired"], True)
        assert_equal(result["ending_block_height"], node.getblockcount() - 1)
        assert_equal(result["votes"], 1)

        self.log.info("a restart gives the same result")
        self.restart_node(0, extra_args=list(EXTRA_ARGS))
        assert_equal(node.getpollresults(poll_id), result)


if __name__ == "__main__":
    PollV15WindowTest().main()
