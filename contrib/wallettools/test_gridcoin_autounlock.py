import unittest
import io
import os
import tempfile
import urllib.error
import urllib.request
from unittest import mock

import gridcoin_autounlock as au


class Args:  # minimal stand-in for argparse.Namespace
    rpcconnect = None
    rpcport = None
    rpcuser = None
    rpcpassword = None
    conf = None
    datadir = None


class TestConfig(unittest.TestCase):
    def test_parse_conf_ignores_comments_and_whitespace(self):
        text = "# a comment\n rpcuser = alice \nrpcpassword=s3cret\nrpcport=15715\n\n"
        conf = au.parse_conf(text)
        self.assertEqual(conf["rpcuser"], "alice")
        self.assertEqual(conf["rpcpassword"], "s3cret")
        self.assertEqual(conf["rpcport"], "15715")

    def test_parse_conf_strips_inline_comments(self):
        # Core (GetConfigOptions) strips from the first '#', inline comments included.
        conf = au.parse_conf("rpcuser=alice  # my user\nrpcport=15715 # the port\n")
        self.assertEqual(conf["rpcuser"], "alice")
        self.assertEqual(conf["rpcport"], "15715")

    def test_parse_conf_duplicate_key_first_wins(self):
        # Matches the core: config-file settings take the FIRST assigned value
        # (src/util/settings.cpp reverse_precedence), not the last.
        conf = au.parse_conf("rpcpassword=first\nrpcpassword=second\n")
        self.assertEqual(conf["rpcpassword"], "first")

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

    def test_resolve_connection_defaults_port_to_mainnet(self):
        # A packaged gridcoinresearch.conf commonly omits rpcport; with no chain
        # selector the config IS mainnet, so the mainnet port is the right default.
        c = au.resolve_connection({"rpcuser": "a", "rpcpassword": "b"}, Args())
        self.assertEqual(c["port"], au.DEFAULT_RPC_PORT)
        self.assertEqual(c["port"], 15715)

    def test_resolve_connection_rejects_non_numeric_port(self):
        with self.assertRaises(ValueError):
            au.resolve_connection({"rpcuser": "a", "rpcpassword": "b", "rpcport": "abc"}, Args())

    def test_default_port_is_chain_aware(self):
        # A testnet/regtest node whose conf omits rpcport must NOT fall back to the
        # mainnet port: if the same uid also runs a mainnet node there, the ownership
        # gate answers "ours" and this wallet's passphrase goes to the OTHER wallet.
        for conf, expected in (({"testnet": "1"}, 25715),
                               ({"testnet": ""}, 25715),      # InterpretBool: empty == true
                               ({"chain": "test"}, 25715),
                               ({"regtest": "1"}, 35715),
                               ({"chain": "regtest"}, 35715),
                               ({"chain": "main"}, 15715),
                               ({"testnet": "0"}, 15715)):
            base = {"rpcuser": "a", "rpcpassword": "b"}
            base.update(conf)
            self.assertEqual(au.resolve_connection(base, Args())["port"], expected, conf)

    def test_refuses_to_guess_port_on_ambiguous_chain(self):
        for conf in ({"testnet": "1", "regtest": "1"},   # the core rejects this too
                     {"testnet": "1", "chain": "main"},
                     {"chain": "signet"}):               # no default port known here
            base = {"rpcuser": "a", "rpcpassword": "b"}
            base.update(conf)
            with self.assertRaises(ValueError, msg=conf):
                au.resolve_connection(base, Args())

    def test_explicit_rpcport_wins_over_chain_default(self):
        c = au.resolve_connection({"rpcuser": "a", "rpcpassword": "b",
                                   "testnet": "1", "rpcport": "9999"}, Args())
        self.assertEqual(c["port"], 9999)


class TestLoopback(unittest.TestCase):
    def test_loopback_hosts(self):
        for h in ("127.0.0.1", "::1", "localhost", "LOCALHOST", "127.5.5.5",
                  " 127.0.0.1 ", "[::1]"):
            self.assertTrue(au.is_loopback(h), h)

    def test_non_loopback_hosts(self):
        for h in ("10.0.0.2", "192.168.1.5", "example.com", "0.0.0.0", "::2"):
            self.assertFalse(au.is_loopback(h), h)

    def test_dns_name_with_loopback_prefix_is_not_loopback(self):
        # A '127.'-prefixed DNS NAME resolves off-box. Classified as loopback (as a
        # string-prefix test does), the gate would check our own port, answer "ours",
        # and then POST the Basic-auth header and the passphrase to the remote host in
        # cleartext. Only IP literals may be loopback.
        for h in ("127.0.0.1.attacker.example", "127.0.0.1.evil.com", "localhost.evil.com",
                  "127.0.0.1x", "::1.evil.com"):
            self.assertFalse(au.is_loopback(h), h)


# --- /proc listener-ownership gate: pure parser ------------------------------

