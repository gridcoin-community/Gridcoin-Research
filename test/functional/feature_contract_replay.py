#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Beacon contract state across a reorganization, on regtest.

The beacon registry is driven by two kinds of block event: a beacon
advertisement contract (ADD) carried by an ordinary block, and a superblock
whose verified-beacon list activates a pending beacon. Both have inverses that
BlockDisconnect runs (BeaconRegistry::Revert for the contract,
BeaconRegistry::Deactivate for the superblock), and the disconnected
advertisement is resurrected into the mempool like any other transaction.

The regtest `reorganize` RPC rolls the chain back to a given hash, so the
sequence advertise -> activate -> roll back one block -> roll back another ->
mine again -> activate again walks every one of those transitions:

  * rolling back the superblock returns the beacon to PENDING;
  * rolling back the advertisement removes it entirely;
  * the identical advertisement transaction is accepted again, the next block
    replays it (same public key), and a new superblock activates it again.

Each step asserts the state that only its inverse (or its replay) produces, so
a missing Revert, a missing Deactivate, or a registry that refuses the replayed
contract each fails a different line.

Whether the disconnected advertisement comes back into the mempool on its own
is not asserted either way: DisconnectBlocksBatch resurrects it once its
re-acceptance runs after the disconnect batch commits, and before that fix
the mempool is empty here. The replay resubmits the transaction's own bytes
only when it has to; either way the identical transaction is what the next
block carries.
"""

import os

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal

CPID = "00010203040506070809101112131415"

# See feature_beacon_activation.py.
STAKE_TIMESTAMP_MASK = 15


class ContractReplayTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [[
            "-staking=0",
            "-devbuild=override",
            "-connect=0",
            "-listen=0",
            f"-forcecpid={CPID}",
        ]]

    def setup_network(self):
        boinc_dir = os.path.join(self.options.tmpdir, "boinc")
        os.makedirs(boinc_dir, exist_ok=True)
        client_state_path = os.path.join(boinc_dir, "client_state.xml")
        with open(client_state_path, "w", encoding="utf-8") as client_state:
            client_state.write("<client_state>\n</client_state>\n")
        self.extra_args[0].append(f"-boincdatadir={boinc_dir}")

        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def advance_to_slot(self, node, seconds=128):
        """Move mocktime forward onto a stake-timestamp boundary past the tip.

        After a roll-back the tip is older than the clock, so the slot is
        taken from the tip rather than from the clock, and far enough ahead
        that a resurrected transaction built at an earlier slot is not newer
        than the block that carries it back.
        """
        tip_time = node.getblock(node.getbestblockhash())["time"]
        slot = (tip_time + seconds) & ~STAKE_TIMESTAMP_MASK
        node.setmocktime(slot)
        return slot

    def beacon_status(self, node):
        status = node.beaconstatus(CPID)
        return status["active"], status["pending"]

    def run_test(self):
        node = self.nodes[0]

        node.generatetoaddress(5, node.getnewaddress())
        base_hash = node.getbestblockhash()
        base_height = node.getblockcount()

        self.log.info("advertise the beacon; a block carries it (pending)")
        self.advance_to_slot(node)
        advertised = node.advertisebeacon()
        assert_equal(advertised["result"], "SUCCESS")
        public_key = advertised["public_key"]
        assert_equal(len(node.getrawmempool()), 1)
        advert_txid = node.getrawmempool()[0]
        advert_hex = node.getrawtransaction(advert_txid)
        node.generatetoaddress(1, node.getnewaddress())
        advert_hash = node.getbestblockhash()
        active, pending = self.beacon_status(node)
        assert_equal(active, [])
        assert_equal([p["public_key"] for p in pending], [public_key])

        self.log.info("a superblock naming the beacon activates it")
        self.advance_to_slot(node)
        node.generatesuperblock({CPID: 100}, None, [public_key])
        assert_equal(node.getblockcount(), base_height + 2)
        active, pending = self.beacon_status(node)
        assert_equal(pending, [])
        assert_equal([a["public_key"] for a in active], [public_key])

        self.log.info("roll back the superblock: the beacon is pending again")
        assert_equal(node.reorganize(advert_hash)["RollbackChain"], True)
        assert_equal(node.getbestblockhash(), advert_hash)
        active, pending = self.beacon_status(node)
        assert_equal(active, [])
        assert_equal([p["public_key"] for p in pending], [public_key])

        self.log.info("roll back the advertisement: the beacon is gone")
        assert_equal(node.reorganize(base_hash)["RollbackChain"], True)
        assert_equal(node.getbestblockhash(), base_hash)
        active, pending = self.beacon_status(node)
        assert_equal(active, [])
        assert_equal(pending, [])

        self.log.info("the identical advertisement is accepted again and replayed by the next block")
        if advert_txid not in node.getrawmempool():
            assert_equal(node.sendrawtransaction(advert_hex), advert_txid)
        assert_equal(node.getrawmempool(), [advert_txid])
        self.advance_to_slot(node)
        node.generatetoaddress(1, node.getnewaddress())
        assert_equal(node.getrawmempool(), [])
        assert_equal(node.getblockcount(), base_height + 1)
        assert node.getbestblockhash() != advert_hash
        active, pending = self.beacon_status(node)
        assert_equal(active, [])
        assert_equal([p["public_key"] for p in pending], [public_key])

        self.log.info("a new superblock activates the replayed beacon")
        self.advance_to_slot(node)
        node.generatesuperblock({CPID: 100}, None, [public_key])
        active, pending = self.beacon_status(node)
        assert_equal(pending, [])
        assert_equal([a["public_key"] for a in active], [public_key])
        assert_equal(active[0]["magnitude"], 100)


if __name__ == "__main__":
    ContractReplayTest().main()
