#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""The POOL and PSGT pool RPCs before and after the v15 height.

POOL contracts are valid only from v15, so registerpool, withdrawpool, approvepool, removepool and authorizepool
refuse before it with a message that names the activation height, or says none is scheduled, rather than building
a transaction the memory pool would bounce. The PSGT pool RPCs that change the pool, submitpsgt and signpsgtinpool,
refuse until v15 too, and getpsgtpoolinfo reports the pool inactive.

node0 runs with a v15 height and is checked at every height below it, then at it: from there the RPCs pass the gate
and fail, or not, on their own merits. node1 has no v15 height.
"""

from test_framework.authproxy import JSONRPCException
from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error

STAKE_TIMESTAMP_MASK = 15
SPACING = 96
V15_HEIGHT = 5
RPC_INVALID_REQUEST = -32600
CPID = "00010203040506070809101112131415"
POOL_GATE = "On-chain pool registration (block version 15) is not active"
PSGT_GATE = "The PSGT pool is not active"


class PoolV15GateTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0", f"-blockv15height={V15_HEIGHT}"], ["-staking=0"]]

    def setup_network(self):
        # The two nodes never connect: each is judged at its own height.
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def stake(self, node):
        """Stake one block SPACING seconds on (as feature_vote_balance_weight.py)."""
        node.advance_mocktime_to((node.mock_now() + SPACING) & ~STAKE_TIMESTAMP_MASK)
        node.generatetoaddress(1, node.getnewaddress())

    def pool_calls(self, node, pubkey):
        return [
            ("registerpool", lambda: node.registerpool(CPID, "gatepool", "https://gate.example/")),
            ("withdrawpool", lambda: node.withdrawpool(CPID)),
            ("approvepool", lambda: node.approvepool(CPID)),
            ("removepool", lambda: node.removepool(CPID)),
            ("authorizepool", lambda: node.authorizepool(CPID, pubkey)),
        ]

    def psgt_calls(self, node):
        # Arguments that parse, so each call reaches the gate.
        return [
            ("submitpsgt", lambda: node.submitpsgt("AA==")),
            ("signpsgtinpool", lambda: node.signpsgtinpool("00" * 20)),
        ]

    def run_test(self):
        node0, node1 = self.nodes
        pubkey = node0.validateaddress(node0.getnewaddress())["pubkey"]

        self.log.info("No v15 height: every call names the missing schedule")
        for name, call in self.pool_calls(node1, pubkey):
            self.log.debug(name)
            assert_raises_rpc_error(RPC_INVALID_REQUEST, "no activation height has been scheduled", call)
        for name, call in self.psgt_calls(node1):
            self.log.debug(name)
            assert_raises_rpc_error(RPC_INVALID_REQUEST, PSGT_GATE, call)
        assert_equal(node1.getpsgtpoolinfo()["active"], False)

        self.log.info("Below the v15 height: refused at every height, naming it")
        while node0.getblockcount() < V15_HEIGHT:
            height = node0.getblockcount()
            for name, call in self.pool_calls(node0, pubkey):
                self.log.debug(f"{name} at {height}")
                assert_raises_rpc_error(
                    RPC_INVALID_REQUEST,
                    f"not active until block {V15_HEIGHT} (current height {height})",
                    call)
            for name, call in self.psgt_calls(node0):
                assert_raises_rpc_error(RPC_INVALID_REQUEST, PSGT_GATE, call)
            assert_equal(node0.getpsgtpoolinfo()["active"], False)
            self.stake(node0)

        self.log.info("At the v15 height: past the gate")
        assert_equal(node0.getblockcount(), V15_HEIGHT)
        assert_equal(node0.getpsgtpoolinfo()["active"], True)
        for name, call in self.pool_calls(node0, pubkey) + self.psgt_calls(node0):
            try:
                call()
                self.log.debug(f"{name}: accepted")
            except JSONRPCException as e:
                message = e.error["message"]
                self.log.debug(f"{name}: {message}")
                assert POOL_GATE not in message and PSGT_GATE not in message, f"{name}: {message}"


if __name__ == '__main__':
    PoolV15GateTest().main()
