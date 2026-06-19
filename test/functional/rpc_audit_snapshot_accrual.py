#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Regtest functional test for the auditsnapshotaccrual / auditsnapshotaccruals RPCs.

These RPCs audit per-CPID research-reward accrual snapshots. They were refactored
(GH #2978) so they no longer hold cs_main across the chain walk and disk reads;
this test exercises the RPC contract and the snapshot-phase early-return paths.

Regtest note: regtest has no BOINC accrual history and `BlockV11Height = 0`, so the
V11 gate passes immediately but there are no beacons or superblock CPIDs. That makes
this a contract/smoke + liveness test. The discriminating numeric-parity and
cs_main hold-time checks must run against a synced mainnet/testnet datadir (see the
plan's golden-diff procedure); they cannot be reproduced on a clean regtest chain.
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error

# Well-formed but unknown CPID (32 hex chars). Parses to a valid CPID with no
# beacon, so the audit returns an empty object from the snapshot phase.
UNKNOWN_CPID = "0123456789abcdef0123456789abcdef"

# Keys the aggregate auditsnapshotaccruals object must always expose.
AGGREGATE_KEYS = {
    "number_of_CPIDs",
    "number_of_matches",
    "number_of_mismatches",
    "number_of_mismatches_last_period_only",
    "number_accrual_accounts_not_present",
    "number_not_present",
    "accrual_mismatch_details",
}


class AuditSnapshotAccrualTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0", "-connect=0", "-listen=0"]]

    def setup_network(self):
        # Single isolated regtest node; the premine key is planted by the daemon
        # (see feature_regtest_staking.py for the rationale behind this override).
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def run_test(self):
        node = self.nodes[0]

        # Advance the chain a little so pindexBest is well past genesis.
        node.generatetoaddress(5, node.getnewaddress())

        # 1. No-CPID invocation: the local regtest node has no researcher CPID, so
        #    the call must fail with an invalid-parameter error rather than crash.
        assert_raises_rpc_error(-8, None, node.auditsnapshotaccrual)
        self.log.info("auditsnapshotaccrual with no researcher CPID rejected as expected")

        # 2. Well-formed but unknown CPID: passes the V11 gate (regtest
        #    BlockV11Height=0), finds no beacon, and returns an empty object from
        #    the snapshot phase without touching disk.
        result = node.auditsnapshotaccrual(UNKNOWN_CPID, False)
        assert_equal(result, {})
        # report_details=True must not change the empty-result behaviour.
        assert_equal(node.auditsnapshotaccrual(UNKNOWN_CPID, True), {})
        self.log.info("auditsnapshotaccrual for unknown CPID returns empty result")

        # 3. Population audit on an empty superblock: returns the documented
        #    aggregate shape with zero CPIDs.
        height_before = node.getblockcount()
        aggregate = node.auditsnapshotaccruals(False)
        assert AGGREGATE_KEYS.issubset(aggregate.keys()), (
            "missing keys: %s" % (AGGREGATE_KEYS - set(aggregate.keys())))
        assert_equal(aggregate["number_of_CPIDs"], 0)
        assert_equal(aggregate["accrual_mismatch_details"], [])
        self.log.info("auditsnapshotaccruals returns aggregate shape with 0 CPIDs")

        # 4. Liveness: the node remains responsive and the chain is intact after the
        #    population audit (a weak signal on an empty regtest chain, but it
        #    guards against a regression that hangs or corrupts state). The real
        #    cs_main hold-time check runs against a synced datadir per the plan.
        assert_equal(node.getblockcount(), height_before)
        node.auditsnapshotaccruals(True)
        assert_equal(node.getblockcount(), height_before)
        self.log.info("node responsive after audit; chain height stable")


if __name__ == "__main__":
    AuditSnapshotAccrualTest().main()
