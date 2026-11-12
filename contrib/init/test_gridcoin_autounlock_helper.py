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
import threading
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
    def decide(self, peer_pid=PEER, peer_uid=UID, answer_pid=PEER, answer_uid=UID, pidfd=7, challenge_ticks=1000,
               main=PEER, starts=(500, 500), pidfd_pid=PEER):
        starts = iter(starts)
        return h.decide(peer_pid, peer_uid, answer_pid, answer_uid, UID, pidfd, challenge_ticks,
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
        allowed, reason = self.decide(peer_pid=PEER + 5, answer_pid=PEER + 5, pidfd_pid=PEER + 5, main=PEER)
        self.assertFalse(allowed)
        self.assertEqual(reason, "not the main process")

    def test_exited_peer_refused_with_pidfd(self):
        allowed, reason = self.decide(pidfd_pid=-1)
        self.assertFalse(allowed)
        self.assertEqual(reason, "not the main process")

    def test_wrong_uid_refused(self):
        allowed, reason = h.decide(PEER, UID + 1, PEER, UID, UID, 7, 1000, never, never, never)
        self.assertFalse(allowed)
        self.assertEqual(reason, "uid")

    def test_answer_under_another_uid_refused(self):
        allowed, reason = h.decide(PEER, UID, PEER, UID + 1, UID, 7, 1000, never, never, never)
        self.assertFalse(allowed)
        self.assertEqual(reason, "uid")

    def test_unknown_pid_refused(self):
        allowed, reason = h.decide(0, UID, 0, UID, UID, 7, 1000, never, never, never)
        self.assertFalse(allowed)
        self.assertEqual(reason, "pid unknown")

    def test_answer_from_another_process_refused(self):
        # The process that connected did not send the answer, with a pidfd or without.
        for pidfd in (7, None):
            allowed, reason = h.decide(PEER, UID, PEER + 1, UID, UID, pidfd, 1000, never, never, never)
            self.assertFalse(allowed)
            self.assertEqual(reason, "the answer came from another process")

    def test_no_main_process_refused(self):
        allowed, reason = self.decide(main=0)
        self.assertFalse(allowed)
        self.assertEqual(reason, "no main process")

    def test_fallback_served(self):
        allowed, reason = self.decide(pidfd=None)
        self.assertTrue(allowed, reason)

    def test_fallback_refuses_a_process_started_once_the_challenge_was_taken(self):
        # The answer was sent after the challenge, so a process that started in the challenge's tick or later
        # cannot be the one that sent it: its pid was another process's then.
        for start in (1000, 1001):
            allowed, reason = h.decide(PEER, UID, PEER, UID, UID, None, 1000, never, lambda pid: start, never)
            self.assertFalse(allowed)
            self.assertEqual(reason, "started after the challenge")

    def test_fallback_served_for_a_process_started_before_the_challenge(self):
        allowed, reason = self.decide(pidfd=None, starts=(999, 999))
        self.assertTrue(allowed, reason)

    def test_fallback_refuses_an_unreadable_start_time_and_says_why(self):
        # A /proc mounted with hidepid hides the core's stat from the helper's account.
        allowed, reason = h.decide(PEER, UID, PEER, UID, UID, None, 1000, never, lambda pid: None, never)
        self.assertFalse(allowed)
        self.assertIn("hidepid", reason)

    def test_fallback_refuses_pid_reused_during_query(self):
        allowed, reason = self.decide(pidfd=None, starts=(500, 501))
        self.assertFalse(allowed)
        self.assertEqual(reason, "not the main process")

    def test_fallback_refuses_other_main_pid(self):
        allowed, reason = self.decide(pidfd=None, main=PEER + 1)
        self.assertFalse(allowed)
        self.assertEqual(reason, "not the main process")

    def test_systemd_failure_propagates(self):
        def fail():
            raise h.SystemdQueryError("boom")
        with self.assertRaises(h.SystemdQueryError):
            h.decide(PEER, UID, PEER, UID, UID, 7, 1000, fail, never, never)


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

    def answer_pair(self):
        a, b = socket.socketpair()
        self.addCleanup(a.close)
        self.addCleanup(b.close)
        a.setsockopt(socket.SOL_SOCKET, socket.SO_PASSCRED, 1)
        return a, b

    def test_answer_carries_the_senders_pid(self):
        a, b = self.answer_pair()
        b.sendall(b"0123456789abcdef")
        self.assertEqual(h.read_answer(a, 16, 1.0), (b"0123456789abcdef", os.getpid(), os.getuid()))

    def test_answer_sent_by_a_child_carries_the_childs_pid(self):
        # The kernel stamps the process that sends, not the one that connected, and keeps the stamp after it exits.
        a, b = self.answer_pair()
        pid = os.fork()
        if pid == 0:
            b.sendall(b"0123456789abcdef")
            os._exit(0)
        os.waitpid(pid, 0)
        self.assertEqual(h.read_answer(a, 16, 1.0), (b"0123456789abcdef", pid, os.getuid()))

    def test_answer_split_between_two_processes_is_none(self):
        a, b = self.answer_pair()
        pid = os.fork()
        if pid == 0:
            b.sendall(b"01234567")
            os._exit(0)
        os.waitpid(pid, 0)
        b.sendall(b"89abcdef")
        self.assertIsNone(h.read_answer(a, 16, 1.0))

    def test_short_answer_is_none(self):
        a, b = self.answer_pair()
        b.sendall(b"01234567")
        b.shutdown(socket.SHUT_WR)
        self.assertIsNone(h.read_answer(a, 16, 1.0))

    def test_no_answer_is_none_once_the_wait_is_over(self):
        a, b = self.answer_pair()
        self.assertIsNone(h.read_answer(a, 16, 0.05))

    def test_start_ticks_without_close_paren(self):
        with tempfile.TemporaryDirectory() as proc:
            os.mkdir(os.path.join(proc, "7"))
            with open(os.path.join(proc, "7", "stat"), "w", encoding="utf8") as f:
                f.write("7 broken S 1 2 3\n")
            self.assertIsNone(h.process_start_ticks(7, proc))

    def test_real_pidfd_path_on_socketpair(self):
        a, b = socket.socketpair()
        self.addCleanup(a.close)
        self.addCleanup(b.close)
        try:
            fd = h.peer_pidfd(a)
        except OSError as e:
            self.skipTest("pidfd lookup unavailable: %s" % e)
        if fd is None:
            self.skipTest("SO_PEERPIDFD unavailable here (kernel or architecture)")
        try:
            self.assertEqual(h.pidfd_pid(fd), os.getpid())
        finally:
            os.close(fd)

    def test_real_pidfd_of_reaped_process_is_minus_one(self):
        pid = os.fork()
        if pid == 0:
            os._exit(0)
        try:
            fd = os.pidfd_open(pid)
        except (OSError, AttributeError) as e:
            os.waitpid(pid, 0)
            self.skipTest("pidfd_open unavailable: %s" % e)
        self.addCleanup(os.close, fd)
        self.assertEqual(h.pidfd_pid(fd), pid)
        os.waitpid(pid, 0)
        self.assertEqual(h.pidfd_pid(fd), -1)

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
        # non-ASCII digits are refused (superscript two and Arabic-Indic digits)
        for text in ("\u00b2", "\u0664\u0662"):
            Done.stdout = text
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
        self.challenges = []
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
            "answer_timeout": 2.0,
        }
        d.update(over)
        return d

    def load(self, cred_dir):
        self.opened.append(cred_dir)
        return b"hunter2"

    @staticmethod
    def echo(conn, challenge):
        conn.sendall(challenge)

    def run_serve(self, answer=None, **over):
        # Serve through a unix listener. The client, on a thread of this process, reads the challenge, answers it
        # with answer(conn, challenge) (by default it sends it back) and reads the rest to end of file.
        path = os.path.join(self.tmp.name, "s%d.sock" % len(self.challenges))
        lst = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        lst.bind(path)
        lst.listen(1)
        self.addCleanup(lst.close)
        c = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.addCleanup(c.close)
        c.connect(path)
        c.settimeout(10)
        got = {"challenge": b"", "data": b""}

        def client():
            while len(got["challenge"]) < h.CHALLENGE_BYTES:
                chunk = c.recv(h.CHALLENGE_BYTES - len(got["challenge"]))
                if not chunk:
                    return
                got["challenge"] += chunk
            (answer or self.echo)(c, got["challenge"])
            while True:
                chunk = c.recv(100)
                if not chunk:
                    return
                got["data"] += chunk

        thread = threading.Thread(target=client)
        thread.start()
        env = {"CREDENTIALS_DIRECTORY": self.sentinel}
        try:
            rc = h.serve_once(os.dup(lst.fileno()), "gridcoinresearchd@alice.service", "alice", env,
                              self.deps(**over))
        finally:
            thread.join(10)
        self.challenges.append(got["challenge"])
        return rc, got["data"]

    def test_served(self):
        rc, data = self.run_serve()
        self.assertEqual(rc, h.EXIT_SERVED)
        self.assertEqual(data, b"hunter2")
        self.assertEqual(self.opened, [self.sentinel])
        self.assertIn("served gridcoinresearchd@alice.service main pid", self.log.getvalue())
        self.assertNotIn("hunter2", self.log.getvalue())

    def test_each_connection_gets_a_fresh_challenge(self):
        self.run_serve()
        self.run_serve()
        self.assertEqual([len(c) for c in self.challenges], [h.CHALLENGE_BYTES, h.CHALLENGE_BYTES])
        self.assertNotEqual(self.challenges[0], self.challenges[1])

    def test_fallback_bound_is_the_time_of_the_challenge(self):
        # The peer started in the tick the challenge was taken in: refused, nothing read.
        rc, data = self.run_serve(start_ticks=lambda pid, proc="/proc": 600, boottime_ticks=lambda: 600)
        self.assertEqual(rc, h.EXIT_REFUSED)
        self.assertEqual(data, b"")
        self.assertEqual(self.opened, [])
        self.assertIn("started after the challenge", self.log.getvalue())
        # One tick earlier it is served.
        rc, data = self.run_serve(start_ticks=lambda pid, proc="/proc": 599, boottime_ticks=lambda: 600)
        self.assertEqual(rc, h.EXIT_SERVED)
        self.assertEqual(data, b"hunter2")

    def test_wrong_answer_reads_no_credential(self):
        rc, data = self.run_serve(answer=lambda conn, challenge: conn.sendall(bytes(len(challenge))))
        self.assertEqual(rc, h.EXIT_REFUSED)
        self.assertEqual(data, b"")
        self.assertEqual(self.opened, [])
        self.assertIn("the challenge was not answered", self.log.getvalue())

    def test_no_answer_reads_no_credential(self):
        rc, data = self.run_serve(answer=lambda conn, challenge: None, answer_timeout=0.1)
        self.assertEqual(rc, h.EXIT_REFUSED)
        self.assertEqual(data, b"")
        self.assertEqual(self.opened, [])
        self.assertIn("the challenge was not answered", self.log.getvalue())

    def test_answer_from_a_child_process_reads_no_credential(self):
        # Another process answers on the connection the main process made: the kernel's stamp names it.
        def from_child(conn, challenge):
            pid = os.fork()
            if pid == 0:
                conn.sendall(challenge)
                os._exit(0)
            os.waitpid(pid, 0)

        rc, data = self.run_serve(answer=from_child)
        self.assertEqual(rc, h.EXIT_REFUSED)
        self.assertEqual(data, b"")
        self.assertEqual(self.opened, [])
        self.assertIn("the answer came from another process", self.log.getvalue())

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
