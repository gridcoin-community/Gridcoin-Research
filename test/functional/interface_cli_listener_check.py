#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""The RPC client identifies the owner of the RPC listener before it sends credentials.

gridcoinresearchd given a non-switch argv acts as the RPC client. After it connects, and
before it writes a byte, it finds the server side of its own connection in the kernel's TCP
table and accepts the listener when this account owns it (root also accepts the owner of
the configuration file it read, and anyone accepts the service account a root-owned
configuration file names with rpcserviceuser).

  - the framework's node, owned by this account, is accepted and answers;
  - -rpcserviceuser on the command line is ignored with a warning, and the call succeeds;
  - so is rpcserviceuser in a configuration file this account owns;
  - -rpcskiplistenercheck sends without the check and says so on stderr.

The client runs as a subprocess so its stderr can be read. A refusal needs a listener owned
by another account, which a test cannot create without privileges; the unit tests
(rpc_listener_check_tests) cover the refusal and that a refused listener receives nothing.
"""
import os
import subprocess
import sys

from test_framework.test_framework import GridcoinTestFramework, SkipTest
from test_framework.util import assert_equal

# The warning for a service account set where it is not honoured. The warning for a name with no
# account starts the same way, so the reason is part of the match.
IGNORED_SERVICE_USER = "ignoring -rpcserviceuser: it is honoured only from a configuration file"


class InterfaceCLIListenerCheckTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0", "-connect=0", "-listen=0"]]
        # The client is run directly; --usecli would not change what is under test.
        self.supports_cli = False

    def skip_test_if_missing_module(self):
        self.skip_if_no_cli()
        if not sys.platform.startswith("linux"):
            raise SkipTest("the listener check's backend under test is Linux's")

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def run_client(self, *options):
        """Run the client for getblockcount with the given options; return the finished process."""
        node = self.nodes[0]
        args = [node.cli.binary, "-datadir=" + node.cli.datadir] + list(options) + ["getblockcount"]
        self.log.debug("Running the client: {}".format(args[2:]))
        return subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              universal_newlines=True, timeout=60)

    def assert_answers(self, result, height):
        assert_equal(result.returncode, 0)
        assert_equal(result.stdout.strip(), str(height))
        assert "refusing" not in result.stderr, result.stderr

    def run_test(self):
        node = self.nodes[0]
        height = node.getblockcount()

        self.log.info("A listener owned by this account is accepted")
        result = self.run_client()
        self.assert_answers(result, height)
        assert "rpcserviceuser" not in result.stderr, result.stderr
        assert "rpcskiplistenercheck" not in result.stderr, result.stderr

        self.log.info("-rpcserviceuser on the command line is ignored, with a warning")
        result = self.run_client("-rpcserviceuser=nobody")
        self.assert_answers(result, height)
        assert IGNORED_SERVICE_USER in result.stderr, result.stderr

        self.log.info("rpcserviceuser in a configuration file this account owns is ignored too")
        conf = os.path.join(node.datadir, self.chain, "gridcoinresearch.conf")
        with open(conf, encoding="utf8") as f:
            original = f.read()
        try:
            with open(conf, "a", encoding="utf8") as f:
                f.write("rpcserviceuser=nobody\n")
            result = self.run_client()
            self.assert_answers(result, height)
            assert IGNORED_SERVICE_USER in result.stderr, result.stderr
        finally:
            with open(conf, "w", encoding="utf8") as f:
                f.write(original)

        self.log.info("-rpcskiplistenercheck sends without the check, and says so")
        result = self.run_client("-rpcskiplistenercheck")
        self.assert_answers(result, height)
        assert "the RPC listener's owner is not checked" in result.stderr, result.stderr
        # Once per run, not once per attempt.
        assert_equal(result.stderr.count("is not checked"), 1)


if __name__ == '__main__':
    InterfaceCLIListenerCheckTest().main()
