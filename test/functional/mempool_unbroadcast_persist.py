#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""unbroadcast.dat follows the unbroadcast set, so a crash reloads the current set.

unbroadcast.dat holds the unbroadcast set's never-sent entries: this node's own
transactions that no peer has fetched yet. The node rewrites it within a few
seconds of each change, as well as at shutdown, so a node killed without a
shutdown restarts with the set it had, in both directions:

  * a transaction sent since the last clean shutdown is reloaded with its
    marker, rather than lost;
  * a transaction withdrawn with cancelunbroadcasttransaction since then stays
    out, rather than coming back with its marker.

Each case waits until the file holds the expected transactions, kills the node
with SIGKILL, and checks what the restart reloaded: the reload's log line
reports how many transactions the file held, and getmempoolinfo how many came
back marked unbroadcast. The other cases check that a clean stop leaves the
file holding the set, that a file the reload cannot read is moved aside rather
than overwritten, replacing an earlier one, with the set still persisted, and
that -persistmempool=0 leaves the file alone.

The withdrawal case does not check the pool itself. cancelunbroadcasttransaction
also marks the wallet's copy abandoned, and whether that wallet.dat write
survives a SIGKILL is not this file's to decide: had it not, the wallet would
re-pool the transaction at startup, without a marker.
"""

import io
import os

from test_framework.messages import CTransaction, deser_compact_size
from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal, chain_subdir

UNBROADCAST_DUMP_VERSION = 1

# Read as the file's version, its first eight bytes are not
# UNBROADCAST_DUMP_VERSION, so the reload refuses the file.
UNREADABLE = b"not an unbroadcast.dat"
UNREADABLE_AGAIN = b"still not an unbroadcast.dat"


class MempoolUnbroadcastPersistTest(GridcoinTestFramework):
    def set_test_params(self):
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.num_nodes = 1
        # No peers, so nothing fetches a transaction and clears its marker, and
        # no staking, so nothing mines one.
        self.extra_args = [["-staking=0", "-connect=0", "-listen=0"]]

    def setup_network(self):
        # Single isolated regtest node; bypass the base regtest createwallet
        # path (Gridcoin has one default BDB wallet, no multiwallet).
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def unbroadcast_path(self, node):
        return os.path.join(node.datadir, chain_subdir(self.chain), "unbroadcast.dat")

    def persisted_txids(self, node):
        """The txids unbroadcast.dat holds, sorted, or None if there is no file.

        The node replaces the file by renaming a complete one over it, so a read
        never sees a partial write.
        """
        try:
            with open(self.unbroadcast_path(node), "rb") as f:
                data = f.read()
        except FileNotFoundError:
            return None

        f = io.BytesIO(data)
        assert_equal(int.from_bytes(f.read(8), "little"), UNBROADCAST_DUMP_VERSION)

        txids = []
        for _ in range(deser_compact_size(f)):
            tx = CTransaction()
            tx.deserialize(f)
            f.read(8)  # the pool entry time
            tx.rehash()
            txids.append(tx.hash)
        assert_equal(f.read(), b"")
        return sorted(txids)

    def wait_for_persisted(self, node, txids):
        self.wait_until(lambda: self.persisted_txids(node) == sorted(txids), timeout=30)

    def crash_node(self, i):
        """Kill node i with SIGKILL: no shutdown code runs."""
        node = self.nodes[i]
        node.process.kill()
        self.wait_for_node_exit(i, timeout=60)
        assert node.process.returncode != 0

        # Clear what is_node_stopped() clears on a clean exit, and close the
        # output files stop_node() would have closed.
        node.running = False
        node.process = None
        node.rpc_connected = False
        node.rpc = None
        node.stdout.close()
        node.stderr.close()

    def start_node_reloading(self, i, persisted):
        """Start node i, checking that the reload found `persisted` transactions."""
        node = self.nodes[i]
        with node.assert_debug_log([f"of {persisted} persisted"], timeout=60):
            self.start_node(i)

        # The restart drops the mock clock, which generatetoaddress() had moved
        # to the tip. Put it back, so a later send is not stamped before its
        # inputs.
        node.advance_mocktime_to(node.getblock(node.getbestblockhash())["time"])

    def unbroadcast_count(self, node):
        return node.getmempoolinfo()["unbroadcastcount"]

    def run_test(self):
        node = self.nodes[0]

        node.generatetoaddress(3, node.getnewaddress())

        self.log.info("the first pass writes the set as reloaded: empty")
        self.wait_for_persisted(node, [])

        self.log.info("a send is in the file within a pass, and survives a crash")
        txid = node.sendtoaddress(node.getnewaddress(), 1)
        assert_equal(self.unbroadcast_count(node), 1)
        self.wait_for_persisted(node, [txid])

        self.crash_node(0)
        self.start_node_reloading(0, persisted=1)
        assert txid in node.getrawmempool()
        assert_equal(self.unbroadcast_count(node), 1)

        self.log.info("a withdrawal leaves the file within a pass, and survives a crash")
        node.cancelunbroadcasttransaction(txid)
        assert_equal(self.unbroadcast_count(node), 0)
        self.wait_for_persisted(node, [])

        self.crash_node(0)
        self.start_node_reloading(0, persisted=0)
        assert_equal(self.unbroadcast_count(node), 0)

        self.log.info("a clean stop leaves the file holding the set")
        txid = node.sendtoaddress(node.getnewaddress(), 1)
        # A scheduled pass may write the send first; the log line shows that the
        # shutdown wrote the set as well.
        with node.assert_debug_log(["Persisted the unbroadcast set"]):
            self.stop_node(0)
        assert_equal(self.persisted_txids(node), [txid])

        self.start_node_reloading(0, persisted=1)
        assert txid in node.getrawmempool()
        assert_equal(self.unbroadcast_count(node), 1)

        self.log.info("an unreadable file is moved aside, not overwritten")
        self.stop_node(0)
        path = self.unbroadcast_path(node)
        with open(path, "wb") as f:
            f.write(UNREADABLE)

        with node.assert_debug_log(["could not reload", "moved it to"], timeout=60):
            self.start_node(0)
        with open(path + ".bad", "rb") as f:
            assert_equal(f.read(), UNREADABLE)

        # Its marker was in the unreadable file, so the set starts empty, and
        # the file is written afresh.
        assert_equal(self.unbroadcast_count(node), 0)
        self.wait_for_persisted(node, [])

        self.log.info("a later unreadable file replaces the earlier one, and the set is still persisted")
        self.stop_node(0)
        with open(path, "wb") as f:
            f.write(UNREADABLE_AGAIN)

        with node.assert_debug_log(["could not reload", "moved it to"], timeout=60):
            self.start_node(0)
        with open(path + ".bad", "rb") as f:
            assert_equal(f.read(), UNREADABLE_AGAIN)
        self.wait_for_persisted(node, [])

        self.log.info("with -persistmempool=0 the file is neither reloaded nor written")
        self.stop_node(0)
        with node.assert_debug_log([], unexpected_msgs=["Reloaded unbroadcast transactions"]):
            self.start_node(0, extra_args=self.extra_args[0] + ["-persistmempool=0"])
        node.advance_mocktime_to(node.getblock(node.getbestblockhash())["time"])

        node.sendtoaddress(node.getnewaddress(), 1)
        assert_equal(self.unbroadcast_count(node), 1)

        # Neither a write nor a failed attempt at one, at shutdown.
        with node.assert_debug_log([], unexpected_msgs=["Persisted the unbroadcast set", "WriteMempoolEntries"]):
            self.stop_node(0)
        assert_equal(self.persisted_txids(node), [])


if __name__ == "__main__":
    MempoolUnbroadcastPersistTest().main()
