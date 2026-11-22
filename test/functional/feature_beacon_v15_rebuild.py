#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""The beacon registry is rebuilt as the last block below block version 15 connects.

Connecting that block replays the beacon contracts from block version 11, so
the first block version 15 block is validated on the rebuilt registry. The
rebuild runs once for that block: not for the blocks around it, the first
block version 15 block among them, again only when that block is disconnected
and a block at its height connects, and on a node that syncs through the
height. A restart keeps the rebuilt history.
"""

import os

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal

CPID = "00010203040506070809101112131415"
STAKE_TIMESTAMP_MASK = 15
V15_HEIGHT = 20
REBUILD = "beacon registry rebuild at the last block below block version 15"


class BeaconV15RebuildTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [
            ["-staking=0", f"-forcecpid={CPID}", f"-blockv15height={V15_HEIGHT}", "-debug=contract"],
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
        tip_time = node.getblock(node.getbestblockhash())["time"]
        slot = (tip_time + seconds) & ~STAKE_TIMESTAMP_MASK
        node.setmocktime(slot)

    def mine(self, node, count=1):
        for _ in range(count):
            self.advance_to_slot(node)
            node.generatetoaddress(1, node.getnewaddress())

    def registry(self, node):
        """The beacon registry as the RPCs report it, in a fixed order."""
        return {
            "beacons": sorted(node.beaconreport(), key=lambda beacon: beacon["cpid"]),
            "pending": sorted(node.pendingbeaconreport(), key=lambda beacon: beacon["cpid"]),
        }

    def run_test(self):
        node = self.nodes[0]

        self.log.info("activate a beacon below the version 15 height")
        self.mine(node, 5)
        advertised = node.advertisebeacon()
        assert_equal(advertised["result"], "SUCCESS")
        self.mine(node)
        self.advance_to_slot(node)
        node.generatesuperblock({CPID: 100}, None, [advertised["public_key"]])
        assert_equal([beacon["public_key"] for beacon in node.beaconstatus(CPID)["active"]],
                     [advertised["public_key"]])

        self.log.info("no rebuild before the last block below the version 15 height")
        with node.assert_debug_log([], unexpected_msgs=[REBUILD]):
            self.mine(node, V15_HEIGHT - 2 - node.getblockcount())
        assert_equal(node.getblockcount(), V15_HEIGHT - 2)
        before = self.registry(node)
        status_before = node.beaconstatus(CPID)

        self.log.info("the last block below the height rebuilds the registry, which holds the same beacons")
        with node.assert_debug_log([REBUILD]):
            self.mine(node)
        assert_equal(node.getblockcount(), V15_HEIGHT - 1)
        assert_equal(node.getblock(node.getbestblockhash())["version"], 14)
        assert_equal(self.registry(node), before)
        assert_equal(node.beaconstatus(CPID), status_before)

        self.log.info("the first version 15 block and the blocks after it do not")
        with node.assert_debug_log([], unexpected_msgs=[REBUILD]):
            self.mine(node, 4)
        assert_equal(node.getblock(node.getblockhash(V15_HEIGHT))["version"], 15)

        self.log.info("a first version 15 block that connects again does not either")
        assert_equal(node.reorganize(node.getblockhash(V15_HEIGHT - 1))["RollbackChain"], True)
        assert_equal(node.getblockcount(), V15_HEIGHT - 1)
        with node.assert_debug_log([], unexpected_msgs=[REBUILD]):
            self.mine(node)
        assert_equal(self.registry(node), before)

        self.log.info("a block connecting again at the last height below it rebuilds again")
        assert_equal(node.reorganize(node.getblockhash(V15_HEIGHT - 2))["RollbackChain"], True)
        assert_equal(node.getblockcount(), V15_HEIGHT - 2)
        with node.assert_debug_log([REBUILD]):
            self.mine(node)
        assert_equal(self.registry(node), before)
        self.mine(node, 4)

        self.log.info("a node that syncs through the version 15 height rebuilds, and holds what the first node holds")
        # node1 takes node0's clock, which runs at the chain's block times: a peer whose time is far off is refused.
        self.advance_to_slot(node)
        self.nodes[1].setmocktime(node.getblock(node.getbestblockhash())["time"] + 128 & ~STAKE_TIMESTAMP_MASK)
        with self.nodes[1].assert_debug_log([REBUILD]):
            self.connect_nodes(1, 0)
            self.sync_blocks()
        assert_equal(self.registry(self.nodes[1]), self.registry(node))

        self.log.info("a restart keeps the rebuilt history")
        expected = self.registry(node)
        self.restart_node(0)
        assert_equal(self.registry(self.nodes[0]), expected)


if __name__ == "__main__":
    BeaconV15RebuildTest().main()
