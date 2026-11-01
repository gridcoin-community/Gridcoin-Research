#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Pending v3 beacons keep their ownership proofs when reloaded, on regtest.

A v3 beacon advertisement carries an ownership proof, which the beacon registry
holds beside the pending beacon for the scrapers to check. The beacon db does
not store it, so any pending beacon that comes back from the db rather than
from a contract being applied has to have its proof read back from its
advertisement transaction. Two such paths are exercised here:

  * a restart: the registry reloads pending beacons from the db, and the
    startup contract replay does not bring the proofs back. On mainnet and
    testnet it skips every beacon contract below the db's stored height; on
    regtest it does not run at all (ReplayContracts returns early below height
    164618 outside testnet);
  * a superblock disconnect: BeaconRegistry::Deactivate returns to pending,
    from their db records, the beacon the superblock activated and the pending
    beacons it expired.

Two beacons are advertised, the second with force, so the restart has more
than one proof to restore.

Regtest has no scrapers, and no RPC reports the proofs, so the checks are the
registry's own log lines.
"""

import base64
import os

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal

CPID = "00010203040506070809101112131415"

# See feature_beacon_activation.py.
STAKE_TIMESTAMP_MASK = 15

# PendingBeacon::RETENTION_AGE: a superblock this long after an advertisement
# expires the pending beacon.
PENDING_RETENTION = 60 * 60 * 24 * 3

MASTER_URL = "https://project.example.org/"
ACCOUNT_ID = 42

# A well-formed signature is 64 to 1024 bytes (BeaconPayload::WellFormed).
SIGNATURE_B64 = base64.b64encode(bytes(range(128))).decode("ascii")


class BeaconPendingProofsTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [[
            "-staking=0",
            "-connect=0",
            "-listen=0",
            "-debug=beacon",
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

    def restored(self, address):
        return f"Restored ownership proof of pending beacon for cpid {CPID}, address {address}"

    def pending_keys(self, node):
        return sorted(p["public_key"] for p in node.beaconstatus(CPID)["pending"])

    def advertise_v3(self, node, force):
        """Advertise a v3 beacon under a new wallet key and mine it."""
        address = node.getnewaddress()
        public_key = node.validateaddress(address)["pubkey"]
        proof_xml = (f"<master_url>{MASTER_URL}</master_url>"
                     f"<msg>{ACCOUNT_ID} {public_key}</msg>"
                     f"<signature>{SIGNATURE_B64}</signature>")

        self.advance_to_slot(node)
        advertised = node.advertisebeaconv3(proof_xml, force)
        assert_equal(advertised["result"], "SUCCESS")
        assert_equal(advertised["public_key"], public_key)

        assert_equal(len(node.getrawmempool()), 1)
        node.generatetoaddress(1, node.getnewaddress())
        assert_equal(node.getrawmempool(), [])

        return address, public_key

    def run_test(self):
        node = self.nodes[0]

        node.generatetoaddress(5, node.getnewaddress())

        self.log.info("advertise two v3 beacons in separate blocks")
        address_1, public_key_1 = self.advertise_v3(node, False)
        address_2, public_key_2 = self.advertise_v3(node, True)
        assert_equal(self.pending_keys(node), sorted([public_key_1, public_key_2]))

        self.log.info("a restart restores both pending beacons' proofs from their transactions")
        with node.assert_debug_log([self.restored(address_1), self.restored(address_2),
                                    "Restored ownership proofs for 2 of 2 pending beacons"], timeout=60):
            self.restart_node(0)
        assert_equal(self.pending_keys(node), sorted([public_key_1, public_key_2]))

        self.log.info("a superblock activates the second beacon")
        before_superblock = node.getbestblockhash()
        self.advance_to_slot(node)
        node.generatesuperblock({CPID: 100}, None, [public_key_2])
        assert_equal(self.pending_keys(node), [public_key_1])
        assert_equal([a["public_key"] for a in node.beaconstatus(CPID)["active"]], [public_key_2])

        self.log.info("disconnecting the superblock restores the second beacon's proof")
        with node.assert_debug_log([self.restored(address_2)]):
            assert_equal(node.reorganize(before_superblock)["RollbackChain"], True)
        assert_equal(node.getbestblockhash(), before_superblock)
        assert_equal(self.pending_keys(node), sorted([public_key_1, public_key_2]))

        self.log.info("a superblock past the pending retention expires both beacons")
        before_expiry = node.getbestblockhash()
        self.advance_to_slot(node, PENDING_RETENTION + 600)
        node.generatesuperblock({CPID: 100})
        assert_equal(self.pending_keys(node), [])

        self.log.info("disconnecting that superblock restores both expired beacons' proofs")
        with node.assert_debug_log([self.restored(address_1), self.restored(address_2)]):
            assert_equal(node.reorganize(before_expiry)["RollbackChain"], True)
        assert_equal(node.getbestblockhash(), before_expiry)
        assert_equal(self.pending_keys(node), sorted([public_key_1, public_key_2]))


if __name__ == "__main__":
    BeaconPendingProofsTest().main()