_PROC_HEADER = ("  sl  local_address rem_address   st tx_queue rx_queue tr "
                "tm->when retrnsmt   uid  timeout inode")


def _proc_line(port, st="0A", uid=1000, ip="0100007F"):
    """One /proc/net/tcp row: fields[1]=local ip:port, [3]=st, [7]=uid."""
    return ("   0: %s:%04X 00000000:0000 %s 00000000:00000000 "
            "00:00000000 00000000 %5d 0 12345 1 0000 100 0 0 10 0"
            % (ip, port, st, uid))


def _proc_table(*rows):
    return "\n".join([_PROC_HEADER] + list(rows)) + "\n"


class TestParseListenUids(unittest.TestCase):
    def test_listen_socket_owned_by_uid(self):
        text = _proc_table(_proc_line(15715, st="0A", uid=1000))
        self.assertEqual(au.parse_listen_uids(text, 15715), {1000})

    def test_ignores_other_ports(self):
        text = _proc_table(_proc_line(9999, uid=1000))
        self.assertEqual(au.parse_listen_uids(text, 15715), set())

    def test_ignores_non_listen_state(self):
        # 01 == TCP_ESTABLISHED; only 0A (LISTEN) counts.
        text = _proc_table(_proc_line(15715, st="01", uid=1000))
        self.assertEqual(au.parse_listen_uids(text, 15715), set())

    def test_collects_multiple_owners(self):
        text = _proc_table(_proc_line(15715, uid=1000), _proc_line(15715, uid=1001))
        self.assertEqual(au.parse_listen_uids(text, 15715), {1000, 1001})

    def test_skips_malformed_lines(self):
        text = _PROC_HEADER + "\ngarbage\n" + _proc_line(15715, uid=1000) + "\n"
        self.assertEqual(au.parse_listen_uids(text, 15715), {1000})

    def test_empty_table(self):
        self.assertEqual(au.parse_listen_uids(_proc_table(), 15715), set())


# --- /proc listener-ownership gate: the real reader (fail-closed accounting) --

def _fake_open(mapping):
    """A stand-in for builtins.open that serves `mapping` (path -> str content or
    an OSError instance to raise) and defers to the real builtin otherwise."""
    real_open = open

    def opener(path, *a, **k):
        if path in mapping:
            val = mapping[path]
            if isinstance(val, OSError):
                raise val
            return io.StringIO(val)
        return real_open(path, *a, **k)
    return opener


class TestListeningUidsReal(unittest.TestCase):
    def test_both_tables_unreadable_fails_closed(self):
        m = {"/proc/net/tcp": PermissionError(), "/proc/net/tcp6": PermissionError()}
        with mock.patch("builtins.open", _fake_open(m)):
            uids, readable, unreadable = au.listening_uids(15715)
        self.assertEqual((uids, readable, unreadable), (set(), 0, True))

    def test_partial_unreadable_is_flagged(self):
        # tcp readable (only us), tcp6 unreadable -> a foreign listener could hide
        # in the unread tcp6 table, so this MUST fail closed.
        m = {"/proc/net/tcp": _proc_table(_proc_line(15715, uid=1000)),
             "/proc/net/tcp6": PermissionError()}
        with mock.patch("builtins.open", _fake_open(m)):
            uids, readable, unreadable = au.listening_uids(15715)
        self.assertTrue(unreadable)
        with mock.patch("builtins.open", _fake_open(m)), \
                mock.patch.object(au.os, "getuid", create=True, return_value=1000):
            self.assertEqual(au.check_listener_ownership(15715), "unreadable")

    def test_ipv6_absent_is_not_unreadable(self):
        # tcp readable+ours, tcp6 absent (IPv6 disabled) -> 'ours', not fail-closed.
        m = {"/proc/net/tcp": _proc_table(_proc_line(15715, uid=1000)),
             "/proc/net/tcp6": FileNotFoundError()}
        with mock.patch("builtins.open", _fake_open(m)), \
                mock.patch.object(au.os, "getuid", create=True, return_value=1000):
            self.assertEqual(au.check_listener_ownership(15715), "ours")

    def test_foreign_owner_in_readable_table(self):
        m = {"/proc/net/tcp": _proc_table(_proc_line(15715, uid=1001)),  # attacker
             "/proc/net/tcp6": FileNotFoundError()}
        with mock.patch("builtins.open", _fake_open(m)), \
                mock.patch.object(au.os, "getuid", create=True, return_value=1000):
            self.assertEqual(au.check_listener_ownership(15715), "foreign")

    def test_nothing_listening_is_none(self):
        m = {"/proc/net/tcp": _proc_table(), "/proc/net/tcp6": _proc_table()}
        with mock.patch("builtins.open", _fake_open(m)), \
                mock.patch.object(au.os, "getuid", create=True, return_value=1000):
            self.assertEqual(au.check_listener_ownership(15715), "none")


