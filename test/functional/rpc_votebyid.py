#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""votebyid takes the numeric ids of a poll's choices.

A vote holds each choice as one byte, so a choice id outside 0..255 is a
parameter error. An id inside that range reaches the vote builder, which
refuses one that names no choice of the poll, or one given twice.
"""

from decimal import Decimal, ROUND_DOWN

from test_framework.test_framework import GridcoinTestFramework, STAKE_TIMESTAMP_MASK
from test_framework.util import assert_equal, assert_greater_than, assert_raises_rpc_error

# A poll's eligibility claim is built from outputs at least this deep
# (CWalletTx::IsConfirmed in the PollClaimBuilder), as in feature_vote_results.py.
CLAIM_DEPTH = 10
# Mock-clock advance per block, past the 90-second target spacing. Blocks mined
# faster raise the difficulty with every block, and the kernel is evaluated at
# one slot per attempt, so a run of them can leave generatetoaddress out of
# attempts ("no stake found").
BLOCK_SPACING = 96
# Ordinary outputs the premine split below makes for the claim.
SPLIT_COUNT = 96


class VoteByIdTest(GridcoinTestFramework):
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
        # addpoll claims 100,000 GRC from outputs CLAIM_DEPTH blocks deep, and
        # every block restakes one output. Before the first block, split two
        # premine outputs into SPLIT_COUNT ordinary outputs of about 2,083 GRC,
        # as feature_vote_results.py splits its premine. The CLAIM_DEPTH blocks
        # after the one that confirms the split restake at most that many of
        # them, which leaves more than 175,000 GRC claimable, and the split and
        # the rest of the premine give every block a stake kernel.
        premine = node.listunspent(0)
        assert_equal(len(premine), 10)
        spent = premine[:2]
        per_out = ((sum(u["amount"] for u in spent) - Decimal("1")) / SPLIT_COUNT).quantize(
            Decimal("0.00000001"), rounding=ROUND_DOWN)  # ~1 GRC fee
        signed = node.signrawtransactionwithwallet(node.createrawtransaction(
            [{"txid": u["txid"], "vout": u["vout"]} for u in spent],
            {node.getnewaddress(): per_out for _ in range(SPLIT_COUNT)}))
        assert_equal(signed["complete"], True)
        split = node.sendrawtransaction(signed["hex"])
        for _ in range(1 + CLAIM_DEPTH):
            node.advance_mocktime_to((node.mock_now() + BLOCK_SPACING) & ~STAKE_TIMESTAMP_MASK)
            node.generatetoaddress(1, node.getnewaddress())
        assert_greater_than(sum(u["amount"] for u in node.listunspent(CLAIM_DEPTH) if u["txid"] == split), 175000)

        single = node.addpoll("survey", "single", 7, "Which?", "A;B;C", 1, 2, "https://gridcoin.us")
        multiple = node.addpoll("survey", "multiple", 7, "Which?", "A;B;C", 1, 3, "https://gridcoin.us")
        node.generatetoaddress(1, node.getnewaddress())

        for choice in (256, 257, 511, -1, -256, 2**31 - 1):
            assert_raises_rpc_error(-8, "choice id %d is out of range" % choice,
                                    node.votebyid, single["id"], choice)
            assert_raises_rpc_error(-8, "choice id %d is out of range" % choice,
                                    node.votebyid, multiple["id"], 0, choice)

        # The ends of the byte range pass to the vote builder.
        assert_raises_rpc_error(-1, "Duplicate response for poll choice: 0",
                                node.votebyid, multiple["id"], 0, 0)
        assert_raises_rpc_error(-1, "Duplicate response for poll choice: 2",
                                node.votebyid, multiple["id"], 2, 2)
        assert_raises_rpc_error(-1, "\"3\" is not a valid poll choice",
                                node.votebyid, single["id"], 3)
        assert_raises_rpc_error(-1, "\"255\" is not a valid poll choice",
                                node.votebyid, single["id"], 255)


if __name__ == "__main__":
    VoteByIdTest().main()
