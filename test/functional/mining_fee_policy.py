#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Miner fee policy: inclusion ordering and the -mintxfee floor.

CreateRestOfTheBlock packs a block from a max-heap keyed on effective fee RATE
(dFeePerKb), and applies -mintxfee as a floor on that same rate. This covers
both, reading inclusion order from the block itself: block.vtx is filled by
push_back as transactions are selected, so vtx[2:] IS the selection order
(vtx[0] is the coinbase and vtx[1] the coinstake).

  1. ordering is by fee RATE, not absolute fee -- pinned with a large
     transaction that pays the highest absolute fee in the block and must still
     sort below smaller transactions paying a higher rate;
  2. the DEFAULT -mintxfee rejects nothing, for a multi-kilobyte transaction
     paying exactly the relay minimum -- the shape whose effective rate comes
     closest to the floor in ordinary use;
  3. a raised -mintxfee excludes a below-floor transaction from the block while
     leaving it in the mempool, and still includes one above the floor;
  4. an unparseable -mintxfee is rejected at startup rather than silently
     falling back to the default -- the failure path that keeps the option from
     being ignored, which is what it did for years.

One node per case. Nodes 0 and 1 run the default -mintxfee (cases 1 and 2),
node 2 a raised one (case 3). They are separate nodes rather than one node
restarted because restart_node rebuilds the ephemeral regtest wallet with a
fresh seed, and deliberately unconnected, so each mines its own chain.

One case per node is a robustness requirement, not tidiness. Each case spends
about half its node's UTXO universe into the mempool and then mines, and
CWallet marks a parent spent on TxStateInMempool as well as TxStateConfirmed,
so those outputs leave AvailableCoinsForStaking. Running two cases on one node
drained it far enough that CreateCoinStake failed with "no stake found (need a
mature UTXO with sufficient weight)" in ~5% of runs. Retrying does not help --
a retry advances the mock clock by one 16 s slot, which adds no meaningful
weight -- so the supply has to be there in the first place.

Background: Gridcoin's schedule is (1 + bytes/1000) * 0.001 GRC, so the
effective rate FALLS with size -- ~0.0051 GRC/KB for a 196-byte transaction
against ~0.0010 GRC/KB for a 3 KB one. That is why case 2 exists, and why
ordering by rate is not the same as ordering by absolute fee.

NOT covered here: the `nTxFees < nMinFee` handler in CreateRestOfTheBlock. At
the default -blockmaxsize that branch is unreachable -- AcceptToMemoryPool
already requires at least the flat fee the miner recomputes, because the miner
passes nBytes = 0 while relay charges (1 + size/1000) * the same base. Only the
block-fill escalator can lift the miner's floor above relay's, and it cannot
fire while -blockmaxsize equals the 250000 threshold it triggers at.