class TestCheckListenerOwnership(unittest.TestCase):
    def test_ours(self):
        with mock.patch.object(au, "listening_uids", return_value=({1000}, 2, False)), \
                mock.patch.object(au.os, "getuid", create=True, return_value=1000):
            self.assertEqual(au.check_listener_ownership(15715), "ours")

    def test_foreign_when_any_owner_differs(self):
        with mock.patch.object(au, "listening_uids", return_value=({1000, 1001}, 2, False)), \
                mock.patch.object(au.os, "getuid", create=True, return_value=1000):
            self.assertEqual(au.check_listener_ownership(15715), "foreign")

    def test_none_when_nothing_listening(self):
        with mock.patch.object(au, "listening_uids", return_value=(set(), 2, False)), \
                mock.patch.object(au.os, "getuid", create=True, return_value=1000):
            self.assertEqual(au.check_listener_ownership(15715), "none")

    def test_unreadable_when_zero_readable(self):
        with mock.patch.object(au, "listening_uids", return_value=(set(), 0, False)), \
                mock.patch.object(au.os, "getuid", create=True, return_value=1000):
            self.assertEqual(au.check_listener_ownership(15715), "unreadable")

    def test_unreadable_when_any_table_unreadable(self):
        # Even with a readable table showing only us, a present-but-unreadable
        # table forces fail-closed.
        with mock.patch.object(au, "listening_uids", return_value=({1000}, 1, True)), \
                mock.patch.object(au.os, "getuid", create=True, return_value=1000):
            self.assertEqual(au.check_listener_ownership(15715), "unreadable")


# --- unlock decision + poll --------------------------------------------------

class FakeClient:
    """Records calls; returns queued getinfo responses. loopback=False so the
    gate is skipped and these tests exercise the unlock logic."""
    loopback = False

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
        new, code = au.run_once(c, "s3cret", 99999999, None)
        self.assertEqual((new, code), (42, au.EXIT_OK))
        self.assertIn(("walletpassphrase", ["s3cret", 99999999, True]), c.calls)

    def test_run_once_no_unlock_when_uptime_grows(self):
        c = FakeClient([200])
        new, code = au.run_once(c, "s3cret", 99999999, 100)
        self.assertEqual((new, code), (200, au.EXIT_OK))
        self.assertNotIn("walletpassphrase", [m for m, _ in c.calls])

    def test_run_once_reunlocks_after_restart(self):
        c = FakeClient([5])
        new, code = au.run_once(c, "s3cret", 99999999, 200)
        self.assertEqual((new, code), (5, au.EXIT_OK))
        self.assertIn(("walletpassphrase", ["s3cret", 99999999, True]), c.calls)

    def test_run_once_keeps_baseline_when_unreachable(self):
        c = FakeClient([])  # getinfo raises RpcError
        new, code = au.run_once(c, "s3cret", 99999999, 100)
        self.assertEqual((new, code), (100, au.EXIT_TRANSIENT))
        self.assertNotIn("walletpassphrase", [m for m, _ in c.calls])


class MalformedInfoClient:
    """getinfo returns something that is not a well-formed {uptime:int} dict."""
    loopback = False

    def __init__(self, info):
        self._info = info
        self.calls = []

    def call(self, method, params):
        self.calls.append(method)
        if method == "getinfo":
            return self._info
        return None


class TestMalformedGetinfo(unittest.TestCase):
    def test_non_dict_getinfo_is_transient_not_a_crash(self):
        for bad in (None, "x", [1, 2], 5):
            c = MalformedInfoClient(bad)
            new, code = au.run_once(c, "pw", 60, None)
            self.assertEqual((new, code), (None, au.EXIT_TRANSIENT))
            self.assertNotIn("walletpassphrase", c.calls)

    def test_null_or_bad_uptime_is_transient_not_a_crash(self):
        # None / non-numeric uptime would raise TypeError/ValueError on int();
        # they must be swallowed as transient rather than killing the loop.
        for bad in ({"uptime": None}, {"uptime": "notanumber"}):
            c = MalformedInfoClient(bad)
            new, code = au.run_once(c, "pw", 60, None)
            self.assertEqual((new, code), (None, au.EXIT_TRANSIENT))
            self.assertNotIn("walletpassphrase", c.calls)

    def test_missing_uptime_defaults_to_zero_and_unlocks(self):
        # A dict without 'uptime' is not malformed: it defaults to 0 (fresh
        # instance) and proceeds to unlock -- not a transient failure.
        c = MalformedInfoClient({})
        new, code = au.run_once(c, "pw", 60, None)
        self.assertEqual((new, code), (0, au.EXIT_OK))
        self.assertIn("walletpassphrase", c.calls)


