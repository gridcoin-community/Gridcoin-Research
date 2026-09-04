#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""The block-fill fee escalator, and the selection walk that survives it.

CreateRestOfTheBlock recomputes a minimum fee per candidate and compares it
against the candidate's ABSOLUTE fee:

    CAmount nMinFee = GetMinFee(tx, nBlockSize, GMF_BLOCK);   # miner.cpp
    ...
    if (nTxFees < nMinFee) { ...; continue; }

This test covers that handler. It is the branch mining_fee_policy.py cannot
reach, because reaching it needs a raised -blockmaxsize.

WHY THE DEFAULT CONFIGURATION CANNOT REACH IT
---------------------------------------------
GetMinFee (policy/fees.h) is called three ways, and the miner's is the cheapest
of the three:

  relay      GetMinFee(tx, 1000, GMF_RELAY, nSize)   (1 + size/1000) * 0.001
  miner      GetMinFee(tx, nBlockSize, GMF_BLOCK)    0.001, times the escalator
  consensus  GetMinFee(tx)                           flat 0.001 (nBlockSize = 1
                                                     disables the escalator)

The miner passes nBytes = 0, so its base requirement is a FLAT 0.001 GRC while
relay already charged (1 + size/1000) * 0.001. Nothing that reached the mempool
can fail the miner's check -- unless the escalator lifts it above relay's:

    if (nBlockSize != 1 && nNewBlockSize >= MAX_BLOCK_SIZE_GEN/2) {
        if (nNewBlockSize >= MAX_BLOCK_SIZE_GEN) return MAX_MONEY;
        nMinFee *= MAX_BLOCK_SIZE_GEN / (MAX_BLOCK_SIZE_GEN - nNewBlockSize);
    }

That needs an accumulated nBlockSize >= 250000. The size check earlier in the
loop holds nBlockSize below -blockmaxsize, whose default is exactly 250000, so
at stock settings the escalator is unreachable and the miner's floor is a flat
0.001. Raising -blockmaxsize (clamped to 999000, and not validated at init)
opens it. That is all this test does that mining_fee_policy.py does not.

Note the multiplier is integer division of two unsigned ints, so the floor is a
STEP function, not a curve:

    nBlockSize in [250000, 333333] -> x2 -> 0.002 GRC
    nBlockSize in [333334, 374999] -> x3 -> 0.003 GRC
    nBlockSize >= 500000           -> MAX_MONEY

This test aims at the middle of the x2 band, which is 83334 bytes wide.

THE SHAPE OF THE PROOF
----------------------
Three roles, all 1-input/N-output spends of node0-exclusive universe outputs:

  filler  x81  ~3563 B  fee 0.012   rate ~0.00337 GRC/KB  fills the block
  trap    x1    ~639 B  fee 0.0015  rate ~0.00235 GRC/KB  fails the 0.002 floor
  prize   x1  ~10023 B  fee 0.012   rate ~0.00120 GRC/KB  clears the 0.002 floor

The heap is keyed on fee RATE, so they pop filler -> trap -> prize. The trap is
therefore popped only once the block has filled past 250000, and the prize is
popped after the trap. The trap pays more than relay and consensus require
(both 0.001 at its size) and less than the escalated miner floor (0.002); its
fee window is only 2x wide, which is the tightest constraint in the design.

  with break:    the trap aborts selection -- the prize is never reached
  with continue: the trap is skipped and the prize is included

So "the prize is in the block" is the assertion that distinguishes the two.

WHY THIS IS TWO CONNECTED NODES
-------------------------------
node0 spends, node1 stakes. A node that has just parked most of its wallet in
the mempool cannot reliably stake: CWallet marks a parent spent on
TxStateInMempool as well as TxStateConfirmed, so those outputs leave
AvailableCoinsForStaking and CreateCoinStake fails with "no stake found".
mining_fee_policy.py documents that hazard at ~1.3% even after tuning; this
test parks 83 transactions, far more than it does.

Splitting the roles works because a wallet only tracks outputs it has keys for
(AddToWalletIfInvolvingMe returns false otherwise), and grow_utxo_universe fans
its outputs to funder.getnewaddress(). node1 never sees node0's spends, so its
stake weight is untouched. The one shared input is the premine: every regtest
node imports the same private key, so the single UTXO the fan-out spends does
leave node1's stakeable set too. Staking itself is UTXO-count-conserving
(CreateCoinStake takes one input and returns one output of equal value to the
same key) and regtest maturity is 0, so that costs one output, not a pool.