Raising that option does reach it: see mining_fee_escalator.py, which runs a
connected pair at -blockmaxsize=470000. A unit-test chain fixture with
spendable coins (issue #3290) is still worth having -- it would cover the
handler without a staked block -- but it is not the only route.

This test is in EXTENDED_SCRIPTS, not the default suite: it asserts on the
contents of a staked block, which makes it subject to the regtest stake-supply
hazard documented in test_runner.py.
"""
from decimal import Decimal

from test_framework.authproxy import JSONRPCException
from test_framework.test_framework import GridcoinTestFramework
from test_framework.test_node import ErrorMatch
from test_framework.util import assert_equal

SAT = Decimal("0.00000001")

# -mintxfee for node 2 (the third node, which runs case 3), in GRC per KB. Sits
# between the two probe transactions in case 3 (~0.0051 and ~0.0102 GRC/KB).
RAISED_MIN_TX_FEE = Decimal("0.008")

# Inputs in the "bulk" transaction. ~148 bytes per P2PKH input puts it near
# 3 KB, far enough from a 1000-byte boundary to be stable run to run.
BULK_INPUTS = 20

# Hoisted out of set_test_params because case 4 restarts a node itself, and
# TestNode.start REPLACES extra_args rather than appending to it -- a restart
# that passes only "-mintxfee=..." would drop -devbuild=override and fail for
# an unrelated reason.
COMMON_ARGS = ["-staking=0", "-connect=0", "-listen=0", "-devbuild=override"]


class MiningFeePolicyTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 3
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [
            COMMON_ARGS,                                              # case 1
            COMMON_ARGS,                                              # case 2
            COMMON_ARGS + ["-mintxfee=%s" % RAISED_MIN_TX_FEE],       # case 3
        ]

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()
        # Deliberately not connected: each node mines its own chain.

    # ------------------------------------------------------------- helpers

    @staticmethod
    def rate(fee, nbytes):
        """Effective fee rate in GRC per 1000 bytes."""
        return (fee * Decimal(1000) / Decimal(nbytes)).quantize(Decimal("0.0000001"))

    def build(self, node, utxos, fee):
        """Spend `utxos` into one output, paying `fee` in total."""
        ins = [{"txid": u["txid"], "vout": u["vout"]} for u in utxos]
        total = sum((u["amount"] for u in utxos), Decimal(0))
        value = (total - fee).quantize(SAT)
        assert value > 0, "fee exceeds input value"
        signed = node.signrawtransactionwithwallet(
            node.createrawtransaction(ins, {node.getnewaddress(): value}))
        assert signed.get("complete"), signed
        return signed["hex"]

    def schedule_fee(self, nbytes):
        """The relay minimum: (1 + bytes/1000) * 0.001 GRC."""
        return (Decimal(1) + Decimal(nbytes // 1000)) * Decimal("0.001")

    def build_at_schedule_minimum(self, node, utxos, attempts=4):
        """Build once to learn the size, again to pay exactly the minimum.

        Signature (DER) lengths vary between signings, so the second build can
        differ in size from the probe. If that straddles a 1000-byte bucket the
        fee derived from the probe is below what relay requires and
        sendrawtransaction throws "insufficient-fee". Re-derive until the fee
        matches the transaction actually built.
        """
        probe = self.build(node, utxos, Decimal("0.05"))
        fee = self.schedule_fee(len(probe) // 2)
        for _ in range(attempts):
            final = self.build(node, utxos, fee)
            size = len(final) // 2
            if self.schedule_fee(size) == fee:
                return final, size, fee
            fee = self.schedule_fee(size)
        raise AssertionError(
            "transaction size did not settle on a fee bucket in %d builds" % attempts)

    def fund(self, node, spends):
        """Fund `node` with roughly twice the UTXOs its case will spend.

        The headroom is what keeps staking alive. Every spend this case makes
        parks an output in the mempool, and CWallet marks a parent spent on
        TxStateInMempool as well as TxStateConfirmed, so it leaves
        AvailableCoinsForStaking before the case mines. Sizing the universe to
        the exact number spent leaves nothing to stake on: at 24 of 28 outputs
        consumed, CreateCoinStake failed with "no stake found (need a mature
        UTXO with sufficient weight)" in ~2% of runs.
        """
        count = spends * 2 + 8
        node.generatetoaddress(12, node.getnewaddress())
        self.grow_utxo_universe(node, count=count)
        pool = [u for u in node.listunspent(1)
                if u["amount"] > Decimal("1.0") and u.get("spendable", True)]
        assert len(pool) >= spends, "need %d UTXOs, have %d" % (spends, len(pool))
        return pool

    def mine_one(self, node, attempts=4):
        """Mine one block, retrying the regtest stake-supply race.

        Each case locks most of the UTXO universe into mempool spends
        immediately before mining, and CWallet marks a parent spent on
        TxStateInMempool as well as TxStateConfirmed (wallet.cpp), so
        AvailableCoinsForStaking can momentarily find no eligible kernel and
        CreateCoinStake fails. nStakeMinAge is 0 on regtest, so the universe
        outputs really are kernel candidates and really do leave the set.

        This is a property of the setup, not of the transactions under test:
        grow_utxo_universe guards its own mine the same way. Without the retry
        this test flakes at roughly 4% -- measured at 11 failures in 263 runs.
        """
        last = None
        for _ in range(attempts):
            self.advance_to_next_stake_slot()
            try:
                node.generatetoaddress(1, node.getnewaddress())
                return
            except JSONRPCException as e:
                # Only the stake-supply race is retried; anything else is a
                # real failure and must not be masked.
                if "no stake found" not in str(e):
                    raise
                last = e
        raise AssertionError(
            "no block mined in %d attempts; last error: %s" % (attempts, last))

    def mine_and_read_order(self, node, submitted):
        """Mine one block; return the submitted names in selection order."""
        self.mine_one(node)
        tip = node.getblock(node.getbestblockhash(), True)
        mined = [t["txid"] if isinstance(t, dict) else t for t in tip["tx"]]
        return [submitted[t] for t in mined if t in submitted]

    # --------------------------------------------------------------- cases

    def test_rate_ordering(self, node, pool):
        """Selection order is descending effective fee rate, not absolute fee."""
        self.log.info("case 1: inclusion order follows fee RATE")

        specs = []
        cursor = 0

        # One bulk transaction paying the LARGEST absolute fee in the block.
        # If selection went by absolute fee it would come first; by rate it
        # belongs in the middle.
        bulk_hex = self.build(node, pool[cursor:cursor + BULK_INPUTS],
                              Decimal("0.035"))
        cursor += BULK_INPUTS
        specs.append(("bulk", bulk_hex, len(bulk_hex) // 2, Decimal("0.035")))

        # Small transactions straddling it in rate.
        for fee in (Decimal("0.001"), Decimal("0.002"), Decimal("0.004"),
                    Decimal("0.006")):
            h = self.build(node, pool[cursor:cursor + 1], fee)
            cursor += 1
            specs.append(("small-%s" % fee, h, len(h) // 2, fee))

        submitted = {}
        for name, h, nbytes, fee in specs:
            submitted[node.sendrawtransaction(h)] = name
            self.log.info("  %-12s %5d bytes  fee %-7s -> %s GRC/KB",
                          name, nbytes, fee, self.rate(fee, nbytes))

        expected = [name for name, _, nbytes, fee in
                    sorted(specs, key=lambda s: self.rate(s[3], s[2]),
                           reverse=True)]
        observed = self.mine_and_read_order(node, submitted)

        self.log.info("  expected: %s", expected)
        self.log.info("  observed: %s", observed)
        assert_equal(observed, expected)

        # The discriminating assertion: highest absolute fee, not selected first.
        biggest_fee = max(specs, key=lambda s: s[3])[0]
        assert_equal(biggest_fee, "bulk")
        assert observed[0] != "bulk", (
            "the transaction paying the largest absolute fee was selected "
            "first -- selection is ordering by fee, not by fee rate")

    def test_default_mintxfee_rejects_nothing(self, node, pool):
        """The default floor must not reject honest minimum-fee traffic.

        A ~3 KB transaction paying exactly the relay minimum. Its effective rate
        is a little over 0.001 GRC/KB against a 0.001 GRC/KB floor -- not the
        theoretical worst case, which is 100001 sat/KB at 99999 bytes (a margin
        of 1 sat/KB), but that needs ~675 inputs to construct. This asserts the
        margin is positive rather than pinning its size.
        """
        self.log.info("case 2: the default -mintxfee rejects nothing")

        hexstr, nbytes, fee = self.build_at_schedule_minimum(
            node, pool[:BULK_INPUTS])
        rate = self.rate(fee, nbytes)
        self.log.info("  %d bytes at the schedule minimum %s GRC -> %s GRC/KB "
                      "(default floor is 0.001)", nbytes, fee, rate)
        assert rate > Decimal("0.001"), (
            "test is not exercising what it claims: this transaction is below "
            "the default floor, so relay should not have accepted it either")

        submitted = {node.sendrawtransaction(hexstr): "schedule-minimum"}
        observed = self.mine_and_read_order(node, submitted)
        assert_equal(observed, ["schedule-minimum"])

    def test_raised_mintxfee_excludes_below_floor(self, node, pool):
        """A raised floor skips below-floor transactions, keeping them pooled."""
        self.log.info("case 3: -mintxfee=%s excludes below-floor transactions",
                      RAISED_MIN_TX_FEE)

        low_hex = self.build(node, pool[0:1], Decimal("0.001"))
        high_hex = self.build(node, pool[1:2], Decimal("0.002"))
        low_rate = self.rate(Decimal("0.001"), len(low_hex) // 2)
        high_rate = self.rate(Decimal("0.002"), len(high_hex) // 2)

        self.log.info("  low  %d bytes -> %s GRC/KB (below floor)",
                      len(low_hex) // 2, low_rate)
        self.log.info("  high %d bytes -> %s GRC/KB (above floor)",
                      len(high_hex) // 2, high_rate)
        assert low_rate < RAISED_MIN_TX_FEE < high_rate, (
            "the floor no longer straddles the two transactions")

        low_txid = node.sendrawtransaction(low_hex)
        high_txid = node.sendrawtransaction(high_hex)

        # -mintxfee is miner policy, not relay policy: both are accepted.
        assert_equal(sorted(node.getrawmempool()), sorted([low_txid, high_txid]))

        submitted = {low_txid: "low", high_txid: "high"}
        observed = self.mine_and_read_order(node, submitted)
        self.log.info("  mined: %s", observed)
        assert_equal(observed, ["high"])

        # Skipped by the miner, not dropped: it is still available to a later
        # block (or to a node running a lower floor).
        assert low_txid in node.getrawmempool(), (
            "the below-floor transaction left the mempool; -mintxfee must skip "
            "it during selection, not evict it")

    def run_test(self):
        node0, node1, node2 = self.nodes

        # fund() is told what each case SPENDS; it provisions the headroom.
        self.test_rate_ordering(node0, self.fund(node0, spends=BULK_INPUTS + 4))
        self.test_default_mintxfee_rejects_nothing(
            node1, self.fund(node1, spends=BULK_INPUTS))
        self.test_raised_mintxfee_excludes_below_floor(
            node2, self.fund(node2, spends=2))

        # Last, because it leaves node2 stopped.
        self.test_bad_mintxfee_refuses_to_start(2)

    def test_bad_mintxfee_refuses_to_start(self, n):
        """Case 4: an unparseable -mintxfee is rejected at startup.

        The point is the REJECTION, not the arithmetic. Parsing into a local and
        failing loudly is what stops the option silently falling back to its
        default, which is how -mintxfee came to be ignored for years -- so the
        failure path is the load-bearing half of that commit and nothing else
        exercises it.

        All three inputs fail inside ParseMoney rather than at the fee_rate < 0
        range check, which is unreachable: ParseMoney stops at the first
        non-digit, so "-1" fails as a parse and never reaches the comparison.
        The empty value is a real case and not a curiosity -- "-mintxfee=" is a
        SET argument whose value is "", ParseMoney accumulates no digits and
        ParseInt64 rejects the empty string, so the node refuses to start.
        """
        self.log.info("case 4: -mintxfee that cannot be parsed refuses to start")
        self.stop_node(n)

        for value in ("abc", "-1", ""):
            # Full args, not just the one under test: TestNode.start replaces
            # extra_args wholesale. PARTIAL_REGEX because the default
            # FULL_TEXT compares the entire stderr for equality.
            self.nodes[n].assert_start_raises_init_error(
                COMMON_ARGS + ["-mintxfee=%s" % value],
                "Invalid amount for -mintxfee=",
                match=ErrorMatch.PARTIAL_REGEX)

        # A valid value still starts, so the guard rejects bad input rather
        # than the option as such.
        self.start_node(n, COMMON_ARGS + ["-mintxfee=0.01"])
        self.stop_node(n)


if __name__ == "__main__":
    MiningFeePolicyTest().main()
