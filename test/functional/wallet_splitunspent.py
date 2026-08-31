#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Test the splitunspent RPC: count / size / optimal modes, the per-piece fee
floor, the piece cap, the -minstakesplitvalue floor, the error paths and the
consolidateunspent round trip.

Three regtest facts shape the design:

1. -devbuild=override is mandatory. Without it CWallet::CommitTransaction
   short-circuits (src/wallet/wallet.cpp) and every splitunspent fails with RPC
   -4 "...coins in your wallet were already spent..." even though the wallet is
   fine. Raw-transaction funding is NOT gated, so a missing flag funds cleanly
   and only fails at the split.

2. generatetoaddress' address argument is advisory: the coinstake pays back to
   the kernel's own script, so on regtest every reward lands on the single
   premine address and a fresh address never receives anything from mining.
   Split targets are funded by explicit raw transactions instead, and the test
   mines no blocks at all.

3. splitunspent calls AvailableCoins with fOnlyConfirmed=false, and depth-0
   mempool outputs clear the remaining filters, so the funding outputs never
   need to confirm.

The test funds fresh addresses rather than reusing the premine address. Not
because premine coinbases are unspendable -- they are spendable, and
splitunspent on the premine address succeeds -- but because splitunspent sweeps
EVERY UTXO at the target, and splitting the premine turns the whole stakeable
supply into depth-0 outputs, after which the node can never stake again
("CreateCoinStake: no stake found").
"""

from decimal import Decimal

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error

# A deliberately generous flat fee for the funding transactions, which are
# hand-built and never fee-estimated.
FUNDING_FEE = Decimal("1")

# A valid regtest address the wallet does not own (validateaddress: ismine false).
FOREIGN_ADDRESS = "mipcBbFg9gMiCh81Kj8tqqdgoZub1ZJRfn"


class WalletSplitUnspentTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        # -paytxfee=0.001 fixes nTransactionFee for this run (0.001 happens
        # to be today's default). The exact-fee assertions below (per-piece
        # floor, conservation sums) are written against this value, so a
        # change to the daemon default cannot shift them.
        self.extra_args = [["-staking=0", "-connect=0", "-listen=0", "-devbuild=override",
                            "-paytxfee=0.001"]]

    def setup_network(self):
        # Single isolated regtest node; bypass the base regtest createwallet path
        # (Gridcoin has one default BDB wallet, no multiwallet).
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def fund(self, node, amounts, source=None, addresses=None):
        """Pay {name: Decimal} in one raw transaction. Mines nothing.

        Returns (addresses, leftover). leftover is the part of the source UTXO
        set this transaction did not consume, so a follow-up funding
        transaction can be built from it directly: re-scanning listunspent
        would happily select the depth-0 outputs just created and double-spend
        the split targets.
        """
        if source is None:
            source = node.listunspent(0)
        if addresses is None:
            addresses = {}

        addr = {name: addresses.get(name) or node.getnewaddress() for name in amounts}
        total = sum(amounts.values())

        inputs, gathered, consumed = [], Decimal(0), 0
        for utxo in source:
            inputs.append({"txid": utxo["txid"], "vout": utxo["vout"]})
            gathered += utxo["amount"]
            consumed += 1
            if gathered >= total + FUNDING_FEE:
                break
        assert gathered >= total + FUNDING_FEE, "funding source does not cover the plan"

        outputs = {addr[name]: value for name, value in amounts.items()}
        outputs[node.getnewaddress()] = gathered - total - FUNDING_FEE

        signed = node.signrawtransactionwithwallet(node.createrawtransaction(inputs, outputs))
        assert_equal(signed["complete"], True)
        node.sendrawtransaction(signed["hex"])

        return addr, source[consumed:]

    def run_test(self):
        node = self.nodes[0]

        addr, leftover = self.fund(node, {
            "A":     Decimal("20000"),    # count mode + round trip
            "B":     Decimal("20000"),    # size mode
            "C":     Decimal("20000"),    # optimal mode
            "D":     Decimal("5000"),     # per-piece fee floor + "0" regression
            "E":     Decimal("500"),      # below-one-piece error paths
            "F":     Decimal("0.01"),     # fee-not-covered error path
            "G":     Decimal("10"),       # nothing-to-split error path
            "BIG":   Decimal("500000"),   # piece cap: reject, clamp, and reversal
            "NUL":   Decimal("4000"),     # explicit JSON null equivalence
            "FL1":   Decimal("20000"),    # -minstakesplitvalue raised to 1500
            "FL2":   Decimal("20000"),    # -minstakesplitvalue below the hard floor
            "MULTI": Decimal("3000"),     # multi-UTXO sweep (paid again below)
        })

        # A second funding transaction pays MULTI again, from premine UTXOs the
        # first one did not touch, so that MULTI holds two spendable outputs and
        # the sweep is exercised with utxos_swept > 1.
        self.fund(node, {"MULTI": Decimal("3000")}, source=leftover, addresses=addr)

        assert_equal(node.getblockcount(), 0)
        self.log.info("funded %d split targets at height 0 with no blocks mined", len(addr))

        # ---- count mode, value conservation, and the returned types ----
        # piece_value / change_value / fee are FormatMoney STRINGS (at least two
        # decimals, no trailing-zero padding beyond that), not numbers. Compare
        # them through Decimal(); a bare numeric compare silently fails.
        r = node.splitunspent(addr["A"], 0, 10)
        assert_equal(r["result"], True)
        assert_equal(r["utxos_swept"], 1)
        assert_equal(r["pieces_created"], 10)
        assert_equal(Decimal(r["fee"]), Decimal(r["pieces_created"]) * Decimal("0.001"))
        assert_equal(Decimal(r["piece_value"]) * r["pieces_created"]
                     + Decimal(r["change_value"]) + Decimal(r["fee"]), Decimal("20000"))
        assert r["txid"] in node.getrawmempool()

        # getrawtransaction serves mempool transactions, but a mempool
        # transaction has no 'confirmations'/'blockhash'/'blocktime' key.
        # vout values are Decimal, unlike the splitunspent result strings.
        decoded = node.getrawtransaction(r["txid"], True)
        values = [out["value"] for out in decoded["vout"]]
        assert_equal(len(values), r["pieces_created"] + 1)
        assert_equal(sum(values) + Decimal(r["fee"]), Decimal("20000"))
        assert_equal({a for out in decoded["vout"]
                      for a in out["scriptPubKey"]["addresses"]}, {addr["A"]})
        # Every output pays the split address, and the change output is not
        # positional, so identify it by value rather than by index.
        piece = Decimal(r["piece_value"])
        assert_equal(sum(1 for v in values if v == piece), r["pieces_created"])
        assert_equal([v for v in values if v != piece], [Decimal(r["change_value"])])

        # ---- size mode: pieces = floor((amount - fee - slack) / piece_size) ----
        r = node.splitunspent(addr["B"], 1000)
        assert_equal(r["result"], True)
        assert_equal(Decimal(r["piece_value"]), Decimal("1000"))
        assert_equal(r["pieces_created"], 19)
        assert_equal(Decimal(r["piece_value"]) * r["pieces_created"]
                     + Decimal(r["change_value"]) + Decimal(r["fee"]), Decimal("20000"))

        # ---- optimal mode ----
        # 800 GRC here is deterministic, not merely likely: GetOptimalStakeSplitValue
        # returns the greater of the -minstakesplitvalue floor and a multiple of
        # GetAverageDifficulty(160), and regtest difficulty never rises far enough
        # to lift the second term above the floor -- walking the chain to staking
        # exhaustion left the optimal size at 800.
        r = node.splitunspent(addr["C"])
        assert_equal(r["result"], True)
        # The one deliberate exact-string assertion, documenting the FormatMoney
        # contract: amounts are strings with a minimum of two decimal places.
        assert_equal(r["piece_value"], "800.00")
        assert_equal(r["pieces_created"], 24)
        assert_equal(Decimal(r["piece_value"]) * r["pieces_created"]
                     + Decimal(r["change_value"]) + Decimal(r["fee"]), Decimal("20000"))

        # ---- per-piece fee floor ----
        # Only observable on a single-input address: with many inputs the
        # size-based fee exceeds the floor and wins.
        r = node.splitunspent(addr["D"], 0, 5)
        assert_equal(r["utxos_swept"], 1)
        assert_equal(r["pieces_created"], 5)
        assert_equal(Decimal(r["fee"]), Decimal("0.005"))

        # ---- the sweep collects every UTXO on the address ----
        r = node.splitunspent(addr["MULTI"], 0, 5)
        assert_equal(r["utxos_swept"], 2)
        assert_equal(r["pieces_created"], 5)
        assert_equal(Decimal(r["piece_value"]) * r["pieces_created"]
                     + Decimal(r["change_value"]) + Decimal(r["fee"]), Decimal("6000"))
        assert_equal(len(node.getrawtransaction(r["txid"], True)["vin"]), 2)

        # ---- piece cap, exercised at the limit ----
        r = node.splitunspent(addr["BIG"], 0, 599)
        assert_equal(r["pieces_created"], 599)
        assert_equal(Decimal(r["fee"]), Decimal("0.599"))
        # The maximum depends on the input count (599 with one input, fewer with
        # many), and the cap is checked before any piece-size arithmetic, so a
        # small address reaches it too. Match the wording, never the number.
        assert_raises_rpc_error(-8, "exceeds the maximum of",
                                node.splitunspent, addr["E"], 0, 600)

        # ---- piece_size as the JSON string "0" is the unset sentinel ----
        # Must be a Python str, because the string is the form the old guard
        # MISSED. That guard was numeric-only -- params[1].isNum() &&
        # get_real() == 0.0 -- so a JSON number 0 was intercepted before
        # AmountFromValue ever saw it, while "0" fell through to
        # AmountFromValue and was rejected -3 "Invalid amount" by its
        # strictly-positive check. An int 0 would pass here with or without
        # the fix and pin nothing.
        r = node.splitunspent(addr["D"], "0", 4)
        assert_equal(r["result"], True)
        assert_equal(r["pieces_created"], 4)
        # The previous split left D holding its five pieces plus the change
        # output, and this one sweeps all six of them back up.
        assert_equal(r["utxos_swept"], 6)

        # ---- explicit JSON null means "omitted" for both parameters ----
        # Assert through the error path rather than by comparing two consecutive
        # splits: the first split consumes the address' UTXOs, so the second call
        # could never return the same thing.
        # "-0" is unset too, and it is the one input whose meaning this
        # change actually flips -- but only as a STRING. A JSON number -0 was
        # already the sentinel before the fix, because get_real() returns
        # -0.0 and -0.0 == 0.0.
        for piece_size in (0, "0", "0.0", "-0", "0e9", None):
            assert_raises_rpc_error(-8, "into 10 pieces results in pieces below the minimum",
                                    node.splitunspent, addr["NUL"], piece_size, 10)

        # The other half of the same contract: what is NOT the sentinel. The
        # guard reads its token exactly the way AmountFromValue does --
        # ParseFixedPoint(getValStr(), 8) -- so membership is decided by that
        # grammar rather than by a string compare against "0", and these three
        # fail the grammar itself rather than the zero test. "00" is a single
        # leading zero followed by trailing garbage, " 0" has no leading
        # digit, "" is empty. All three fall through to AmountFromValue, which
        # re-runs the same parse and raises -3. Pinning them is the point:
        # a hand-rolled sentinel would drift the moment either side changed.
        # "0e10" pins the one place the grammar is NARROWER than the old
        # numeric acceptance: ParseFixedPoint's overflow guard fires on the
        # exponent field alone (a zero mantissa does not cancel an e-notation
        # exponent the way trailing decimal zeros do), so zero written with an
        # exponent of 10 or more fails the grammar outright -- while "0e9"
        # above still parses to zero and is unset. The old guard accepted the
        # raw JSON token 0e10 as the sentinel (get_real() == 0.0); rejecting
        # it keeps the sentinel aligned with the amount grammar, which never
        # accepted 0e10 as an amount either.
        for piece_size in ("00", " 0", "", "0e10"):
            assert_raises_rpc_error(-3, "Invalid amount",
                                    node.splitunspent, addr["NUL"], piece_size, 10)

        # Every rejection above raises before any wallet work, so NUL still
        # holds its funding output for the real split here.
        assert_equal(Decimal(node.splitunspent(addr["NUL"], 0, None)["piece_value"]),
                     Decimal("800"))

        # ---- the -minstakesplitvalue floor, moved at runtime ----
        # changesettings does a gArgs.ForceSetArg and GetOptimalStakeSplitValue
        # re-reads gArgs on every call, so the new floor takes effect immediately.
        # A restart would be worse than unnecessary: it re-derives the wallet's
        # HD seed, every address created here becomes ismine=false, and
        # splitunspent then quietly returns the no-op object.
        changed = node.changesettings("minstakesplitvalue=1500")
        assert_equal(changed["settings_change_requires_restart"], False)
        assert_equal(changed["settings_changed_taking_immediate_effect"],
                     ["minstakesplitvalue=1500"])
        assert_raises_rpc_error(-8, "piece_size must be at least 1500.00 GRC",
                                node.splitunspent, addr["FL1"], 1499)
        r = node.splitunspent(addr["FL1"])
        assert_equal(Decimal(r["piece_value"]), Decimal("1500"))
        assert_equal(r["pieces_created"], 13)
        assert_equal(Decimal(r["piece_value"]) * r["pieces_created"]
                     + Decimal(r["change_value"]) + Decimal(r["fee"]), Decimal("20000"))

        # A -minstakesplitvalue below the hard stake-split minimum is clamped
        # back up to it, so the floor returns to 800 GRC.
        node.changesettings("minstakesplitvalue=100")
        assert_raises_rpc_error(-8, "piece_size must be at least 800.00 GRC",
                                node.splitunspent, addr["FL2"], 100)
        r = node.splitunspent(addr["FL2"])
        assert_equal(Decimal(r["piece_value"]), Decimal("800"))
        assert_equal(r["pieces_created"], 24)

        # ---- error paths ----
        assert_raises_rpc_error(-8, "At most one of piece_size and piece_count may be provided",
                                node.splitunspent, addr["E"], 1000, 10)
        assert_raises_rpc_error(-8, "piece_count cannot be negative.",
                                node.splitunspent, addr["E"], 0, -1)
        assert_raises_rpc_error(-8, "results in pieces below the minimum piece size of 800.00 GRC",
                                node.splitunspent, addr["E"], 0, 1)
        assert_raises_rpc_error(-8, "is too small to create one piece of 800.00 after the fee.",
                                node.splitunspent, addr["E"], 800)
        assert_raises_rpc_error(-8, "is already at or below the efficiency-optimal UTXO size",
                                node.splitunspent, addr["G"])
        assert_raises_rpc_error(-6, "does not cover the transaction fee",
                                node.splitunspent, addr["F"], 0, 599)
        assert_raises_rpc_error(-5, "Invalid Gridcoin address: notanaddress",
                                node.splitunspent, "notanaddress", 0, 10)
        assert_raises_rpc_error(-3, "Invalid amount",
                                node.splitunspent, addr["E"], -1)

        # A valid address with nothing spendable on it -- owned or not -- is a
        # no-op rather than an error.
        assert_equal(node.splitunspent(node.getnewaddress(), 0, 10),
                     {"result": False, "utxos_swept": 0})
        info = node.validateaddress(FOREIGN_ADDRESS)
        assert_equal(info["isvalid"], True)   # self-check the constant's claim
        assert_equal(info["ismine"], False)
        assert_equal(node.splitunspent(FOREIGN_ADDRESS, 0, 10),
                     {"result": False, "utxos_swept": 0})

        # ---- round trip: the piece cap keeps a split reversible ----
        # A split leaves pieces_created pieces plus one change output, all on
        # the split address, and a single consolidateunspent takes them back to
        # one UTXO -- while still unconfirmed, because consolidateunspent calls
        # AvailableCoins with fOnlyConfirmed=false too.
        r = node.consolidateunspent(addr["A"], 100000, 599)
        assert_equal(r["result"], True)
        assert_equal(r["utxos_consolidated"], 11)
        assert_equal(len([u for u in node.listunspent(0) if u["address"] == addr["A"]]), 1)

        # 599 is the cap precisely so that the pieces plus the change output
        # still fit under the 600-input consolidation limit.
        r = node.consolidateunspent(addr["BIG"], 10000000, 600)
        assert_equal(r["result"], True)
        assert_equal(r["utxos_consolidated"], 600)
        assert_equal(len([u for u in node.listunspent(0) if u["address"] == addr["BIG"]]), 1)

        # ---- the piece cap clamps rather than raising, in size and optimal mode ----
        # Count mode rejects a request above the cap (asserted earlier); size and
        # optimal mode have no count to reject, so they silently take the minimum
        # of the requested and the maximum piece count. That clamp is the whole
        # reason a split stays reversible by a single consolidateunspent, and
        # nothing else in this test pins it.
        #
        # BIG is back to one ~500,000 GRC output, which asks for ~624 pieces of
        # 800 -- well clear of the cap, so neither assertion sits on a boundary.
        # The address is reconsolidated between the two modes so they do not
        # share a starting state.
        for mode, args in (("size", (800,)), ("optimal", ())):
            big = next(u for u in node.listunspent(0) if u["address"] == addr["BIG"])
            assert big["amount"] // Decimal("800") > 599, \
                "BIG no longer over-subscribes the cap in %s mode" % mode

            r = node.splitunspent(addr["BIG"], *args)
            assert_equal(r["result"], True)
            assert_equal(r["utxos_swept"], 1)
            assert_equal(r["pieces_created"], 599)
            assert_equal(Decimal(r["piece_value"]), Decimal("800"))
            # The clamp caps the piece count, it does not pay for the pieces that
            # were not created: the fee covers the 599 actually made, and the rest
            # of the balance comes back as change.
            assert_equal(Decimal(r["fee"]), Decimal("0.599"))
            assert_equal(Decimal(r["piece_value"]) * r["pieces_created"]
                         + Decimal(r["change_value"]) + Decimal(r["fee"]), big["amount"])
            # A clamped split is still reversible in one transaction, which is what
            # the cap exists to guarantee.
            assert_equal(node.consolidateunspent(addr["BIG"], 10000000, 600)["utxos_consolidated"], 600)

        # Fewer than two candidate inputs is a no-op, and the no-op branch
        # spells the count key 'UTXOs consolidated', not 'utxos_consolidated'.
        assert_equal(node.consolidateunspent(addr["A"]),
                     {"result": False, "UTXOs consolidated": 0})

        self.log.info("splitunspent count/size/optimal, fee floor, piece cap, "
                      "-minstakesplitvalue floor, error paths and round trip all pass "
                      "at height %d", node.getblockcount())


if __name__ == "__main__":
    WalletSplitUnspentTest().main()
