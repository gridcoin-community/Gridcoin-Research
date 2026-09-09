#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""A regtest node must never seed itself from the compiled-in mainnet lists.

Two paths in net.cpp hand a node addresses that were compiled into the binary
rather than learned from the network, and both were guarded by `!OnTestnet()`.
There are three networks, so that predicate is true on regtest as well as on
mainnet, and a regtest node took both (issue #3345):

- `ThreadDNSAddressSeed2` resolves the five hostnames in `strDNSSeed` at
  startup and adds whatever they resolve to;
- `ThreadOpenConnections2` injects the 39 addresses in `pnSeed` once the node
  has been up more than 60 seconds with an empty addrman.

Both now key on `OnMainnet()`. Each is tested on its own node, because on one
node they mask each other: the DNS path populates addrman at startup, and a
non-empty addrman is exactly what switches the `pnSeed` path off. A single node
would therefore pass against a broken `pnSeed` guard.

Every arm pairs a "must not appear" assertion with a positive one that the code
carrying it actually ran, so that a node which never got that far -- a thread
that did not start, a category that was not enabled, a log that was not written
-- fails rather than passes quietly.
"""

import os
import time

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import (
    assert_equal,
    chain_subdir,
)

# Logged inside the DNS guard, before any name resolution, so this arm needs no
# working DNS and no network: with the guard broken the line is there whether or
# not the lookups then succeed.
DNS_ENTERED = "Loading addresses from DNS seeds"

# Logged after the guard, unconditionally, and last in the function. Its
# presence is the proof that the thread ran and got past the guard.
DNS_FINISHED = "addresses found from DNS seeds"

# Logged at the top of the connection thread.
CONN_STARTED = "ThreadOpenConnections started"

# Deliberately does not name the seed count. The unexpected-message check is a
# substring search, so pinning "39" here would turn the assertion into a no-op
# the day the seed list changes length.
SEED_ADDED = "fixed seed nodes to addrman"

# Each step is comfortably past the 60-second gate, and the clock is advanced
# by one of them per second of the observation window rather than once at the
# start. See the loop in run_test for why once is not enough.
CLOCK_STEP = 300

# Ten steps, so twenty turns of the 500 ms connection loop.
OBSERVE_STEPS = 10


class NoMainnetSeedsOnRegtestTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.chain = "regtest"
        self.setup_clean_chain = True
        # -debug=net makes both seed paths visible. Neither node may be given
        # -connect: any -connect argument sends ThreadOpenConnections2 into a
        # different infinite loop at the top of the function that never reaches
        # the pnSeed path, and node1 would pass against a broken guard. It also
        # soft-sets -dnsseed=0, which would disarm node0.
        common = ["-staking=0", "-debug=net"]
        # node0 runs the DNS path. The generated config already sets dnsseed=0,
        # so it is turned back on explicitly here -- otherwise this arm would be
        # testing a disabled thread.
        # node1 keeps DNS seeding off so its addrman stays empty and the pnSeed
        # gate is reachable.
        self.extra_args = [common + ["-dnsseed=1"], common + ["-dnsseed=0"]]

    def setup_network(self):
        # Deliberately not the default: that one links the nodes into a chain
        # and syncs them, which would put a peer address in both addrmans. An
        # empty addrman is the precondition for the pnSeed path, so the default
        # would leave node1 asserting nothing at all.
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def debug_log(self, n):
        path = os.path.join(self.nodes[n].datadir, chain_subdir(self.chain), "debug.log")
        with open(path, encoding="utf-8") as log_file:
            return log_file.read()

    def run_test(self):
        dns_node, seed_node = self.nodes

        self.log.info("the DNS seed thread runs and declines to resolve the "
                      "mainnet seed hostnames")

        # With the guard broken this waits for five real DNS lookups, so it is
        # given room; with the guard held it is satisfied at once.
        self.wait_until(lambda: DNS_FINISHED in self.debug_log(0), timeout=60)

        log = self.debug_log(0)
        assert DNS_ENTERED not in log, \
            "regtest node entered the mainnet DNS seed block"
        assert "0 addresses found from DNS seeds" in log, \
            "regtest node took addresses from the mainnet DNS seeds"
        # That count is the load-bearing check for this arm: it is incremented
        # once per resolved address, so it pins the outcome exactly. addrman is
        # only corroboration here, and weak corroboration -- getnodeaddresses
        # returns a 23% sample, which rounds to nothing below five entries, and
        # five hostnames could well resolve to fewer than that.
        assert_equal(dns_node.getnodeaddresses(1000), [])
        assert_equal(dns_node.getpeerinfo(), [])

        self.log.info("the connection thread crosses the 60 second gate on an "
                      "empty addrman without injecting pnSeed")

        assert CONN_STARTED in self.debug_log(1), \
            "the connection thread never started, so nothing was tested"
        assert_equal(seed_node.getnodeaddresses(1000), [])
        assert_equal(seed_node.getpeerinfo(), [])

        # The gate is `GetAdjustedTime() - nStart > 60`, where nStart is sampled
        # on the connection thread a few statements after the line asserted
        # above. So the assertion does not prove nStart has been read yet, and a
        # single setmocktime could land first -- which would pin nStart AT the
        # mocked value, freeze the difference at zero, and leave the gate
        # uncrossable for the rest of the run. That is a silent pass, not a
        # flake: the test would report success against a broken guard.
        #
        # Advancing the clock repeatedly removes the ordering question instead
        # of racing it. Wherever nStart was sampled, the next step puts the
        # clock CLOCK_STEP seconds beyond it. The seed block also sits at the
        # top of a loop that turns over every 500 ms and does not latch, so the
        # window covers about twenty opportunities rather than one.
        #
        # timeout_factor is not usable as a multiplier for the per-step sleep:
        # the framework rewrites a factor of 0 to 99999, which would park this
        # test for a day. The window is wall-clock and so is the loop it
        # watches, so it needs no scaling; the clamp only stretches it for a
        # heavily loaded or instrumented run.
        step_seconds = min(max(self.options.timeout_factor, 1), 4)
        base_time = int(time.time())

        with seed_node.assert_debug_log(expected_msgs=[], unexpected_msgs=[SEED_ADDED]):
            for step in range(1, OBSERVE_STEPS + 1):
                seed_node.setmocktime(base_time + step * CLOCK_STEP)
                time.sleep(step_seconds)

        # getnodeaddresses returns a 23% sample of addrman, so it is a sound
        # check for the 39 entries pnSeed would have added and not a general
        # emptiness test -- for a handful of entries the sample rounds to zero.
        # The log assertion above is what covers the general case.
        assert_equal(seed_node.getnodeaddresses(1000), [])
        assert_equal(seed_node.getpeerinfo(), [])


if __name__ == "__main__":
    NoMainnetSeedsOnRegtestTest().main()
