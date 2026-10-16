#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Independent tally of balance polls that start at or after v15, for tests.

The tally is computed from chain data alone (getblock with transaction detail,
getvotingclaim, and decodescript to turn a claim's public key into its
address), with the rules of src/gridcoin/voting/result.cpp, registry.cpp and
poll.cpp written out again in Python. A test compares it with
getpollresults; when the two disagree, the test fails and a person decides
which side is wrong. Nothing here is derived from a tally the node reports.

The rules, for a BALANCE poll whose block is at or after the v15 height:
- a poll starts at its block's time and ends `duration_days` days later; its
  first block past the end is the first block after the poll's block whose time
  exceeds the end, and its closing block is the block before that
  (PollReference::StartTime, PollRegistry::Snapshot);
- the tally is bounded by the closing block once the poll has one, and by the
  tip while it is open (PollSnapshot::m_pindex_bound); a block is after the
  bound when its height is above the bound's;
- a vote is linked to the poll when its block is after the poll's block and
  not after the bound (PollRegistry::VoteInWindow);
- linked votes resolve newest first (VoteCounter::CountVotes);
- no vote transaction time is read (FetchVoteCandidate);
- a claimed output the vote has already recorded gives 0;
- a claimed output whose transaction is not in the chain, or is confirmed in a
  block after the bound, gives 0;
- a claimed output out of range, under 1 GRC, without a single destination, or
  at another address than its claim's key makes the vote invalid;
- a claimed output a counted vote holds gives 0;
- an unspent claimed output is recorded for the vote and credited;
- a spent claimed output is recorded for the vote, then the walk follows its
  spender, if that is a coinstake or a vote transaction, to the spender's
  outputs at the claim's address that no counted vote holds and the vote has
  not recorded, recording each output it follows and following each of those
  that is spent in turn; a spend by any other transaction in a block not after
  the bound takes the value away, and a spend in a block after the bound is
  irrelevant (VoteResolver::ResolveAmount);
- a vote resolved to no weight is superseded if it reached an output a counted
  vote holds, and invalid otherwise; a vote with weight and a response out of
  range is invalid;
- the outputs a vote recorded are held for the poll only once the vote is
  counted (VoteResolver::Commit);
- a counted vote's weight is split over its responses by integer division, and
  the poll's total is the sum of the response weights (CalculateWeight,
  TallyVote).

Address claim signatures are outside the oracle's scope: it reads each claim
as the node decodes it (getvotingclaim).
"""

from collections import Counter, deque
from decimal import Decimal

COIN = 100_000_000

# PollWeightType::BALANCE, as the poll contract stores it.
WEIGHT_BALANCE = 2

# The output types ExtractDestination gives one destination for. A bare
# multisig output may list addresses, but has no destination.
SINGLE_DESTINATION_TYPES = ("pubkey", "pubkeyhash", "scripthash")

NULL_TXID = "0" * 64


def sats(value):
    """An RPC amount as an integer number of 1/100000000 GRC."""
    return int((Decimal(str(value)) * COIN).to_integral_value())


class InvalidVote(Exception):
    """The tally counts the vote as invalid."""


class VoteRecords:
    """What resolving one vote's claim reached."""

    def __init__(self, held):
        self.held = held          # outputs the counted votes recorded (read only)
        self.recorded = set()     # outputs this vote recorded
        self.reached_held = False  # whether it reached an output a counted vote holds