This is a separate file rather than a fourth mining_fee_policy.py case because
that file keeps three DELIBERATELY UNCONNECTED nodes, while grow_utxo_universe,
sync_blocks, sync_mempools and advance_to_next_stake_slot all default to every
node in self.nodes and assert each has a peer. Here self.nodes IS the connected
pair, so every framework default is correct and no framework change is needed.
"""
from decimal import Decimal

from test_framework.authproxy import JSONRPCException
from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than

SAT = Decimal("0.00000001")

# Raised so the escalator can fire at all. Anything above 250000 works; 470000
# leaves room above the target fill without approaching the 500000 MAX_MONEY
# cliff. The miner clamps this to [1000, MAX_BLOCK_SIZE - 1000].
BLOCK_MAX_SIZE = 470000

# MAX_BLOCK_SIZE_GEN/2, the point at which the escalator starts, and the top of
# the x2 band. Mirrors consensus/consensus.h and policy/fees.h.
ESCALATOR_START = 250000
X2_BAND_TOP = 333333

# The escalated floor in the x2 band: GetBaseFee (MIN_TX_FEE * 10 for
# nVersion >= 2) times 2.
ESCALATED_FLOOR = Decimal("0.002")

# The default -mintxfee floor, in GRC per 1000 bytes. Every role has to clear
# this or the case would be exercising that check instead of this one.
MIN_TX_FEE_RATE = Decimal("0.001")

# Output counts per role. A P2PKH output serializes to 34 bytes and a P2PKH
# input to ~148, so a 1-in/N-out transaction is ~163 + 34N bytes.
FILLER_OUTPUTS = 100   # ~3563 B
TRAP_OUTPUTS = 14      # ~639 B
PRIZE_OUTPUTS = 290    # ~10023 B

# Fees. The filler overpays so its RATE stays above the trap's; the trap sits
# inside its 2x window [0.001, 0.002); the prize pays a large absolute fee at a
# low rate, which is what puts it last in the heap and above the floor.
FILLER_FEE = Decimal("0.012")
TRAP_FEE = Decimal("0.0015")
PRIZE_FEE = Decimal("0.012")

N_FILLERS = 81

# Universe slots to provision. 83 are spent; the rest is headroom.
UNIVERSE = 90

# No selected input may be this large. The regtest premine is 10 outputs of
# 100000 GRC under a key EVERY node imports, so a premine output is shared with
# the staker; the universe outputs this test spends are ~1111 GRC. Any value in
# between separates the two, and the assertion is what stops a future change to
# UNIVERSE or the funding path from quietly reintroducing the 15% stake flake.
PREMINE_FLOOR = Decimal("10000")

# Distinct addresses to pre-generate. Sized for the LARGEST transaction, not
# the filler: createrawtransaction takes its outputs as a JSON object, so
# repeated addresses collapse in the Python dict before the RPC ever sees them
# and its duplicate-address guard never fires. Too small a pool would silently
# shrink the prize instead of raising.
ADDRESS_POOL = PRIZE_OUTPUTS + 10


class MiningFeeEscalatorTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.chain = "regtest"
        self.setup_clean_chain = True
        common = ["-staking=0", "-devbuild=override",
                  "-blockmaxsize=%d" % BLOCK_MAX_SIZE]
        self.extra_args = [
            common,                     # node0: spends
            common + ["-debug=noisy"],  # node1: stakes, and logs the handler
        ]

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()
        self.connect_nodes(1, 0)
        self.sync_blocks()

    # ------------------------------------------------------------- helpers

    @staticmethod
    def rate(fee, nbytes):
        """Effective fee rate in GRC per 1000 bytes -- the miner's dFeePerKb."""
        return (fee * Decimal(1000) / Decimal(nbytes)).quantize(Decimal("0.0000001"))

    @staticmethod
    def schedule_fee(nbytes):
        """What relay requires: (1 + bytes/1000) * 0.001 GRC."""
        return (Decimal(1) + Decimal(nbytes // 1000)) * Decimal("0.001")

    def build_multi_output(self, node, utxo, n_outputs, fee):
        """Spend one UTXO into `n_outputs` outputs, paying `fee` in total.

        The address pool is reused across transactions -- duplicates are only
        rejected WITHIN one transaction (rpc/rawtransaction.cpp). The length
        assertion is what makes a pool shortfall raise here rather than
        silently produce a smaller transaction at the wrong fee rate.
        """
        assert n_outputs <= len(self.addresses), (
            "address pool holds %d, need %d" % (len(self.addresses), n_outputs))
        value = ((utxo["amount"] - fee) / n_outputs).quantize(SAT)
        assert value > 0, "fee exceeds input value"
        outputs = {addr: value for addr in self.addresses[:n_outputs]}
        assert_equal(len(outputs), n_outputs)
        raw = node.createrawtransaction(
            [{"txid": utxo["txid"], "vout": utxo["vout"]}], outputs)
        signed = node.signrawtransactionwithwallet(raw)
        assert signed.get("complete"), signed
        return signed["hex"]

    def mine_one(self, node, attempts=4):
        """Mine one block on `node`, retrying the regtest stake slot."""
        last = None
        for _ in range(attempts):
            self.advance_to_next_stake_slot()
            try:
                node.generatetoaddress(1, node.getnewaddress())
                return
            except JSONRPCException as e:
                if "no stake found" not in str(e):
                    raise
                last = e
        raise AssertionError(
            "no block mined in %d attempts; last error: %s" % (attempts, last))

    # ------------------------------------------------------------ the case

    def run_test(self):
        node0, node1 = self.nodes

        self.log.info("funding node0 with a %d-slot universe", UNIVERSE)
        node0.generatetoaddress(12, node0.getnewaddress())
        self.sync_blocks()
        # confirm_on=node1 keeps the split's confirming block off node0, whose
        # stake budget the funding blocks already drew on.
        self.grow_utxo_universe(node0, count=UNIVERSE,
                                agree_with=node1, confirm_on=node1)

        self.addresses = [node0.getnewaddress() for _ in range(ADDRESS_POOL)]
        needed = N_FILLERS + 2

        # SMALLEST FIRST, and never a premine output. Every regtest node
        # imports the same premine key, so node0's wallet also lists the ~10
        # genesis outputs of 100000 GRC -- and those are exactly what node1 has
        # left to stake on. Spending them here strips node1's stakeable set and
        # CreateCoinStake fails with "no stake found": measured at 15% before
        # this filter, against 0% after. The universe outputs are ~1111 GRC and
        # node0-exclusive, which is what makes the spend invisible to node1.
        # Same reasoning as rpc_psgtpool.py, which selects smallest-first to
        # keep its funding input out of the staker's likeliest picks.
        candidates = sorted((u for u in node0.listunspent(1)
                             if u.get("spendable", True)),
                            key=lambda u: u["amount"])
        assert len(candidates) >= needed, (
            "need %d UTXOs, have %d" % (needed, len(candidates)))
        pool = candidates[:needed]
        assert_greater_than(PREMINE_FLOOR, max(u["amount"] for u in pool))
        assert_greater_than(min(u["amount"] for u in pool), Decimal("1.0"))

        self.log.info("building %d fillers, one trap and one prize", N_FILLERS)
        fillers = [self.build_multi_output(node0, pool[i], FILLER_OUTPUTS,
                                           FILLER_FEE)
                   for i in range(N_FILLERS)]
        trap = self.build_multi_output(node0, pool[N_FILLERS], TRAP_OUTPUTS,
                                       TRAP_FEE)
        prize = self.build_multi_output(node0, pool[N_FILLERS + 1],
                                        PRIZE_OUTPUTS, PRIZE_FEE)

        # ---- pre-flight on the MEASURED sizes -----------------------------
        #
        # Everything below is derived from the transactions actually built, not
        # from the sizes this file predicts. If signature-length drift moves a
        # transaction across a fee bucket or a rate boundary, the test fails
        # here -- loudly, before it can turn into a case that passes with and
        # without the fix.
        filler_sz = [len(h) // 2 for h in fillers]
        trap_sz, prize_sz = len(trap) // 2, len(prize) // 2
        filler_rate = min(self.rate(FILLER_FEE, s) for s in filler_sz)
        trap_rate = self.rate(TRAP_FEE, trap_sz)
        prize_rate = self.rate(PRIZE_FEE, prize_sz)
        filled = sum(filler_sz)

        self.log.info("  filler %d x %d B  fee %s -> rate %s GRC/KB (min)",
                      N_FILLERS, filler_sz[0], FILLER_FEE, filler_rate)
        self.log.info("  trap      %5d B  fee %s -> rate %s GRC/KB",
                      trap_sz, TRAP_FEE, trap_rate)
        self.log.info("  prize     %5d B  fee %s -> rate %s GRC/KB",
                      prize_sz, PRIZE_FEE, prize_rate)
        self.log.info("  fillers total %d B (escalator starts at %d)",
                      filled, ESCALATOR_START)

        # The heap pops by descending rate, and the whole proof depends on this
        # order: fill the block, THEN hit the trap, THEN offer the prize.
        assert_greater_than(filler_rate, trap_rate)
        assert_greater_than(trap_rate, prize_rate)

        # The fillers must carry nBlockSize into the x2 band and not past it.
        # A margin is left at both ends for the block's own fixed overhead
        # (header + coinbase + coinstake + the reserved coinstake outputs).
        assert_greater_than(filled, ESCALATOR_START + 2000)
        assert_greater_than(X2_BAND_TOP - 2000, filled)

        # The trap is the only transaction that must FAIL the escalated floor,
        # and it has to be valid everywhere else: relay and consensus both
        # charge the flat schedule at its size.
        assert_greater_than(ESCALATED_FLOOR, TRAP_FEE)
        assert_greater_than(TRAP_FEE + SAT, self.schedule_fee(trap_sz))
        # Everything else must clear the escalated floor, or the block would
        # stop filling for a reason this test is not about.
        assert_greater_than(FILLER_FEE, ESCALATED_FLOOR)
        assert_greater_than(PRIZE_FEE, ESCALATED_FLOOR)
        assert_greater_than(PRIZE_FEE + SAT, self.schedule_fee(prize_sz))
        assert_greater_than(FILLER_FEE + SAT, self.schedule_fee(max(filler_sz)))
        # And every rate must clear the default -mintxfee floor, so this case
        # cannot accidentally re-test that check instead.
        assert_greater_than(prize_rate, MIN_TX_FEE_RATE)

        # ---- submit --------------------------------------------------------
        self.log.info("submitting %d transactions on node0", needed)
        for h in fillers:
            node0.sendrawtransaction(h)
        trap_txid = node0.sendrawtransaction(trap)
        prize_txid = node0.sendrawtransaction(prize)

        # flush_scheduler=False: the default path calls
        # syncwithvalidationinterfacequeue, which this daemon does not
        # implement (-32601). No other functional test calls sync_mempools, so
        # that default has never been exercised here. Mempool equality across
        # the pair is the only thing this needs.
        self.sync_mempools([node0, node1], flush_scheduler=False)
        assert_equal(len(node1.getrawmempool()), needed)

        # ---- mine on node1 -------------------------------------------------
        self.log.info("staking one block on node1")
        # The handler logs under BCLog::NOISY before it skips. Matching it
        # witnesses that the trap left the selection walk through the intended
        # exit and not through one of the five earlier `continue`s (size,
        # -mintxfee, sigops, nTime, FetchInputs). It fires on the fixed and the
        # unfixed tree alike -- it proves reachability, not the fix.
        expected_log = "Not including tx %s  due to TxFees of" % trap_txid
        with node1.assert_debug_log([expected_log], timeout=60):
            self.mine_one(node1)

        tip = node1.getblock(node1.getbestblockhash(), True)
        mined = [t["txid"] if isinstance(t, dict) else t for t in tip["tx"]]
        self.log.info("  block holds %d transactions (%d submitted)",
                      len(mined) - 2, needed)

        # THE discriminating assertion. With `break` the trap ends selection and
        # the prize -- lower rate, so popped later -- never gets its turn.
        assert prize_txid in mined, (
            "the prize transaction was not selected. It pays %s GRC, well over "
            "the escalated floor of %s, and was still in the mempool. This is "
            "what `break` in the nTxFees < nMinFee handler does: one "
            "underpaying transaction ends selection for the whole block."
            % (PRIZE_FEE, ESCALATED_FLOOR))

        # The trap itself must be out -- otherwise the escalator never fired and
        # the block filled below 250000, so nothing above was exercised.
        assert trap_txid not in mined, (
            "the trap was included, so the escalated floor never applied: "
            "fillers totalled %d bytes against a %d start"
            % (filled, ESCALATOR_START))

        # Skipped by the miner, not evicted: still available to a later block,
        # or to a node running a smaller -blockmaxsize.
        self.sync_blocks([node0, node1])
        assert trap_txid in node1.getrawmempool(), (
            "the trap left the mempool; the handler must skip during selection, "
            "not evict")


if __name__ == "__main__":
    MiningFeeEscalatorTest().main()
