#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Poll results under seeded random activity, compared with an independent tally.

Three balance polls start at or after the v15 height. A seeded random sequence
of votes, immediate re-votes, votes with a claim of chosen outputs (through
votewithclaim), plain sends, stakes, blocks, reorganizations and node restarts
runs against them. After
every step, getpollresults on every node must equal the tally that
test_framework/vote_oracle.py computes from chain data alone: the total weight,
each response's weight, the invalid votes, the superseded votes and the linked
votes. The oracle
writes the v15 rules out again in Python; when the two disagree, the test
fails.

After the steps the polls end. A vote cast while a poll is open and mined in
the first block past its end is not linked. From v15 a poll starts at its block
and ends at its closing block, the block before its first block past the end;
feature_poll_v15_window.py checks that boundary to the block (a block stamped at
the end leaves the poll open, the next one closes it), and the unit tests check
the closing-block rule.

node0 holds the regtest premine, funds the voters and creates the polls;
node1 stakes the blocks; node2-4 vote, send and stake, and node2 is the side
that splits off in a reorganization; node5 only observes, and it is the node
the restart action restarts (a regtest wallet is rebuilt on restart, so a voter
would lose its keys). node0 links to every other node, so no node holds more
than five links.

--seed fixes the sequence of actions and choices, but not every interleaving
(the keys, transaction sizes and relay order differ from run to run), so a
seed is an aid to reproducing a failure, not an exact replay. --steps sets the
number of random steps. A run at the default number of steps, whatever its
seed, must reach each rule assert_coverage() lists.
"""

from collections import Counter, defaultdict
from decimal import Decimal
import random
import time

from test_framework.authproxy import JSONRPCException
from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than, p2p_port
from test_framework.vote_oracle import VoteOracle, sats

STAKE_TIMESTAMP_MASK = 15
STAKE_SLOT = 16
# Block spacing on the mock clock: close to the 90 s target, so the difficulty
# stays where the stakers' weight meets it on every block.
SPACING = 96

V15_HEIGHT = 1
POLL_DAYS = 7
CLAIM_DEPTH = 10
FEE = Decimal("0.01")
# At the regtest minimum difficulty a 60000 GRC coin finds a kernel on every
# stake slot; a voter whose largest confirmed coin is at least STAKE_READY can
# stake.
STAKE_COIN = Decimal("60000")
STAKE_READY = Decimal("50000")

FUNDER = 0
STAKER = 1
VOTERS = (2, 3, 4)
REORG_NODE = 2
OBSERVER = 5

# addpoll arguments: the weight type is passed as PollWeightType - 1, so 1 is
# BALANCE; response type 2 is SINGLE_CHOICE and 3 MULTIPLE_CHOICE. SURVEY is
# the one poll type that may weigh by balance alone.
WEIGHT_BALANCE = 1
RESPONSE_SINGLE_CHOICE = 2
RESPONSE_MULTIPLE_CHOICE = 3

# The vote builders' refusals a random step may meet: coins too young or not
# confirmed, too little to pay the vote's fee, or nothing to claim. Any other
# RPC error fails the test.
ALLOWED_REFUSALS = ("confirmation", "Insufficient funds", "No eligible outputs")

# The wallet's limit on how many of its own vote or coinstake transactions it
# traces back through to find a claimable origin (MAX_TRACE_DEPTH, builders.cpp).
MAX_TRACE_DEPTH = 16

DEFAULT_SEED = 1
DEFAULT_STEPS = 40

ACTIONS = [
    (25, "vote"),
    (10, "revote"),
    (10, "vote_with_claim"),
    (15, "send"),
    (10, "voter_stakes"),
    (20, "mine"),
    (5, "reorg"),
    (5, "restart"),
]
MORE_ACTIONS = [
    (15, "send"),
    (10, "voter_stakes"),
    (20, "mine"),
]
MORE_STEPS = 15


class VoteTallyOracleTest(GridcoinTestFramework):
    def add_options(self, parser):
        parser.add_argument("--seed", dest="seed", type=int, default=DEFAULT_SEED,
                            help="seed of the random sequence of actions; it fixes the actions, not every "
                                 "interleaving, so it aids reproduction but is not an exact replay "
                                 "(default: %d)" % DEFAULT_SEED)
        parser.add_argument("--steps", dest="steps", type=int, default=DEFAULT_STEPS,
                            help="number of random steps before the polls end (default: %d)" % DEFAULT_STEPS)

    def set_test_params(self):
        self.num_nodes = 6
        self.chain = "regtest"
        self.setup_clean_chain = True
        common = ["-staking=0", f"-blockv15height={V15_HEIGHT}", "-debug=vote"]
        self.extra_args = [common[:] for _ in range(self.num_nodes)]
        self.cut_off = set()

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()
        self.connect_all()

    # ------------------------------------------------------------------
    # Network, clocks and blocks
    # ------------------------------------------------------------------

    def connect_all(self):
        for i in range(1, self.num_nodes):
            if i not in self.cut_off:
                self.connect_nodes(FUNDER, i)

    def linked(self):
        return [node for i, node in enumerate(self.nodes) if i not in self.cut_off]

    def stake(self, node=None, count=1, timeout=60):
        """Stake `count` blocks on `node` (the staker unless told), each SPACING
        seconds on, and sync the linked nodes.

        Every pending transaction is stamped before the new slot, so the first
        block carries the miner's mempool (see advance_to_next_stake_slot).
        """
        node = node or self.nodes[STAKER]
        linked = self.linked()
        for _ in range(count):
            now = max(n.mock_now() for n in linked)
            target = (now + SPACING) & ~STAKE_TIMESTAMP_MASK
            for n in linked:
                n.advance_mocktime_to(target)
            node.generatetoaddress(1, node.getnewaddress())
            self.sync_blocks(linked, wait=0.1, timeout=timeout)

    def gather(self, miner):
        """Wait until `miner` holds every transaction a linked node holds in its
        mempool, so the block it stakes next carries them all.

        A transaction a reorganization puts back into a node's mempool is not
        announced again, so one still missing after a short wait is handed to
        the miner directly, parents before children, if it was in a block of a
        side that lost a reorganization (or spends such a transaction). Any
        other transaction still missing fails the test.
        """
        def missing():
            have = set(miner.getrawmempool())
            found = {}
            for node in self.linked():
                for txid in node.getrawmempool():
                    if txid not in have:
                        found.setdefault(txid, node)
            return found

        deadline = time.time() + 10 * self.options.timeout_factor
        while missing() and time.time() < deadline:
            time.sleep(0.1)
        pending = missing()
        if not pending:
            return
        allowed = {txid for txid in pending if txid in self.put_back}
        grew = True
        while grew:
            grew = False
            for txid, node in pending.items():
                if txid not in allowed and any(i.get("txid") in allowed
                                               for i in node.getrawtransaction(txid, 1)["vin"]):
                    allowed.add(txid)
                    grew = True
        unexpected = sorted(set(pending) - allowed)
        assert not unexpected, "node%d did not receive %s" % (miner.index, unexpected)
        self.log.info("  handing %d transaction(s) a reorganization put back to node%d", len(pending), miner.index)
        while pending:
            accepted = []
            for txid, node in pending.items():
                try:
                    miner.sendrawtransaction(node.getrawtransaction(txid))
                except JSONRPCException as e:
                    self.log.debug("  node%d refuses %s for now: %s", miner.index, txid, e.error["message"])
                    continue
                accepted.append(txid)
            if not accepted:
                raise AssertionError("node%d refuses %s" % (miner.index, sorted(pending)))
            for txid in accepted:
                del pending[txid]
        self.wait_until(lambda: not missing(), timeout=10)

    def confirm(self, node=None):
        """Carry the pending transactions into the next block."""
        node = node or self.nodes[STAKER]
        self.gather(node)
        self.stake(node)

    def bury(self):
        """Stake CLAIM_DEPTH blocks, so the newest coins can be claimed by the
        stock wallet (CWalletTx::IsConfirmed) and spent by any wallet."""
        self.stake(count=CLAIM_DEPTH)

    def warp_to(self, target):
        """Move every clock to `target`, re-make the links and stake two blocks.

        A jump past the peer timeout (20 minutes on the adjusted clock) drops the
        connections, as in feature_mrc.py. A node counts itself in sync by the
        age of its tip's parent (OutOfSyncByAge), and votebyid refuses until it
        is, so two blocks follow the jump.
        """
        assert_greater_than(target, max(node.mock_now() for node in self.nodes) + 1200)
        for node in self.nodes:
            node.setmocktime(target)
        self.wait_until(lambda: all(node.getconnectioncount() == 0 for node in self.nodes),
                        timeout=30)
        self.connect_all()
        self.stake(count=2)

    @staticmethod
    def send_raw(node, inputs, outputs):
        raw = node.createrawtransaction(
            [{"txid": u["txid"], "vout": u["vout"]} for u in inputs], outputs)
        signed = node.signrawtransactionwithwallet(raw)
        assert_equal(signed["complete"], True)
        return node.sendrawtransaction(signed["hex"])

    # ------------------------------------------------------------------
    # Setup
    # ------------------------------------------------------------------

    def take_premine_off_shared_key(self):
        """Take the premine off the key every regtest node holds.

        init.cpp plants the premine key in every regtest wallet, so every voter
        would otherwise own node0's coins and vote with them. node0 spends nine
        premine outputs to its own addresses and the staker's in a block it stakes
        with the tenth, then spends that block's coinstake output, which returns
        to the shared key, in a block the staker stakes. The premine outputs are
        the ones with more confirmations than the chain height (see
        feature_reorg_conflicted.py).
        """
        node0, staker, viewer = self.nodes[FUNDER], self.nodes[STAKER], self.nodes[OBSERVER]
        premine = [u for u in node0.listunspent(0) if u["confirmations"] > node0.getblockcount()]
        assert_equal(len(premine), 10)
        spent = premine[:9]
        outputs = {node0.getnewaddress(): Decimal("150000") for _ in range(2)}
        outputs.update({staker.getnewaddress(): Decimal("150000") for _ in range(3)})
        outputs[staker.getnewaddress()] = sum(u["amount"] for u in spent) - Decimal("750000") - FEE
        self.send_raw(node0, spent, outputs)
        self.confirm(node0)

        shared = viewer.listunspent(0)
        assert_equal(len(shared), 1)
        self.send_raw(node0, shared, {node0.getnewaddress(): shared[0]["amount"] - FEE})
        self.confirm()
        for i in VOTERS + (OBSERVER,):
            assert_equal(self.nodes[i].listunspent(0), [])

    def add_polls(self):
        """A single-choice poll with two choices, one with three, and a
        multiple-choice poll with three, for the random steps."""
        self.polls = []
        for title, answers, response_type in [
            ("two choices", "A;B", RESPONSE_SINGLE_CHOICE),
            ("three choices", "A;B;C", RESPONSE_SINGLE_CHOICE),
            ("three choices, several allowed", "A;B;C", RESPONSE_MULTIPLE_CHOICE),
        ]:
            poll_id = self.nodes[FUNDER].addpoll("survey", title, POLL_DAYS, "Which?", answers,
                                                 WEIGHT_BALANCE, response_type,
                                                 "https://gridcoin.us")["id"]
            self.confirm()
            start = self.nodes[FUNDER].getpollresults(poll_id)["starting_block_height"]
            assert V15_HEIGHT <= start
            self.polls.append(dict(id=poll_id, title=title, choices=len(answers.split(";")),
                                   multiple=response_type == RESPONSE_MULTIPLE_CHOICE))

    def all_polls(self):
        return self.polls

    def poll_end(self, poll):
        """A v15 poll's end: its block's time plus its duration."""
        node0 = self.nodes[FUNDER]
        return node0.getblock(node0.getrawtransaction(poll["id"], 1)["blockhash"])["time"] + POLL_DAYS * 86400

    def fund_voters(self):
        """Send each voter one STAKE_COIN coin and 3-6 coins of 1-500 GRC, spread
        over the stake coin's address and a second address (each gets at least
        one)."""
        outputs = {}
        more = []
        for i in VOTERS:
            voter = self.nodes[i]
            stake_address, second = voter.getnewaddress(), voter.getnewaddress()
            self.stake_address[i] = stake_address
            outputs[stake_address] = STAKE_COIN
            for k in range(self.rng.randint(3, 6)):
                address = [second, stake_address][k] if k < 2 else self.rng.choice([stake_address, second])
                amount = Decimal(self.rng.randint(100, 50000)) / 100
                if address in outputs:
                    more.append((address, amount))
                else:
                    outputs[address] = amount
        node0 = self.nodes[FUNDER]
        node0.sendmany("", outputs)
        # sendmany pays each address once; a second coin at an address comes in
        # its own transaction.
        for address, amount in more:
            node0.sendtoaddress(address, amount)
        self.confirm()
        self.record_outputs()

    # ------------------------------------------------------------------
    # Bookkeeping
    # ------------------------------------------------------------------

    def record_outputs(self):
        """Remember every output each voter has held, for votewithclaim."""
        for i in VOTERS:
            for u in self.nodes[i].listunspent(0):
                outpoint = (u["txid"], u["vout"])
                if outpoint not in self.history_seen[i]:
                    self.history_seen[i].add(outpoint)
                    self.history[i].append(outpoint)

    def wallet_knows(self, node, txid):
        try:
            node.gettransaction(txid)
            return True
        except JSONRPCException as e:
            if e.error["code"] != -5:
                raise
            return False

    def stake_coin_ready(self, node):
        return any(u["amount"] >= STAKE_READY for u in node.listunspent(1))

    def make_stake_coin_ready(self, node):
        """A vote spends its voter's largest coin; carry the pending vote into a
        block so its change can stake."""
        if not self.stake_coin_ready(node):
            self.confirm()
        assert self.stake_coin_ready(node), "node%d has no coin to stake" % node.index

    def pick_choices(self, poll):
        if poll["multiple"]:
            return self.rng.sample(range(poll["choices"]), self.rng.randint(1, poll["choices"]))
        return [self.rng.randrange(poll["choices"])]

    def attempt(self, what, call, *args):
        """Make a vote builder call; log and count an allowed refusal."""
        try:
            return call(*args)
        except JSONRPCException as e:
            message = e.error["message"]
            if not any(refusal in message for refusal in ALLOWED_REFUSALS):
                raise
            self.log.info("  %s refused: %s", what, message)
            self.refusals[message.split(":")[0]] += 1
            return None

    # ------------------------------------------------------------------
    # Actions
    # ------------------------------------------------------------------

    def cast(self, i, poll, choices):
        """votebyid by voter i; returns the vote's txid, or None if refused."""
        result = self.attempt("node%d votebyid" % i, self.nodes[i].votebyid, poll["id"], *choices)
        if result is None:
            return None
        self.last_vote = (i, poll)
        self.votes_by_voter[i].append((result["vote_txid"], poll))
        self.log.info("  node%d votes %s on '%s': %s", i, choices, poll["title"], result["vote_txid"])
        return result["vote_txid"]

    def act_vote(self):
        i = self.rng.choice(VOTERS)
        poll = self.rng.choice(self.polls)
        self.cast(i, poll, self.pick_choices(poll))

    def act_revote(self):
        """The newest vote's voter votes again on its poll at once."""
        if self.last_vote is None:
            self.act_vote()
            return
        i, poll = self.last_vote
        self.cast(i, poll, self.pick_choices(poll))

    def draw(self, i, count):
        """Up to `count` outputs drawn from every output voter i has held."""
        return self.rng.sample(self.history[i], min(count, len(self.history[i])))

    def submit_claim(self, i, poll, choices, drawn, pin_fee, exclude_fee=None):
        """votewithclaim by voter i with a claim of the `drawn` outputs, in order;
        with `pin_fee`, one of its confirmed coins of at least 2 GRC (other than
        `exclude_fee`) alone pays for the vote. Returns the vote's txid, or None."""
        voter = self.nodes[i]
        # A coinstake a reorganization orphaned may leave the wallet.
        claimed = [o for o in drawn if self.wallet_knows(voter, o[0])]
        if not claimed:
            self.log.info("  node%d has none of the drawn outputs left to claim", i)
            return None
        args = [poll["id"], choices, [{"txid": t, "vout": n} for t, n in claimed]]
        if pin_fee:
            fee_coins = sorted((u for u in voter.listunspent(3)
                                if u["amount"] >= 2 and (u["txid"], u["vout"]) != exclude_fee),
                               key=lambda u: (u["amount"], u["confirmations"]))
            if not fee_coins:
                self.log.info("  node%d has no coin to pin the fee to", i)
                return None
            fee = self.rng.choice(fee_coins)
            args.append("%s:%d" % (fee["txid"], fee["vout"]))
        result = self.attempt("node%d votewithclaim" % i, voter.votewithclaim, *args)
        if result is None:
            return None
        self.last_vote = (i, poll)
        self.votes_by_voter[i].append((result["vote_txid"], poll))
        self.log.info("  node%d votes %s on '%s' claiming %d output(s)%s: %s", i, choices, poll["title"],
                      len(claimed), ", fee pinned" if pin_fee else "", result["vote_txid"])
        return result["vote_txid"]

    def act_vote_with_claim(self):
        """A claim of outputs drawn from the voter's history."""
        i = self.rng.choice(VOTERS)
        count = self.rng.randint(1, 6)
        pin_fee = self.rng.random() < 0.3
        poll = self.rng.choice(self.polls)
        self.submit_claim(i, poll, self.pick_choices(poll), self.draw(i, count), pin_fee)

    def act_send(self):
        """A plain transaction from a voter's confirmed coins to a new address of
        its own or of another voter, with the change back to the first input's
        address or to a new address. Now and then the amount is under 1 GRC."""
        i = self.rng.choice(VOTERS)
        voter = self.nodes[i]
        to = self.rng.choice(VOTERS)
        n_inputs = self.rng.randint(1, 2)
        small = self.rng.random() < 0.15
        amount = Decimal(self.rng.randint(10, 99) if small else self.rng.randint(100, 50000)) / 100
        change_to_input = self.rng.random() < 0.5

        coins = sorted(voter.listunspent(1), key=lambda u: (u["amount"], u["confirmations"], u["vout"]))
        if not coins:
            self.log.info("  node%d has no confirmed coin to send", i)
            return
        inputs = self.rng.sample(coins, min(n_inputs, len(coins)))
        total = sum(u["amount"] for u in inputs)
        amount = min(amount, total - FEE)
        if amount <= 0:
            self.log.info("  node%d's drawn coins do not cover the fee", i)
            return
        outputs = {self.nodes[to].getnewaddress(): amount}
        change = total - FEE - amount
        if change >= Decimal("0.01"):
            change_address = inputs[0]["address"] if change_to_input else voter.getnewaddress()
            outputs[change_address] = outputs.get(change_address, 0) + change
        txid = self.send_raw(voter, inputs, outputs)
        self.log.info("  node%d sends %s to node%d from %d coin(s)%s: %s", i, amount, to, len(inputs),
                      ", change to the input's address" if change_to_input else "", txid)

    def act_voter_stakes(self):
        i = self.rng.choice(VOTERS)
        voter = self.nodes[i]
        self.make_stake_coin_ready(voter)
        self.gather(voter)
        self.stake(voter)
        self.log.info("  node%d stakes block %d", i, voter.getblockcount())

    def act_mine(self):
        count = self.rng.randint(1, 11)
        self.gather(self.nodes[STAKER])
        self.stake(count=count)
        self.log.info("  the staker stakes %d block(s), to height %d", count, self.nodes[STAKER].getblockcount())

    def act_reorg(self):
        """The staker first carries the pending transactions into a block both
        sides share. Then node2 splits off; the rest stake `main` blocks, and
        node2 stakes `side` blocks with its clock two stake slots past the other
        side's tip (as feature_reorg.py), with an optional vote of its own whose
        fee is pinned so its stake coin stays put. Half the time node3 or node4
        votes before the other side's first block and re-votes with the stock
        wallet after it (its claim may name an output of the first vote's
        lineage); node2's side is then the longer more often than not. On
        reconnecting, the longer side stakes one more block and the
        other side reorganizes onto it."""
        main_voter = self.rng.choice([j for j in VOTERS if j != REORG_NODE]) if self.rng.random() < 0.5 else None
        lengths = [(a, b) for a in (1, 2, 3) for b in (1, 2, 3) if a != b]
        if main_voter is not None and self.rng.random() < 0.5:
            lengths = [(a, b) for a, b in lengths if b > a]
        main, side = self.rng.choice(lengths)
        main_poll = self.rng.choice(self.polls)
        main_choices = self.pick_choices(main_poll)
        side_vote = self.rng.random() < 0.5
        side_poll = self.rng.choice(self.polls)
        side_choices = self.pick_choices(side_poll)
        side_count = self.rng.randint(1, 6)

        node2, node0, staker = self.nodes[REORG_NODE], self.nodes[FUNDER], self.nodes[STAKER]
        self.make_stake_coin_ready(node2)
        # Before node2 splits off, the staker carries the pending transactions
        # into a block both sides share, so both sides start from the same
        # transactions.
        self.gather(staker)
        self.stake()

        self.cut_off.add(REORG_NODE)
        self.disconnect_nodes(FUNDER, REORG_NODE)
        first_vote = self.cast(main_voter, main_poll, main_choices) if main_voter is not None else None
        if side_vote:
            largest = max(node2.listunspent(1), key=lambda u: u["amount"])
            self.submit_claim(REORG_NODE, side_poll, side_choices, self.draw(REORG_NODE, side_count),
                              pin_fee=True, exclude_fee=(largest["txid"], largest["vout"]))

        self.gather(staker)
        self.stake()
        # The stock re-vote after the first vote's block. Its claim traces through
        # the first vote when it names an output of that vote's lineage.
        traced = False
        if first_vote is not None:
            revote = self.cast(main_voter, main_poll, self.pick_choices(main_poll))
            if revote is not None:
                voter = self.nodes[main_voter]
                named = {(o["txid"], o["offset"]) for a in voter.getvotingclaim(revote)["balance_claim"]
                         for o in a["outpoints"]}
                traced = bool(named & self.lineage(voter, first_vote))
        if main > 1:
            self.gather(staker)
            self.stake(count=main - 1)
        main_tip = node0.getbestblockhash()
        node2.advance_mocktime_to(node0.getblock(main_tip)["time"] + 2 * STAKE_SLOT)
        for k in range(side):
            if k:
                node2.advance_mocktime_to((node2.mock_now() + SPACING) & ~STAKE_TIMESTAMP_MASK)
            node2.generatetoaddress(1, node2.getnewaddress())
        side_tip = node2.getbestblockhash()
        # node2's coinstakes on its side: claimable later, and out of the chain if
        # its side loses (while its wallet still holds them).
        self.record_outputs()
        # The losing side's transactions go back into its nodes' mempools.
        loser, loser_tip, depth = (node2, side_tip, side) if main > side else (node0, main_tip, main)
        block_hash = loser_tip
        for _ in range(depth):
            block = loser.getblock(block_hash)
            self.put_back.update(block["tx"])
            block_hash = block["previousblockhash"]

        # Neither side may see the other's blocks as from the future, and node2
        # must be past the inbound rate limit (5 s since its last accept).
        target = max(n.mock_now() for n in self.nodes) + 6
        for n in self.nodes:
            n.advance_mocktime_to(target)
        self.cut_off.discard(REORG_NODE)
        self.connect_nodes(FUNDER, REORG_NODE)
        winner = staker if main > side else node2
        self.stake(winner, timeout=120)
        assert_equal(loser.getblock(loser_tip)["confirmations"], -1)
        self.log.info("  reorganized: %d block(s) on node2's side, %d on the other; node%d's side won",
                      side, main, winner.index)
        if traced and winner is node2:
            self.reorgs_through_traced_votes += 1
            self.log.info("  node%d's vote %s was on the losing side; its re-vote claims an output of its lineage",
                          main_voter, first_vote)

    @staticmethod
    def carries_vote_or_stake(tx):
        """A verbose transaction is a coinstake or carries a vote contract (see
        vote_oracle.py for the coinstake test)."""
        first_in = tx["vin"][0] if tx["vin"] else {}
        first_out = tx["vout"][0] if tx["vout"] else None
        coinstake = ("txid" in first_in and len(tx["vout"]) >= 2
                     and sats(first_out["value"]) == 0 and first_out["scriptPubKey"]["hex"] == "")
        return coinstake or any(c["type"] == "vote" for c in tx.get("contracts", []))

    def lineage(self, node, txid):
        """The outputs a tally walk can pass through on its way to the outputs of
        vote `txid` at its change address: its inputs at that address and, for
        each one a vote or a coinstake paid, that transaction's inputs at the
        address in turn, MAX_TRACE_DEPTH transactions back at most."""
        tx = node.getrawtransaction(txid, 1)
        addresses = [o["scriptPubKey"]["addresses"][0] for o in tx["vout"] if o["scriptPubKey"].get("addresses")]
        found = set()
        frontier = [tx]
        for _ in range(MAX_TRACE_DEPTH):
            following = []
            for spender in frontier:
                for txin in spender["vin"]:
                    prev = node.getrawtransaction(txin["txid"], 1)
                    if prev["vout"][txin["vout"]]["scriptPubKey"].get("addresses", [None])[0] not in addresses:
                        continue
                    found.add((txin["txid"], txin["vout"]))
                    if self.carries_vote_or_stake(prev):
                        following.append(prev)
            frontier = following
        return found

    def act_restart(self):
        observer = self.nodes[OBSERVER]
        self.stop_node(OBSERVER)
        address = "127.0.0.1:%d" % p2p_port(OBSERVER)
        self.wait_until(lambda: all(p["addr"] != address for p in self.nodes[FUNDER].getpeerinfo()))
        self.start_node(OBSERVER)
        observer.setmocktime(max(n.mock_now() for n in self.nodes if n is not observer))
        self.connect_nodes(FUNDER, OBSERVER)
        self.sync_blocks()
        self.log.info("  node%d restarted at height %d", OBSERVER, observer.getblockcount())

    # ------------------------------------------------------------------
    # Comparison
    # ------------------------------------------------------------------

    @staticmethod
    def result_of(node, poll_id):
        result = node.getpollresults(poll_id)
        return dict(total_weight=sats(result["total_weight"]),
                    responses=[sats(r["weight"]) for r in result["responses"]],
                    invalid_votes=result["invalid_votes"],
                    superseded_votes=result["superseded_votes"],
                    votes=result["votes"])

    def compare(self, step, action):
        """Every node's result for every poll equals the oracle's tally."""
        assert_equal(len({n.getbestblockhash() for n in self.nodes}), 1)
        self.oracle.refresh(self.nodes[FUNDER])
        for poll in self.all_polls():
            expected = self.oracle.tally(poll["id"])
            self.events.update(self.oracle.events)
            self.events_by_poll[poll["id"]].update(self.oracle.events)
            for node in self.nodes:
                got = self.result_of(node, poll["id"])
                if got != expected:
                    self.log.error("MISMATCH at step %s (%s), seed %d, node%d, poll '%s' %s\n"
                                   "  node:   %s\n  oracle: %s\n  node's valid votes: %s",
                                   step, action, self.options.seed, node.index, poll["title"], poll["id"],
                                   got, expected, node.votedetails(poll["id"]))
                    raise AssertionError("poll '%s' at step %s (%s), seed %d: node%d %s != oracle %s" % (
                        poll["title"], step, action, self.options.seed, node.index, got, expected))
            self.log.debug("  '%s': %s", poll["title"], expected)

    def random_steps(self, actions, steps, label):
        names = [name for _, name in actions]
        weights = [weight for weight, _ in actions]
        for step in range(1, steps + 1):
            action = self.rng.choices(names, weights)[0]
            self.log.info("%s step %d/%d: %s", label, step, steps, action)
            self.record_outputs()
            getattr(self, "act_" + action)()
            self.counts[action] += 1
            self.compare("%s %d" % (label, step), action)
            if self.snapshot is not None:
                self.check_snapshot()

    def check_snapshot(self):
        for node in self.nodes:
            for poll in self.all_polls():
                assert_equal(node.getpollresults(poll["id"]), self.snapshot[(node.index, poll["id"])])

    def cast_any(self, poll):
        """votebyid on `poll` by the first voter, in a random order, whose wallet
        can vote now."""
        for i in self.rng.sample(VOTERS, len(VOTERS)):
            txid = self.cast(i, poll, self.pick_choices(poll))
            if txid is not None:
                return i, txid
        raise AssertionError("no voter could vote on '%s'" % poll["title"])

    def end_polls(self):
        """Mine, in the first block past the polls' end, a vote on one of them cast
        while it was open: it is not linked.

        The clocks first jump to shortly before the first poll ends; the vote is
        cast there, and the further step to the slot past the last poll's end
        stays within the peer timeout and within the age at which a wallet counts
        itself in sync (OutOfSyncByAge, 900 s since the tip's parent), which
        voting needs. From v15 a memory pool takes a vote only for a poll that has
        not finished, so the staker takes the vote before the clocks move past the
        end.
        """
        node0, staker = self.nodes[FUNDER], self.nodes[STAKER]
        first_end = min(self.poll_end(poll) for poll in self.polls)
        last_end = max(self.poll_end(poll) for poll in self.polls)
        jump = (first_end - 320) & ~STAKE_TIMESTAMP_MASK
        past = (last_end + STAKE_SLOT) & ~STAKE_TIMESTAMP_MASK
        assert_greater_than(900 - 60, past - (jump + SPACING))
        # The jump drops the links, and a reconnected peer is not told about the
        # mempool it missed: the staker takes the pending transactions first.
        self.gather(staker)
        self.warp_to(jump)
        for poll in self.polls:
            assert not node0.getpollresults(poll["id"])["poll_expired"]

        late_poll = self.rng.choice(self.polls)
        _, late = self.cast_any(late_poll)
        self.gather(staker)
        for n in self.nodes:
            n.advance_mocktime_to(past)
        staker.generatetoaddress(1, staker.getnewaddress())
        self.sync_blocks(wait=0.1)
        block = node0.getblock(node0.getbestblockhash())
        assert late in block["tx"], late
        assert_greater_than(block["time"], last_end)
        assert_equal(node0.getpollresults(late_poll["id"])["poll_expired"], True)

    # ------------------------------------------------------------------

    def run_test(self):
        self.rng = random.Random(self.options.seed)
        self.oracle = VoteOracle(V15_HEIGHT)
        self.history = {i: [] for i in VOTERS}
        self.history_seen = {i: set() for i in VOTERS}
        self.stake_address = {}
        self.votes_by_voter = {i: [] for i in VOTERS}
        self.last_vote = None
        self.put_back = set()
        self.snapshot = None
        self.counts = Counter()
        self.refusals = Counter()
        self.events = Counter()
        self.events_by_poll = defaultdict(Counter)
        self.reorgs_through_traced_votes = 0
        self.log.info("seed %d, %d steps", self.options.seed, self.options.steps)

        self.take_premine_off_shared_key()
        # A poll's balance claim names coins at least CLAIM_DEPTH deep.
        self.bury()
        self.add_polls()
        self.fund_voters()
        self.bury()
        self.compare("setup", "setup")

        self.random_steps(ACTIONS, self.options.steps, "main")

        self.log.info("a vote cast while a poll was open, mined in the first block past the polls' end")
        self.end_polls()
        self.compare("end", "a vote mined in the first block past the polls' end")
        node0 = self.nodes[FUNDER]
        self.stake()
        for poll in self.all_polls():
            assert node0.getpollresults(poll["id"])["poll_expired"]
        self.compare("ended", "one block past every poll's end")

        self.log.info("more random steps")
        self.snapshot = {(node.index, poll["id"]): node.getpollresults(poll["id"])
                         for node in self.nodes for poll in self.all_polls()}
        self.random_steps(MORE_ACTIONS, MORE_STEPS, "more")

        self.log.info("actions: %s", dict(sorted(self.counts.items())))
        self.log.info("refusals: %s", dict(sorted(self.refusals.items())))
        # How often each tally rule decided something, summed over every
        # comparison, then in the final tallies.
        self.log.info("rules reached over the run: %s", dict(sorted(self.events.items())))
        self.log.info("reorganizations with a vote on the losing side whose lineage its re-vote's claim names: %d",
                      self.reorgs_through_traced_votes)
        for poll in self.polls:
            self.log.info("'%s': %s", poll["title"], self.oracle.tally(poll["id"]))
            self.log.info("  rules reached: %s", dict(sorted(self.oracle.events.items())))
        self.log.info("coverage counters: %s", self.coverage_counters())

        if self.options.steps == DEFAULT_STEPS:
            self.assert_coverage()

    def coverage_counters(self):
        """Run-wide counts of the rules assert_coverage() requires, summed over
        every comparison."""
        counters = {"vote counted on '%s'" % poll["title"]: self.events_by_poll[poll["id"]]["vote counted"]
                    for poll in self.polls}
        for event in ("walk: spent by a vote", "walk: spent by a coinstake", "vote superseded"):
            counters[event] = self.events[event]
        return counters

    def assert_coverage(self):
        """The rules a run at the default number of steps must reach, so a
        change to the activity that stops reaching them is noticed."""
        for name, count in self.coverage_counters().items():
            assert count > 0, "coverage: the run must reach %s" % name


if __name__ == "__main__":
    VoteTallyOracleTest().main()
