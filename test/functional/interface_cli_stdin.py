#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""The RPC client takes wallet passphrases from standard input.

gridcoinresearchd given a non-switch argv acts as the RPC client. With
-stdinwalletpassphrase the passphrases of walletpassphrase, walletpassphrasechange,
encryptwallet, setautounlock and clearautounlock are read from standard input, one line each,
and never appear on the command line. With -stdinelevate one passphrase is read and sent with the request in the
X-Gridcoin-Elevate header, so that single command may use the wallet's keys while the wallet
stays unlocked for staking only.

The regtest wallet database is in memory, so encryption does not survive the restart that
encryptwallet forces. The unlock and elevation calls therefore run against an unencrypted
wallet and are judged by the server's reply to them, not by the wallet's acceptance of the
passphrase.

  - encryptwallet, walletpassphrase, walletpassphrasechange, setautounlock and clearautounlock
    from standard input;
  - encryptwallet names the automatic backups from before the encryption and deletes none;
  - -stdinelevate sends the passphrase base64-encoded in the header;
  - the flags are refused for methods they do not apply to, and a short input is reported;
  - the command-line form still works and warns.
"""
import base64
import os
import socket
import subprocess
import threading

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal, chain_subdir, rpc_port

PASSPHRASE = "pä ss wörd"
NEW_PASSPHRASE = "new pw"


class CaptureServer:
    """Accepts one connection on 127.0.0.1, keeps the request it reads and answers with a canned reply."""

    def __init__(self, port):
        self.request = b""
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.listener.bind(("127.0.0.1", port))
        self.listener.listen(1)
        self.listener.settimeout(30)
        self.thread = threading.Thread(target=self.serve)
        self.thread.start()

    def serve(self):
        try:
            conn, _ = self.listener.accept()
        except OSError:
            return
        with conn:
            conn.settimeout(30)
            data = b""
            while b"\r\n\r\n" not in data:
                chunk = conn.recv(4096)
                if not chunk:
                    break
                data += chunk
            head, _, body = data.partition(b"\r\n\r\n")
            length = 0
            for line in head.split(b"\r\n"):
                if line.lower().startswith(b"content-length:"):
                    length = int(line.split(b":", 1)[1])
            while len(body) < length:
                chunk = conn.recv(4096)
                if not chunk:
                    break
                body += chunk
            self.request = head + b"\r\n\r\n" + body
            reply = b'{"result":"x","error":null,"id":1}'
            conn.sendall(b"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "
                         + str(len(reply)).encode() + b"\r\nConnection: close\r\n\r\n" + reply)

    def finish(self):
        """The request captured; the listener is closed."""
        self.thread.join(timeout=35)
        self.listener.close()
        return self.request.decode("utf-8", "replace")


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

    def capture_headers(self, address, options, input):
        """The X-Gridcoin-Elevate lines of the request the client writes for signmessage."""
        port = rpc_port(1)
        server = CaptureServer(port)
        try:
            result = self.run_client("-rpcconnect=127.0.0.1", "-rpcport=" + str(port), *options, "signmessage",
                                     address, "m", input=input)
        finally:
            request = server.finish()
        self.assert_client_ok(result)
        assert_equal(result.stdout.strip(), "x")
        return [line for line in request.split("\r\n") if line.lower().startswith("x-gridcoin-elevate:")]

    def run_test(self):
        node = self.nodes[0]

        # An automatic backup from before the encryption, which encryptwallet lists and leaves in place.
        backup_dir = os.path.join(node.datadir, chain_subdir(self.chain), "walletbackups")
        os.makedirs(backup_dir, exist_ok=True)
        backup = os.path.join(backup_dir, "wallet.dat-2020-01-01T00-00-00Z")
        os.close(os.open(backup, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600))

        self.log.info("encryptwallet takes its passphrase from standard input")
        result = self.run_client("-stdinwalletpassphrase", "encryptwallet", input=PASSPHRASE + "\n")
        self.assert_client_ok(result)
        node.wait_until_stopped()

        self.log.info("encryptwallet names the automatic backups from before the encryption and deletes none")
        assert "wallet.dat-2020-01-01T00-00-00Z" in result.stdout, result.stdout
        assert "made by hand before encryption" in result.stdout, result.stdout
        assert os.path.exists(backup)

        # The regtest wallet database is in memory, so the restarted node has an unencrypted wallet
        # and the unlock calls below end at the wallet's own refusal. What is checked is that the
        # request, built from standard input, reaches the server and is answered by the wallet; the
        # positions of the passphrases are pinned by the build_params_with_secrets unit test.
        self.start_node(0)

        self.log.info("walletpassphrase and walletpassphrasechange read their passphrases from standard input")
        result = self.run_client("-stdinwalletpassphrase", "walletpassphrase", 600, "true", input=PASSPHRASE + "\n")
        assert result.returncode != 0
        assert "-15" in result.stderr and "unencrypted wallet" in result.stderr, result.stderr
        result = self.run_client("-stdinwalletpassphrase", "walletpassphrasechange",
                                 input=PASSPHRASE + "\n" + NEW_PASSPHRASE + "\n")
        assert result.returncode != 0
        assert "-15" in result.stderr and "unencrypted wallet" in result.stderr, result.stderr

        self.log.info("setautounlock and clearautounlock read their passphrase from standard input")
        for method in ("setautounlock", "clearautounlock"):
            result = self.run_client("-stdinwalletpassphrase", method, input=PASSPHRASE + "\n")
            assert result.returncode != 0
            assert "-15" in result.stderr and "unencrypted wallet" in result.stderr, result.stderr

        self.log.info("-stdinelevate sends the passphrase in the request header")
        # The node's wallet is unencrypted here, so its reply cannot show whether the header was
        # sent; a capture listener on the port the node does not use shows what the client wrote.
        # The call without the flag carries no such header.
        address = node.getnewaddress()
        elevate_lines = self.capture_headers(address, ["-stdinelevate"], PASSPHRASE + "\n")
        expected = "X-Gridcoin-Elevate: " + base64.b64encode(PASSPHRASE.encode("utf-8")).decode()
        assert_equal(elevate_lines, [expected])
        assert_equal(self.capture_headers(address, [], None), [])

        self.log.info("The flags are refused where they do not apply, and a short input is reported")
        self.assert_client_fails(["-stdinwalletpassphrase", "getblockcount"],
                                 "-stdinwalletpassphrase applies only to walletpassphrase, "
                                 "walletpassphrasechange, encryptwallet, setautounlock and clearautounlock")
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
