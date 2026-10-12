#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Votes claim the wallet's own recirculated change by its origin (v15 polls).

The wallet claims only outputs with at least CLAIM_DEPTH confirmations
(CWalletTx::IsConfirmed). For polls that start at or after the v15 height, the
tally follows a claimed output through a coinstake and through any vote
transaction to their outputs at the same address. A young output that one of
the wallet's own votes or coinstakes paid back to the address of one of its
inputs is therefore claimed by naming that input (its origin) once the input
has the usual confirmations; the tally walks forward from it. A claim never
names an outpoint younger than CLAIM_DEPTH.

node0 holds the regtest premine and creates the polls; node2 stakes the blocks.
The voters:

  - node1 starts with a single coin: immediate re-votes (on a poll mined in the
    block at exactly V15_HEIGHT), votes on two polls in one sitting, a vote in
    the block after its coin stakes, a plain send of its coin to its own
    address, votes cast before its last vote confirms, and finally a poll that
    started before v15;
  - node3 holds 30 small coins and one large one;
  - node4 holds one large coin and a few small ones, and chains votes deeper
    than the trace limit;
  - node5 holds an ordinary coin and a 2-of-2 multisig output of its own keys;
  - node6 holds a single coin and splits its stakes (-enablestakesplit), so
    several young outputs trace to one origin;
  - node7 holds 1.01 and 0.5 GRC at one address, so its first vote spends both;
  - node8 holds 1.012 GRC at a single-key address and 1.015 GRC at a multisig
    address of its own keys, so its first vote spends both;
  - node9 holds only a multisig output of its own keys that it has just
    received.

node3 also votes again before a block confirms its first vote, on one poll and
then on two: the second claim names the first vote's mature input through that
vote's unconfirmed change. A poll mined one block before V15_HEIGHT keeps the
earlier rules, so node1's re-vote there waits for the confirmations.