class ErrClient:
    """getinfo succeeds (fresh instance); walletpassphrase raises a coded RpcError."""
    loopback = False

    def __init__(self, code, http_status=None):
        self._code = code
        self._http = http_status
        self.calls = []

    def call(self, method, params):
        self.calls.append(method)
        if method == "getinfo":
            return {"uptime": 5}
        if method == "walletpassphrase":
            raise au.RpcError("boom", code=self._code, http_status=self._http)
        raise AssertionError("unexpected method " + method)


class Getinfo401Client:
    """getinfo itself fails auth (HTTP 401) -- wrong rpcuser/rpcpassword."""
    loopback = False

    def __init__(self):
        self.calls = []

    def call(self, method, params):
        self.calls.append(method)
        if method == "getinfo":
            raise au.RpcError("unauthorized", http_status=401)
        raise AssertionError("should not reach " + method)


class TestWalletpassphraseErrors(unittest.TestCase):
    def test_already_unlocked_is_ok(self):
        c = ErrClient(au.RPC_WALLET_ALREADY_UNLOCKED)  # -17
        self.assertEqual(au.run_once(c, "pw", 60, None), (5, au.EXIT_OK))

    def test_not_encrypted_is_ok(self):
        c = ErrClient(au.RPC_WALLET_WRONG_ENC_STATE)  # -15
        self.assertEqual(au.run_once(c, "pw", 60, None), (5, au.EXIT_OK))

    def test_wrong_passphrase_is_unrecoverable(self):
        c = ErrClient(au.RPC_WALLET_PASSPHRASE_INCORRECT)  # -14
        self.assertEqual(au.run_once(c, "pw", 60, None), (5, au.EXIT_UNRECOVERABLE))

    def test_bad_parameter_is_unrecoverable(self):
        c = ErrClient(au.RPC_INVALID_PARAMETER)  # -8
        self.assertEqual(au.run_once(c, "pw", 60, None), (5, au.EXIT_UNRECOVERABLE))

    def test_unknown_error_is_transient(self):
        c = ErrClient(None)  # transport / unknown -> retry may help
        self.assertEqual(au.run_once(c, "pw", 60, None), (5, au.EXIT_TRANSIENT))

    def test_walletpassphrase_http_401_is_unrecoverable(self):
        # 401 has no JSON-RPC code (code=None) but must NOT fall through to transient.
        c = ErrClient(None, http_status=401)
        self.assertEqual(au.run_once(c, "pw", 60, None), (5, au.EXIT_UNRECOVERABLE))

    def test_getinfo_http_401_is_unrecoverable(self):
        # Bad rpcuser/rpcpassword surface on the getinfo probe first; that must be
        # unrecoverable (park), not transient (retry forever), and send no passphrase.
        c = Getinfo401Client()
        new, code = au.run_once(c, "pw", 60, None)
        self.assertEqual(code, au.EXIT_UNRECOVERABLE)
        self.assertNotIn("walletpassphrase", c.calls)

    def test_helper_never_crashes_on_walletpassphrase_error(self):
        for code in (au.RPC_WALLET_ALREADY_UNLOCKED, au.RPC_WALLET_PASSPHRASE_INCORRECT,
                     au.RPC_INVALID_PARAMETER, None):
            au.run_once(ErrClient(code), "pw", 60, None)  # must not raise


# --- the listener gate, wired through run_once (security-critical path) -------

class GateFake:
    """loopback=True so run_once consults the gate. allow_unverified default False."""
    loopback = True
    allow_unverified = False
    port = 15715

    def __init__(self):
        self.calls = []

    def call(self, method, params):
        self.calls.append((method, params))
        if method == "getinfo":
            return {"uptime": 7}
        if method == "walletpassphrase":
            return None
        raise AssertionError("unexpected method " + method)


class TestGate(unittest.TestCase):
    def test_foreign_listener_sends_nothing(self):
        c = GateFake()
        with mock.patch.object(au, "check_listener_ownership", return_value="foreign"):
            new, code = au.run_once(c, "pw", 60, None)
        self.assertEqual(code, au.EXIT_UNRECOVERABLE)
        self.assertEqual(new, None)      # baseline unchanged
        self.assertEqual(c.calls, [])    # CRUCIAL: no credential ever went on the wire

    def test_unreadable_proc_sends_nothing(self):
        c = GateFake()
        with mock.patch.object(au, "check_listener_ownership", return_value="unreadable"):
            new, code = au.run_once(c, "pw", 60, None)
        self.assertEqual(code, au.EXIT_UNRECOVERABLE)
        self.assertEqual(c.calls, [])

    def test_nothing_listening_is_transient_and_silent(self):
        c = GateFake()
        with mock.patch.object(au, "check_listener_ownership", return_value="none"):
            new, code = au.run_once(c, "pw", 60, None)
        self.assertEqual((new, code), (None, au.EXIT_TRANSIENT))
        self.assertEqual(c.calls, [])

    def test_owned_listener_proceeds_to_unlock(self):
        c = GateFake()
        with mock.patch.object(au, "check_listener_ownership", return_value="ours"):
            new, code = au.run_once(c, "pw", 60, None)
        self.assertEqual((new, code), (7, au.EXIT_OK))
        self.assertIn(("getinfo", []), c.calls)
        self.assertIn(("walletpassphrase", ["pw", 60, True]), c.calls)


