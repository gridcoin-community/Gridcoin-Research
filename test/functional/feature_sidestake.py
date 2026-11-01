#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Phase 4A local sidestaking on regtest.

Investor-mode only: a sidestake is a coinstake reward split, NOT a researcher
feature, so it works on plain PoS blocks with no beacon/CPID.

Local sidestaking is configured by startup args (no addsidestake RPC):
  -enablesidestaking=1 -sidestake=<address>,<percent>
The destination must exist before the node starts, so we mint it first, then
restart with sidestaking enabled.

Asserts the config path end to end: the configured local sidestake entry is
surfaced by listsidestakes, the chain still advances (stakes) with sidestaking
enabled, and the coinstake actually carries a sidestake output paying the
configured destination.

That last assertion needs the regtest mining path to run SplitCoinStakeOutput,
which it did not until issue #3229 -- generatetoaddress goes through
TryMineRegtestBlock, and only StakeMiner used to call the splitter.
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal


class SidestakeTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0", "-connect=0", "-listen=0"]]

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def run_test(self):
        node = self.nodes[0]

        # Mint the sidestake destination, then restart with local sidestaking on
        # (the -sidestake arg is read at startup, so the address must pre-exist).
        ss_addr = node.getnewaddress()
        ss_key = node.dumpprivkey(ss_addr)
        self.restart_node(0, extra_args=[
            "-staking=0", "-connect=0", "-listen=0",
            "-enablesidestaking=1", "-sidestake={},50".format(ss_addr),
        ])
        node = self.nodes[0]

        # restart_node is a full stop/start, and under regtest the wallet's BDB env is an
        # in-memory mock rebuilt from scratch on every launch with a fresh HD seed, so ss_addr
        # is NOT ours in the new process. -sidestake= only records a destination and an
        # allocation; it registers no key or watch-only script. Without re-importing,
        # getreceivedbyaddress would short-circuit to 0 no matter what the miner produced.
        node.importprivkey(ss_key, "sidestake")

        # --- config path: listsidestakes surfaces the configured local entry ---
        listed = node.listsidestakes()
        assert ss_addr in str(listed), \
            "configured sidestake address not surfaced by listsidestakes: {}".format(listed)
        self.log.info("local sidestake configured + listed: %s @ 50%%", ss_addr)

        # --- chain still advances (stakes) with sidestaking enabled ---
        start = node.getblockcount()
        stake_addr = node.getnewaddress()
        blocks = node.generatetoaddress(10, stake_addr)
        assert_equal(node.getblockcount(), start + 10)

        # --- the coinstake actually carries the sidestake output ---
        #
        # This is the assertion that discriminates, and it is deliberately wallet-independent:
        # it reads the raw transaction rather than a balance, so it cannot be satisfied by
        # anything other than the miner having split the coinstake. Before issue #3229 the
        # regtest coinstake had exactly two outputs and this failed.
        coinstake_txid = node.getblock(blocks[0])["tx"][1]
        coinstake = node.getrawtransaction(coinstake_txid, 1)

        assert_equal(coinstake["vout"][0]["value"], 0)  # the empty coinstake marker
        assert_equal(coinstake["vout"][1]["scriptPubKey"]["type"], "pubkey")

        # The sidestake output is P2PKH (SetDestination), unlike the P2PK coinstake outputs, and
        # the output reversal in SplitCoinStakeOutput puts it past the stake outputs.
        ss_outs = [o for o in coinstake["vout"]
                   if o["n"] >= 2 and ss_addr in o["scriptPubKey"].get("addresses", [])]
        assert_equal(len(ss_outs), 1)
        # 50% of the 10 GRC regtest constant block reward.
        assert_equal(ss_outs[0]["value"], 5)
        self.log.info("coinstake %s carries a %s GRC sidestake output to %s",
                      coinstake_txid, ss_outs[0]["value"], ss_addr)

        # --- and the wallet counts it as received ---
        #
        # Secondary guard, on the receivedby* tally rather than the miner. 5 GRC per block over
        # 10 blocks, all mature immediately because regtest is a mockable chain.
        received = node.getreceivedbyaddress(ss_addr, 0)
        assert received > 0, \
            "sidestake receipts not counted for {}: {}".format(ss_addr, received)
        self.log.info("sidestake address %s received %s GRC over 10 staked blocks "
                      "(expected 50 = 10 blocks x 50%% of the 10 GRC reward)", ss_addr, received)

        # The staking destination is a coinstake receipt too, counted net of the staked
        # principal. It is P2PK in the coinstake even though getnewaddress handed out a P2PKH
        # address, so this also covers destination matching.
        stake_dest = coinstake["vout"][1]["scriptPubKey"]["addresses"][0]
        stake_received = node.getreceivedbyaddress(stake_dest, 0)
        assert stake_received > 0, \
            "coinstake receipt not counted for staking address {}: {}".format(
                stake_dest, stake_received)
        self.log.info("staking address %s received %s GRC (net of the staked principal)",
                      stake_dest, stake_received)


if __name__ == "__main__":
    SidestakeTest().main()
