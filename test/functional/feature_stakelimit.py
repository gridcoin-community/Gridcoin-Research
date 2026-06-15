#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Behavioral test for the regtest `stakelimit` height ceiling.

`stakelimit <height>` (regtest-only RPC, src/wallet/rpcwallet.cpp) sets a global
ceiling: ThreadStakeMiner pauses once the chain reaches that height and resumes
when the ceiling is raised (src/miner.cpp StakeMiner; g_stakelimit_height in
src/miner.h; 0 = no cap). This test drives the *background staker*, unlike
feature_regtest_staking.py which uses the deterministic `generatetoaddress` path
that bypasses StakeMiner and ignores the ceiling.

Solo regtest staking: IsMiningAllowed normally forces a node OFFLINE without the
minimum peer count / a synced tip, but that gate is skipped on IsMockableChain
(src/miner.cpp), so a single isolated regtest node stakes on its own here.

Timing: staked blocks are timestamp-masked to ~16s spacing
(STAKE_TIMESTAMP_MASK), so climbing a few blocks takes tens of seconds. This test
lives in EXTENDED_SCRIPTS (opt-in via --extended), out of the default CI suite.
"""

import time

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal


# Blocks to let the staker climb per phase. Small because each staked block is
# ~16s of wall-clock (stake timestamp mask).
STEP = 2
# Generous: staking cadence is wall-clock bound (~16s/block), not CPU bound.
CLIMB_TIMEOUT = 180
# Window over which a paused ceiling must hold the height steady.
PLATEAU_SECONDS = 25


class RegtestStakeLimitTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        # -staking=1 runs the background ThreadStakeMiner (the path stakelimit
        # gates). -devbuild=override clears fDevbuildCripple, which otherwise
        # disables staking on a dev build outside testnet (regtest is not
        # testnet). Isolated (-connect=0/-listen=0); solo staking is allowed on
        # regtest via the IsMockableChain bypass in IsMiningAllowed.
        self.extra_args = [["-staking=1", "-devbuild=override", "-connect=0", "-listen=0"]]

    def setup_network(self):
        # Single isolated node; bypass the base regtest createwallet path (Gridcoin
        # has a single default wallet). The premine key is planted by the daemon.
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def _height(self):
        return self.nodes[0].getblockcount()

    def _assert_plateau(self, expected, seconds):
        """Assert the height stays == expected over a window (ceiling holds)."""
        deadline = time.time() + seconds
        while time.time() < deadline:
            assert_equal(self._height(), expected)
            time.sleep(2)

    def run_test(self):
        node = self.nodes[0]

        # Pause the staker at a low ceiling before it can run away. h0 is the
        # pre-staking height: the ~16s block cadence means the staker cannot have
        # advanced in the sub-second start -> first-RPC window.
        h0 = self._height()
        cap_a = h0 + STEP
        assert_equal(node.stakelimit(cap_a)["height"], cap_a)

        # Phase 1 - ceiling honored: the staker climbs to cap_a then pauses there.
        # StakeMiner stops *at* the limit (it skips once height >= limit), so the
        # final height is exactly cap_a, never cap_a + 1.
        self.wait_until(lambda: self._height() >= cap_a, timeout=CLIMB_TIMEOUT)
        assert_equal(self._height(), cap_a)
        self.log.info("staker reached ceiling %d; verifying it holds", cap_a)
        self._assert_plateau(cap_a, PLATEAU_SECONDS)
        self.log.info("ceiling honored: height steady at %d", cap_a)

        # Phase 2 - resume: raising the ceiling lets the staker advance again.
        cap_b = cap_a + STEP
        assert_equal(node.stakelimit(cap_b)["height"], cap_b)
        self.wait_until(lambda: self._height() >= cap_b, timeout=CLIMB_TIMEOUT)
        assert_equal(self._height(), cap_b)
        self.log.info("staker resumed and reached new ceiling %d", cap_b)

        # Quiesce: re-cap at the current height so the staker is paused for a clean
        # shutdown (0 would mean 'no cap' = unbounded staking during teardown).
        assert_equal(node.stakelimit(cap_b)["height"], cap_b)
        self._assert_plateau(cap_b, 6)
        self.log.info("stakelimit behavioral contract OK (honored + resume)")


if __name__ == "__main__":
    RegtestStakeLimitTest().main()
