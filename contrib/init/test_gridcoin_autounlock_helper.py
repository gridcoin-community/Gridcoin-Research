#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Tests for the socket-activated autounlock helper.

No test calls systemctl, opens a real credential or binds a socket outside a
temporary directory. Systemd is replaced by fakes; SO_PEERCRED is exercised on a
real socketpair.
"""

import importlib.machinery
import importlib.util
import io
import os
import socket
import sys
import tempfile
import unittest

HELPER_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "gridcoin_autounlock_helper.py")


def load_helper():
    loader = importlib.machinery.SourceFileLoader("gridcoin_autounlock_helper", HELPER_PATH)
    spec = importlib.util.spec_from_loader(loader.name, loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


h = load_helper()

PEER = 4242
UID = 1000


def never(*args):
    raise AssertionError("must not be called")


class TestEnvironment(unittest.TestCase):
    def test_listen_fd_requires_matching_pid(self):
        with self.assertRaises(h.UsageError):
            h.listen_fd_from_environment({"LISTEN_PID": "99", "LISTEN_FDS": "1"}, 100)
        with self.assertRaises(h.UsageError):
            h.listen_fd_from_environment({"LISTEN_FDS": "1"}, 100)
        with self.assertRaises(h.UsageError):
            h.listen_fd_from_environment({"LISTEN_PID": "100", "LISTEN_FDS": "2"}, 100)
        self.assertEqual(h.listen_fd_from_environment({"LISTEN_PID": "100", "LISTEN_FDS": "1"}, 100), 3)


class TestDecide(unittest.TestCase):
    def decide(self, peer_pid=PEER, peer_uid=UID, pidfd=7, accept_ticks=1000, main=PEER,
               starts=(500, 500), pidfd_pid=PEER):
        starts = iter(starts)
        return h.decide(peer_pid, peer_uid, UID, pidfd, accept_ticks,
                        lambda: main, lambda pid: next(starts), lambda fd: pidfd_pid)

    def test_main_pid_peer_served(self):
        allowed, reason = self.decide()
        self.assertTrue(allowed, reason)

    def test_non_main_pid_refused(self):
        allowed, reason = self.decide(main=PEER + 1)
        self.assertFalse(allowed)
        self.assertEqual(reason, "not the main process")

    def test_cgroup_child_refused(self):
        # a child in the unit's cgroup is not MainPID; no cgroup lookup exists
        self.assertFalse(hasattr(h, "cgroup_members"))
        allowed, reason = self.decide(peer_pid=PEER + 5, pidfd_pid=PEER + 5, main=PEER)
        self.assertFalse(allowed)
        self.assertEqual(reason, "not the main process")

    def test_exited_peer_refused_with_pidfd(self):
        allowed, reason = self.decide(pidfd_pid=-1)
        self.assertFalse(allowed)
        self.assertEqual(reason, "not the main process")

    def test_wrong_uid_refused(self):
        allowed, reason = h.decide(PEER, UID + 1, UID, 7, 1000, never, never, never)
        self.assertFalse(allowed)
        self.assertEqual(reason, "uid")

    def test_unknown_pid_refused(self):
        allowed, reason = h.decide(0, UID, UID, 7, 1000, never, never, never)
        self.assertFalse(allowed)
        self.assertEqual(reason, "pid unknown")

    def test_no_main_process_refused(self):
        allowed, reason = self.decide(main=0)
        self.assertFalse(allowed)
        self.assertEqual(reason, "no main process")

    def test_fallback_served(self):
        allowed, reason = self.decide(pidfd=None)
        self.assertTrue(allowed, reason)

    def test_fallback_refuses_process_started_after_accept(self):
        allowed, reason = h.decide(PEER, UID, UID, None, 1000, never, lambda pid: 1001, never)
        self.assertFalse(allowed)
        self.assertEqual(reason, "started after accept")
        allowed, reason = h.decide(PEER, UID, UID, None, 1000, never, lambda pid: None, never)
        self.assertFalse(allowed)
        self.assertEqual(reason, "started after accept")

    def test_fallback_refuses_pid_reused_during_query(self):
        allowed, reason = self.decide(pidfd=None, starts=(500, 501))
        self.assertFalse(allowed)

    def test_fallback_refuses_other_main_pid(self):
        allowed, reason = self.decide(pidfd=None, main=PEER + 1)
        self.assertFalse(allowed)

    def test_systemd_failure_propagates(self):
        def fail():
            raise h.SystemdQueryError("boom")
        with self.assertRaises(h.SystemdQueryError):
            h.decide(PEER, UID, UID, 7, 1000, fail, never, never)


class TestCredential(unittest.TestCase):
    def write(self, data):
        d = tempfile.TemporaryDirectory()
        self.addCleanup(d.cleanup)
        path = os.path.join(d.name, "wallet-passphrase")
        with open(path, "wb") as f:
            f.write(data)
        return d.name

    def test_load_passphrase_strips_one_newline(self):
        self.assertEqual(h.load_passphrase(self.write(b"pw\n")), b"pw")
        self.assertEqual(h.load_passphrase(self.write(b"pw\r\n")), b"pw")
        self.assertEqual(h.load_passphrase(self.write(b"pw\n\n")), b"pw\n")
        self.assertEqual(h.load_passphrase(self.write(b"pw")), b"pw")

    def test_load_passphrase_rejects_empty_oversize_non_utf8(self):
        cases = ((b"", "empty"), (b"\n", "empty"), (b"x" * 1025, "too long"),
                 (b"secret\xff\xfe", "not valid UTF-8"))
        for data, reason in cases:
            with self.assertRaises(h.CredentialError) as ctx:
                h.load_passphrase(self.write(data))
            self.assertEqual(str(ctx.exception), reason)
            self.assertNotIn("secret", str(ctx.exception))
            self.assertNotIn("\\xff", str(ctx.exception))

    def test_load_passphrase_missing(self):
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaises(h.CredentialError) as ctx:
                h.load_passphrase(d)
            self.assertEqual(str(ctx.exception), "missing")

    def test_max_length_accepted(self):
        self.assertEqual(len(h.load_passphrase(self.write(b"x" * 1024))), 1024)


class TestProc(unittest.TestCase):
    def test_start_ticks_parses_comm_with_parens(self):
        # field 22 is starttime: after "pid (comm) " field 3 is the state
        rest = ["S"] + ["0"] * 18 + ["987654"] + ["0"] * 5
        line = "4242 ((a) b)) " + " ".join(rest) + "\n"
        with tempfile.TemporaryDirectory() as proc:
            os.mkdir(os.path.join(proc, "4242"))
            with open(os.path.join(proc, "4242", "stat"), "w", encoding="utf8") as f:
                f.write(line)
            self.assertEqual(h.process_start_ticks(4242, proc), 987654)
            self.assertIsNone(h.process_start_ticks(1, proc))

    def test_pidfd_pid_parses_fdinfo(self):
        with tempfile.TemporaryDirectory() as proc:
            os.makedirs(os.path.join(proc, "self", "fdinfo"))
            with open(os.path.join(proc, "self", "fdinfo", "9"), "w", encoding="utf8") as f:
                f.write("pos:\t0\nflags:\t02000000\nPid:\t-1\n")
            self.assertEqual(h.pidfd_pid(9, proc), -1)
            with open(os.path.join(proc, "self", "fdinfo", "9"), "w", encoding="utf8") as f:
                f.write("pos:\t0\nPid:\t321\n")
            self.assertEqual(h.pidfd_pid(9, proc), 321)

    def test_boottime_ticks_positive(self):
        self.assertGreater(h.boottime_ticks(), 0)

    def test_peer_credentials_real_socketpair(self):
        a, b = socket.socketpair()
        self.addCleanup(a.close)
        self.addCleanup(b.close)
        self.assertEqual(h.peer_credentials(a), (os.getpid(), os.getuid(), os.getgid()))

    def test_query_main_pid(self):
        calls = []

        class Done:
            returncode = 0
            stdout = "4242\n"

        def run(argv, **kw):
            calls.append((argv, kw))
            return Done()

        self.assertEqual(h.query_main_pid("u.service", run), 4242)
        self.assertEqual(calls[0][0], ["systemctl", "show", "--property=MainPID", "--value", "--", "u.service"])
        self.assertEqual(calls[0][1]["env"]["PATH"], "/usr/bin:/bin")

        Done.stdout = "garbage"
        with self.assertRaises(h.SystemdQueryError):
            h.query_main_pid("u.service", run)
        Done.stdout = "1\n"
        Done.returncode = 1
        with self.assertRaises(h.SystemdQueryError):
            h.query_main_pid("u.service", run)

        def boom(argv, **kw):
            raise OSError("no systemctl")
        with self.assertRaises(h.SystemdQueryError):
            h.query_main_pid("u.service", boom)


class TestServe(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        # a sentinel credential directory that must stay untouched on refusal
        self.sentinel = os.path.join(self.tmp.name, "never-opened")
        self.opened = []
        self.log = io.StringIO()

    def deps(self, **over):
        d = {
            "main_pid": lambda unit: os.getpid(),
            "start_ticks": lambda pid, proc="/proc": 1,
            "boottime_ticks": lambda: 10 ** 9,
            "pidfd": lambda conn: None,
            "pidfd_pid": lambda fd: os.getpid(),
            "load_passphrase": self.load,
            "uid_of_user": lambda name: os.getuid(),
            "log": self.log,
            "accept_timeout": 2.0,
        }
        d.update(over)
        return d

    def load(self, cred_dir):
        self.opened.append(cred_dir)
        return b"hunter2"

    def run_serve(self, **over):
        server, client = socket.socketpair()
        self.addCleanup(server.close)
        self.addCleanup(client.close)
        # a socketpair end is not a listening socket, so serve through a unix listener
        path = os.path.join(self.tmp.name, "s.sock")
        lst = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        lst.bind(path)
        lst.listen(1)
        self.addCleanup(lst.close)
        c = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.addCleanup(c.close)
        c.connect(path)
        env = {"CREDENTIALS_DIRECTORY": self.sentinel}
        rc = h.serve_once(os.dup(lst.fileno()), "gridcoinresearchd@alice.service", "alice", env, self.deps(**over))
        data = b""
        while True:
            chunk = c.recv(100)
            if not chunk:
                break
            data += chunk
        return rc, data

    def test_served(self):
        rc, data = self.run_serve()
        self.assertEqual(rc, h.EXIT_SERVED)
        self.assertEqual(data, b"hunter2")
        self.assertEqual(self.opened, [self.sentinel])
        self.assertIn("served gridcoinresearchd@alice.service main pid", self.log.getvalue())
        self.assertNotIn("hunter2", self.log.getvalue())

    def test_refusal_sends_nothing(self):
        rc, data = self.run_serve(main_pid=lambda unit: os.getpid() + 1)
        self.assertEqual(rc, h.EXIT_REFUSED)
        self.assertEqual(data, b"")
        self.assertEqual(self.opened, [])
        self.assertIn("not the main process", self.log.getvalue())

    def test_wrong_uid_reads_no_credential(self):
        rc, data = self.run_serve(uid_of_user=lambda name: os.getuid() + 1)
        self.assertEqual(rc, h.EXIT_REFUSED)
        self.assertEqual(data, b"")
        self.assertEqual(self.opened, [])

    def test_systemd_query_failure_reads_no_credential(self):
        def fail(unit):
            raise h.SystemdQueryError("down")
        rc, data = self.run_serve(main_pid=fail)
        self.assertEqual(rc, h.EXIT_SYSTEMD)
        self.assertEqual(data, b"")
        self.assertEqual(self.opened, [])
        self.assertFalse(os.path.exists(self.sentinel))

    def test_credential_failure(self):
        def bad(cred_dir):
            raise h.CredentialError("empty")
        rc, data = self.run_serve(load_passphrase=bad)
        self.assertEqual(rc, h.EXIT_CREDENTIAL)
        self.assertEqual(data, b"")

    def test_no_connection(self):
        path = os.path.join(self.tmp.name, "idle.sock")
        lst = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        lst.bind(path)
        lst.listen(1)
        self.addCleanup(lst.close)
        rc = h.serve_once(os.dup(lst.fileno()), "u.service", "alice", {}, self.deps(accept_timeout=0.05))
        self.assertEqual(rc, h.EXIT_NO_CONNECTION)


class TestMain(unittest.TestCase):
    def test_usage(self):
        err = io.StringIO()
        self.assertEqual(h.main(["--bound-unit=x.service"], stderr=err), h.EXIT_USAGE)
        self.assertEqual(h.main([], stderr=err), h.EXIT_USAGE)
        self.assertEqual(h.main(["--bound-unit=x.service", "--peer-user=a", "--junk"], stderr=err), h.EXIT_USAGE)

    def test_no_activation_environment(self):
        err = io.StringIO()
        saved = {k: os.environ.pop(k, None) for k in ("LISTEN_PID", "LISTEN_FDS")}
        try:
            rc = h.main(["--bound-unit=x.service", "--peer-user=a"], stderr=err)
        finally:
            for k, v in saved.items():
                if v is not None:
                    os.environ[k] = v
        self.assertEqual(rc, h.EXIT_USAGE)


if __name__ == "__main__":
    unittest.main(argv=sys.argv)