Weights are compared with each voter's unspent outputs once its votes are
confirmed and buried.
"""

from decimal import Decimal

from test_framework.authproxy import JSONRPCException
from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than, assert_raises_rpc_error

STAKE_TIMESTAMP_MASK = 15
# Block spacing on the mock clock: close to the 90 s target, so the difficulty
# stays where node2's weight can meet it on every block.
SPACING = 96

V15_HEIGHT = 30
POLL_DAYS = 7
CLAIM_DEPTH = 10
# The wallet's limit on how many of its own vote or coinstake transactions it
# traces back through to find a claimable origin (MAX_TRACE_DEPTH, builders.cpp).
MAX_TRACE_DEPTH = 16
# node1's single coin, which node1 also stakes: at the regtest minimum
# difficulty 50000 GRC finds a kernel on every stake slot.
FUNDING = Decimal("50000")
FRAGMENTS = 30
FRAGMENT = Decimal("2")
LARGE = Decimal("1000")
SMALL = Decimal("10")
MULTISIG_FUNDING = Decimal("2")
# A vote's burn for one claimed output is 0.021 GRC, and largest-first fee
# selection wants burn + 1 GRC, so a largest coin below 1.021 GRC takes a second.
TWO_INPUT_LARGE = Decimal("1.01")
TWO_INPUT_SMALL = Decimal("0.5")
# Both below burn + 1 GRC, so node8's first vote spends both; the multisig coin
# is the larger, and the change still goes to the single-key coin's address.
MIXED_SINGLE_KEY = Decimal("1.012")
MIXED_MULTISIG = Decimal("1.015")
FEE = Decimal("0.01")

# addpoll arguments: the weight type is passed as PollWeightType - 1, so 1 is
# BALANCE; response type 2 is SINGLE_CHOICE. SURVEY is the one poll type that
# may weigh by balance alone, which keeps magnitude and superblocks out of it.
WEIGHT_BALANCE = 1
RESPONSE_SINGLE_CHOICE = 2


class VoteOriginClaimsTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 10
        self.chain = "regtest"
        self.setup_clean_chain = True
        common = ["-staking=0", f"-blockv15height={V15_HEIGHT}", "-debug=vote"]
        self.extra_args = [common[:] for _ in range(self.num_nodes)]
        self.extra_args[6].append("-enablestakesplit=1")

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()
        self.connect_all()

    def connect_all(self):
        # Two hubs, so no node holds more than six links: past six, a node drops
        # every link older than two hours on its clock (CConnman inactivity check).
        for i in (1, 2, 3, 4, 5):
            self.connect_nodes(0, i)
        for i in (6, 7, 8, 9):
            self.connect_nodes(2, i)

    def stake(self, node=None):
        """Stake one block SPACING seconds on (node2 unless told) and sync all nodes.

        Every pending transaction is stamped before the new slot, so the block
        carries them (see advance_to_next_stake_slot).
        """
        node = node or self.nodes[2]
        now = max(n.mock_now() for n in self.nodes)
        target = (now + SPACING) & ~STAKE_TIMESTAMP_MASK
        for n in self.nodes:
            n.advance_mocktime_to(target)
        node.generatetoaddress(1, node.getnewaddress())
        self.sync_blocks(wait=0.1)

    def confirm(self, node=None):
        """Carry the mempool into the next block."""
        self.sync_mempools(wait=0.1, flush_scheduler=False)
        self.stake(node)

    def bury(self):
        """Stake CLAIM_DEPTH blocks, so the newest coins can be claimed."""
        for _ in range(CLAIM_DEPTH):
            self.stake()

    def send_raw(self, node, inputs, outputs):
        raw = node.createrawtransaction(
            [{"txid": u["txid"], "vout": u["vout"]} for u in inputs], outputs)
        signed = node.signrawtransactionwithwallet(raw)
        assert_equal(signed["complete"], True)
        return node.sendrawtransaction(signed["hex"])

    def take_premine_off_shared_key(self):
        """Take the premine off the key every regtest node holds.

        init.cpp plants the premine key in every regtest wallet, so node1 would
        otherwise own node0's coins and vote with them. node0 spends nine premine
        outputs to its own addresses and node2's in a block it stakes with the
        tenth, then spends that block's coinstake output, which returns to the
        shared key, in a block node2 stakes. The premine outputs are the ones with
        more confirmations than the chain height (see feature_reorg_conflicted.py).
        """
        node0, node1, node2 = self.nodes[:3]
        premine = [u for u in node0.listunspent(0) if u["confirmations"] > node0.getblockcount()]
        assert_equal(len(premine), 10)
        spent = premine[:9]
        outputs = {node0.getnewaddress(): Decimal("150000") for _ in range(2)}
        outputs.update({node2.getnewaddress(): Decimal("150000") for _ in range(3)})
        outputs[node2.getnewaddress()] = sum(u["amount"] for u in spent) - Decimal("750000") - FEE
        self.send_raw(node0, spent, outputs)
        self.confirm(node0)

        shared = node1.listunspent(0)
        assert_equal(len(shared), 1)
        self.send_raw(node0, shared, {node0.getnewaddress(): shared[0]["amount"] - FEE})
        self.confirm()
        assert_equal(node1.listunspent(0), [])

    def add_poll(self, title):
        poll = self.nodes[0].addpoll("survey", title, POLL_DAYS, "Which?", "A;B",
                                     WEIGHT_BALANCE, RESPONSE_SINGLE_CHOICE,
                                     "https://gridcoin.us")
        self.confirm()
        return poll["id"]

    def results(self, poll_id):
        return self.nodes[1].getpollresults(poll_id)

    @staticmethod
    def claimed(voter, txid):
        """The outpoints a vote's balance claim names, as (txid, vout) pairs."""
        return [(o["txid"], o["offset"])
                for address in voter.getvotingclaim(txid)["balance_claim"]
                for o in address["outpoints"]]

    def cast(self, voter, poll, choice):
        """Vote now and return the vote's txid.

        Checked before the vote is mined, while the chain is where the wallet
        built the claim: every outpoint the claim names has at least CLAIM_DEPTH
        confirmations.
        """
        txid = voter.votebyid(poll, choice)["vote_txid"]
        for outpoint in self.claimed(voter, txid):
            depth = voter.gettransaction(outpoint[0])["confirmations"]
            assert depth >= CLAIM_DEPTH, (outpoint, depth)
        return txid

    @staticmethod
    def outpoints(unspent):
        return [(u["txid"], u["vout"]) for u in unspent]

    @staticmethod
    def own_multisig_address(node):
        """A 2-of-2 multisig (P2SH) address of two of the node's own keys."""
        keys = [node.validateaddress(node.getnewaddress())["pubkey"] for _ in range(2)]
        return node.addmultisigaddress(2, keys)

    def fund_voters(self):
        """Fund the voters from node0; return node5's multisig outpoint, node4's
        large coin and node8's multisig outpoint."""
        node0 = self.nodes[0]
        single, fragmented, deep, multisig, splitter, sub_coin, mixed = (
            self.nodes[i] for i in (1, 3, 4, 5, 6, 7, 8))
        for voter in (single, fragmented, deep, multisig, splitter, sub_coin, mixed):
            assert_equal(voter.listunspent(0), [])

        p2sh = self.own_multisig_address(multisig)
        mixed_p2sh = self.own_multisig_address(mixed)
        deep_large = deep.getnewaddress()
        sub_coin_address = sub_coin.getnewaddress()

        outputs = {single.getnewaddress(): FUNDING, splitter.getnewaddress(): FUNDING}
        outputs.update({fragmented.getnewaddress(): FRAGMENT for _ in range(FRAGMENTS)})
        outputs[fragmented.getnewaddress()] = LARGE
        outputs[deep_large] = LARGE
        outputs.update({deep.getnewaddress(): SMALL for _ in range(3)})
        outputs[multisig.getnewaddress()] = LARGE
        outputs[p2sh] = MULTISIG_FUNDING
        outputs[sub_coin_address] = TWO_INPUT_LARGE
        outputs[mixed.getnewaddress()] = MIXED_SINGLE_KEY
        outputs[mixed_p2sh] = MIXED_MULTISIG
        txid = node0.sendmany("", outputs)
        # sendmany pays each address once; the second coin at node7's address
        # comes in its own transaction.
        node0.sendtoaddress(sub_coin_address, TWO_INPUT_SMALL)
        self.confirm()

        vouts = node0.getrawtransaction(txid, 1)["vout"]

        def outpoint_to(address):
            found = [o["n"] for o in vouts if o["scriptPubKey"].get("addresses") == [address]]
            assert_equal(len(found), 1)
            return (txid, found[0])

        return outpoint_to(p2sh), outpoint_to(deep_large), outpoint_to(mixed_p2sh)

    def scenario_single_coin_immediate_revotes(self, voter, poll):
        """Single coin: vote, then two re-votes without waiting; the newest vote
        carries the coin, and every claim names the coin's mature origin."""
        coin = self.outpoints(voter.listunspent(0))
        assert_equal(len(coin), 1)
        votes = []
        for choice in (0, 1, 0):
            votes.append(self.cast(voter, poll, choice))
            self.confirm()
        self.bury()
        for txid in votes:
            assert_equal(self.claimed(voter, txid), coin)
        held = sum(u["amount"] for u in voter.listunspent(0))
        result = self.results(poll)
        assert_equal(result["total_weight"], held)
        assert_equal(result["responses"][0]["weight"], held)
        assert_equal(result["responses"][1]["weight"], 0)
        assert_equal((result["invalid_votes"], result["superseded_votes"]), (0, 2))

    def scenario_two_polls_in_one_sitting(self, voter):
        """Vote on A then immediately on B, then immediately re-vote A: both polls
        carry the coin."""
        poll_a, poll_b = self.add_poll("sitting a"), self.add_poll("sitting b")
        self.cast(voter, poll_a, 0)
        self.confirm()
        self.cast(voter, poll_b, 1)
        self.confirm()
        self.cast(voter, poll_a, 1)
        self.confirm()
        self.bury()
        held = sum(u["amount"] for u in voter.listunspent(0))
        assert_equal(self.results(poll_a)["total_weight"], held)
        assert_equal(self.results(poll_a)["responses"][1]["weight"], held)
        assert_equal(self.results(poll_b)["total_weight"], held)

    def scenario_vote_right_after_stake(self, voter, split=False):
        """The coin stakes and the wallet votes in the next block: the claim names
        the staked input, and the vote carries the coinstake output. When the
        stake splits, every split output traces to that input, named once."""
        poll = self.add_poll("after split stake" if split else "after stake")
        staked = self.outpoints(voter.listunspent(0))
        assert_equal(len(staked), 1)
        self.stake(voter)
        assert_equal(len(voter.listunspent(0)) > 1, split)
        txid = self.cast(voter, poll, 0)
        assert_equal(self.claimed(voter, txid), staked)
        self.confirm()
        self.bury()
        held = sum(u["amount"] for u in voter.listunspent(0))
        assert_equal(self.results(poll)["total_weight"], held)

    def scenario_fragmented_wallet(self, voter, fragments):
        """A wallet with 30 small coins and one large one re-votes immediately: the
        claim lists every coin, and the new answer carries them all."""
        poll = self.add_poll("fragmented")
        self.cast(voter, poll, 0)
        self.confirm()
        txid = self.cast(voter, poll, 1)
        self.confirm()
        self.bury()
        assert_equal(len(self.claimed(voter, txid)), fragments + 1)
        held = sum(u["amount"] for u in voter.listunspent(0) if u["amount"] >= 1)
        result = self.results(poll)
        assert_equal(result["responses"][1]["weight"], held)
        assert_equal(result["responses"][0]["weight"], 0)

    def scenario_revote_before_the_block(self, voter):
        """A wallet with many coins re-votes before its first vote confirms. The
        re-vote's claim names the first vote's mature input through that vote's
        unconfirmed change, so once both confirm the new answer carries the
        whole holding and the first vote is superseded."""
        poll = self.add_poll("re-vote before the block")
        first = self.cast(voter, poll, 0)
        second = self.cast(voter, poll, 1)
        spent = {(i["txid"], i["vout"]) for i in voter.getrawtransaction(first, 1)["vin"]}
        assert spent & set(self.claimed(voter, second)), "the re-vote names none of the first vote's inputs"
        self.confirm()
        self.bury()
        held = sum(u["amount"] for u in voter.listunspent(0) if u["amount"] >= 1)
        result = self.results(poll)
        assert_equal(result["responses"][1]["weight"], held)
        assert_equal(result["responses"][0]["weight"], 0)
        assert_equal(result["total_weight"], held)
        assert_equal((result["invalid_votes"], result["superseded_votes"]), (0, 1))

    def scenario_two_polls_before_the_block(self, voter):
        """The same wallet votes on two polls before the first vote confirms: both
        polls carry the whole holding."""
        poll_a, poll_b = self.add_poll("before the block a"), self.add_poll("before the block b")
        self.cast(voter, poll_a, 0)
        self.cast(voter, poll_b, 1)
        self.confirm()
        self.bury()
        held = sum(u["amount"] for u in voter.listunspent(0) if u["amount"] >= 1)
        for poll in (poll_a, poll_b):
            result = self.results(poll)
            assert_equal(result["total_weight"], held)
            assert_equal(result["invalid_votes"], 0)

    def scenario_multisig_output(self, voter, multisig):
        """A 2-of-2 multisig output of the voter's own keys has no P2PK/P2PKH
        destination: no claim names it, and the votes beside it succeed and carry
        the voter's other coin."""
        poll = self.add_poll("multisig")
        # The wallet counts the output as its own, so it is among the vote's
        # candidate coins.
        assert multisig in self.outpoints(voter.listunspent(0))
        votes = [self.cast(voter, poll, 0)]
        self.confirm()
        votes.append(self.cast(voter, poll, 1))
        self.confirm()
        self.bury()
        for txid in votes:
            assert multisig not in self.claimed(voter, txid), txid
        unspent = voter.listunspent(0)
        assert multisig in self.outpoints(unspent)
        held = sum(u["amount"] for u in unspent if (u["txid"], u["vout"]) != multisig)
        result = self.results(poll)
        assert_equal(result["total_weight"], held)
        assert_equal(result["responses"][1]["weight"], held)

    def scenario_sub_coin_input(self, voter):
        """The first vote spends two coins at one address, 1.01 and 0.5 GRC. The
        immediate re-vote claims the change by the 1.01 GRC input alone, since a
        claim never names an output under 1 GRC, and it carries the change."""
        poll = self.add_poll("input under 1 GRC")
        unspent = voter.listunspent(0)
        assert_equal(sorted(u["amount"] for u in unspent), [TWO_INPUT_SMALL, TWO_INPUT_LARGE])
        assert_equal(len({u["address"] for u in unspent}), 1)
        large = [(u["txid"], u["vout"]) for u in unspent if u["amount"] == TWO_INPUT_LARGE]
        small = [(u["txid"], u["vout"]) for u in unspent if u["amount"] == TWO_INPUT_SMALL]
        first = self.cast(voter, poll, 0)
        self.confirm()
        spent = [(i["txid"], i["vout"]) for i in voter.getrawtransaction(first, 1)["vin"]]
        assert_equal(sorted(spent), sorted(large + small))
        second = self.cast(voter, poll, 1)
        assert_equal(self.claimed(voter, second), large)
        self.confirm()
        self.bury()
        held = sum(u["amount"] for u in voter.listunspent(0))
        result = self.results(poll)
        assert_equal(result["responses"][1]["weight"], held)
        assert_equal(result["responses"][0]["weight"], 0)
        assert_equal(result["total_weight"], held)

    def scenario_other_address_input(self, voter, multisig):
        """The first vote spends two coins at different addresses: 1.012 GRC at a
        single-key address and 1.015 GRC at a multisig address of the voter's own
        keys. The change goes to the single-key coin's address, though the
        multisig coin is the larger input. The immediate re-vote claims the
        change by the input at its own address only; the multisig input is never
        named as an origin, and the re-vote carries the change."""
        poll = self.add_poll("input at another address")
        single = [(u["txid"], u["vout"]) for u in voter.listunspent(0)
                  if (u["txid"], u["vout"]) != multisig]
        assert_equal(len(single), 1)
        first = self.cast(voter, poll, 0)
        self.confirm()
        spent = [(i["txid"], i["vout"]) for i in voter.getrawtransaction(first, 1)["vin"]]
        assert_equal(sorted(spent), sorted(single + [multisig]))
        second = self.cast(voter, poll, 1)
        assert_equal(self.claimed(voter, second), single)
        self.confirm()
        self.bury()
        held = sum(u["amount"] for u in voter.listunspent(0))
        result = self.results(poll)
        assert_equal(result["responses"][1]["weight"], held)
        assert_equal(result["responses"][0]["weight"], 0)
        assert_equal(result["total_weight"], held)

    def scenario_poll_one_block_before_v15(self, voter, early_poll):
        """A poll mined one block before V15_HEIGHT keeps the earlier rules: a
        re-vote in the block after the first vote cannot claim that vote's change
        by its origin, so it is refused until the change has the usual
        confirmations."""
        self.cast(voter, early_poll, 0)
        self.confirm()
        assert_raises_rpc_error(None, f"need {CLAIM_DEPTH - 1} more confirmation",
                                voter.votebyid, early_poll, 1)

    def scenario_plain_send_to_self(self, voter):
        """A plain transaction, neither a vote nor a coinstake, pays the voter's only
        coin back to its own address. Only votes and coinstakes are traced, so the
        vote is refused until that output has the usual confirmations."""
        poll = self.add_poll("plain send to self")
        coin = voter.listunspent(0)
        assert_equal(len(coin), 1)
        self.send_raw(voter, coin, {coin[0]["address"]: coin[0]["amount"] - FEE})
        self.confirm()
        assert_raises_rpc_error(None, f"need {CLAIM_DEPTH - 1} more confirmation",
                                voter.votebyid, poll, 0)
        self.bury()

    def scenario_vote_before_the_change_confirms(self, voter, legacy_poll):
        """A single-coin wallet votes again before its last vote confirms, so its
        only coin is that vote's unconfirmed change. On a v15 poll the change is
        claimed by its origin at once, and both polls carry the whole coin; on a
        poll that started before v15 the change needs CLAIM_DEPTH confirmations."""
        poll_a, poll_b = self.add_poll("before the change confirms a"), self.add_poll("before the change confirms b")
        coin = self.outpoints(voter.listunspent(0))
        assert_equal(len(coin), 1)
        self.cast(voter, poll_a, 0)
        assert_equal(self.claimed(voter, self.cast(voter, poll_b, 0)), coin)
        self.confirm()
        self.bury()
        held = sum(u["amount"] for u in voter.listunspent(0))
        assert_equal(self.results(poll_a)["total_weight"], held)
        assert_equal(self.results(poll_b)["total_weight"], held)

        self.cast(voter, legacy_poll, 0)
        assert_raises_rpc_error(None, f"need {CLAIM_DEPTH} more confirmation",
                                voter.votebyid, legacy_poll, 1)
        self.confirm()
        self.bury()

    def scenario_young_multisig_output_only(self, voter, poll):
        """A wallet whose only coin is a multisig output of its own keys, one
        block deep, has no single-key coin to wait for: the refusal names no
        number of confirmations."""
        p2sh = self.own_multisig_address(voter)
        self.nodes[0].sendtoaddress(p2sh, MULTISIG_FUNDING)
        self.confirm()
        unspent = voter.listunspent(0)
        assert_equal([(u["address"], u["confirmations"]) for u in unspent], [(p2sh, 1)])
        try:
            voter.votebyid(poll, 0)
        except JSONRPCException as e:
            assert_equal(e.error["message"], "No eligible outputs greater than 1 GRC.")
        else:
            raise AssertionError("a vote with no single-key coin was not refused")

    def scenario_trace_limit(self, voter, large):
        """Two votes per block for eight blocks chain one lineage sixteen votes deep
        inside the claim window. The next vote still traces it to the large coin;
        one more link puts the coin past the trace limit, and the vote after that
        skips the young coin. Once it matures the next vote
        carries it.

        One vote per block can never reach the limit (16 > the 10-block window), so
        the voter holds other mature coins that keep each vote claimable while the
        largest coin's change pays every fee (largest-first) and chains through
        the mempool."""
        poll_a, poll_b = self.add_poll("trace limit a"), self.add_poll("trace limit b")
        others = [u for u in voter.listunspent(0) if (u["txid"], u["vout"]) != large]
        assert_equal(len(others), 3)
        others_held = sum(u["amount"] for u in others)
        for _ in range(MAX_TRACE_DEPTH // 2):
            self.cast(voter, poll_a, 0)
            self.cast(voter, poll_b, 0)
            self.confirm()
            for poll in (poll_a, poll_b):
                result = self.results(poll)
                held = sum(u["amount"] for u in voter.listunspent(0))
                assert result["total_weight"] <= held, (result, held)

        self.log.info("  sixteen links: the trace reaches the large coin at the last step it takes")
        txid = self.cast(voter, poll_a, 0)
        assert_equal(sorted(self.claimed(voter, txid)), sorted(self.outpoints(others) + [large]))
        self.confirm()

        self.log.info("  seventeen links: past the trace limit the young coin is skipped")
        txid = self.cast(voter, poll_a, 1)
        assert_equal(sorted(self.claimed(voter, txid)), sorted(self.outpoints(others)))
        self.confirm()
        held = sum(u["amount"] for u in voter.listunspent(0))
        result = self.results(poll_a)
        # The vote that skipped the young coin carries only the other coins; the
        # previous vote, the newest to reach the large coin's lineage, keeps it.
        assert_equal(result["responses"][1]["weight"], others_held)
        assert_equal(result["responses"][0]["weight"], held - others_held)
        assert_equal(result["total_weight"], held)

        self.log.info("  once the lineage matures the next vote carries it")
        self.bury()
        self.cast(voter, poll_a, 1)
        self.confirm()
        self.bury()
        held = sum(u["amount"] for u in voter.listunspent(0))
        result = self.results(poll_a)
        assert_equal(result["responses"][1]["weight"], held)
        assert_equal(result["responses"][0]["weight"], 0)
        assert_equal(result["total_weight"], held)

    def scenario_before_v15_wallet_messages(self, voter, legacy_poll):
        """A poll that started before v15: an immediate re-vote is refused with the
        confirmation message; the wallet claims only mature coins."""
        self.cast(voter, legacy_poll, 0)
        self.confirm()
        assert_raises_rpc_error(None, f"need {CLAIM_DEPTH - 1} more confirmation",
                                voter.votebyid, legacy_poll, 1)

        # With two young coins the wait is the deeper coin's.
        self.nodes[0].sendtoaddress(voter.getnewaddress(), SMALL)
        self.confirm()
        depths = sorted(u["confirmations"] for u in voter.listunspent(0))
        assert_equal(depths, [1, 2])
        assert_raises_rpc_error(None, f"need {CLAIM_DEPTH - 2} more confirmation",
                                voter.votebyid, legacy_poll, 1)

    def run_test(self):
        node0, node1 = self.nodes[:2]

        self.take_premine_off_shared_key()
        self.bury()

        legacy_poll = self.add_poll("before v15")
        assert_greater_than(V15_HEIGHT, self.results(legacy_poll)["starting_block_height"])

        multisig, large, mixed_multisig = self.fund_voters()
        self.bury()

        self.log.info("a poll mined one block before V15_HEIGHT, and one in the block at exactly V15_HEIGHT")
        assert_greater_than(V15_HEIGHT - 2, node0.getblockcount())
        while node0.getblockcount() < V15_HEIGHT - 2:
            self.stake()
        early_poll = self.add_poll("one block before v15")
        assert_equal(self.results(early_poll)["starting_block_height"], V15_HEIGHT - 1)
        boundary_poll = self.add_poll("immediate re-votes")
        assert_equal(self.results(boundary_poll)["starting_block_height"], V15_HEIGHT)

        self.log.info("a single coin votes, then two re-votes without waiting (poll at V15_HEIGHT)")
        self.scenario_single_coin_immediate_revotes(node1, boundary_poll)

        self.log.info("votes on two polls in one sitting")
        self.scenario_two_polls_in_one_sitting(node1)

        self.log.info("a vote in the block after the coin stakes")
        self.scenario_vote_right_after_stake(node1)
        self.log.info("a vote in the block after a split stake")
        self.scenario_vote_right_after_stake(self.nodes[6], split=True)

        self.log.info("a fragmented wallet re-votes without waiting")
        self.scenario_fragmented_wallet(self.nodes[3], FRAGMENTS)

        self.log.info("a wallet with many coins re-votes before its first vote confirms")
        self.scenario_revote_before_the_block(self.nodes[3])
        self.log.info("a wallet with many coins votes on two polls before its first vote confirms")
        self.scenario_two_polls_before_the_block(self.nodes[3])

        self.log.info("a multisig output is never claimed")
        self.scenario_multisig_output(self.nodes[5], multisig)

        self.log.info("an input under 1 GRC is never named as an origin")
        self.scenario_sub_coin_input(self.nodes[7])

        self.log.info("an input at another address, a multisig one, is never named as an origin")
        self.scenario_other_address_input(self.nodes[8], mixed_multisig)

        self.log.info("a plain send to the voter's own address is not traced")
        self.scenario_plain_send_to_self(node1)

        self.log.info("a poll mined one block before V15_HEIGHT keeps the earlier rules")
        self.scenario_poll_one_block_before_v15(node1, early_poll)

        self.log.info("re-votes deeper than the trace limit (%d)", MAX_TRACE_DEPTH)
        self.scenario_trace_limit(self.nodes[4], large)

        self.log.info("a vote cast before the last vote confirms")
        self.scenario_vote_before_the_change_confirms(node1, legacy_poll)

        self.log.info("a young multisig output alone gives no wait")
        self.scenario_young_multisig_output_only(self.nodes[9], boundary_poll)

        self.log.info("a poll that started before v15 keeps the confirmation rule")
        self.scenario_before_v15_wallet_messages(node1, legacy_poll)


if __name__ == "__main__":
    VoteOriginClaimsTest().main()
