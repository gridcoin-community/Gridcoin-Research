#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""The time a beacon is stamped with from the block version 15 height.

From block version 15 a beacon carries the time of its block, and a beacon
contract is judged at that time.

node0 activates a beacon below the height, then advertises another at it. That
beacon is stamped with its block's time. A node that syncs the finished chain,
and a restart that rebuilds the beacon history, hold the same stamps.
"""

import os

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than

CPID = "00010203040506070809101112131415"
STAKE_TIMESTAMP_MASK = 15
V15_HEIGHT = 12


class BeaconV15StampTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [
            ["-staking=0", f"-forcecpid={CPID}", f"-blockv15height={V15_HEIGHT}"],
            ["-staking=0", f"-blockv15height={V15_HEIGHT}"],
        ]

    def setup_network(self):
        # See feature_contract_replay.py: a researcher node needs a BOINC data directory to advertise its beacon.
        boinc_dir = os.path.join(self.options.tmpdir, "boinc0")
        os.makedirs(boinc_dir, exist_ok=True)
        with open(os.path.join(boinc_dir, "client_state.xml"), "w", encoding="utf-8") as client_state:
            client_state.write("<client_state>\n</client_state>\n")
        self.extra_args[0].append(f"-boincdatadir={boinc_dir}")

        # node1 stays apart until it syncs the finished chain.
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def advance_to_slot(self, node, seconds=128):
        """Move the node's clock onto a later stake slot and return its time."""
        tip_time = node.getblock(node.getbestblockhash())["time"]
        slot = (tip_time + seconds) & ~STAKE_TIMESTAMP_MASK
        node.setmocktime(slot)

        return slot

    def mine(self, node, count=1):
        for _ in range(count):
            self.advance_to_slot(node)
            node.generatetoaddress(1, node.getnewaddress())

    def advertise_and_mine(self, node, force):
        """Advertise a beacon, and mine it in a block of a later stake slot.

        Returns the key, the time of the transaction and the time of the block.
        """
        sent_at = self.advance_to_slot(node)
        advertised = node.advertisebeacon(force)
        assert_equal(advertised["result"], "SUCCESS")

        mined_at = self.advance_to_slot(node, 256)
        assert_greater_than(mined_at, sent_at)
        node.generatetoaddress(1, node.getnewaddress())
        assert_equal(node.getrawmempool(), [])
        assert_equal(node.getblock(node.getbestblockhash())["time"], mined_at)

        return advertised["public_key"], sent_at, mined_at

    def stamps(self, node):
        """The stamp of each beacon the registry holds for the CPID, pending or not, by address."""
        beacons = [beacon for beacon in node.beaconreport() if beacon["cpid"] == CPID]
        pending = [beacon for beacon in node.pendingbeaconreport() if beacon["cpid"] == CPID]

        return {
            "beacons": {beacon["address"]: beacon["timestamp"] for beacon in beacons},
            "pending": {beacon["address"]: beacon["timestamp"] for beacon in pending},
        }

    def run_test(self):
        node = self.nodes[0]

        self.mine(node, 5)

        self.log.info("a superblock activates a beacon below the version 15 height")
        first_key, _, _ = self.advertise_and_mine(node, False)
        assert_greater_than(V15_HEIGHT, node.getblockcount())
        self.advance_to_slot(node)
        node.generatesuperblock({CPID: 100}, None, [first_key])
        assert_equal(self.stamps(node)["pending"], {})

        self.log.info("the chain reaches the last block below the version 15 height")
        self.mine(node, V15_HEIGHT - 1 - node.getblockcount())
        assert_equal(node.getblockcount(), V15_HEIGHT - 1)
        assert_equal(node.getblock(node.getbestblockhash())["version"], 14)

        self.log.info("from the version 15 height a beacon is stamped with the time of its block")
        second_key, second_sent, second_mined = self.advertise_and_mine(node, True)
        assert_equal(node.getblockcount(), V15_HEIGHT)
        assert_equal(node.getblock(node.getbestblockhash())["version"], 15)
        assert second_key != first_key
        assert_equal(list(self.stamps(node)["pending"].values()), [second_mined])

        self.mine(node, 3)
        expected = self.stamps(node)

        self.log.info("a node that syncs the chain holds the same stamps")
        # node1 takes node0's clock, which runs at the chain's block times: a peer whose time is far off is refused.
        self.nodes[1].setmocktime(self.advance_to_slot(node))
        self.connect_nodes(1, 0)
        self.sync_blocks()
        assert_equal(self.stamps(self.nodes[1]), expected)

        self.log.info("a restart that rebuilds the beacon history holds them too")
        self.restart_node(0, extra_args=self.extra_args[0] + ["-clearbeaconhistory"])
        assert_equal(self.stamps(self.nodes[0]), expected)


if __name__ == "__main__":
    BeaconV15StampTest().main()