class TestGateIsReCheckedBeforeThePassphrase(unittest.TestCase):
    """The gate must be re-run immediately before the walletpassphrase POST, not just
    once per poll: every call opens a fresh connection, so a whole getinfo round trip
    elapses between the ownership check and the only request that carries the secret."""

    def test_owner_change_between_getinfo_and_unlock_sends_nothing(self):
        c = GateFake()
        # 'ours' for the pre-poll gate, then 'foreign' when re-checked: the core exited
        # during the getinfo round trip and a foreign process won the bind race.
        with mock.patch.object(au, "check_listener_ownership", side_effect=["ours", "foreign"]):
            new, code = au.run_once(c, "pw", 60, None)
        self.assertEqual(code, au.EXIT_UNRECOVERABLE)
        self.assertEqual([m for m, _ in c.calls], ["getinfo"])  # CRUCIAL: no passphrase sent
        self.assertIsNone(new)  # baseline unchanged, so the next poll retries the unlock

    def test_listener_vanishing_before_unlock_is_transient_and_silent(self):
        c = GateFake()
        with mock.patch.object(au, "check_listener_ownership", side_effect=["ours", "none"]):
            new, code = au.run_once(c, "pw", 60, 999)
        self.assertEqual(code, au.EXIT_TRANSIENT)
        self.assertEqual([m for m, _ in c.calls], ["getinfo"])
        self.assertEqual(new, 999)  # baseline NOT advanced: the unlock still has to happen

    def test_gate_is_consulted_twice_on_the_happy_path(self):
        c = GateFake()
        with mock.patch.object(au, "check_listener_ownership", return_value="ours") as chk:
            new, code = au.run_once(c, "pw", 60, None)
        self.assertEqual((new, code), (7, au.EXIT_OK))
        self.assertEqual(chk.call_count, 2)


class TestGateFailsClosedOnAMissingAttribute(unittest.TestCase):
    def test_missing_loopback_attribute_still_gates(self):
        # A client object without .loopback must be treated as loopback so the gate
        # RUNS. Reading a missing attribute as "remote" would skip the check entirely.
        class NoAttrs:
            port = 15715

            def __init__(self):
                self.calls = []

            def call(self, method, params):
                self.calls.append(method)
                return {"uptime": 1}

        c = NoAttrs()
        with mock.patch.object(au, "check_listener_ownership", return_value="foreign"):
            new, code = au.run_once(c, "pw", 60, None)
        self.assertEqual(code, au.EXIT_UNRECOVERABLE)
        self.assertEqual(c.calls, [])


class TestGatePlatform(unittest.TestCase):
    def test_loopback_without_gate_support_refuses_by_default(self):
        # Simulate Windows: loopback target, platform cannot verify ownership.
        c = GateFake()  # loopback True, allow_unverified False
        with mock.patch.object(au, "_platform_can_gate", return_value=False):
            new, code = au.run_once(c, "pw", 60, None)
        self.assertEqual(code, au.EXIT_UNRECOVERABLE)
        self.assertEqual(c.calls, [])  # fail closed: nothing sent

    def test_loopback_without_gate_support_proceeds_with_flag(self):
        c = GateFake()
        c.allow_unverified = True
        with mock.patch.object(au, "_platform_can_gate", return_value=False):
            new, code = au.run_once(c, "pw", 60, None)
        self.assertEqual((new, code), (7, au.EXIT_OK))
        self.assertIn(("walletpassphrase", ["pw", 60, True]), c.calls)

    def test_remote_host_skips_gate_even_if_owner_would_be_foreign(self):
        c = GateFake()
        c.loopback = False  # remote target: gate cannot apply
        with mock.patch.object(au, "check_listener_ownership", return_value="foreign"):
            new, code = au.run_once(c, "pw", 60, None)
        self.assertEqual((new, code), (7, au.EXIT_OK))
        self.assertIn(("walletpassphrase", ["pw", 60, True]), c.calls)


# --- transport / proxy / redaction -------------------------------------------

class FakeResp:
    def __init__(self, data):
        self._data = data

    def read(self):
        return self._data

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        return False


