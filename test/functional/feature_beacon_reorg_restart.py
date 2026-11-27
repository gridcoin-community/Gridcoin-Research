#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Beacon registry state across a block disconnect and a restart, on regtest.

A node that restarts rebuilds its beacon registry from the beacon db. The test
checks the registry after a block disconnect, and again after a restart.
Two disconnects are exercised, each with the regtest reorganize RPC and a
restart before anything is mined again:

  * a block with a beacon deletion (revokebeacon): the revert restores the
    beacon, and the deletion record has to go with the block;
  * a superblock that expired a pending beacon: Deactivate brings the pending
    beacon back, and the expiration record has to go with the superblock.

On regtest the startup contract replay does not run (ReplayContracts returns
early below height 164618 outside testnet), so the state after a restart comes
from the beacon db alone.

The node runs with block version 15 active from height 1, so the block
version 15 revert rules apply.
"""

import os

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal

CPID = "00010203040506070809101112131415"

# See feature_beacon_activation.py.
STAKE_TIMESTAMP_MASK = 15

# PendingBeacon::RETENTION_AGE: a superblock this long after an advertisement
# expires the pending beacon.
PENDING_RETENTION = 60 * 60 * 24 * 3

# GetSuperblockAgeSpacing on regtest: a superblock needs this long since the last one.
SUPERBLOCK_SPACING = 43200


class BeaconReorgRestartTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [[
            "-staking=0",
            "-connect=0",
            "-listen=0",
            "-blockv15height=1",
            f"-forcecpid={CPID}",
        ]]

    def setup_network(self):
        # See feature_beacon_activation.py.
        boinc_dir = os.path.join(self.options.tmpdir, "boinc")
        os.makedirs(boinc_dir, exist_ok=True)
        client_state_path = os.path.join(boinc_dir, "client_state.xml")
        with open(client_state_path, "w", encoding="utf-8") as client_state:
            client_state.write("<client_state>\n</client_state>\n")
        self.extra_args[0].append(f"-boincdatadir={boinc_dir}")

        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def advance_to_slot(self, node, seconds=128):
        """Move mocktime forward onto a stake-timestamp boundary past the tip."""
        tip_time = node.getblock(node.getbestblockhash())["time"]
        slot = (tip_time + seconds) & ~STAKE_TIMESTAMP_MASK
        node.setmocktime(slot)
        return slot

    def beacon_keys(self, node):
        """Return the (active, pending) public keys for our CPID."""
        status = node.beaconstatus(CPID)
        return ([a["public_key"] for a in status["active"]],
                sorted(p["public_key"] for p in status["pending"]))

    def mine_one(self, node):
        assert_equal(len(node.getrawmempool()), 1)
        node.generatetoaddress(1, node.getnewaddress())
        assert_equal(node.getrawmempool(), [])

    def run_test(self):
        node = self.nodes[0]

        node.generatetoaddress(5, node.getnewaddress())

        self.log.info("advertise a beacon and activate it with a superblock")
        self.advance_to_slot(node)
        key_1 = node.advertisebeacon()["public_key"]
        self.mine_one(node)
        self.advance_to_slot(node)
        node.generatesuperblock({CPID: 100}, None, [key_1])
        assert_equal(self.beacon_keys(node), ([key_1], []))

        self.log.info("a block deleting the beacon, disconnected, then a restart")
        before_deletion = node.getbestblockhash()
        self.advance_to_slot(node)
        node.revokebeacon()
        self.mine_one(node)
        assert_equal(self.beacon_keys(node), ([], []))

        assert_equal(node.reorganize(before_deletion)["RollbackChain"], True)
        assert_equal(self.beacon_keys(node), ([key_1], []))

        self.restart_node(0)
        assert_equal(self.beacon_keys(node), ([key_1], []))

        self.log.info("a superblock expiring a pending beacon, disconnected, then a restart")
        self.advance_to_slot(node)
        key_2 = node.advertisebeacon(True)["public_key"]
        assert key_2 != key_1
        # The deletion restored by the reorganization may be back in the
        # mempool; this block carries it too, so drop it first.
        node.generatetoaddress(1, node.getnewaddress())
        active, pending = self.beacon_keys(node)
        assert key_2 in pending

        before_expiry = node.getbestblockhash()
        self.advance_to_slot(node, PENDING_RETENTION + 600)
        node.generatesuperblock({CPID: 100})
        assert key_2 not in self.beacon_keys(node)[1]

        assert_equal(node.reorganize(before_expiry)["RollbackChain"], True)
        assert key_2 in self.beacon_keys(node)[1]

        self.restart_node(0)
        assert key_2 in self.beacon_keys(node)[1]

        self.log.info("a superblock replacing an active beacon, disconnected, then a restart")
        self.advance_to_slot(node, SUPERBLOCK_SPACING + 600)
        node.generatesuperblock({CPID: 100}, None, [key_2])
        active, _ = self.beacon_keys(node)
        assert_equal(active, [key_2])
        before_replacement = node.getblock(node.getbestblockhash())["previousblockhash"]

        assert_equal(node.reorganize(before_replacement)["RollbackChain"], True)
        assert_equal(self.beacon_keys(node)[0], [key_1])

        self.restart_node(0)
        assert_equal(self.beacon_keys(node)[0], [key_1])


if __name__ == "__main__":
    BeaconReorgRestartTest().main()
