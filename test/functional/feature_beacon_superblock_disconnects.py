#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Two superblocks disconnected by two reorganizations.

From block version 15, a superblock disconnect that finds the beacon
registry's superblock state not current rebuilds the registry. The second
disconnect here is one. One node runs with block version 15 active and one
below it, where the behaviour is unchanged. The rebuild replays beacon
contracts alone and leaves every other registry as the disconnects left it.
Either way a beacon activated before both superblocks is still active
afterwards, and after a restart.
"""

import os
from decimal import Decimal, ROUND_DOWN

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal

CPID = "00010203040506070809101112131415"
STAKE_TIMESTAMP_MASK = 15
REBUILD = "Rebuilding beacon registry in-line"
# Logged by the startup routine for each registry it reloads; a rebuild replays beacons alone and reloads none.
STARTUP_LOAD = "Loading stored history for contract type"
# addpoll arguments.
POLL_DAYS = 7
WEIGHT_BALANCE = 1
RESPONSE_SINGLE_CHOICE = 2
FEE = Decimal("0.01")
# GetSuperblockAgeSpacing on regtest: a superblock needs this long since the last one.
SUPERBLOCK_SPACING = 43200


class BeaconSuperblockDisconnectsTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [
            ["-staking=0", "-connect=0", "-listen=0", f"-forcecpid={CPID}", "-blockv15height=1", "-debug=contract"],
            ["-staking=0", "-connect=0", "-listen=0", f"-forcecpid={CPID}"],
        ]

    def setup_network(self):
        # See feature_contract_replay.py: a researcher node needs a BOINC data directory to advertise its beacon.
        for i, args in enumerate(self.extra_args):
            boinc_dir = os.path.join(self.options.tmpdir, f"boinc{i}")
            os.makedirs(boinc_dir, exist_ok=True)
            with open(os.path.join(boinc_dir, "client_state.xml"), "w", encoding="utf-8") as client_state:
                client_state.write("<client_state>\n</client_state>\n")
            args.append(f"-boincdatadir={boinc_dir}")

        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def advance_to_slot(self, node, seconds=128):
        tip_time = node.getblock(node.getbestblockhash())["time"]
        slot = (tip_time + seconds) & ~STAKE_TIMESTAMP_MASK
        node.setmocktime(slot)

    def active_keys(self, node):
        return [beacon["public_key"] for beacon in node.beaconstatus(CPID)["active"]]

    def activate_beacon(self, node):
        """Advertise the researcher's beacon and activate it with a superblock; return its public key."""
        node.generatetoaddress(5, node.getnewaddress())
        self.advance_to_slot(node)
        advertised = node.advertisebeacon()
        assert_equal(advertised["result"], "SUCCESS")
        node.generatetoaddress(1, node.getnewaddress())
        self.advance_to_slot(node)
        node.generatesuperblock({CPID: 100}, None, [advertised["public_key"]])
        assert_equal(self.active_keys(node), [advertised["public_key"]])
        return advertised["public_key"]

    def mine_poll(self, node):
        """Mine a poll, a contract of another registry; return its txid and height.

        A poll needs a balance in ordinary outputs, so three of the premine's genesis outputs move to thirty new
        addresses first and are buried. A coinstake spends one output, so the ten burying blocks leave at least
        twenty of the thirty, about 200,000 GRC, for the claim's 100,000.
        """
        # Seven blocks precede this on the node's own chain, each staking at most one genesis output.
        premine = [u for u in node.listunspent(0) if u["confirmations"] > node.getblockcount()][:3]
        assert_equal(len(premine), 3)
        per_output = ((sum(u["amount"] for u in premine) - FEE) / 30).quantize(Decimal("0.00000001"),
                                                                                rounding=ROUND_DOWN)
        raw = node.createrawtransaction([{"txid": u["txid"], "vout": u["vout"]} for u in premine],
                                        {node.getnewaddress(): per_output for _ in range(30)})
        signed = node.signrawtransactionwithwallet(raw)
        assert_equal(signed["complete"], True)
        moved = node.sendrawtransaction(signed["hex"])

        # A poll's claim takes outputs ten blocks deep (CWalletTx::IsConfirmed).
        for _ in range(10):
            self.advance_to_slot(node)
            node.generatetoaddress(1, node.getnewaddress())
        assert node.gettransaction(moved)["confirmations"] >= 10

        poll = node.addpoll("survey", "Rebuild", POLL_DAYS, "Which?", "A;B", WEIGHT_BALANCE,
                            RESPONSE_SINGLE_CHOICE, "https://gridcoin.us")
        self.advance_to_slot(node)
        node.generatetoaddress(1, node.getnewaddress())
        assert_equal(node.getrawmempool(), [])
        return poll["id"], node.getblockcount()

    def two_superblocks(self, node):
        """Mine two superblocks; return the blocks just before each."""
        node.generatetoaddress(5, node.getnewaddress())
        self.advance_to_slot(node, SUPERBLOCK_SPACING + 600)
        before_first = node.getbestblockhash()
        node.generatesuperblock({CPID: 100})
        node.generatetoaddress(2, node.getnewaddress())
        self.advance_to_slot(node, SUPERBLOCK_SPACING + 600)
        before_second = node.getbestblockhash()
        node.generatesuperblock({CPID: 100})
        node.generatetoaddress(2, node.getnewaddress())
        return before_first, before_second

    def run_test(self):
        for i, v15 in ((0, True), (1, False)):
            node = self.nodes[i]
            self.log.info(f"two superblock disconnects, v15 {'on' if v15 else 'off'}")
            public_key = self.activate_beacon(node)
            poll_txid, poll_height = self.mine_poll(node)
            before_first, before_second = self.two_superblocks(node)

            with node.assert_debug_log([], unexpected_msgs=[REBUILD]):
                assert_equal(node.reorganize(before_second)["RollbackChain"], True)

            if v15:
                # The rebuild applies the beacon contracts alone, not the poll mined before the reorg point.
                poll_applied = f"INFO: Add contract: block {poll_height}, txid {poll_txid}"
                with node.assert_debug_log([REBUILD], unexpected_msgs=[STARTUP_LOAD, poll_applied]):
                    assert_equal(node.reorganize(before_first)["RollbackChain"], True)
            else:
                with node.assert_debug_log([], unexpected_msgs=[REBUILD]):
                    assert_equal(node.reorganize(before_first)["RollbackChain"], True)

            self.log.info("the beacon activated before both superblocks is still active, and after a restart")
            assert_equal(self.active_keys(node), [public_key])
            self.restart_node(i)
            assert_equal(self.active_keys(self.nodes[i]), [public_key])


if __name__ == "__main__":
    BeaconSuperblockDisconnectsTest().main()
