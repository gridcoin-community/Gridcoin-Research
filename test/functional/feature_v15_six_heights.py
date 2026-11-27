#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""All six v15 heights at one block, the first superblocks past it, and a reorg back across it.

On mainnet the v15 block version, poll multi-address claims, the AutoGreylist deep copy, total-credit fix and
redesign, and the MESSAGE contract cutoff all activate at one height. Each has tests of its own; this one sets all six
to one height V and crosses it both ways.

node0 is a researcher and mines every block; node1 follows. node0 advertises its beacon before V, and a superblock
before V leaves it pending. The first superblock past V activates it, and a second one follows. node1's registries at
V - 1 are recorded. Then node1 alone rolls back to V - 1, disconnecting both superblocks past V in one reorg: every
registry must be as it was at V - 1, the beacon pending again. node1 crosses V forward again when node0 extends the
chain, and must then match node0, including after a restart.
"""

import os

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal

CPID = "00010203040506070809101112131415"
STAKE_TIMESTAMP_MASK = 15
# GetSuperblockAgeSpacing on regtest: a superblock needs this long since the last one.
SUPERBLOCK_SPACING = 43200
V15_HEIGHT = 30
HEIGHT_ARGS = [
    "blockv15height",
    "pollmultiaddressheight",
    "autogreylistdeepcopyheight",
    "autogreylisttotalcreditfixheight",
    "autogreylistredesignheight",
    "messagecontractdisableheight",
]
# The startup lines that name the heights (PollMultiAddressHeight has none).
STARTUP_LINES = [
    f"Block version 15 hard fork configured for block {V15_HEIGHT}",
    f"MESSAGE contracts disabled from block {V15_HEIGHT}",
    f"AutoGreylist deep-copy configured for block {V15_HEIGHT}",
    f"AutoGreylist total-credit fix configured for block {V15_HEIGHT}",
    f"AutoGreylist redesign configured for block {V15_HEIGHT}",
]
# Registries compared across the reorg. The superblock is compared by height and hash, not age.
REGISTRY_RPCS = [
    ("beaconreport",),
    ("pendingbeaconreport",),
    ("listprojects", True),
    ("listprotocolentries",),
    ("listmandatorysidestakes",),
    ("listpools",),
    ("getautogreylist",),
]


class V15SixHeightsTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.chain = "regtest"
        self.setup_clean_chain = True
        heights = [f"-{arg}={V15_HEIGHT}" for arg in HEIGHT_ARGS]
        self.extra_args = [
            ["-staking=0", f"-forcecpid={CPID}"] + heights,
            ["-staking=0"] + heights,
        ]

    def setup_network(self):
        # See feature_contract_replay.py: a researcher node needs a BOINC data directory to advertise its beacon.
        boinc_dir = os.path.join(self.options.tmpdir, "boinc0")
        os.makedirs(boinc_dir, exist_ok=True)
        with open(os.path.join(boinc_dir, "client_state.xml"), "w", encoding="utf-8") as client_state:
            client_state.write("<client_state>\n</client_state>\n")
        self.extra_args[0].append(f"-boincdatadir={boinc_dir}")

        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()
        self.connect_nodes(1, 0)

    def advance(self, seconds=128):
        """Move every node's clock to the next stake slot at least `seconds` past node0's tip."""
        tip_time = self.nodes[0].getblock(self.nodes[0].getbestblockhash())["time"]
        slot = (tip_time + seconds) & ~STAKE_TIMESTAMP_MASK
        for node in self.nodes:
            node.setmocktime(slot)

    def mine(self, count=1):
        node0 = self.nodes[0]
        for _ in range(count):
            self.advance()
            node0.generatetoaddress(1, node0.getnewaddress())

    def mine_to(self, height):
        self.mine(height - self.nodes[0].getblockcount())
        assert_equal(self.nodes[0].getblockcount(), height)

    def superblock(self, verified_keys=None):
        """Stage a superblock on node0 and mine the block that carries it; return its height."""
        node0 = self.nodes[0]
        self.advance(SUPERBLOCK_SPACING + 600)
        node0.generatesuperblock({CPID: 100}, None, verified_keys)
        return node0.getblockcount()

    def registries(self, node):
        state = {rpc[0]: getattr(node, rpc[0])(*rpc[1:]) for rpc in REGISTRY_RPCS}
        age = node.superblockage()
        state["superblock"] = (age["Superblock Block Number"], age["Superblock Timestamp"])
        return state

    def assert_registries(self, actual, expected):
        for key in expected:
            assert_equal((key, actual[key]), (key, expected[key]))

    def run_test(self):
        node0, node1 = self.nodes

        self.log.info("Both nodes name the one height for every gate that logs one")
        for node in self.nodes:
            with open(node.debug_log_path, encoding="utf-8") as log:
                text = log.read()
            for line in STARTUP_LINES:
                assert line in text, f"node{node.index}: {line}"

        self.log.info("Before V: the beacon is advertised, and a superblock leaves it pending")
        self.mine(5)
        self.advance()
        advertised = node0.advertisebeacon()
        assert_equal(advertised["result"], "SUCCESS")
        key = advertised["public_key"]
        self.mine()
        below = self.superblock()
        assert below < V15_HEIGHT - 1, below
        assert_equal(node0.beaconstatus(CPID)["active"], [])

        self.mine_to(V15_HEIGHT - 1)
        self.sync_blocks()
        last_below = node0.getbestblockhash()
        before_v = self.registries(node1)
        assert_equal(before_v["superblock"][0], below)
        assert_equal([b["cpid"] for b in before_v["pendingbeaconreport"]], [CPID])

        self.log.info("Across V: the first superblock past it activates the beacon, then a second follows")
        self.mine()
        assert_equal(node0.getblock(last_below)["version"], 14)
        assert_equal(node0.getblock(node0.getbestblockhash())["version"], 15)
        first_above = self.superblock([key])
        assert first_above > V15_HEIGHT - 1
        assert_equal([b["public_key"] for b in node0.beaconstatus(CPID)["active"]], [key])
        self.mine(2)
        second_above = self.superblock()
        self.mine(2)
        self.sync_blocks()
        after_v = self.registries(node0)
        assert_equal(after_v["superblock"][0], second_above)
        assert_equal(after_v["pendingbeaconreport"], [])
        self.assert_registries(self.registries(node1), after_v)

        self.log.info("node1 alone rolls back to V - 1: every registry is as it was there")
        # The rollback disconnects both superblocks past V in one reorg, which from v15 rebuilds the beacon registry.
        with node1.assert_debug_log(["RebuildBeaconRegistry: starting in-line beacon registry rebuild"]):
            assert_equal(node1.reorganize(last_below)["RollbackChain"], True)
        assert_equal(node1.getbestblockhash(), last_below)
        self.assert_registries(self.registries(node1), before_v)

        self.log.info("node1 crosses V forward again when node0 extends the chain")
        # node1 still holds node0's blocks past V, so it returns to that chain only when a new block extends it.
        self.mine()
        self.sync_blocks()
        after_v = self.registries(node0)
        self.assert_registries(self.registries(node1), after_v)

        self.log.info("And after a restart")
        self.restart_node(1)
        self.assert_registries(self.registries(self.nodes[1]), after_v)


if __name__ == "__main__":
    V15SixHeightsTest().main()