class VoteOracle:
    def __init__(self, v15_height):
        self.v15_height = v15_height
        self.node = None
        self.hashes = {}    # height -> hash of the active chain's block
        self.blocks = {}    # block hash -> parsed block (content never changes for a hash)
        self.claims = {}    # vote txid -> [(address, [(txid, n), ...]), ...]
        self.addresses = {}  # public key hex -> address
        self.txs = {}       # txid -> parsed transaction of the active chain
        self.spender = {}   # (txid, n) -> txid of the active chain's transaction that spends it
        # How often each rule decided something in the last tally, so a test
        # can show which rules its activity reached.
        self.events = Counter()

    # ------------------------------------------------------------------
    # Chain data
    # ------------------------------------------------------------------

    def refresh(self, node):
        """Re-read the active chain of `node`, keeping the blocks it still shares
        with the last read (so it is safe across reorganizations)."""
        self.node = node
        height = node.getblockcount()
        for h in [h for h in self.hashes if h > height]:
            del self.hashes[h]
        # The highest height whose block is unchanged; every block below it is
        # unchanged too.
        common = min(height, max(self.hashes, default=0))
        while common > 0 and node.getblockhash(common) != self.hashes[common]:
            common -= 1
        # The read starts at height 1, the first block the tests can hold a
        # poll, vote or claimed output in.
        for h in range(common + 1, height + 1):
            block_hash = node.getblockhash(h)
            self.hashes[h] = block_hash
            if block_hash not in self.blocks:
                self.blocks[block_hash] = self.parse_block(node.getblock(block_hash, True), h)

        self.txs.clear()
        self.spender.clear()
        for h in range(1, height + 1):
            for tx in self.blocks[self.hashes[h]]:
                self.txs[tx["txid"]] = tx
                for prevout in tx["vin"]:
                    self.spender[prevout] = tx["txid"]

    @staticmethod
    def parse_block(block, height):
        # getblock with transaction detail prints each transaction's own time
        # (tx.nTime); getrawtransaction prints the block's time in its place
        # for a confirmed transaction.
        parsed = []
        for index, tx in enumerate(block["tx"]):
            vin = [(i["txid"], i["vout"]) for i in tx["vin"] if "txid" in i]
            vout = []
            for o in tx["vout"]:
                script = o["scriptPubKey"]
                destination = None
                if script["type"] in SINGLE_DESTINATION_TYPES:
                    destination = script["addresses"][0]
                vout.append((sats(o["value"]), destination, script["type"]))
            # CTransaction::IsCoinStake: a first input that is not null, at least
            # two outputs, and an empty first output (no value, empty script).
            # A coinbase input has no txid here, and the output index of a null
            # prevout prints as -1.
            first_in = tx["vin"][0] if tx["vin"] else {}
            first_out = tx["vout"][0] if tx["vout"] else None
            coinstake = ("txid" in first_in and (first_in["txid"], first_in["vout"]) != (NULL_TXID, -1)
                         and len(tx["vout"]) >= 2
                         and sats(first_out["value"]) == 0 and first_out["scriptPubKey"]["hex"] == "")
            contracts = tx.get("contracts", [])
            parsed.append(dict(
                txid=tx["txid"], height=height, index=index, block_time=block["time"], time=tx["time"],
                vin=vin, vout=vout, coinstake=coinstake,
                carries_vote=any(c["type"] == "vote" for c in contracts),
                votes=[(c["body"]["poll_txid"], c["body"]["responses"]) for c in contracts if c["type"] == "vote"],
                poll=next((c["body"] for c in contracts if c["type"] == "poll"), None)))
        return parsed

    def claim(self, vote_txid):
        """The vote's balance claim: (address of the claim's key, outpoints) per
        address claim, in the order the claim lists them."""
        if vote_txid not in self.claims:
            address_claims = []
            for address_claim in self.node.getvotingclaim(vote_txid)["balance_claim"]:
                address_claims.append((self.address_of(address_claim["public_key"]),
                                       [(o["txid"], o["offset"]) for o in address_claim["outpoints"]]))
            self.claims[vote_txid] = address_claims
        return self.claims[vote_txid]

    def address_of(self, public_key):
        """The address of a public key (its key ID), as the node encodes it: the
        destination of a pay-to-pubkey script."""
        if public_key not in self.addresses:
            push = "%02x" % (len(public_key) // 2)
            self.addresses[public_key] = self.node.decodescript(push + public_key + "ac")["addresses"][0]
        return self.addresses[public_key]

    def block_time(self, height):
        """The time of the active chain's block at `height`."""
        return self.blocks[self.hashes[height]][0]["block_time"]

    # ------------------------------------------------------------------
    # Tally
    # ------------------------------------------------------------------

    def tally(self, poll_id):
        """The poll's result: total weight, weight per response (both in
        1/100000000 GRC), invalid votes, superseded votes and linked votes."""
        self.events = Counter()
        poll = self.txs[poll_id]
        body = poll["poll"]
        assert body is not None, "%s is not a poll" % poll_id
        assert body["weight_type"] == WEIGHT_BALANCE, "only balance polls are implemented"
        assert poll["height"] >= self.v15_height, "only polls that start at or after v15 are implemented"
        end = poll["block_time"] + body["duration_days"] * 86400
        choice_count = len(body["choices"])
        tip = max(self.hashes)
        past_end = next((h for h in range(poll["height"] + 1, tip + 1) if self.block_time(h) > end), None)
        bound = past_end - 1 if past_end is not None else tip

        def after_end(height):
            return height > bound

        linked = sorted(
            (tx["height"], tx["index"], txid) for txid, tx in self.txs.items()
            if any(p == poll_id for p, _ in tx["votes"])
            and poll["height"] < tx["height"] <= bound)

        held = set()  # outputs the counted votes recorded
        responses = [0] * choice_count
        total = 0
        invalid = 0
        superseded = 0

        for _, _, txid in reversed(linked):
            vote_tx = self.txs[txid]
            # The protocol allows one contract per transaction. These checks come
            # before the claim resolves.
            choices = next((rs for _, rs in vote_tx["votes"] if rs), None)
            if choices is None:
                self.events["vote invalid before its claim resolves"] += 1
                invalid += 1
                continue
            vote = VoteRecords(held)
            try:
                amount = self.resolve_claim(txid, vote, after_end)
            except InvalidVote:
                self.events["vote invalid while its claim resolves"] += 1
                invalid += 1
                continue
            if amount == 0:
                if vote.reached_held:
                    self.events["vote superseded"] += 1
                    superseded += 1
                else:
                    self.events["vote with no weight"] += 1
                    invalid += 1
                continue
            if any(not 0 <= c < choice_count for c in choices):
                self.events["vote with a bad response"] += 1
                invalid += 1
                continue
            self.events["vote counted"] += 1
            held |= vote.recorded
            weight = amount // len(choices)
            for choice in choices:
                responses[choice] += weight
                total += weight

        return dict(total_weight=total, responses=responses, invalid_votes=invalid,
                    superseded_votes=superseded, votes=len(linked))

    def resolve_claim(self, vote_txid, vote, after_end):
        """The weight of a vote's balance claim, recording into `vote` the outputs
        it reaches. The caller adds them to the held outputs only if the vote is
        counted."""
        amount = 0
        for address, outpoints in self.claim(vote_txid):
            for outpoint in outpoints:
                amount += self.resolve_output(outpoint, address, vote, after_end)
        return amount

    def resolve_output(self, outpoint, address, vote, after_end):
        """One claimed output, with VoteResolver::Resolve(COutPoint)'s checks in
        its order."""
        if outpoint in vote.recorded:
            self.events["claimed output already recorded for the vote"] += 1
            return 0
        txid, n = outpoint
        if txid not in self.txs:
            self.events["claimed output not in the chain"] += 1
            return 0  # no transaction index entry
        tx = self.txs[txid]
        if after_end(tx["height"]):
            self.events["claimed output confirmed after the end"] += 1
            return 0
        if n >= len(tx["vout"]):
            raise InvalidVote()
        value, destination, script_type = tx["vout"][n]
        if value < COIN:
            self.events["claimed output under 1 GRC"] += 1
            raise InvalidVote()
        if destination is None or destination != address:
            self.events["claimed output at another address"] += 1
            raise InvalidVote()
        if outpoint in vote.held:
            self.events["claimed output a counted vote holds"] += 1
            vote.reached_held = True
            return 0
        if script_type == "pubkey":
            self.events["claimed output pays to a public key"] += 1
        vote.recorded.add(outpoint)
        if outpoint not in self.spender:
            self.events["claimed output unspent"] += 1
            return value
        self.events["claimed output spent"] += 1
        return self.walk(address, self.spender[outpoint], value, vote, after_end)

    def walk(self, address, spender, value, vote, after_end):
        """VoteResolver::ResolveAmount: follow a spent claimed output through
        coinstakes and vote transactions to their outputs at `address`."""
        amount = value
        queue = deque([(spender, value)])
        while queue:
            txid, frame_amount = queue.popleft()
            tx = self.txs[txid]
            if after_end(tx["height"]):
                self.events["walk: spent after the end"] += 1
                continue  # the value stays
            amount -= frame_amount
            if not (tx["coinstake"] or tx["carries_vote"]):
                self.events["walk: spent by another transaction"] += 1
                continue  # not followed
            self.events["walk: spent by a coinstake" if tx["coinstake"] else "walk: spent by a vote"] += 1
            followed = []
            for i, (out_value, destination, script_type) in enumerate(tx["vout"]):
                if destination != address:
                    continue
                if (txid, i) in vote.held:
                    self.events["walk: output a counted vote holds"] += 1
                    vote.reached_held = True
                    continue
                if (txid, i) in vote.recorded:
                    self.events["walk: output already recorded for the vote"] += 1
                    continue
                if script_type == "pubkey":
                    self.events["walk: follows an output that pays to a public key"] += 1
                amount += out_value
                followed.append((i, out_value))
            for i, out_value in followed:
                if (txid, i) in self.spender:
                    queue.append((self.spender[(txid, i)], out_value))
                vote.recorded.add((txid, i))
        return amount
