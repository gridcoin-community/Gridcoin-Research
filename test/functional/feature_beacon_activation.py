#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Beacon advertisement through superblock activation, on regtest.

A beacon reaches ACTIVE only through BeaconRegistry::ActivatePending, which
ConnectBlock calls with superblock->m_verified_beacons.m_verified. On mainnet
that list is filled from scraper convergence; regtest has no scrapers, so
generatesuperblock names the beacons instead. Nothing downstream of the
superblock inspects how the list was produced, so no BOINC project data, no
project RSA key and no scraper convergence is needed to exercise the path.

The negative case is the point of the test: a superblock that omits the beacon
leaves it pending. Without it, "the beacon is active after a superblock" would
also pass if activation happened for some unrelated reason.
"""

import os

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error

CPID = "00010203040506070809101112131415"

# CreateRestOfTheBlock skips any mempool transaction with tx.nTime > block.nTime,
# and CreateCoinStake masks the block time down to a STAKE_TIMESTAMP_MASK
# boundary. A contract transaction created part-way through a 16-second slot is
# therefore newer than the block that would carry it, and stays in the mempool.
# Every block here is mined at a mocktime that is already on a boundary, so the
# transaction and the block share a timestamp and the transaction is included.
STAKE_TIMESTAMP_MASK = 15

# GetSuperblockAgeSpacing() is 43200s off testnet below height 364500, and
# Quorum::ValidateSuperblockClaim rejects a superblock that is not needed yet.
# Two superblocks in one test therefore have to be more than 12 hours apart.
SUPERBLOCK_SPACING = 43200


class BeaconActivationTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        # -staking=0 keeps the background staker out of it, so the chain only
        # moves on an explicit generate call. -forcecpid supplies the CPID that
        # BOINC would otherwise have to provide. -devbuild=override lets a
        # development build send a transaction at all.
        self.extra_args = [[
            "-staking=0",
            "-devbuild=override",
            "-connect=0",
            "-listen=0",
            f"-forcecpid={CPID}",
        ]]

    def setup_network(self):
        # Point the CPID probe inside the test directory: without this the node
        # reads whatever BOINC installation the host happens to have, so the
        # result would depend on the machine. client_state.xml has to exist and
        # be readable, or the node puts "Could not access BOINC data directory"
        # on stderr and the framework fails any node that writes to stderr. An
        # empty <client_state> carries no <project> sections, which is what we
        # want -- the CPID comes from -forcecpid, not from BOINC.
        boinc_dir = os.path.join(self.options.tmpdir, "boinc")
        os.makedirs(boinc_dir, exist_ok=True)

        # lint-python-utf8-encoding.sh greps line by line, so the encoding
        # argument has to stay on the same line as the call itself.
        client_state_path = os.path.join(boinc_dir, "client_state.xml")
        with open(client_state_path, "w", encoding="utf-8") as client_state:
            client_state.write("<client_state>\n</client_state>\n")

        self.extra_args[0].append(f"-boincdatadir={boinc_dir}")

        # As in feature_generatesuperblock.py: the base setup_nodes() regtest
        # branch calls import_deterministic_coinbase_privkeys() -> createwallet,
        # which Gridcoin does not have.
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def advance_to_slot(self, node, seconds=64):
        """Move mocktime forward onto a stake-timestamp boundary.

        Anything created at the returned timestamp can ride in a block mined at
        it, because masking the block time is then a no-op.
        """
        tip_time = node.getblock(node.getbestblockhash())["time"]
        slot = (tip_time + seconds) & ~STAKE_TIMESTAMP_MASK
        if slot <= tip_time:
            # The masking floors; keep the docstring's promise of moving
            # forward even for a small `seconds`.
            slot += STAKE_TIMESTAMP_MASK + 1
        node.setmocktime(slot)

        return slot

    def beacon_status(self, node):
        """Return the (active, pending) entry lists for our CPID."""
        status = node.beaconstatus(CPID)

        return status["active"], status["pending"]

    def run_test(self):
        node = self.nodes[0]

        # Fund the wallet: the beacon advertisement is an ordinary transaction
        # and has to pay a fee. Regtest skips the coinbase/coinstake maturity
        # check, so these are spendable immediately.
        node.generatetoaddress(5, node.getnewaddress())

        self.log.info("the forced CPID is picked up without BOINC")
        assert_equal(node.beaconstatus(CPID)["active"], [])

        self.log.info("advertisebeacon leaves the beacon pending")
        self.advance_to_slot(node)

        advertised = node.advertisebeacon()
        assert_equal(advertised["result"], "SUCCESS")
        assert_equal(advertised["cpid"], CPID)
        public_key = advertised["public_key"]

        # The advertisement is in the mempool; the contract only reaches the
        # registry once a block carries it. The block is mined at the slot the
        # transaction was built at, so tx.nTime == block.nTime.
        assert_equal(len(node.getrawmempool()), 1)
        node.generatetoaddress(1, node.getnewaddress())
        assert_equal(node.getrawmempool(), [])

        active, pending = self.beacon_status(node)
        assert_equal(active, [])
        assert_equal(len(pending), 1)
        assert_equal(pending[0]["public_key"], public_key)

        self.log.info("a superblock that omits the beacon leaves it pending")
        self.advance_to_slot(node)
        node.generatesuperblock({CPID: 100})

        active, pending = self.beacon_status(node)
        assert_equal(active, [])
        assert_equal(len(pending), 1)
        assert_equal(pending[0]["public_key"], public_key)

        self.log.info("a superblock naming the beacon activates it")
        # Past the superblock spacing, or ValidateSuperblockClaim rejects the
        # second superblock as "superblock too early".
        self.advance_to_slot(node, SUPERBLOCK_SPACING + 600)
        node.generatesuperblock({CPID: 100}, None, [public_key])

        active, pending = self.beacon_status(node)
        assert_equal(pending, [])
        assert_equal(len(active), 1)
        assert_equal(active[0]["public_key"], public_key)
        assert_equal(active[0]["active"], True)
        assert_equal(active[0]["cpid"], CPID)

        self.log.info("the activated beacon carries the superblock magnitude")
        assert_equal(active[0]["magnitude"], 100)

        self.log.info("input validation")
        assert_raises_rpc_error(
            -8, "must be a hex string",
            node.generatesuperblock, {CPID: 100}, ["regtest"], [12345])
        assert_raises_rpc_error(
            -8, "must be an array",
            node.generatesuperblock, {CPID: 100}, ["regtest"], "notanarray")
        assert_raises_rpc_error(
            -8, "invalid beacon public key",
            node.generatesuperblock, {CPID: 100}, ["regtest"], ["nothex"])


if __name__ == "__main__":
    BeaconActivationTest().main()