class TestRpcClient(unittest.TestCase):
    def _client(self):
        return au.RpcClient({"host": "127.0.0.1", "port": 1, "user": "rpcuser", "password": "rpcpassword"})

    def test_loopback_flag_on_loopback_host(self):
        self.assertTrue(self._client().loopback)

    def test_loopback_flag_off_remote_host(self):
        c = au.RpcClient({"host": "10.0.0.2", "port": 1, "user": "rpcuser", "password": "rpcpassword"})
        self.assertFalse(c.loopback)

    def test_ipv6_literal_host_is_bracketed_in_url(self):
        c = au.RpcClient({"host": "::1", "port": 15715, "user": "rpcuser", "password": "rpcpassword"})
        self.assertEqual(c._url, "http://[::1]:15715/")

    def test_ipv4_host_url_unbracketed(self):
        c = au.RpcClient({"host": "127.0.0.1", "port": 15715, "user": "rpcuser", "password": "rpcpassword"})
        self.assertEqual(c._url, "http://127.0.0.1:15715/")

    def test_httperror_json_body_yields_code_and_message(self):
        body = b'{"result": null, "error": {"code": -17, "message": "already unlocked"}}'
        http_err = urllib.error.HTTPError("http://127.0.0.1:1/", 500,
                                          "Internal Server Error", {}, io.BytesIO(body))
        with mock.patch.object(au._OPENER, "open", side_effect=http_err):
            with self.assertRaises(au.RpcError) as ctx:
                self._client().call("walletpassphrase", ["p", 60, True])
        self.assertEqual(ctx.exception.code, -17)
        self.assertIn("already unlocked", str(ctx.exception))

    def test_http_401_flagged_as_auth_failure(self):
        http_err = urllib.error.HTTPError("http://127.0.0.1:1/", 401,
                                          "Unauthorized", {}, io.BytesIO(b""))
        with mock.patch.object(au._OPENER, "open", side_effect=http_err):
            with self.assertRaises(au.RpcError) as ctx:
                self._client().call("getinfo", [])
        self.assertEqual(ctx.exception.http_status, 401)

    def test_application_error_on_200_yields_code(self):
        data = b'{"result": null, "error": {"code": -8, "message": "bad param"}}'
        with mock.patch.object(au._OPENER, "open", return_value=FakeResp(data)):
            with self.assertRaises(au.RpcError) as ctx:
                self._client().call("walletpassphrase", ["p", 60, True])
        self.assertEqual(ctx.exception.code, -8)

    def test_non_object_payload_is_rpcerror_not_crash(self):
        # A syntactically-valid but non-object 200 body must not raise AttributeError.
        with mock.patch.object(au._OPENER, "open", return_value=FakeResp(b"5")):
            with self.assertRaises(au.RpcError):
                self._client().call("getinfo", [])

    def test_transport_error_has_no_code(self):
        with mock.patch.object(au._OPENER, "open",
                               side_effect=urllib.error.URLError("refused")):
            with self.assertRaises(au.RpcError) as ctx:
                self._client().call("getinfo", [])
        self.assertIsNone(ctx.exception.code)

    def test_malformed_http_is_rpcerror_not_crash(self):
        # http.client.HTTPException (e.g. BadStatusLine) is NOT a URLError/OSError;
        # it must still be mapped to RpcError so it cannot kill the resident loop.
        import http.client
        with mock.patch.object(au._OPENER, "open",
                               side_effect=http.client.BadStatusLine("garbage")):
            with self.assertRaises(au.RpcError) as ctx:
                self._client().call("getinfo", [])
        self.assertIsNone(ctx.exception.code)

    def test_opener_ignores_env_proxy(self):
        # The passphrase POST must never be diverted by an inherited http_proxy.
        with mock.patch.dict(os.environ,
                             {"http_proxy": "http://evil.example:8080",
                              "https_proxy": "http://evil.example:8080"}):
            default = urllib.request.build_opener()  # picks up the env proxy
            default_proxies = [h.proxies for h in default.handlers
                               if isinstance(h, urllib.request.ProxyHandler)]
            self.assertTrue(any(p for p in default_proxies),
                            "expected the default opener to pick up the env proxy")
            ours = urllib.request.build_opener(urllib.request.ProxyHandler({}))
            for h in ours.handlers:
                if isinstance(h, urllib.request.ProxyHandler):
                    self.assertFalse(h.proxies)
        for h in au._OPENER.handlers:
            if isinstance(h, urllib.request.ProxyHandler):
                self.assertFalse(h.proxies)


