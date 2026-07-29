import unittest
import os
import tempfile
import gridcoin_autounlock as au


class Args:  # minimal stand-in for argparse.Namespace
    rpcconnect = None
    rpcport = None
    rpcuser = None
    rpcpassword = None


class TestConfig(unittest.TestCase):
    def test_parse_conf_ignores_comments_and_whitespace(self):
        text = "# a comment\n rpcuser = alice \nrpcpassword=s3cret\nrpcport=15715\n\n"
        conf = au.parse_conf(text)
        self.assertEqual(conf["rpcuser"], "alice")
        self.assertEqual(conf["rpcpassword"], "s3cret")
        self.assertEqual(conf["rpcport"], "15715")

    def test_resolve_connection_from_conf(self):
        conf = {"rpcuser": "alice", "rpcpassword": "s3cret", "rpcport": "15715"}
        c = au.resolve_connection(conf, Args())
        self.assertEqual(c, {"host": "127.0.0.1", "port": 15715,
                             "user": "alice", "password": "s3cret"})

    def test_resolve_connection_args_override_conf(self):
        conf = {"rpcuser": "alice", "rpcpassword": "s3cret", "rpcport": "15715"}
        a = Args()
        a.rpcport = 25715
        a.rpcconnect = "10.0.0.2"
        c = au.resolve_connection(conf, a)
        self.assertEqual(c["port"], 25715)
        self.assertEqual(c["host"], "10.0.0.2")

    def test_resolve_connection_requires_credentials(self):
        with self.assertRaises(ValueError):
            au.resolve_connection({"rpcuser": "alice"}, Args())  # no password

    def test_resolve_connection_requires_port(self):
        with self.assertRaises(ValueError):
            au.resolve_connection({"rpcuser": "a", "rpcpassword": "b"}, Args())


class FakeClient:
    """Records calls; returns queued getinfo responses."""
    def __init__(self, uptimes):
        self._uptimes = list(uptimes)
        self.calls = []

    def call(self, method, params):
        self.calls.append((method, params))
        if method == "getinfo":
            if not self._uptimes:
                raise au.RpcError("unreachable")
            return {"uptime": self._uptimes.pop(0)}
        if method == "walletpassphrase":
            return None
        raise AssertionError("unexpected method " + method)


class TestUnlockDecision(unittest.TestCase):
    def test_should_unlock_first_contact(self):
        self.assertTrue(au.should_unlock(None, 5))

    def test_should_not_unlock_while_uptime_grows(self):
        self.assertFalse(au.should_unlock(100, 200))

    def test_should_unlock_when_uptime_resets(self):
        self.assertTrue(au.should_unlock(200, 5))  # new instance

    def test_run_once_unlocks_stake_only_on_first_contact(self):
        c = FakeClient([42])
        new = au.run_once(c, "s3cret", 99999999, None)
        self.assertEqual(new, 42)
        self.assertIn(("walletpassphrase", ["s3cret", 99999999, True]), c.calls)

    def test_run_once_no_unlock_when_uptime_grows(self):
        c = FakeClient([200])
        new = au.run_once(c, "s3cret", 99999999, 100)
        self.assertEqual(new, 200)
        self.assertNotIn("walletpassphrase", [m for m, _ in c.calls])

    def test_run_once_reunlocks_after_restart(self):
        c = FakeClient([5])
        new = au.run_once(c, "s3cret", 99999999, 200)
        self.assertEqual(new, 5)
        self.assertIn(("walletpassphrase", ["s3cret", 99999999, True]), c.calls)

    def test_run_once_keeps_baseline_when_unreachable(self):
        c = FakeClient([])  # getinfo raises RpcError
        new = au.run_once(c, "s3cret", 99999999, 100)
        self.assertEqual(new, 100)  # unchanged; no unlock attempted
        self.assertNotIn("walletpassphrase", [m for m, _ in c.calls])

    def test_run_once_survives_walletpassphrase_error(self):
        class FailingUnlockClient:
            def __init__(self):
                self.calls = []

            def call(self, method, params):
                self.calls.append(method)
                if method == "getinfo":
                    return {"uptime": 5}
                if method == "walletpassphrase":
                    raise au.RpcError("Error: Wallet is already unlocked.")
                raise AssertionError("unexpected method " + method)

        c = FailingUnlockClient()
        new = au.run_once(c, "s3cret", 99999999, None)  # first contact -> attempts unlock
        self.assertEqual(new, 5)                    # baseline recorded, did not crash
        self.assertIn("walletpassphrase", c.calls)  # it did attempt the unlock


class TestCli(unittest.TestCase):
    def test_read_passphrase_strips_trailing_newline(self):
        with tempfile.NamedTemporaryFile("w", delete=False) as f:
            f.write("hunter2\n")
            path = f.name
        try:
            self.assertEqual(au.read_passphrase(path), "hunter2")
        finally:
            os.unlink(path)

    def test_read_passphrase_rejects_empty(self):
        with tempfile.NamedTemporaryFile("w", delete=False) as f:
            f.write("\n")
            path = f.name
        try:
            with self.assertRaises(ValueError):
                au.read_passphrase(path)
        finally:
            os.unlink(path)

    def test_arg_parser_requires_passphrase_file(self):
        p = au.build_arg_parser()
        with self.assertRaises(SystemExit):
            p.parse_args([])  # --passphrase-file is required


if __name__ == "__main__":
    unittest.main()
