import unittest
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
        a = Args(); a.rpcport = 25715; a.rpcconnect = "10.0.0.2"
        c = au.resolve_connection(conf, a)
        self.assertEqual(c["port"], 25715)
        self.assertEqual(c["host"], "10.0.0.2")

    def test_resolve_connection_requires_credentials(self):
        with self.assertRaises(ValueError):
            au.resolve_connection({"rpcuser": "alice"}, Args())  # no password

    def test_resolve_connection_requires_port(self):
        with self.assertRaises(ValueError):
            au.resolve_connection({"rpcuser": "a", "rpcpassword": "b"}, Args())


if __name__ == "__main__":
    unittest.main()