class TestRedaction(unittest.TestCase):
    def setUp(self):
        self._saved = list(au._SECRETS)
        au._SECRETS.clear()

    def tearDown(self):
        au._SECRETS[:] = self._saved

    def test_registered_secret_is_scrubbed(self):
        au.register_secret("topsecret")
        self.assertEqual(au.redact("passphrase=topsecret ok"), "passphrase=<REDACTED> ok")

    def test_multiple_secrets_scrubbed(self):
        au.register_secret("pw123-long")
        au.register_secret("rpcpw-long")
        self.assertEqual(au.redact("a pw123-long b rpcpw-long c"),
                         "a <REDACTED> b <REDACTED> c")

    def test_empty_secret_is_ignored(self):
        au.register_secret("")  # must not turn every char into <REDACTED>
        self.assertEqual(au.redact("abc"), "abc")

    def test_short_secret_is_not_registered(self):
        # redact() is an unbounded substring replace: registering a 3-char secret
        # shreds unrelated text, including the security alerts. Such a secret cannot
        # be meaningfully hidden in prose anyway, so it is skipped (with a warning).
        with mock.patch.object(au.sys, "stderr", io.StringIO()) as err:
            au.register_secret("ass")
        self.assertEqual(au._SECRETS, [])
        self.assertEqual(au.redact("REFUSING TO SEND CREDENTIALS: walletpassphrase failed"),
                         "REFUSING TO SEND CREDENTIALS: walletpassphrase failed")
        self.assertIn("NOT registered for log redaction", err.getvalue())

    def test_client_registers_auth_token(self):
        au.RpcClient({"host": "127.0.0.1", "port": 1, "user": "u", "password": "s3cret"})
        token = au.base64.b64encode(b"u:s3cret").decode()
        self.assertEqual(au.redact("auth=" + token), "auth=<REDACTED>")

    def test_warn_redacts_at_the_sink(self):
        # Pin redaction where it actually protects the operator -- in warn() itself.
        # Testing redact() alone would still pass if warn() stopped calling it.
        au.register_secret("hunter2-passphrase")
        with mock.patch.object(au.sys, "stderr", io.StringIO()) as err:
            au.warn("walletpassphrase failed for hunter2-passphrase")
        self.assertNotIn("hunter2-passphrase", err.getvalue())
        self.assertIn("<REDACTED>", err.getvalue())

    def test_log_redacts_at_the_sink(self):
        au.register_secret("hunter2-passphrase")
        with mock.patch.object(au.sys, "stdout", io.StringIO()) as out:
            au.log("unlocked with hunter2-passphrase")
        self.assertNotIn("hunter2-passphrase", out.getvalue())
        self.assertIn("<REDACTED>", out.getvalue())


# --- CLI / passphrase / conf -------------------------------------------------

class TestCli(unittest.TestCase):
    def test_read_passphrase_strips_trailing_newline(self):
        with tempfile.NamedTemporaryFile("w", delete=False) as f:
            f.write("hunter2\n")
            path = f.name
        try:
            self.assertEqual(au.read_passphrase(path), "hunter2")
        finally:
            os.unlink(path)

    def test_read_passphrase_strips_crlf(self):
        with tempfile.NamedTemporaryFile("w", newline="", delete=False) as f:
            f.write("hunter2\r\n")
            path = f.name
        try:
            self.assertEqual(au.read_passphrase(path), "hunter2")
        finally:
            os.unlink(path)

    def test_read_passphrase_drops_utf8_bom(self):
        # Windows Notepad / PowerShell redirection prepend a UTF-8 BOM.
        with tempfile.NamedTemporaryFile("wb", delete=False) as f:
            f.write(b"\xef\xbb\xbfhunter2\r\n")
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

    def test_read_passphrase_rejects_whitespace_only(self):
        with tempfile.NamedTemporaryFile("w", delete=False) as f:
            f.write("   \t\n")
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

    def test_load_conf_prefers_gridcoinresearch_conf(self):
        import shutil
        d = tempfile.mkdtemp()
        try:
            with open(os.path.join(d, "gridcoinresearch.conf"), "w", encoding="utf8") as f:
                f.write("rpcuser=fromresearch\nrpcpassword=x\nrpcport=1\n")
            with open(os.path.join(d, "gridcoin.conf"), "w", encoding="utf8") as f:
                f.write("rpcuser=fromalias\nrpcpassword=x\nrpcport=1\n")
            a = Args()
            a.datadir = d
            self.assertEqual(au._load_conf(a)["rpcuser"], "fromresearch")  # core default wins
        finally:
            shutil.rmtree(d)

    def test_load_conf_falls_back_to_gridcoin_conf(self):
        import shutil
        d = tempfile.mkdtemp()
        try:
            with open(os.path.join(d, "gridcoin.conf"), "w", encoding="utf8") as f:
                f.write("rpcuser=fromalias\nrpcpassword=x\nrpcport=1\n")
            a = Args()
            a.datadir = d
            self.assertEqual(au._load_conf(a)["rpcuser"], "fromalias")  # alias used when default absent
        finally:
            shutil.rmtree(d)


# --- main() / --once / resident-loop catch-all -------------------------------

class OnceFake:
    """A well-behaved client for --once: loopback False so the gate is skipped."""
    loopback = False

    def __init__(self):
        self.calls = []

    def call(self, method, params):
        self.calls.append(method)
        if method == "getinfo":
            return {"uptime": 5}
        return None


class BoomClient:
    """Raises a non-RpcError from call() to exercise the resident-loop catch-all."""
    loopback = False

    def __init__(self, message="kaboom"):
        self._message = message

    def call(self, method, params):
        raise RuntimeError(self._message)


