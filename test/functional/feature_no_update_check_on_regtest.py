#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""A regtest node must not check GitHub for a newer release.

`GRC::ScheduleUpdateChecks` arms a scheduler job that issues a synchronous
libcurl GET to api.github.com, once a minute after start and then on the
-updatecheckinterval cycle. It was guarded by `OnTestnet()`, and there are three
networks, so regtest took the mainnet branch and dialled GitHub out of a test
run (issue #3357).

The guard now keys on `OnMainnet()`, as does the leaf guard in
`Upgrade::CheckForLatestUpdate` that the About dialog and walletdiagnose reach.

The 60-second delay is not reachable from here: CScheduler runs on
std::chrono::system_clock, which setmocktime does not move, so the request
cannot be provoked and then observed not to happen. What the test pins instead
is the decision -- the node must record that it declined to arm the job, and
must not record that it armed it.

The two are not redundant, though they are not symmetric either. "Declined is
present" is the load-bearing one: it can only be logged by the guard itself, so
it proves the code ran and chose. "Armed is absent" would pass on its own for
the wrong reason -- a node whose scheduler never started logs neither line --
but it is kept because it is the property actually being demanded, and it stays
meaningful if the decline log and the arming ever stop being adjacent.

Deliberately not exercised: the walletdiagnose RPC, which also funnels through
the leaf guard. It runs two further ungated outbound checks in the same loop --
a UDP NTP query and a TCP connect to portquiz.net, the latter through a throwing
resolver with no handler -- so calling it from a test would reintroduce exactly
the unsolicited network traffic this test exists to forbid, and would fail
outright on a runner without DNS.
"""

import os

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import chain_subdir

# Logged when the guard declines. Includes the network name, so the assertion
# cannot be satisfied by the old testnet-only wording.
DECLINED = 'Gridcoin: update checks are mainnet-only; disabled on chain "regtest"'

# Logged only after the guard falls through, when the job is actually armed.
ARMED = "Gridcoin: checking for updates every"


class NoUpdateCheckOnRegtestTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        # No -disableupdatecheck: that would satisfy a different guard and the
        # test would pass without the fix. The node has to decline on the
        # strength of the network alone.
        self.extra_args = [["-staking=0"]]

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def debug_log(self):
        path = os.path.join(self.nodes[0].datadir, chain_subdir(self.chain), "debug.log")
        with open(path, encoding="utf-8") as log_file:
            return log_file.read()

    def run_test(self):
        self.log.info("the update check declines to arm on regtest")

        # Not a plain read: StartRPCThreads runs well before
        # ScheduleBackgroundJobs, so the node answers RPC -- and the framework
        # therefore returns from start_nodes -- before ScheduleUpdateChecks has
        # decided anything. Reading debug.log straight away would race, and the
        # race is silent in the direction that matters: the DECLINED line would
        # simply not be there yet.
        self.wait_until(lambda: DECLINED in self.debug_log(), timeout=60)

        # Re-read once, so both assertions see one consistent snapshot taken
        # after the decision was recorded.
        log = self.debug_log()

        assert DECLINED in log, \
            "regtest node did not record declining the update check"
        assert ARMED not in log, \
            "regtest node armed the GitHub update check"


if __name__ == "__main__":
    NoUpdateCheckOnRegtestTest().main()
