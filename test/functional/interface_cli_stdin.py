#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""The RPC client takes wallet passphrases from standard input.

gridcoinresearchd given a non-switch argv acts as the RPC client. With
-stdinwalletpassphrase the passphrases of walletpassphrase, walletpassphrasechange and
encryptwallet are read from standard input, one line each, and never appear on the command
line. With -stdinelevate one passphrase is read and sent with the request in the
X-Gridcoin-Elevate header, so that single command may use the wallet's keys while the wallet
stays unlocked for staking only.

The regtest wallet database is in memory, so encryption does not survive the restart that
encryptwallet forces. The unlock and elevation calls therefore run against an unencrypted
wallet and are judged by the server's reply to them, not by the wallet's acceptance of the
passphrase.

  - encryptwallet, walletpassphrase and walletpassphrasechange from standard input;
  - -stdinelevate sends the passphrase base64-encoded in the header;
  - the flags are refused for methods they do not apply to, and a short input is reported;
  - the command-line form still works and warns.
"""
import subprocess

from test_framework.test_framework import GridcoinTestFramework

PASSPHRASE = "pä ss wörd"
NEW_PASSPHRASE = "new pw"


class InterfaceCLIStdinTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0", "-connect=0", "-listen=0"]]
        # The client is run directly with its own options; --usecli would not change what is under test.
        self.supports_cli = False

    def skip_test_if_missing_module(self):
        self.skip_if_no_cli()

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def run_client(self, *args, input=None):
        """Run the client with the given arguments; return the finished process."""
        node = self.nodes[0]
        argv = [node.cli.binary, "-datadir=" + node.cli.datadir] + [str(a) for a in args]
        self.log.debug("Running the client: {}".format(argv[2:]))
        return subprocess.run(argv, input=input, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              universal_newlines=True, timeout=60)

    def assert_client_ok(self, result):
        assert result.returncode == 0, "exit {}: {}".format(result.returncode, result.stderr)

    def assert_client_fails(self, args, message, input=None):
        result = self.run_client(*args, input=input)
        assert result.returncode != 0, result.stdout
        assert message in result.stderr, result.stderr

    def run_test(self):
        node = self.nodes[0]

        self.log.info("encryptwallet takes its passphrase from standard input")
        result = self.run_client("-stdinwalletpassphrase", "encryptwallet", input=PASSPHRASE + "\n")
        self.assert_client_ok(result)
        node.wait_until_stopped()

        # The regtest wallet database is in memory, so the restarted node has an unencrypted wallet
        # and the unlock calls below end at the wallet's own refusal. What is checked is that each
        # passphrase reaches the server in the right place, from standard input, and not how the
        # wallet then judges it (rpc_elevation_tests and the wallet tests cover that).
        self.start_node(0)

        self.log.info("walletpassphrase and walletpassphrasechange read their passphrases from standard input")
        result = self.run_client("-stdinwalletpassphrase", "walletpassphrase", 600, "true", input=PASSPHRASE + "\n")
        assert result.returncode != 0
        assert "-15" in result.stderr and "unencrypted wallet" in result.stderr, result.stderr
        result = self.run_client("-stdinwalletpassphrase", "walletpassphrasechange",
                                 input=PASSPHRASE + "\n" + NEW_PASSPHRASE + "\n")
        assert result.returncode != 0
        assert "-15" in result.stderr and "unencrypted wallet" in result.stderr, result.stderr

        self.log.info("-stdinelevate sends the passphrase in the request header")
        address = node.getnewaddress()
        # A header that did not decode as base64 of a passphrase is answered with -14 before the
        # command runs, so the command's success shows the server took the header.
        result = self.run_client("-stdinelevate", "signmessage", address, "m", input=PASSPHRASE + "\n")
        self.assert_client_ok(result)
        assert node.verifymessage(address, result.stdout.strip(), "m")

        self.log.info("The flags are refused where they do not apply, and a short input is reported")
        self.assert_client_fails(["-stdinwalletpassphrase", "getblockcount"],
                                 "-stdinwalletpassphrase applies only to walletpassphrase, "
                                 "walletpassphrasechange and encryptwallet")
        self.assert_client_fails(["-stdinwalletpassphrase", "walletpassphrasechange"],
                                 "expected 2 passphrase line(s) on standard input, got 1", input="only one\n")
        self.assert_client_fails(["-stdinwalletpassphrase", "walletpassphrase"],
                                 "expected 1 passphrase line(s) on standard input, got 0", input="")
        self.assert_client_fails(["-stdinelevate", "walletpassphrase", 60],
                                 "-stdinelevate is not used with walletpassphrase; give its passphrases with "
                                 "-stdinwalletpassphrase")
        self.assert_client_fails(["-stdinelevate", "signmessage", address, "m"],
                                 "this passphrase cannot be sent in a request header", input="\n")

        self.log.info("The command-line form still works and warns")
        result = self.run_client("walletpassphrase", NEW_PASSPHRASE, 60)
        assert "given on the command line" in result.stderr, result.stderr
        assert "-15" in result.stderr, result.stderr

        self.log.info("No warning when the passphrase comes from standard input")
        result = self.run_client("-stdinwalletpassphrase", "walletpassphrase", 60, input=NEW_PASSPHRASE + "\n")
        assert "given on the command line" not in result.stderr, result.stderr


if __name__ == '__main__':
    InterfaceCLIStdinTest().main()