_PASSPHRASE = "correct-horse-battery-staple"
_RPCPASSWORD = "rpcpassword-not-short"


class TestMain(unittest.TestCase):
    def setUp(self):
        self._saved = list(au._SECRETS)
        au._SECRETS.clear()

    def tearDown(self):
        au._SECRETS[:] = self._saved

    def _passfile(self, content=_PASSPHRASE + "\n"):
        f = tempfile.NamedTemporaryFile("w", delete=False)
        f.write(content)
        f.close()
        self.addCleanup(os.unlink, f.name)
        return f.name

    def _argv(self, pf, *extra):
        return ["--passphrase-file", pf, "--rpcuser", "u",
                "--rpcpassword", _RPCPASSWORD, "--rpcport", "15715", "--once", *extra]

    def test_once_returns_ok_on_successful_unlock(self):
        pf = self._passfile()
        with mock.patch.object(au, "RpcClient", return_value=OnceFake()):
            rc = au.main(self._argv(pf))
        self.assertEqual(rc, au.EXIT_OK)

    def test_once_catchall_maps_unexpected_exception_to_transient(self):
        pf = self._passfile()
        with mock.patch.object(au, "RpcClient", return_value=BoomClient()):
            rc = au.main(self._argv(pf))  # must NOT raise
        self.assertEqual(rc, au.EXIT_TRANSIENT)

    def test_main_registers_the_passphrase_and_rpcpassword_for_redaction(self):
        # Without this, an exception carrying either secret would be logged verbatim.
        pf = self._passfile()
        with mock.patch.object(au, "RpcClient", return_value=OnceFake()):
            au.main(self._argv(pf))
        self.assertIn(_PASSPHRASE, au._SECRETS)
        self.assertIn(_RPCPASSWORD, au._SECRETS)
        self.assertEqual(au.redact("pw=" + _PASSPHRASE), "pw=<REDACTED>")

    def test_main_catchall_redacts_the_secret(self):
        # An unexpected exception whose text embeds the passphrase must reach the log
        # redacted -- the catch-all is the last line of defence for a stray secret.
        pf = self._passfile()
        boom = BoomClient("kaboom while sending " + _PASSPHRASE)
        with mock.patch.object(au, "RpcClient", return_value=boom), \
                mock.patch.object(au.sys, "stderr", io.StringIO()) as err:
            rc = au.main(self._argv(pf))
        self.assertEqual(rc, au.EXIT_TRANSIENT)
        self.assertNotIn(_PASSPHRASE, err.getvalue())
        self.assertIn("<REDACTED>", err.getvalue())

    def test_allow_unverified_listener_defaults_to_false(self):
        # Pin the DEFAULT itself: flipping the argparse default to True would silently
        # disable the fail-closed behaviour on every platform that cannot verify the
        # listener's owner, and no other test would notice.
        args = au.build_arg_parser().parse_args(["--passphrase-file", "/dev/null"])
        self.assertFalse(args.allow_unverified_listener)
        self.assertTrue(au.build_arg_parser()
                        .parse_args(["--passphrase-file", "/dev/null", "--allow-unverified-listener"])
                        .allow_unverified_listener)

    def test_main_wires_the_flag_into_the_client(self):
        # ...and pin that main() actually PASSES it to RpcClient, defaulting to False.
        pf = self._passfile()
        seen = {}

        def fake_client(conn, allow_unverified=False):
            seen["allow_unverified"] = allow_unverified
            return OnceFake()

        with mock.patch.object(au, "RpcClient", fake_client):
            au.main(self._argv(pf))
        self.assertIs(seen["allow_unverified"], False)

        with mock.patch.object(au, "RpcClient", fake_client):
            au.main(self._argv(pf, "--allow-unverified-listener"))
        self.assertIs(seen["allow_unverified"], True)


class TestPassphraseFilePermissions(unittest.TestCase):
    def _passfile(self, mode):
        f = tempfile.NamedTemporaryFile("w", delete=False)
        f.write(_PASSPHRASE + "\n")
        f.close()
        self.addCleanup(os.unlink, f.name)
        os.chmod(f.name, mode)
        return f.name

    @unittest.skipUnless(os.name == "posix", "POSIX modes only")
    def test_group_or_world_readable_warns_but_still_reads(self):
        path = self._passfile(0o644)
        with mock.patch.object(au.sys, "stderr", io.StringIO()) as err:
            self.assertEqual(au.read_passphrase(path), _PASSPHRASE)
        self.assertIn("group/world-accessible", err.getvalue())

    @unittest.skipUnless(os.name == "posix", "POSIX modes only")
    def test_owner_only_file_is_silent(self):
        path = self._passfile(0o400)
        with mock.patch.object(au.sys, "stderr", io.StringIO()) as err:
            self.assertEqual(au.read_passphrase(path), _PASSPHRASE)
        self.assertEqual(err.getvalue(), "")


if __name__ == "__main__":
    unittest.main()
