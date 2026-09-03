#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""A researcher's own coinstake pays its accrued research reward, on regtest.

Continues where feature_beacon_activation.py stops. The claim in a staked block
carries a research subsidy only when the staker's CPID has an ACTIVE beacon and
positive accrual at the block's time (CreateGridcoinReward in miner.cpp: the
subsidy is Tally::GetAccrual at block time, and a zero subsidy makes the miner
stake the block as a non-cruncher). Accrual for a CPID that has never staked
runs from the superblock that first carries it, at the superblock magnitude.

Two blocks are staked by the -forcecpid node and their claims compared:

  * with the beacon still PENDING (the superblock omitted it), the claim has no
    CPID and a zero research subsidy -- the negative control;
  * after a superblock activates the beacon at magnitude 100 and a day passes,
    the claim names the CPID, its research subsidy is positive, and the
    coinstake pays block subsidy plus research subsidy.

Without the reward path the second block would look like the first, so the
comparison is what discriminates.
"""

import os
from decimal import Decimal

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than

CPID = "00010203040506070809101112131415"

# See feature_beacon_activation.py for both constants.
STAKE_TIMESTAMP_MASK = 15
SUPERBLOCK_SPACING = 43200

# How long the activated CPID accrues before it stakes. Accrual is magnitude x
# magnitude unit x time, so any positive span works; a day keeps the amount
# comfortably above the coinstake's rounding.
ACCRUAL_SPAN = 86400


class ResearchRewardTest(GridcoinTestFramework):
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
        # Same BOINC stub as feature_beacon_activation.py: the CPID comes from
        # -forcecpid, and an empty client_state.xml keeps the host's BOINC out.
        boinc_dir = os.path.join(self.options.tmpdir, "boinc")
        os.makedirs(boinc_dir, exist_ok=True)
        client_state_path = os.path.join(boinc_dir, "client_state.xml")
        with open(client_state_path, "w", encoding="utf-8") as client_state:
            client_state.write("<client_state>\n</client_state>\n")
        self.extra_args[0].append(f"-boincdatadir={boinc_dir}")

        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def advance_to_slot(self, node, seconds=64):
        """Move mocktime forward onto a stake-timestamp boundary."""
        tip_time = node.getblock(node.getbestblockhash())["time"]
        slot = (tip_time + seconds) & ~STAKE_TIMESTAMP_MASK
        node.setmocktime(slot)
        return slot

    def stake_one(self, node):
        """Stake one block on the current slot and return its verbose getblock."""
        node.generatetoaddress(1, node.getnewaddress())
        return node.getblock(node.getbestblockhash(), True)

    def run_test(self):
        node = self.nodes[0]

        node.generatetoaddress(5, node.getnewaddress())

        self.log.info("advertise the beacon and carry it in a block")
        self.advance_to_slot(node)
        advertised = node.advertisebeacon()
        assert_equal(advertised["result"], "SUCCESS")
        public_key = advertised["public_key"]
        node.generatetoaddress(1, node.getnewaddress())
        assert_equal(node.beaconstatus(CPID)["active"], [])

        self.log.info("a superblock that omits the beacon leaves it pending")
        self.advance_to_slot(node)
        node.generatesuperblock({CPID: 100})
        assert_equal(node.beaconstatus(CPID)["active"], [])

        self.log.info("control: staking with a pending beacon pays no research reward")
        self.advance_to_slot(node, ACCRUAL_SPAN)
        control = self.stake_one(node)
        control_claim = control["claim"]
        assert control_claim["mining_id"] != CPID, control_claim
        assert_equal(Decimal(control_claim["research_subsidy"]), Decimal(0))
        block_subsidy = Decimal(control_claim["block_subsidy"])
        assert_greater_than(block_subsidy, 0)

        self.log.info("a superblock naming the beacon activates it")
        self.advance_to_slot(node, SUPERBLOCK_SPACING + 600)
        node.generatesuperblock({CPID: 100}, None, [public_key])
        active = node.beaconstatus(CPID)["active"]
        assert_equal(len(active), 1)
        assert_equal(active[0]["magnitude"], 100)

        # A day of accrual at magnitude 100 before the CPID stakes.
        self.advance_to_slot(node, ACCRUAL_SPAN)

        self.log.info("auditsnapshotaccrual survives a CPID with no accrual baseline")
        # The RPC's newbie correction used to dereference a null tally baseline
        # here (regtest never gets one) and take the node down with it. The
        # return value is not inspected: on a chain this short the audit's
        # history walk reports an empty object, which is the RPC's own gap.
        node.auditsnapshotaccrual(CPID, False)
        assert_greater_than(node.getblockcount(), 0)

        self.log.info("the researcher's coinstake pays block subsidy plus research subsidy")
        rewarded = self.stake_one(node)
        claim = rewarded["claim"]
        assert_equal(claim["mining_id"], CPID)
        research_subsidy = Decimal(claim["research_subsidy"])
        assert_greater_than(research_subsidy, 0)
        assert_equal(Decimal(claim["block_subsidy"]), block_subsidy)

        # getblock's "mint" is what the coinstake created beyond its input: with
        # an empty mempool that is the block subsidy plus the research subsidy.
        minted = Decimal(str(rewarded["mint"]))
        assert_equal(minted, block_subsidy + research_subsidy)
        assert_equal(Decimal(str(control["mint"])), block_subsidy)
        self.log.info("coinstake minted %s = %s block + %s research",
                      minted, block_subsidy, research_subsidy)


if __name__ == "__main__":
    ResearchRewardTest().main()
