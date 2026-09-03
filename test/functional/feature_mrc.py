#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""A manual research claim (MRC) requested by one node and paid by another's coinstake.

Two regtest nodes. node1 runs as a researcher (-forcecpid) and advertises a
beacon; node0 is an investor and does all the staking, including the two
superblocks (the second activates node1's beacon at magnitude 100). After a day
of accrual node1 sends an MRC request. The request is a contract transaction
bound to the current tip (its last_block_hash), so it is valid only for the very
next block, and the miner refuses to pay an MRC for its own CPID: node0, staking
as an investor, is the one that can pay it.

Asserted on the block node0 stakes: the claim's MRC map names node1's CPID, the
coinstake carries an output for the request's research subsidy minus its fee to
node1's beacon address, node1's wallet sees that output, and the block minted
block subsidy plus the whole research subsidy (payout plus the fee split between
staker and foundation). Before the request the same node's block carried no
such output, which is what the comparison is against.
"""

import os
from decimal import Decimal

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than

CPID = "00010203040506070809101112131415"

# See feature_beacon_activation.py for both constants.
STAKE_TIMESTAMP_MASK = 15
SUPERBLOCK_SPACING = 43200

# A day of accrual at magnitude 100 before the request; also well past the
# regtest MRCZeroPaymentInterval (10 minutes), after which the fee is a fraction
# of the reward rather than all of it (createmrcrequest refuses reward == fee
# off testnet).
ACCRUAL_SPAN = 86400


class MrcTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.chain = "regtest"
        self.setup_clean_chain = True
        common = ["-staking=0", "-devbuild=override"]
        self.extra_args = [common[:], common + [f"-forcecpid={CPID}"]]

    def setup_network(self):
        # Both nodes get the empty BOINC stub (see feature_beacon_activation.py):
        # node1's CPID comes from -forcecpid, and node0 must not pick up a CPID
        # from the host's BOINC installation, or it would stop being an investor.
        boinc_dir = os.path.join(self.options.tmpdir, "boinc")
        os.makedirs(boinc_dir, exist_ok=True)
        client_state_path = os.path.join(boinc_dir, "client_state.xml")
        with open(client_state_path, "w", encoding="utf-8") as client_state:
            client_state.write("<client_state>\n</client_state>\n")
        for args in self.extra_args:
            args.append(f"-boincdatadir={boinc_dir}")

        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()
        self.connect_nodes(0, 1)

    def advance_to_slot(self, seconds=64):
        """Move both nodes' mocktime forward onto one stake-timestamp boundary.

        A jump longer than the peer timeout (TIMEOUT_INTERVAL in net.h, 20
        minutes on the adjusted clock, which setmocktime pins) makes each node
        see the other as silent for that long and drop the link, so after such
        a jump the link is re-made. The jump itself has moved the accepting
        node past its inbound rate-limit window (see disconnect_nodes).
        """
        node0, node1 = self.nodes
        tip_time = node0.getblock(node0.getbestblockhash())["time"]
        slot = (tip_time + seconds) & ~STAKE_TIMESTAMP_MASK
        for node in self.nodes:
            node.setmocktime(slot)
        if seconds > 20 * 60:
            self.wait_until(lambda: node0.getconnectioncount() == 0
                            and node1.getconnectioncount() == 0, timeout=30)
            self.connect_nodes(0, 1)
        return slot

    def stake_and_sync(self):
        """node0 stakes one block on the current slot; both nodes see it."""
        node0 = self.nodes[0]
        node0.generatetoaddress(1, node0.getnewaddress())
        self.sync_blocks()
        return node0.getblock(node0.getbestblockhash(), True)

    def run_test(self):
        node0, node1 = self.nodes

        node0.generatetoaddress(5, node0.getnewaddress())
        self.sync_blocks()

        self.log.info("node1 advertises its beacon; node0 carries it in a block")
        self.advance_to_slot()
        advertised = node1.advertisebeacon()
        assert_equal(advertised["result"], "SUCCESS")
        public_key = advertised["public_key"]
        self.sync_mempools(flush_scheduler=False)
        self.stake_and_sync()
        assert_equal(len(node1.beaconstatus(CPID)["pending"]), 1)

        self.log.info("two superblocks from node0: the second activates the beacon")
        self.advance_to_slot()
        node0.generatesuperblock({CPID: 100})
        self.sync_blocks()
        self.advance_to_slot(SUPERBLOCK_SPACING + 600)
        node0.generatesuperblock({CPID: 100}, None, [public_key])
        self.sync_blocks()
        active = node1.beaconstatus(CPID)["active"]
        assert_equal(len(active), 1)
        assert_equal(active[0]["magnitude"], 100)
        beacon_address = active[0]["address"]
        assert_equal(node1.validateaddress(beacon_address)["ismine"], True)

        self.log.info("control: a block with no request pays nothing beyond the block subsidy")
        self.advance_to_slot(ACCRUAL_SPAN)
        control = self.stake_and_sync()
        assert_equal(control["claim"]["m_mrc_tx_map_size"], 0)
        block_subsidy = Decimal(control["claim"]["block_subsidy"])
        assert_equal(Decimal(str(control["mint"])), block_subsidy)

        self.log.info("node1 requests an MRC for its accrued reward")
        self.advance_to_slot()
        request = node1.createmrcrequest()
        mrc = request["mrc"]
        assert_equal(mrc["cpid"], CPID)
        research_subsidy = Decimal(mrc["research_subsidy"])
        fee = Decimal(mrc["fee"])
        assert_greater_than(research_subsidy, 0)
        assert_greater_than(fee, 0)
        assert_greater_than(research_subsidy, fee)
        assert_equal(mrc["last_block_hash"], node0.getbestblockhash())
        txid = request["txid"]
        self.sync_mempools(flush_scheduler=False)
        assert txid in node0.getrawmempool()

        self.log.info("node0's next coinstake pays the request to node1's beacon address")
        paid = self.stake_and_sync()
        claim = paid["claim"]
        assert_equal(claim["m_mrc_tx_map_size"], 1)
        assert_equal(node0.getrawmempool(), [])

        # getblock's verbose "tx" entries are objects here, not txid strings.
        coinstake_entry = paid["tx"][1]
        coinstake_txid = coinstake_entry["txid"] if isinstance(coinstake_entry, dict) else coinstake_entry
        coinstake = node0.getrawtransaction(coinstake_txid, True)
        payout = research_subsidy - fee
        payout_outputs = [
            out for out in coinstake["vout"]
            if Decimal(str(out["value"])) == payout
            and beacon_address in out["scriptPubKey"].get("addresses", [])
        ]
        assert_equal(len(payout_outputs), 1)

        # The whole research subsidy is minted: the payout to node1 plus the MRC
        # fee, which the staker and the foundation split between them. The only
        # other thing in the block is the request transaction itself, whose
        # network fee the coinstake also collects.
        request_fee = abs(Decimal(str(node1.gettransaction(txid)["fee"])))
        assert_greater_than(request_fee, 0)
        assert_equal(Decimal(str(paid["mint"])), block_subsidy + research_subsidy + request_fee)

        self.log.info("node1's wallet sees the payout as a spendable output")
        # Coinstake outputs are mature at once under -regtest
        # (CMerkleTx::GetBlocksToMaturity), so the payout is already unspent
        # and spendable from node1's point of view.
        received = [
            u for u in node1.listunspent(0)
            if u["txid"] == coinstake_txid and u["address"] == beacon_address
            and Decimal(str(u["amount"])) == payout
        ]
        assert_equal(len(received), 1)
        self.log.info("MRC paid: %s research subsidy, %s fee, %s to the requester",
                      research_subsidy, fee, payout)


if __name__ == "__main__":
    MrcTest().main()
