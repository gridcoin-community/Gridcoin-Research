#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Tests for gridcoin-service.

Every side effect goes through a FakeHost and a Paths rooted in a temporary
directory. No test runs systemctl, gpasswd, loginctl or any other host command.
The tests run unprivileged, so the copies run as the test user throughout.
"""

import collections
import contextlib
import dataclasses
import datetime
import importlib.machinery
import importlib.util
import io
import json
import os
import re
import shutil
import socket
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest import mock

TOOL_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "gridcoin_service.py.in")
SOURCE_ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
SYSTEMD_UNITS_CMAKE = os.path.join(SOURCE_ROOT, "cmake", "systemd_units.cmake")
SRC_CMAKELISTS = os.path.join(SOURCE_ROOT, "src", "CMakeLists.txt")
PATTERNS_FILE = os.path.join(SOURCE_ROOT, "share", "gridcoin", "refused-setting-keys.txt")


def load_tool(path=TOOL_PATH):
    loader = importlib.machinery.SourceFileLoader("gridcoin_service", path)
    spec = importlib.util.spec_from_loader(loader.name, loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


svc = load_tool()

Passwd = collections.namedtuple("Passwd", "pw_name pw_uid pw_dir pw_gid", defaults=(os.getgid(),))
CONFIGURED = {"bindir": "/usr/bin", "default_network": "main", "python3": "/usr/bin/python3",
              "libexec_subdir_main": "gridcoin", "libexec_subdir_test": "gridcointestnet",
              "refused_keys_main": "/usr/share/gridcoin/refused-setting-keys.txt",
              "refused_keys_test": "/usr/share/gridcoin/refused-setting-keys.txt"}


class FakeHost:
    def __init__(self, users=None, groups=None, root=True, enabled=(), active=(), fail=(), processes=None,
                 terminate_ends_processes=True, login=None):
        self.users = users if users is not None else {}
        self.groups = groups if groups is not None else {}
        self.root = root
        self.fail = set(fail)
        self.enabled = set(enabled)
        self.active = set(active)
        self.calls = []
        self.processes = dict(processes) if processes is not None else {}
        self.terminate_ends_processes = terminate_ends_processes
        self.clock = 0.0
        self.stamps = 0
        self.login = login

    def login_uid(self):
        return self.login

    def user_processes(self, uid):
        return list(self.processes.get(uid, []))

    def terminate_user(self, name):
        self.calls.append(("terminate_user", name))
        if self.terminate_ends_processes:
            self.processes.clear()

    def add_to_group(self, user, group):
        self.calls.append(("add_to_group", user, group))
        if "add_to_group" in self.fail:
            raise svc.StepError("step 'gpasswd -a %s %s' failed with exit status 3" % (user, group))

    def remove_from_group(self, user, group):
        self.calls.append(("remove_from_group", user, group))
        if "remove_from_group" in self.fail:
            raise svc.StepError("step 'gpasswd -d %s %s' failed with exit status 3" % (user, group))

    def sleep(self, seconds):
        self.clock += seconds

    def monotonic(self):
        return self.clock

    def utcnow(self):
        # One second later on every call, so two commands in one test get distinct names.
        self.stamps += 1
        return (datetime.datetime(2026, 11, 4, 12, 0, tzinfo=datetime.timezone.utc)
                + datetime.timedelta(seconds=self.stamps))

    def setgroups(self, groups):
        self.calls.append(("setgroups", list(groups)))

    def setgid(self, gid):
        self.calls.append(("setgid", gid))

    def setuid(self, uid):
        self.calls.append(("setuid", uid))

    def systemctl(self, *args):
        self.calls.append(args)
        return 1 if args[0] in self.fail else 0

    def is_enabled(self, unit):
        return unit in self.enabled

    def is_active(self, unit):
        return unit in self.active

    def getpwnam(self, name):
        if name not in self.users:
            raise KeyError(name)
        return self.users[name]

    def user_groups(self, name):
        return list(self.groups.get(name, []))

    def is_root(self):
        return self.root


class ServiceTestBase(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        root = self.tmp.name
        self.paths = svc.Paths(
            etc_systemd=os.path.join(root, "systemd"),
            credstore=os.path.join(root, "credstore"),
            var_lib=os.path.join(root, "var_lib"),
            tool_state=os.path.join(root, "state"),
            login_defs=os.path.join(root, "login.defs"),
        )
        for d in (self.paths.etc_systemd, self.paths.credstore):
            os.makedirs(d)
        with open(self.paths.login_defs, "w", encoding="utf8") as f:
            f.write("UID_MIN 1000\n")
        self.home = os.path.join(root, "home", "bob")
        os.makedirs(self.home)
        self.uid = os.getuid()
        self.host = FakeHost(users={"bob": Passwd("bob", self.uid + 0, self.home)})
        saved = dict(svc.CONFIG)
        svc.CONFIG.update(CONFIGURED)
        self.addCleanup(lambda: (svc.CONFIG.clear(), svc.CONFIG.update(saved)))

    def run_main(self, *argv, host=None):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            try:
                rc = svc.main(list(argv), host or self.host, self.paths)
            except SystemExit as e:
                rc = e.code
        return rc, out.getvalue(), err.getvalue()

    def dropin(self, unit, name="50-gridcoin-service.conf"):
        path = os.path.join(self.paths.etc_systemd, unit + ".d", name)
        with open(path, encoding="utf8") as f:
            return f.read()

    def make_credential(self, name, mode=0o600):
        path = os.path.join(self.paths.credstore, name)
        with open(path, "w", encoding="utf8") as f:
            f.write("x")
        os.chmod(path, mode)
        return path


class ValidateUserTests(ServiceTestBase):
    def test_validate_user_refuses_root_system_and_bad_names(self):
        host = FakeHost(users={
            "root": Passwd("root", 0, "/root"),
            "daemon": Passwd("daemon", 999, "/"),
            "bob": Passwd("bob", 1000, self.home),
        })
        for name in ("root", "daemon", "a/b", "nobody", "", "b ob"):
            with self.assertRaises(svc.UserError, msg=name):
                svc.validate_user(name, host, self.paths)
        self.assertEqual(svc.validate_user("bob", host, self.paths).pw_uid, 1000)

    def test_names_made_only_of_dots_refused(self):
        host = FakeHost(users={name: Passwd(name, 1000, self.home) for name in (".", "..", "...")})
        for name in (".", "..", "..."):
            with self.assertRaises(svc.UserError, msg=name):
                svc.validate_name(name)
            with self.assertRaises(svc.UserError, msg=name):
                svc.validate_user(name, host, self.paths)
            rc, out, err = self.run_main("disable-user", name, host=host)
            self.assertEqual(rc, 1, name)
            rc, out, err = self.run_main("autounlock", "disable", name, host=host)
            self.assertEqual(rc, 1, name)
        self.assertEqual(host.calls, [])
        self.assertEqual(os.listdir(self.paths.etc_systemd), [])
        for name in (".bob", "bob.", "b.o.b", "a..b"):
            svc.validate_name(name)

    def test_names_with_a_trailing_newline_refused(self):
        host = FakeHost(users={"bob\n": Passwd("bob\n", 1000, self.home)})
        for name in ("bob\n", "bob\n\n", "\nbob"):
            with self.assertRaises(svc.UserError, msg=repr(name)):
                svc.validate_name(name)
        with self.assertRaises(svc.UserError):
            svc.validate_user("bob\n", host, self.paths)
        rc, out, err = self.run_main("disable-user", "bob\n", host=host)
        self.assertEqual(rc, 1)
        self.assertEqual(host.calls, [])

    def test_validate_user_refuses_gid_0(self):
        host = FakeHost(users={"bob": Passwd("bob", 1000, self.home, 0)})
        with self.assertRaisesRegex(svc.UserError, "gid 0"):
            svc.validate_user("bob", host, self.paths)

    def test_uid_min_default_when_login_defs_missing(self):
        os.unlink(self.paths.login_defs)
        host = FakeHost(users={"low": Passwd("low", 999, "/h"), "ok": Passwd("ok", 1000, "/h")})
        with self.assertRaises(svc.UserError):
            svc.validate_user("low", host, self.paths)
        svc.validate_user("ok", host, self.paths)


class CMakeValuesTests(unittest.TestCase):
    def test_libexec_subdir_comes_from_cmake(self):
        with open(SYSTEMD_UNITS_CMAKE, encoding="utf8") as f:
            cmake = f.read()
        values = dict(re.findall(r'set\(GRC_LIBEXEC_SUBDIR_(MAIN|TEST) "([^"]*)"\)', cmake))
        self.assertEqual(sorted(values), ["MAIN", "TEST"])
        # Each network's branch installs the helper under the same value.
        for net in ("MAIN", "TEST"):
            branch = re.search(r'GRC_UNIT_NETWORK STREQUAL "%s"\)(.*?)(?:elseif|else)\(' % net.lower(), cmake, re.S)
            self.assertIsNotNone(branch, net)
            self.assertIn('set(GRC_LIBEXEC_SUBDIR "${GRC_LIBEXEC_SUBDIR_%s}")' % net, branch.group(1))
        self.assertIn('DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}/${GRC_LIBEXEC_SUBDIR}"', cmake)

        # The tool configured with those values names the same directories.
        with open(TOOL_PATH, encoding="utf8") as f:
            text = f.read()
        for net in ("MAIN", "TEST"):
            placeholder = "@GRC_LIBEXEC_SUBDIR_%s@" % net
            self.assertIn(placeholder, text)
            text = text.replace(placeholder, values[net])
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "gridcoin_service.py")
            with open(path, "w", encoding="utf8") as f:
                f.write(text)
            configured = load_tool(path)
        self.assertEqual(configured.NETWORKS["main"].subdir, values["MAIN"])
        self.assertEqual(configured.NETWORKS["test"].subdir, values["TEST"])

    def test_refused_keys_path_comes_from_cmake(self):
        with open(SYSTEMD_UNITS_CMAKE, encoding="utf8") as f:
            cmake = f.read()
        subdirs = dict(re.findall(r'set\(GRC_SHARE_SUBDIR_(MAIN|TEST) "([^"]*)"\)', cmake))
        self.assertEqual(sorted(subdirs), ["MAIN", "TEST"])
        for net in ("MAIN", "TEST"):
            self.assertIn('set(GRC_REFUSED_KEYS_FILE_%s "${CMAKE_INSTALL_FULL_DATADIR}/${GRC_SHARE_SUBDIR_%s}/'
                          'refused-setting-keys.txt")' % (net, net), cmake)
            branch = re.search(r'GRC_UNIT_NETWORK STREQUAL "%s"\)(.*?)(?:elseif|else)\(' % net.lower(), cmake, re.S)
            self.assertIsNotNone(branch, net)
            self.assertIn('set(GRC_SHARE_SUBDIR "${GRC_SHARE_SUBDIR_%s}")' % net, branch.group(1))
        # The file is installed where this network's tool looks for it.
        with open(SRC_CMAKELISTS, encoding="utf8") as f:
            src = f.read()
        self.assertRegex(src, r'install\(FILES "\$\{CMAKE_SOURCE_DIR\}/share/gridcoin/refused-setting-keys\.txt"'
                              r' COMPONENT daemon\s+DESTINATION "\$\{CMAKE_INSTALL_DATADIR\}/\$\{GRC_SHARE_SUBDIR\}"')

        with open(TOOL_PATH, encoding="utf8") as f:
            text = f.read()
        expected = {}
        for net in ("MAIN", "TEST"):
            placeholder = "@GRC_REFUSED_KEYS_FILE_%s@" % net
            self.assertIn(placeholder, text)
            expected[net] = "/usr/share/%s/refused-setting-keys.txt" % subdirs[net]
            text = text.replace(placeholder, expected[net])
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "gridcoin_service.py")
            with open(path, "w", encoding="utf8") as f:
                f.write(text)
            configured = load_tool(path)
        self.assertEqual(configured.NETWORKS["main"].refused_keys, expected["MAIN"])
        self.assertEqual(configured.NETWORKS["test"].refused_keys, expected["TEST"])


    def test_tmpfiles_paths_are_declared_once_across_networks(self):
        # gridcoin.tmpfiles.in is installed once per network; any other *.tmpfiles file is
        # installed under the same name by both, so it counts once.
        init_dir = os.path.dirname(TOOL_PATH)
        declared = []
        with open(os.path.join(init_dir, "gridcoin.tmpfiles.in"), encoding="utf8") as f:
            per_network = f.read()
        for net in svc.NETWORKS.values():
            text = per_network.replace("@GRC_CONF_DIR@", net.conf_dir).replace("@GRC_ACCOUNT@", net.account)
            self.assertNotIn("@", text)
            declared += [line.split()[1] for line in text.splitlines() if line and not line.startswith("#")]
        shared = sorted(name for name in os.listdir(init_dir) if name.endswith(".tmpfiles"))
        for name in shared:
            with open(os.path.join(init_dir, name), encoding="utf8") as f:
                declared += [line.split()[1] for line in f.read().splitlines() if line and not line.startswith("#")]
        self.assertEqual(len(declared), len(set(declared)), declared)
        self.assertIn("/var/lib/gridcoin-service", declared)
        with open(SYSTEMD_UNITS_CMAKE, encoding="utf8") as f:
            cmake = f.read()
        for name in shared:
            self.assertIn(name, cmake)


class ResolveDatadirTests(ServiceTestBase):
    def test_no_override_means_no_datadir(self):
        self.assertIsNone(svc.resolve_datadir(self.host.users["bob"], None))

    def test_override_must_be_absolute_owned_directory_without_symlink(self):
        pw = self.host.users["bob"]
        good = os.path.join(self.home, "data")
        os.makedirs(good)
        self.assertEqual(svc.resolve_datadir(pw, good), good)
        with self.assertRaises(svc.UserError):
            svc.resolve_datadir(pw, "relative/data")
        with self.assertRaises(svc.UserError):
            svc.resolve_datadir(pw, os.path.join(self.home, "missing"))
        link = os.path.join(self.home, "link")
        os.symlink(good, link)
        with self.assertRaises(svc.UserError):
            svc.resolve_datadir(pw, link)
        with self.assertRaises(svc.UserError):
            svc.resolve_datadir(pw, os.path.join(link, "."))
        other = Passwd("bob", pw.pw_uid + 1, self.home)
        with self.assertRaises(svc.UserError):
            svc.resolve_datadir(other, good)

    def test_override_with_unit_special_characters_refused(self):
        pw = self.host.users["bob"]
        for bad in ("d ata", "d%ata", "d$ata", 'd"ata'):
            path = os.path.join(self.home, bad)
            os.makedirs(path)
            with self.assertRaises(svc.UserError, msg=bad):
                svc.resolve_datadir(pw, path)


class EnableUserTests(ServiceTestBase):
    def test_enable_user_nonstandard_home_writes_no_datadir(self):
        self.host.users["bob"] = Passwd("bob", self.uid, "/srv/homes/DOMAIN/bob")
        rc, out, err = self.run_main("enable-user", "bob")
        self.assertEqual(rc, 0, err)
        text = self.dropin("gridcoinresearchd@bob.service")
        self.assertNotIn("-datadir", text)
        self.assertNotIn("BindPaths", text)
        self.assertNotIn("BindReadOnlyPaths", text)
        self.assertIn("ExecStart=/usr/bin/gridcoinresearchd  -nodaemon -nodaemonwait -multiprocess $GRIDCOIN_AUTOUNLOCK_ARGS", text)

    def test_enable_user_with_datadir_writes_exactly_that_path(self):
        data = os.path.join(self.home, "elsewhere")
        os.makedirs(data)
        rc, out, err = self.run_main("enable-user", "bob", "--datadir=" + data)
        self.assertEqual(rc, 0, err)
        text = self.dropin("gridcoinresearchd@bob.service")
        self.assertIn("-multiprocess -datadir=%s $GRIDCOIN_AUTOUNLOCK_ARGS" % data, text)
        self.assertNotIn("BindPaths", text)

    def test_enable_user_testnet_adds_testnet_and_no_binds(self):
        rc, out, err = self.run_main("enable-user", "bob", "--network=test")
        self.assertEqual(rc, 0, err)
        text = self.dropin("gridcointestnetd@bob.service")
        self.assertIn("ExecStart=/usr/bin/gridcointestnetd -testnet -nodaemon", text)
        self.assertNotIn("Bind", text)
        self.assertNotIn("-datadir", text)

    def test_dropin_resets_execstart_and_exit_78(self):
        text = svc.render_user_dropin(svc.NETWORKS["main"], None)
        lines = text.splitlines()
        first = lines.index("ExecStart=")
        real = [i for i, l in enumerate(lines) if l.startswith("ExecStart=/")]
        self.assertEqual(len(real), 1)
        self.assertLess(first, real[0])
        self.assertIn("-nodaemon -nodaemonwait -multiprocess", lines[real[0]])
        self.assertNotRegex(lines[real[0]], r"(^| )-daemon( |$)")
        self.assertIn("RestartPreventExitStatus=", lines[real[0] + 1:])
        self.assertIn("Environment=GRIDCOIN_AUTOUNLOCK_ARGS=", lines)
        self.assertTrue(text.startswith("# Written by gridcoin-service; rerun the tool instead of editing"))

    def test_enable_user_commands_and_file_mode(self):
        rc, out, err = self.run_main("enable-user", "bob")
        self.assertEqual(rc, 0, err)
        self.assertEqual(self.host.calls, [
            ("daemon-reload",),
            ("enable", "--now", "gridcoinresearchd@bob.service"),
        ])
        path = os.path.join(self.paths.etc_systemd, "gridcoinresearchd@bob.service.d", "50-gridcoin-service.conf")
        self.assertEqual(os.stat(path).st_mode & 0o777, 0o644)

    def test_enable_user_refused_when_system_service_enabled(self):
        self.host.enabled.add("gridcoinresearchd.service")
        rc, out, err = self.run_main("enable-user", "bob")
        self.assertNotEqual(rc, 0)
        self.assertEqual(self.host.calls, [])
        self.assertFalse(os.path.exists(os.path.join(self.paths.etc_systemd, "gridcoinresearchd@bob.service.d")))

    def test_root_equivalent_groups_warned(self):
        self.host.groups["bob"] = ["bob", "docker", "audio", "wheel"]
        rc, out, err = self.run_main("enable-user", "bob")
        self.assertEqual(rc, 0, err)
        text = out + err
        self.assertIn("docker", text)
        self.assertIn("wheel", text)
        self.assertNotIn("audio", text)
        self.assertIn("gridcoin-service enable", text)

    def test_no_warning_without_such_groups(self):
        self.host.groups["bob"] = ["bob", "audio"]
        rc, out, err = self.run_main("enable-user", "bob")
        self.assertNotIn("convenience", out + err)

    def test_not_root_refused(self):
        host = FakeHost(users=self.host.users, root=False)
        rc, out, err = self.run_main("enable-user", "bob", host=host)
        self.assertNotEqual(rc, 0)
        self.assertEqual(host.calls, [])

    def test_unknown_user_refused_without_writes(self):
        rc, out, err = self.run_main("enable-user", "nobody")
        self.assertNotEqual(rc, 0)
        self.assertEqual(self.host.calls, [])
        self.assertEqual(os.listdir(self.paths.etc_systemd), [])

    def test_removed_options_are_usage_errors(self):
        for opt in ("--backupdir=/x", "--boincdatadir=/x"):
            rc, out, err = self.run_main("enable-user", "bob", opt)
            self.assertEqual(rc, 2)
        self.assertEqual(self.host.calls, [])

    def test_rewrite_replaces_existing_dropin(self):
        self.run_main("enable-user", "bob")
        data = os.path.join(self.home, "d")
        os.makedirs(data)
        rc, out, err = self.run_main("enable-user", "bob", "--datadir=" + data)
        self.assertEqual(rc, 0, err)
        self.assertIn("-datadir=" + data, self.dropin("gridcoinresearchd@bob.service"))
        leftovers = os.listdir(os.path.join(self.paths.etc_systemd, "gridcoinresearchd@bob.service.d"))
        self.assertEqual(leftovers, ["50-gridcoin-service.conf"])


class DisableUserTests(ServiceTestBase):
    def test_disable_user_removes_both_dropins(self):
        self.make_credential("gridcoinresearch-bob.wallet-passphrase")
        self.run_main("enable-user", "bob")
        self.run_main("autounlock", "enable", "bob")
        self.host.calls.clear()
        rc, out, err = self.run_main("disable-user", "bob")
        self.assertEqual(rc, 0, err)
        self.assertEqual(self.host.calls[0], ("disable", "--now", "gridcoinresearchd@bob.service", "gridcoin-autounlock@bob.socket"))
        self.assertEqual(self.host.calls[-1], ("daemon-reload",))
        self.assertEqual(os.listdir(self.paths.etc_systemd), [])

    def test_disable_user_without_dropins_is_ok(self):
        rc, out, err = self.run_main("disable-user", "bob")
        self.assertEqual(rc, 0, err)


class AutounlockTests(ServiceTestBase):
    def test_autounlock_enable_requires_credential(self):
        rc, out, err = self.run_main("autounlock", "enable", "bob")
        self.assertNotEqual(rc, 0)
        self.assertEqual(self.host.calls, [])
        self.assertEqual(os.listdir(self.paths.etc_systemd), [])

    def test_autounlock_dropin_paths(self):
        cases = [
            ("main", ["bob"], "gridcoinresearchd@bob.service", "gridcoin-autounlock@bob.socket",
             "gridcoinresearch-bob.wallet-passphrase", "/run/gridcoin-autounlock/bob.sock"),
            ("test", ["bob"], "gridcointestnetd@bob.service", "gridcointestnet-autounlock@bob.socket",
             "gridcointestnet-bob.wallet-passphrase", "/run/gridcointestnet-autounlock/bob.sock"),
            ("main", ["--system"], "gridcoinresearchd.service", "gridcoin-autounlock.socket",
             "gridcoinresearch.wallet-passphrase", "/run/gridcoin-autounlock/gridcoin.sock"),
            ("test", ["--system"], "gridcointestnetd.service", "gridcointestnet-autounlock.socket",
             "gridcointestnet.wallet-passphrase", "/run/gridcointestnet-autounlock/gridcointestnet.sock"),
        ]
        for network, who, unit, sock, cred, sockpath in cases:
            with self.subTest(network=network, who=who):
                self.host.calls.clear()
                self.make_credential(cred)
                rc, out, err = self.run_main("autounlock", "enable", *who, "--network=" + network)
                self.assertEqual(rc, 0, err)
                text = self.dropin(unit, "60-gridcoin-autounlock.conf")
                self.assertIn("Wants=" + sock, text)
                self.assertIn("After=" + sock, text)
                self.assertIn("Environment=GRIDCOIN_AUTOUNLOCK_ARGS=-autounlocksocket=" + sockpath, text)
                self.assertNotIn("BindReadOnlyPaths", text)
                self.assertIn(("enable", "--now", sock), self.host.calls)
                self.assertNotIn(("restart", "--no-block", unit), self.host.calls)

    def test_autounlock_restarts_active_core_without_blocking(self):
        self.make_credential("gridcoinresearch-bob.wallet-passphrase")
        self.host.active.add("gridcoinresearchd@bob.service")
        rc, out, err = self.run_main("autounlock", "enable", "bob")
        self.assertEqual(rc, 0, err)
        self.assertEqual(self.host.calls[-1], ("restart", "--no-block", "gridcoinresearchd@bob.service"))

    def test_autounlock_warns_on_credential_mode(self):
        self.make_credential("gridcoinresearch-bob.wallet-passphrase", mode=0o644)
        rc, out, err = self.run_main("autounlock", "enable", "bob")
        self.assertEqual(rc, 0, err)
        self.assertIn("0600", out + err)

    def test_system_credential_must_be_where_the_helper_loads_it(self):
        conf = os.path.join(self.tmp.name, "etc-gridcoin")
        os.makedirs(conf)
        legacy = os.path.join(conf, "wallet-passphrase.cred")
        with open(legacy, "w", encoding="utf8") as f:
            f.write("x")
        nets = dict(svc.NETWORKS)
        nets["main"] = dataclasses.replace(svc.NETWORKS["main"], conf_dir=conf)
        saved = svc.NETWORKS
        svc.NETWORKS = nets
        self.addCleanup(lambda: setattr(svc, "NETWORKS", saved))
        rc, out, err = self.run_main("autounlock", "enable", "--system")
        self.assertEqual(rc, 1)
        expected = os.path.join(self.paths.credstore, "gridcoinresearch.wallet-passphrase")
        self.assertIn(expected, err)
        self.assertIn(legacy, err)
        self.assertEqual(self.host.calls, [])
        self.assertEqual(os.listdir(self.paths.etc_systemd), [])
        # The tool does not move the secret itself.
        with open(legacy, encoding="utf8") as f:
            self.assertEqual(f.read(), "x")
        self.assertFalse(os.path.exists(expected))
        # In place, it is accepted.
        self.make_credential("gridcoinresearch.wallet-passphrase")
        rc, out, err = self.run_main("autounlock", "enable", "--system")
        self.assertEqual(rc, 0, err)

    def test_credential_path_is_what_the_helper_units_load(self):
        init_dir = os.path.dirname(TOOL_PATH)
        for net in svc.NETWORKS.values():
            for template, user in (("gridcoin-autounlock.service.in", None),
                                   ("gridcoin-autounlock@.service.in", "%i")):
                with open(os.path.join(init_dir, template), encoding="utf8") as f:
                    text = f.read().replace("@GRC_CRED_PREFIX@", net.cred_prefix)
                loaded = re.findall(r"^LoadCredentialEncrypted=wallet-passphrase:(\S+)$", text, re.M)
                self.assertEqual(loaded, [svc.credential_path(svc.Paths(), net, user)], template)

    def test_autounlock_disable_reverses_and_notes_credential(self):
        self.make_credential("gridcoinresearch-bob.wallet-passphrase")
        self.run_main("autounlock", "enable", "bob")
        self.host.calls.clear()
        rc, out, err = self.run_main("autounlock", "disable", "bob")
        self.assertEqual(rc, 0, err)
        self.assertIn(("disable", "--now", "gridcoin-autounlock@bob.socket"), self.host.calls)
        self.assertFalse(os.path.exists(os.path.join(
            self.paths.etc_systemd, "gridcoinresearchd@bob.service.d", "60-gridcoin-autounlock.conf")))
        self.assertIn("remains", out + err)
        self.assertTrue(os.path.exists(os.path.join(self.paths.credstore, "gridcoinresearch-bob.wallet-passphrase")))

    def test_autounlock_needs_exactly_one_target(self):
        rc, out, err = self.run_main("autounlock", "enable")
        self.assertEqual(rc, 2)
        rc, out, err = self.run_main("autounlock", "enable", "bob", "--system")
        self.assertEqual(rc, 2)


class ConfigTests(ServiceTestBase):
    def test_unconfigured_build_refuses(self):
        svc.CONFIG["bindir"] = "@GRC_BINDIR@"
        rc, out, err = self.run_main("enable-user", "bob")
        self.assertEqual(rc, 64)
        self.assertIn("not configured", err)
        self.assertEqual(self.host.calls, [])

    def test_default_network_comes_from_config(self):
        svc.CONFIG["default_network"] = "test"
        rc, out, err = self.run_main("enable-user", "bob")
        self.assertEqual(rc, 0, err)
        self.assertTrue(os.path.exists(os.path.join(
            self.paths.etc_systemd, "gridcointestnetd@bob.service.d", "50-gridcoin-service.conf")))


class FixRoundTests(ServiceTestBase):
    def failing_host(self, verb, active=()):
        return FakeHost(users=self.host.users, fail=(verb,), active=active)

    def test_enable_user_step_failures_propagate(self):
        for verb in ("daemon-reload", "enable"):
            with self.subTest(verb=verb):
                host = self.failing_host(verb)
                rc, out, err = self.run_main("enable-user", "bob", host=host)
                self.assertEqual(rc, 1)
                self.assertIn(verb, err)
                self.assertIn("kept", err)
                self.assertTrue(os.path.exists(os.path.join(
                    self.paths.etc_systemd, "gridcoinresearchd@bob.service.d", "50-gridcoin-service.conf")))

    def test_autounlock_enable_step_failures_propagate(self):
        self.make_credential("gridcoinresearch-bob.wallet-passphrase")
        for verb in ("daemon-reload", "enable", "restart"):
            with self.subTest(verb=verb):
                host = self.failing_host(verb, active=("gridcoinresearchd@bob.service",))
                rc, out, err = self.run_main("autounlock", "enable", "bob", host=host)
                self.assertEqual(rc, 1)
                self.assertIn(verb, err)

    def test_disable_step_failures_propagate_after_cleanup(self):
        self.run_main("enable-user", "bob")
        host = self.failing_host("disable")
        rc, out, err = self.run_main("disable-user", "bob", host=host)
        self.assertEqual(rc, 1)
        self.assertIn("disable", err)
        self.assertEqual(os.listdir(self.paths.etc_systemd), [])
        rc, out, err = self.run_main("disable-user", "bob", host=self.failing_host("daemon-reload"))
        self.assertEqual(rc, 1)
        self.make_credential("gridcoinresearch-bob.wallet-passphrase")
        self.run_main("autounlock", "enable", "bob")
        rc, out, err = self.run_main("autounlock", "disable", "bob", host=self.failing_host("disable"))
        self.assertEqual(rc, 1)
        self.assertEqual(os.listdir(self.paths.etc_systemd), [])

    def test_dropin_directory_symlink_refused(self):
        target = os.path.join(self.tmp.name, "elsewhere")
        os.makedirs(target)
        os.symlink(target, os.path.join(self.paths.etc_systemd, "gridcoinresearchd@bob.service.d"))
        rc, out, err = self.run_main("enable-user", "bob")
        self.assertEqual(rc, 1)
        self.assertEqual(os.listdir(target), [])
        self.assertEqual(self.host.calls, [])

    def test_removing_from_a_symlinked_dropin_directory_refused(self):
        target = os.path.join(self.tmp.name, "elsewhere")
        os.makedirs(target)
        for name in ("50-gridcoin-service.conf", "60-gridcoin-autounlock.conf"):
            with open(os.path.join(target, name), "w", encoding="utf8") as f:
                f.write("keep")
        os.symlink(target, os.path.join(self.paths.etc_systemd, "gridcoinresearchd@bob.service.d"))
        for argv in (("disable-user", "bob"), ("autounlock", "disable", "bob")):
            rc, out, err = self.run_main(*argv)
            self.assertEqual(rc, 1, argv)
            self.assertIn("refusing", err)
            self.assertEqual(sorted(os.listdir(target)), ["50-gridcoin-service.conf", "60-gridcoin-autounlock.conf"])
        self.assertTrue(os.path.islink(os.path.join(self.paths.etc_systemd, "gridcoinresearchd@bob.service.d")))

    def test_removing_from_a_dropin_path_that_is_not_a_directory_refused(self):
        path = os.path.join(self.paths.etc_systemd, "gridcoinresearchd@bob.service.d")
        with open(path, "w", encoding="utf8") as f:
            f.write("keep")
        rc, out, err = self.run_main("disable-user", "bob")
        self.assertEqual(rc, 1)
        self.assertIn("refusing", err)
        with open(path, encoding="utf8") as f:
            self.assertEqual(f.read(), "keep")

    def test_dropin_directory_created_0755(self):
        self.run_main("enable-user", "bob")
        d = os.path.join(self.paths.etc_systemd, "gridcoinresearchd@bob.service.d")
        self.assertEqual(os.stat(d).st_mode & 0o777, 0o755)

    def test_disable_works_for_deleted_or_low_uid_user(self):
        self.run_main("enable-user", "bob")
        self.make_credential("gridcoinresearch-bob.wallet-passphrase")
        self.run_main("autounlock", "enable", "bob")
        gone = FakeHost(users={})
        rc, out, err = self.run_main("autounlock", "disable", "bob", host=gone)
        self.assertEqual(rc, 0, err)
        rc, out, err = self.run_main("disable-user", "bob", host=gone)
        self.assertEqual(rc, 0, err)
        self.assertEqual(os.listdir(self.paths.etc_systemd), [])
        low = FakeHost(users={"svc": Passwd("svc", 5, "/")})
        rc, out, err = self.run_main("disable-user", "svc", host=low)
        self.assertEqual(rc, 0, err)
        rc, out, err = self.run_main("disable-user", "a/b", host=gone)
        self.assertNotEqual(rc, 0)

    def test_parser_prog_uses_network_tool_name(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
            with self.assertRaises(SystemExit):
                svc.main(["--network=test", "enable-user", "-h"], self.host, self.paths)
        self.assertIn("gridcointestnet-service", out.getvalue())

    def test_write_dropin_temp_is_exclusive(self):
        unit = "gridcoinresearchd@bob.service"
        directory = os.path.join(self.paths.etc_systemd, unit + ".d")
        os.makedirs(directory)
        victim = os.path.join(self.tmp.name, "victim")
        with open(victim, "w", encoding="utf8") as f:
            f.write("keep")
        planted = os.path.join(directory, ".50-gridcoin-service.conf.%d.tmp" % os.getpid())
        os.symlink(victim, planted)
        with self.assertRaises(FileExistsError):
            svc.write_dropin(self.paths, unit, "50-gridcoin-service.conf", "new")
        with open(victim, encoding="utf8") as f:
            self.assertEqual(f.read(), "keep")
        self.assertTrue(os.path.islink(planted))
        self.assertFalse(os.path.exists(os.path.join(directory, "50-gridcoin-service.conf")))

    def test_real_host_uses_fixed_path(self):
        calls = []
        saved = svc.subprocess.call
        svc.subprocess.call = lambda cmd, **kw: calls.append(kw) or 0
        try:
            host = svc.Host()
            host.systemctl("daemon-reload")
            host.is_enabled("x")
            host.is_active("x")
        finally:
            svc.subprocess.call = saved
        self.assertEqual(len(calls), 3)
        for kw in calls:
            self.assertEqual(kw["env"], {"PATH": "/usr/bin:/bin"})


def write_tree(root, files):
    """Create files ({relative path: bytes}) below root, with their directories."""
    for rel, data in files.items():
        path = os.path.join(root, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(data)


def read_tree(root):
    """Return {relative path: bytes} for every regular file and "<dir>/" for every directory below root."""
    found = {}
    for dirpath, dirnames, filenames in os.walk(root):
        rel_dir = os.path.relpath(dirpath, root)
        for name in dirnames:
            found[os.path.normpath(os.path.join(rel_dir, name)) + "/"] = None
        for name in filenames:
            found[os.path.normpath(os.path.join(rel_dir, name))] = read_bytes(os.path.join(dirpath, name))
    return found


def read_bytes(path):
    with open(path, "rb") as f:
        return f.read()


def make_socket_file(path):
    # AF_UNIX paths are limited to about 108 bytes, so bind by a relative name.
    old = os.getcwd()
    os.chdir(os.path.dirname(path))
    try:
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.bind(os.path.basename(path))
        s.close()
    finally:
        os.chdir(old)


def changed_stat(st, **fields):
    """st with the given fields (st_dev, st_ino) changed, as os.stat_result."""
    values = [st.st_mode, st.st_ino, st.st_dev, st.st_nlink, st.st_uid, st.st_gid, st.st_size,
              int(st.st_atime), int(st.st_mtime), int(st.st_ctime)]
    for name, value in fields.items():
        values[["st_mode", "st_ino", "st_dev"].index(name)] = value
    return os.stat_result(values)


def lock_is_held_elsewhere(lock_path):
    """Whether a second process fails to take the core's own lock (F_SETLK through lockf)."""
    code = ("import fcntl, os, sys\n"
            "fd = os.open(sys.argv[1], os.O_RDWR)\n"
            "try:\n"
            "    fcntl.lockf(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)\n"
            "except OSError:\n"
            "    sys.exit(3)\n")
    return subprocess.run([sys.executable, "-I", "-c", code, lock_path]).returncode == 3


def crafted_stream(members):
    """A tar stream with the given (name, type, data) members, written without tarfile's own checks."""
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w|", format=tarfile.PAX_FORMAT) as tar:
        for name, kind, data in members:
            info = tarfile.TarInfo(name)
            info.type = kind
            if kind == tarfile.SYMTYPE or kind == tarfile.LNKTYPE:
                info.linkname = data.decode()
                tar.addfile(info)
            elif kind == tarfile.DIRTYPE:
                tar.addfile(info)
            else:
                info.size = len(data)
                tar.addfile(info, io.BytesIO(data))
    buf.seek(0)
    return buf


class RefusedKeyTests(unittest.TestCase):
    def setUp(self):
        self.patterns = svc.load_refused_patterns(PATTERNS_FILE)

    def test_match_follows_MatchSettingKeyPattern(self):
        # The vectors of setting_key_patterns_match_whole_names (src/test/util_tests.cpp), which
        # pin MatchSettingKeyPattern in src/util/system.cpp.
        vectors = [
            ("*notify", "blocknotify", True),
            ("*notify", "notify", True),
            ("*notify", "notifyx", False),
            ("rpc*", "rpc", True),
            ("clear*history", "clearbeaconhistory", True),
            ("clear*history", "clearbeaconhistoryx", False),
            ("min", "min", True),
            ("min", "mininput", False),
            ("rpc*", "RPCPORT", False),
            ("*", "", True),
            ("a*b*c", "acb", False),
        ]
        for pattern, name, expected in vectors:
            self.assertEqual(svc.match_setting_key_pattern(pattern, name), expected, (pattern, name))

    def test_refused_keys_follow_IsRefusedSettingKey(self):
        # IsRefusedSettingKey (src/util/system.cpp) drops everything up to the first '.' and keeps a
        # leading "no"; the first six are refused_setting_keys_ignore_a_section_prefix_but_not_no
        # (src/test/util_tests.cpp).
        vectors = [
            ("blocknotify", True),
            ("test.blocknotify", True),
            ("rpcpassword", True),
            ("staking", False),
            ("noncruncher", False),
            ("printtoconsole", False),
            ("main.rpcpassword", True),
            ("RPCPASSWORD", False),
            ("noblocknotify", True),
            ("norpcpassword", False),
            ("main.test.blocknotify", True),
            ("x.rpc.y", True),
        ]
        for key, expected in vectors:
            self.assertEqual(svc.is_refused_key(key, self.patterns), expected, key)

    def test_pattern_file_grammar_is_the_build_step_s(self):
        # build-aux/cmake/refusedkeys2header.cmake: '\r' removed, blank and '#' lines skipped,
        # every other line ^[a-z0-9_*-]+$, at least one pattern.
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "keys.txt")
            for text, ok in (("# c\r\nrpc*\r\n\r\n*notify\n", True), ("Rpc*\n", False), ("  # c\nrpc*\n", False),
                             ("rpc* \n", False), ("# only\n\n", False), ("rpc?\n", False)):
                with open(path, "w", encoding="utf8", newline="") as f:
                    f.write(text)
                if ok:
                    self.assertEqual(svc.load_refused_patterns(path), ["rpc*", "*notify"])
                else:
                    with self.assertRaises(svc.UserError, msg=repr(text)):
                        svc.load_refused_patterns(path)
            with self.assertRaises(svc.UserError):
                svc.load_refused_patterns(os.path.join(tmp, "missing.txt"))

    def test_sanitize_settings_strips_what_the_core_refuses(self):
        # ReadSettingsFile (src/util/system.cpp) refuses a key when IsRefusedSettingKey(key) holds, or
        # when the key read through InterpretOption (section and a leading "no" off) is
        # COMMAND_LINE_ONLY; the patterns stand in for those flags (init_tests.cpp).
        text = json.dumps({"blocknotify": "x", "main.walletnotify": "x", "test.rpcport": 1, "noncruncher": True,
                           "noautounlocksocket": True, "test.noipcbind": 1, "norpcpassword": "x", "staking": True,
                           "addnode": ["a", "b"]})
        out, removed = svc.sanitize_settings(text, self.patterns)
        self.assertEqual(removed, ["blocknotify", "main.walletnotify", "test.rpcport", "noautounlocksocket",
                                   "test.noipcbind", "norpcpassword"])
        self.assertEqual(json.loads(out), {"noncruncher": True, "staking": True, "addnode": ["a", "b"]})
        # The core refuses the file on a duplicate key, a non-object, a non-JSON constant or nesting
        # deeper than MAX_JSON_DEPTH (src/univalue/lib/univalue_read.cpp); a number Python reads as
        # infinite cannot be written back as JSON.
        for bad in ('{"a": 1, "a": 2}', "[]", '{"a": NaN}', '{"a": 1e999}', "", "{",
                    '{"a": ' + "[" * 512 + "]" * 512 + "}"):
            with self.assertRaises(ValueError, msg=bad[:40]):
                svc.sanitize_settings(bad, self.patterns)
        with self.assertRaises(RecursionError):
            svc.sanitize_settings('{"a": ' + "[" * 100000 + "]" * 100000 + "}", self.patterns)
        # 512 levels, the object itself included, is what the core still loads.
        deep = '{"a": ' + "[" * 511 + "]" * 511 + "}"
        self.assertEqual(svc.sanitize_settings(deep, self.patterns)[1], [])

    def test_sanitize_conf_follows_the_config_reader(self):
        # GetConfigOptions (src/util/system.cpp) cuts at '#', trims, prefixes "<section>." after a
        # [section] line; InterpretOption splits at the first '.' and reads a leading "no" as negation.
        text = ("# rpcpassword=commented\n"
                "rpcuser=u\n"
                "staking=1\n"
                "noblocknotify=0\n"
                "norpcpassword=1\n"
                "noncruncher=1\n"
                "main.walletnotify=x\n"
                "[test]\n"
                "rpcpassword=hunter2\n"
                "addnode=1.2.3.4\n"
                "  blocknotify = x # trailing\n"
                "rpcSecretAloneOnALine\n"
                "-daemon=1\n")
        out, removed = svc.sanitize_conf(text, self.patterns)
        self.assertEqual(removed, ["rpcuser", "noblocknotify", "norpcpassword", "main.walletnotify",
                                   "test.rpcpassword", "test.blocknotify", "line 12 (not a setting)",
                                   "line 13 (not a setting)"])
        self.assertEqual(out, "# rpcpassword=commented\nstaking=1\nnoncruncher=1\n[test]\naddnode=1.2.3.4\n")
        self.assertNotIn("hunter2", " ".join(removed))
        self.assertNotIn("Secret", " ".join(removed))

    def test_conf_selects_testnet_as_the_core_reads_it(self):
        # Top-level testnet= / notestnet= lines, the last one winning, each value read as
        # InterpretBool (src/util/system.cpp) reads it: empty is true, else ParseInt32 or 0.
        cases = [
            ("testnet=1\n", True), ("testnet=0\n", False), ("testnet=\n", True), ("testnet=abc\n", False),
            ("testnet = 2 # comment\n", True), ("testnet=1\ntestnet=0\n", False), ("notestnet=0\n", True),
            ("notestnet=1\n", False), ("[main]\ntestnet=1\n", False), ("staking=1\n[test]\ntestnet=1\n", False),
            ("main.testnet=1\n", False), ("testnet=+1\n", True), ("testnet=+-1\n", False),
            ("testnet=99999999999\n", False), ("# testnet=1\n", False), ("", False),
        ]
        for text, expected in cases:
            self.assertEqual(svc.conf_selects_testnet(text), expected, repr(text))

    def test_every_command_line_only_example_is_refused(self):
        # The pattern file covers the switch's own concerns: commands, files and secrets.
        for key in ("blocknotify", "walletnotify", "alertnotify", "rpcpassword", "rpcuser", "rpcallowip",
                    "conf", "includeconf", "datadir", "wallet", "walletdir", "autounlocksocket", "ipcbind"):
            self.assertTrue(svc.is_refused_key(key, self.patterns), key)


class CopyTests(ServiceTestBase):
    def setUp(self):
        super().setUp()
        self.src = os.path.join(self.tmp.name, "src")
        self.dst = os.path.join(self.tmp.name, "dst")
        os.makedirs(self.src)
        os.makedirs(self.dst)

    def copy(self, exclude=frozenset()):
        svc.copy_as(self.src, self.dst, "bob", "bob", set(exclude), self.host)

    def test_copy_makes_fresh_inodes(self):
        write_tree(self.src, {"wallet.dat": b"w" * 70000, "a/b/c.dat": b"abc", "a/empty": b"", ".lock": b"",
                              "walletbackups/one": b"1"})
        os.makedirs(os.path.join(self.src, "emptydir"))
        os.link(os.path.join(self.src, "wallet.dat"), os.path.join(self.src, "a", "hardlink.dat"))
        self.copy(exclude={".lock"})
        src_tree = read_tree(self.src)
        del src_tree[".lock"]
        self.assertEqual(read_tree(self.dst), src_tree)
        src_inodes = set()
        for dirpath, dirnames, filenames in os.walk(self.src):
            for name in dirnames + filenames:
                src_inodes.add(os.lstat(os.path.join(dirpath, name)).st_ino)
        dst_inodes = []
        for dirpath, dirnames, filenames in os.walk(self.dst):
            for name in filenames:
                st = os.lstat(os.path.join(dirpath, name))
                self.assertEqual(st.st_mode & 0o777, 0o600, name)
                self.assertEqual(st.st_nlink, 1, name)
                dst_inodes.append(st.st_ino)
            for name in dirnames:
                st = os.lstat(os.path.join(dirpath, name))
                self.assertEqual(st.st_mode & 0o777, 0o700, name)
                dst_inodes.append(st.st_ino)
        self.assertEqual(len(dst_inodes), len(set(dst_inodes)))
        self.assertFalse(src_inodes & set(dst_inodes))

    def test_copy_refuses_symlink_fifo_and_socket(self):
        outside = os.path.join(self.tmp.name, "outside")
        with open(outside, "wb") as f:
            f.write(b"secret")
        makers = {
            "symlink": lambda p: os.symlink(outside, p),
            "dirsymlink": lambda p: os.symlink(self.tmp.name, p),
            "fifo": os.mkfifo,
            "socket": make_socket_file,
        }
        for kind, make in makers.items():
            for where in ("bad", "a/b/bad"):
                with self.subTest(kind=kind, where=where):
                    for d in (self.src, self.dst):
                        svc_rmtree(d)
                        os.makedirs(d)
                    write_tree(self.src, {"a/b/before": b"1", "a/b/z_after": b"2", "z_top": b"3"})
                    write_tree(self.dst, {"keep": b"k"})
                    make(os.path.join(self.src, where))
                    with self.assertRaises(svc.TreeError) as cm:
                        self.copy()
                    self.assertIn(where, str(cm.exception))
                    self.assertNotIn("secret", str(cm.exception))
                    # Nothing from this run is left in the target; what was there stays.
                    self.assertEqual(read_tree(self.dst), {"keep": b"k"})
                    with self.assertRaises(svc.TreeError):
                        svc.stream_tree(self.src, io.BytesIO(), set())

    def test_copy_target_entry_that_exists_fails_and_cleans_up(self):
        write_tree(self.src, {"a/x": b"1", "wallet.dat": b"w"})
        write_tree(self.dst, {"wallet.dat": b"theirs"})
        with self.assertRaises(svc.CopyError):
            self.copy()
        self.assertEqual(read_tree(self.dst), {"wallet.dat": b"theirs"})

    def test_writer_removes_only_what_it_created(self):
        # Something else adds an entry to the target during the copy; the failed writer leaves it.
        write_tree(self.src, {"a/x": b"1", "wallet.dat": b"w"})
        write_tree(self.dst, {"wallet.dat": b"theirs"})
        original = svc.extract_stream
        dst = self.dst

        def with_a_visitor(inp, dst_dir, created=None):
            write_tree(dst, {"visitor": b"v"})
            return original(inp, dst_dir, created)

        with mock.patch.object(svc, "extract_stream", with_a_visitor):
            with self.assertRaises(svc.CopyError):
                self.copy()
        self.assertEqual(read_tree(self.dst), {"wallet.dat": b"theirs", "visitor": b"v"})

    def test_a_writer_that_dies_has_its_entries_removed(self):
        write_tree(self.src, {"a/x": b"1"})
        write_tree(self.dst, {"keep": b"k"})
        dst = self.dst

        def dies(inp, dst_dir, created=None):
            write_tree(dst, {"partial/f": b"1", "visitor": b"v"})
            created.append("partial")
            os._exit(9)

        with mock.patch.object(svc, "extract_stream", dies):
            with self.assertRaisesRegex(svc.CopyError, "without a report.*removed"):
                self.copy()
        self.assertEqual(read_tree(self.dst), {"keep": b"k", "visitor": b"v"})

    def test_extract_rejects_traversal_names(self):
        work = os.path.join(self.tmp.name, "work")
        # Each case and the check that must refuse it. The names are refused by name before any
        # lookup; a later layer (the parent must be a directory of this stream, O_EXCL) would
        # stop most of them too, so the message is what pins the first.
        cases = [
            ([("../x", tarfile.REGTYPE, b"x")], "plain relative name"),
            ([("/abs", tarfile.REGTYPE, b"x")], "plain relative name"),
            ([("a", tarfile.DIRTYPE, b""), ("a/../../x", tarfile.REGTYPE, b"x")], "plain relative name"),
            ([("a", tarfile.DIRTYPE, b""), ("a/..", tarfile.DIRTYPE, b"")], "plain relative name"),
            ([("./x", tarfile.REGTYPE, b"x")], "plain relative name"),
            ([("a", tarfile.DIRTYPE, b""), ("a//x", tarfile.REGTYPE, b"x")], "plain relative name"),
            ([("..", tarfile.DIRTYPE, b"")], "plain relative name"),
            ([("link", tarfile.SYMTYPE, b"../outside")], "only directories and regular files"),
            ([("ok", tarfile.REGTYPE, b"1"), ("hard", tarfile.LNKTYPE, b"ok")], "only directories and regular files"),
            ([("dev", tarfile.CHRTYPE, b"")], "only directories and regular files"),
            ([("fifo", tarfile.FIFOTYPE, b"")], "only directories and regular files"),
            ([("nodir/x", tarfile.REGTYPE, b"x")], "not earlier in the stream"),
        ]
        for members, reason in cases:
            with self.subTest(members=[m[0] for m in members]):
                svc_rmtree(work)
                dst = os.path.join(work, "a", "dst")
                os.makedirs(dst)
                with self.assertRaisesRegex(svc.TreeError, reason):
                    svc.extract_stream(crafted_stream(members), dst)
                self.assertFalse(os.path.lexists(os.path.join(work, "x")))
                self.assertFalse(os.path.lexists(os.path.join(work, "a", "x")))
                for name in ("link", "hard", "dev", "fifo", "nodir"):
                    self.assertFalse(os.path.lexists(os.path.join(dst, name)))
        # A second file of the same name is never written over.
        svc_rmtree(work)
        os.makedirs(os.path.join(work, "dst"))
        with self.assertRaises(FileExistsError):
            svc.extract_stream(crafted_stream([("x", tarfile.REGTYPE, b"1"), ("x", tarfile.REGTYPE, b"2")]),
                               os.path.join(work, "dst"))
        self.assertEqual(read_bytes(os.path.join(work, "dst", "x")), b"1")

    def test_root_reads_only_files_of_the_source_owner(self):
        # Root reads the quarantined files itself, so each must belong to the side it comes from:
        # a hard link to another account's file would otherwise be read with root's rights.
        write_tree(self.src, {"gridcoinresearch.conf": b"staking=1\n", "DB_CONFIG": b"x"})
        src_fd = os.open(self.src, os.O_RDONLY | os.O_DIRECTORY)
        self.addCleanup(os.close, src_fd)
        q_fd = os.open(self.dst, os.O_RDONLY | os.O_DIRECTORY)
        self.addCleanup(os.close, q_fd)
        with self.assertRaisesRegex(svc.TreeError, "owned by uid"):
            svc.quarantine_files(src_fd, self.src, os.getuid() + 1, q_fd)
        self.assertEqual(os.listdir(self.dst), [])
        found, backups = svc.quarantine_files(src_fd, self.src, os.getuid(), q_fd)
        self.assertEqual(found, {"gridcoinresearch.conf": b"staking=1\n", "DB_CONFIG": b"x"})
        self.assertEqual(backups, [])
        self.assertEqual(read_tree(self.dst), found)

    def test_removal_refuses_an_rmtree_that_follows_symlinks(self):
        write_tree(self.dst, {"a/b": b"1", "c": b"2"})
        dst_fd = os.open(self.dst, os.O_RDONLY | os.O_DIRECTORY)
        self.addCleanup(os.close, dst_fd)
        # In a child, as in the tool: the removal changes the working directory.
        with mock.patch.object(svc.shutil.rmtree, "avoids_symlink_attacks", False):
            with self.assertRaisesRegex(svc.CopyError, "can follow symlinks"):
                svc.run_as(self.host, self.host.users["bob"], lambda: svc.empty_dir(dst_fd, set()), (dst_fd,))
        self.assertEqual(read_tree(self.dst), {"a/": None, "a/b": b"1", "c": b"2"})
        svc.run_as(self.host, self.host.users["bob"], lambda: svc.empty_dir(dst_fd, {"c"}), (dst_fd,))
        self.assertEqual(read_tree(self.dst), {"c": b"2"})

    def test_stream_leaves_out_config_backups(self):
        write_tree(self.src, {"walletbackups/gridcoinresearch.conf-x": b"1",
                              "walletbackups/gridcoinsettings.json-y": b"2",
                              "walletbackups/wallet-z.dat": b"3", "gridcoinresearch.conf-x": b"4",
                              "a/walletbackups/gridcoinresearch.conf-x": b"5"})
        buf = io.BytesIO()
        svc.stream_tree(self.src, buf, set())
        buf.seek(0)
        with tarfile.open(fileobj=buf, mode="r|") as tar:
            names = sorted(m.name for m in tar)
        self.assertEqual(names, ["a", "a/walletbackups", "a/walletbackups/gridcoinresearch.conf-x",
                                 "gridcoinresearch.conf-x", "walletbackups", "walletbackups/wallet-z.dat"])

    def test_open_entry_refuses_an_entry_swapped_after_lstat(self):
        write_tree(self.src, {"a": b"1"})
        real_stat = os.stat

        def stale(name, *args, **kwargs):
            st = real_stat(name, *args, **kwargs)
            return changed_stat(st, st_ino=st.st_ino + 1) if name == "a" else st

        src_fd = os.open(self.src, os.O_RDONLY | os.O_DIRECTORY)
        self.addCleanup(os.close, src_fd)
        with mock.patch.object(svc.os, "stat", stale):
            with self.assertRaisesRegex(svc.TreeError, "changed while it was read"):
                svc.open_entry(src_fd, "a", "a")

    def test_stream_refuses_a_sub_mount(self):
        write_tree(self.src, {"blocks/blk0001.dat": b"1", "chainstate/x": b"2"})
        real_stat = os.stat

        def mounted(name, *args, **kwargs):
            st = real_stat(name, *args, **kwargs)
            return changed_stat(st, st_dev=st.st_dev + 1) if name == "chainstate" else st

        with mock.patch.object(svc.os, "stat", mounted):
            with self.assertRaisesRegex(svc.TreeError, "chainstate.*another filesystem"):
                svc.stream_tree(self.src, io.BytesIO(), set())

    def test_create_paths_refuse_an_existing_or_swapped_directory(self):
        dst_fd = os.open(self.dst, os.O_RDONLY | os.O_DIRECTORY)
        self.addCleanup(os.close, dst_fd)
        os.mkdir(os.path.join(self.dst, "there"))
        with self.assertRaisesRegex(svc.UserError, "exists"):
            svc.create_dir("there", dst_fd, "there")
        real_mkdir = os.mkdir

        def swapped(name, mode=0o777, *, dir_fd=None):
            # Made, then replaced at once by a directory with something in it.
            real_mkdir(name, mode, dir_fd=dir_fd)
            os.rename(name, name + ".moved", src_dir_fd=dir_fd, dst_dir_fd=dir_fd)
            real_mkdir(name, 0o700, dir_fd=dir_fd)
            real_mkdir(name + "/inside", 0o700, dir_fd=dir_fd)

        with mock.patch.object(svc.os, "mkdir", swapped):
            with self.assertRaisesRegex(svc.UserError, "replaced"):
                svc.create_dir("new", dst_fd, "new")

    def test_extract_accepts_plain_members_and_counts_bytes(self):
        stream = crafted_stream([("a", tarfile.DIRTYPE, b""), ("a/f", tarfile.REGTYPE, b"12345"),
                                 ("g", tarfile.REGTYPE, b"")])
        self.assertEqual(svc.extract_stream(stream, self.dst), 5)
        self.assertEqual(read_tree(self.dst), {"a/": None, "a/f": b"12345", "g": b""})

    def test_extract_refuses_data_after_the_archive(self):
        stream = crafted_stream([("f", tarfile.REGTYPE, b"1")])
        raw = stream.getvalue() + b"trailing"
        with self.assertRaises(svc.TreeError):
            svc.extract_stream(io.BytesIO(raw), self.dst)


def svc_rmtree(path):
    if os.path.lexists(path):
        shutil.rmtree(path)


def hold_lock_in_other_process(test, lock_path):
    """Hold the core's fcntl lock on lock_path from a second process until the test ends."""
    code = ("import fcntl, os, sys\n"
            "fd = os.open(sys.argv[1], os.O_RDWR)\n"
            "fcntl.lockf(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)\n"
            "print('locked', flush=True)\n"
            "sys.stdin.read()\n")
    proc = subprocess.Popen([sys.executable, "-I", "-c", code, lock_path], stdin=subprocess.PIPE,
                            stdout=subprocess.PIPE)
    test.addCleanup(proc.wait)
    test.addCleanup(proc.stdin.close)
    test.addCleanup(proc.stdout.close)
    test.assertEqual(proc.stdout.readline(), b"locked\n")


class LockTests(ServiceTestBase):
    def test_lock_absent_held_and_symlinked(self):
        d = os.path.join(self.tmp.name, "d")
        os.makedirs(d)
        uid = os.getuid()
        self.assertIsNone(svc.hold_datadir_lock(d, uid))
        lock = os.path.join(d, ".lock")
        open(lock, "wb").close()
        fd = svc.hold_datadir_lock(d, uid)
        self.assertIsInstance(fd, int)
        os.close(fd)
        hold_lock_in_other_process(self, lock)
        with self.assertRaises(svc.UserError):
            svc.hold_datadir_lock(d, uid)
        dir_fd = os.open(d, os.O_RDONLY | os.O_DIRECTORY)
        self.addCleanup(os.close, dir_fd)
        with self.assertRaises(svc.UserError):
            svc.hold_datadir_lock(dir_fd, uid)
        e = os.path.join(self.tmp.name, "e")
        os.makedirs(e)
        os.symlink(lock, os.path.join(e, ".lock"))
        with self.assertRaises(svc.UserError):
            svc.hold_datadir_lock(e, uid)

    def test_lock_is_open_file_description_lock_against_the_cores(self):
        d = os.path.join(self.tmp.name, "d")
        os.makedirs(d)
        lock = os.path.join(d, ".lock")
        open(lock, "wb").close()
        fd = svc.hold_datadir_lock(d, os.getuid())
        self.addCleanup(os.close, fd)
        # The core's F_SETLK (FileLock::TryLock) conflicts with it...
        self.assertTrue(lock_is_held_elsewhere(lock))
        # ...and closing another descriptor of the file here does not drop it, as it would a
        # traditional fcntl lock.
        os.close(os.open(lock, os.O_RDONLY))
        self.assertTrue(lock_is_held_elsewhere(lock))

    def test_lock_file_must_be_the_owners_and_single_linked(self):
        d = os.path.join(self.tmp.name, "d")
        os.makedirs(d)
        lock = os.path.join(d, ".lock")
        open(lock, "wb").close()
        with self.assertRaisesRegex(svc.UserError, "owned"):
            svc.hold_datadir_lock(d, os.getuid() + 1)
        os.link(lock, os.path.join(self.tmp.name, "other-link"))
        with self.assertRaisesRegex(svc.UserError, "link"):
            svc.hold_datadir_lock(d, os.getuid())


class PrivilegeTests(ServiceTestBase):
    def test_drop_order_is_groups_gid_uid(self):
        host = FakeHost()
        svc.drop_privileges(host, Passwd("gridcoin", 990, "/var/lib/gridcoinresearchd", 991))
        self.assertEqual(host.calls, [("setgroups", []), ("setgid", 991), ("setuid", 990)])

    def test_drop_refuses_root_ids(self):
        for pw in (Passwd("x", 0, "/", 991), Passwd("x", 990, "/", 0)):
            host = FakeHost()
            with self.assertRaises(svc.StepError):
                svc.drop_privileges(host, pw)
            self.assertEqual(host.calls, [])

    def test_service_account_must_be_a_system_account(self):
        net = svc.NETWORKS["main"]
        for uid, ok in ((0, False), (1000, False), (1500, False), (999, True), (990, True)):
            host = FakeHost(users={"gridcoin": Passwd("gridcoin", uid, "/var/lib/gridcoinresearchd", 990)})
            if ok:
                self.assertEqual(svc.service_account(net, host, self.paths).pw_uid, uid)
            else:
                with self.assertRaises(svc.UserError, msg=uid):
                    svc.service_account(net, host, self.paths)
        os.unlink(self.paths.login_defs)
        host = FakeHost(users={"gridcoin": Passwd("gridcoin", 1000, "/", 990)})
        with self.assertRaises(svc.UserError):
            svc.service_account(net, host, self.paths)
        with open(self.paths.login_defs, "w", encoding="utf8") as f:
            f.write("UID_MIN 1000\n")
        host = FakeHost(users={"gridcoin": Passwd("gridcoin", 990, "/var/lib/gridcoinresearchd", 0)})
        with self.assertRaisesRegex(svc.UserError, "gid 0"):
            svc.service_account(net, host, self.paths)

    def test_real_host_checks_each_drop_result(self):
        host = svc.Host()
        with mock.patch.object(svc.os, "setgroups"), mock.patch.object(svc.os, "getgroups", return_value=[27]):
            with self.assertRaisesRegex(svc.StepError, "setgroups.*did not take effect"):
                host.setgroups([])
        with mock.patch.object(svc.os, "setgid"), mock.patch.object(svc.os, "getresgid", return_value=(0, 991, 0)):
            with self.assertRaisesRegex(svc.StepError, "setgid.*did not take effect"):
                host.setgid(991)
        with mock.patch.object(svc.os, "setuid", side_effect=[None, PermissionError()]), \
                mock.patch.object(svc.os, "getresuid", return_value=(990, 990, 0)):
            with self.assertRaisesRegex(svc.StepError, "setuid.*did not take effect"):
                host.setuid(990)
        # A drop that could be undone is refused too.
        state = {"uid": 0}

        def fake_setuid(uid):
            state["uid"] = uid

        with mock.patch.object(svc.os, "setuid", side_effect=fake_setuid), \
                mock.patch.object(svc.os, "getresuid", side_effect=lambda: (state["uid"],) * 3):
            with self.assertRaisesRegex(svc.StepError, "could be undone"):
                host.setuid(990)

    def test_copy_as_drops_each_child_to_its_user_when_root(self):
        src = os.path.join(self.tmp.name, "src")
        dst = os.path.join(self.tmp.name, "dst")
        write_tree(src, {"f": b"1"})
        os.makedirs(dst)
        self.host.users["gridcoin"] = Passwd("gridcoin", self.uid, "/", 4321)
        log = os.path.join(self.tmp.name, "drops")

        def record(host, pw):
            with open(log, "a", encoding="utf8") as f:
                f.write("%s %s\n" % (pw.pw_name, pw.pw_gid))

        with mock.patch.object(svc, "drop_privileges", record), mock.patch.object(svc.os, "geteuid", return_value=0):
            svc.copy_as(src, dst, "bob", "gridcoin", set(), self.host)
        with open(log, encoding="utf8") as f:
            self.assertEqual(sorted(f.read().splitlines()), ["bob %d" % os.getgid(), "gridcoin 4321"])
        # Unprivileged, nothing is dropped.
        os.unlink(log)
        svc_rmtree(dst)
        os.makedirs(dst)
        with mock.patch.object(svc, "drop_privileges", record):
            svc.copy_as(src, dst, "bob", "gridcoin", set(), self.host)
        self.assertFalse(os.path.exists(log))

    def test_real_host_leaves_out_its_own_process_tree(self):
        child = subprocess.Popen([sys.executable, "-I", "-c", "import sys; sys.stdin.read()"], stdin=subprocess.PIPE)
        self.addCleanup(child.wait)
        self.addCleanup(child.stdin.close)
        pids = svc.Host().user_processes(os.getuid())
        for pid in (os.getpid(), os.getppid(), child.pid):
            self.assertNotIn(pid, pids)

    def test_own_process_tree_is_not_the_users(self):
        # {pid: (ppid, uids)}: 10 -> 20 (sudo, real uid 1000) -> 30 (the tool) -> 40 (a child);
        # 50 and 60 are the user's other processes, 70 another user's.
        table = {1: (0, [0, 0, 0, 0]), 10: (1, [1000, 1000, 1000, 1000]), 20: (10, [1000, 0, 0, 0]),
                 30: (20, [0, 0, 0, 0]), 40: (30, [1000, 1000, 1000, 1000]), 50: (1, [1000, 1000, 1000, 1000]),
                 60: (50, [0, 1000, 1000, 1000]), 70: (1, [1001, 1001, 1001, 1001])}
        self.assertEqual(svc.foreign_processes(table, 1000, 30), [50, 60])
        self.assertEqual(svc.foreign_processes(table, 1001, 30), [70])
        # A loop in the table (a reused pid) ends the walk.
        self.assertEqual(svc.foreign_processes({5: (6, [1000]), 6: (5, [1000])}, 1000, 30), [5, 6])

    def test_login_uid_from_loginuid_then_sudo(self):
        self.assertEqual(svc.parse_login_uid("1000", None), 1000)
        self.assertEqual(svc.parse_login_uid("0\n", "1000"), 0)
        self.assertEqual(svc.parse_login_uid("4294967295", "1000"), 1000)
        self.assertEqual(svc.parse_login_uid("", "1000"), 1000)
        self.assertIsNone(svc.parse_login_uid("4294967295", None))
        self.assertIsNone(svc.parse_login_uid("4294967295", "x"))
        uid = svc.Host().login_uid()
        self.assertTrue(uid is None or isinstance(uid, int))


class SwitchTestBase(ServiceTestBase):
    ORIGINAL_CONF = b"rpcuser=u\nrpcpassword=SECRETPW\nblocknotify=/bin/evil %s\nstaking=1\n"
    ORIGINAL_SETTINGS = json.dumps({"blocknotify": "evil", "rpcpassword": "SECRETJSON", "staking": True}).encode()

    def setUp(self):
        super().setUp()
        for name in ("gridcoin", "gridcointestnet"):
            self.host.users[name] = Passwd(name, self.uid, os.path.join(self.paths.var_lib, name))
        os.makedirs(self.paths.var_lib)
        saved = svc.NETWORKS
        svc.NETWORKS = {k: dataclasses.replace(v, refused_keys=PATTERNS_FILE) for k, v in saved.items()}
        self.addCleanup(setattr, svc, "NETWORKS", saved)
        self.datadir = os.path.join(self.home, ".GridcoinResearch")
        # The tests run as one uid, which the user (at or above UID_MIN) and the service account
        # (a system account, below it) share. The account's check reads a login.defs that puts
        # that uid below UID_MIN; the check itself is the real one.
        service_defs = os.path.join(self.tmp.name, "login.defs.service")
        with open(service_defs, "w", encoding="utf8") as f:
            f.write("UID_MIN %d\n" % (self.uid + 1))
        self.real_service_account = svc.service_account
        patcher = mock.patch.object(svc, "service_account", lambda net, host, paths: self.real_service_account(
            net, host, dataclasses.replace(paths, login_defs=service_defs)))
        patcher.start()
        self.addCleanup(patcher.stop)

    def user_files(self):
        return {
            "wallet.dat": b"wallet",
            "walletbackups/wallet-1.dat": b"backup",
            "blocks/blk0001.dat": b"\0" * 5000,
            "gridcoinresearch.conf": self.ORIGINAL_CONF,
            "gridcoinsettings.json": self.ORIGINAL_SETTINGS,
            "DB_CONFIG": b"set_lg_dir /tmp\n",
            ".lock": b"",
            "ipc.cookie": b"cookie",
        }

    def target(self, network="main"):
        if network == "main":
            return os.path.join(self.paths.var_lib, "gridcoinresearchd")
        return os.path.join(self.paths.var_lib, "gridcointestnetd", "testnet")

    def quarantine_dirs(self):
        root = os.path.join(self.paths.tool_state, "quarantine")
        return sorted(os.path.join(root, n) for n in os.listdir(root)) if os.path.isdir(root) else []

    def stashes(self, parent, prefix):
        return sorted(os.path.join(parent, n) for n in os.listdir(parent) if n.startswith(prefix))

    def record_path(self, network="main"):
        return os.path.join(self.paths.tool_state, network + ".owner")

    def write_record(self, network="main", user="bob", uid=None, stash=None):
        """The record enable leaves, for tests that set up the service side by hand."""
        os.makedirs(self.paths.tool_state, 0o700, exist_ok=True)
        if stash is None:
            stash = (self.datadir + ".in-service-20260101T000000Z" if network == "main"
                     else os.path.join(self.datadir, "testnet.in-service-20260101T000000Z"))
        fd = os.open(self.record_path(network), os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(fd, "w", encoding="utf8") as f:
            json.dump({"uid": self.uid if uid is None else uid, "user": user, "stash": stash}, f)


class EnableTests(SwitchTestBase):
    def test_enable_quarantines_conf_settings_db_config(self):
        write_tree(self.datadir, self.user_files())
        rc, out, err = self.run_main("enable", "bob")
        self.assertEqual(rc, 0, err)
        t = read_tree(self.target())
        for name in ("gridcoinresearch.conf", "DB_CONFIG", ".lock", "ipc.cookie"):
            self.assertNotIn(name, t)
        self.assertEqual(t["wallet.dat"], b"wallet")
        self.assertEqual(t["walletbackups/wallet-1.dat"], b"backup")
        self.assertEqual(json.loads(t["gridcoinsettings.json"]), {"staking": True})
        [q] = self.quarantine_dirs()
        self.assertTrue(os.path.basename(q).startswith("bob-main-2026"))
        self.assertEqual(os.stat(q).st_mode & 0o777, 0o700)
        quarantined = read_tree(q)
        self.assertEqual(quarantined["gridcoinresearch.conf"], self.ORIGINAL_CONF)
        self.assertEqual(quarantined["gridcoinsettings.json"], self.ORIGINAL_SETTINGS)
        self.assertEqual(quarantined["DB_CONFIG"], b"set_lg_dir /tmp\n")
        self.assertEqual(quarantined["gridcoinresearch.conf.sanitized"], b"staking=1\n")
        text = out + err
        for key in ("blocknotify", "rpcpassword", "rpcuser"):
            self.assertIn(key, text)
        for secret in ("SECRETPW", "SECRETJSON", "evil"):
            self.assertNotIn(secret, text)
        self.assertIn(q, text)
        # What is carried over is named for review too, names only.
        self.assertRegex(out, r"Carried over in gridcoinsettings\.json.*: staking")

    def test_config_backups_are_quarantined_not_copied(self):
        # The core keeps dated copies of both files in walletbackups/ (GetBackupFilename).
        conf_backup = "walletbackups/gridcoinresearch.conf-2026-11-01T00-00-00"
        settings_backup = "walletbackups/gridcoinsettings.json-2026-11-01T00-00-00-manual"
        files = self.user_files()
        files[conf_backup] = b"blocknotify=/bin/evil\n"
        files[settings_backup] = b'{"blocknotify": "evil"}'
        write_tree(self.datadir, files)
        rc, out, err = self.run_main("enable", "bob")
        self.assertEqual(rc, 0, err)
        t = read_tree(self.target())
        self.assertNotIn(conf_backup, t)
        self.assertNotIn(settings_backup, t)
        self.assertEqual(t["walletbackups/wallet-1.dat"], b"backup")
        [q] = self.quarantine_dirs()
        quarantined = read_tree(q)
        self.assertEqual(quarantined[conf_backup], b"blocknotify=/bin/evil\n")
        self.assertEqual(quarantined[settings_backup], b'{"blocknotify": "evil"}')
        self.assertNotIn("evil", out + err)

        # And on the way back.
        write_tree(self.target(), {"walletbackups/gridcoinresearch.conf-2026-11-02T00-00-00": b"walletnotify=x\n",
                                   "walletbackups/gridcoinsettings.json-2026-11-02T00-00-00": b"{}"})
        rc, out, err = self.run_main("disable", "bob")
        self.assertEqual(rc, 0, err)
        s = read_tree(self.datadir)
        self.assertEqual([k for k in s if "-2026-11-02" in k], [])
        self.assertEqual(s["walletbackups/wallet-1.dat"], b"backup")
        back = read_tree(self.quarantine_dirs()[-1])
        self.assertEqual(back["walletbackups/gridcoinresearch.conf-2026-11-02T00-00-00"], b"walletnotify=x\n")
        self.assertEqual(back["walletbackups/gridcoinsettings.json-2026-11-02T00-00-00"], b"{}")

    def test_a_config_backup_that_is_not_a_plain_file_refuses(self):
        write_tree(self.datadir, self.user_files())
        os.symlink(os.path.join(self.tmp.name, "login.defs"),
                   os.path.join(self.datadir, "walletbackups", "gridcoinresearch.conf-2026-11-01T00-00-00"))
        rc, out, err = self.run_main("enable", "bob")
        self.assertEqual(rc, 1)
        self.assertIn("gridcoinresearch.conf-2026-11-01T00-00-00", err)
        self.assertNotIn(("add_to_group", "bob", "gridcoin-gui"), self.host.calls)
        self.assertEqual(os.listdir(self.target()), [])

    def test_enable_stashes_the_source_root_only(self):
        write_tree(self.datadir, self.user_files())
        rc, out, err = self.run_main("enable", "bob")
        self.assertEqual(rc, 0, err)
        self.assertFalse(os.path.lexists(self.datadir))
        [stash] = self.stashes(self.home, ".GridcoinResearch.in-service-")
        self.assertEqual(os.stat(stash).st_mode & 0o777, 0o700)
        self.assertEqual(read_tree(stash), read_tree_from(self.user_files()))
        self.assertIn(stash, out)
        self.assertIn("log out and back in", out)
        self.assertIn("gridcoin-gui", out)

    def test_enable_refuses_nonempty_target(self):
        write_tree(self.datadir, self.user_files())
        write_tree(self.target(), {"wallet.dat": b"someone else's"})
        rc, out, err = self.run_main("enable", "bob")
        self.assertEqual(rc, 1)
        self.assertIn("not empty", err)
        self.assertEqual(self.host.calls, [])
        self.assertEqual(read_tree(self.datadir), read_tree_from(self.user_files()))
        self.assertEqual(read_tree(self.target()), {"wallet.dat": b"someone else's"})

    def test_enable_accepts_an_empty_target(self):
        write_tree(self.datadir, self.user_files())
        os.makedirs(self.target())
        rc, out, err = self.run_main("enable", "bob")
        self.assertEqual(rc, 0, err)
        self.assertIn("wallet.dat", os.listdir(self.target()))

    def test_enable_refuses_running_user_processes(self):
        write_tree(self.datadir, self.user_files())
        self.host.processes = {self.uid: [4242]}
        rc, out, err = self.run_main("enable", "bob")
        self.assertEqual(rc, 1)
        self.assertIn("--terminate-sessions", err)
        self.assertEqual(self.host.calls, [
            ("disable", "--now", "gridcoinresearchd@bob.service", "gridcoin-autounlock@bob.socket"),
            ("stop", "gridcoinresearchd.service"),
        ])
        self.assertFalse(os.path.exists(self.target()))
        self.assertEqual(read_tree(self.datadir), read_tree_from(self.user_files()))

    def test_enable_terminates_sessions_when_asked(self):
        write_tree(self.datadir, self.user_files())
        self.host.processes = {self.uid: [4242]}
        rc, out, err = self.run_main("enable", "bob", "--terminate-sessions")
        self.assertEqual(rc, 0, err)
        calls = self.host.calls
        self.assertLess(calls.index(("stop", "gridcoinresearchd.service")), calls.index(("terminate_user", "bob")))
        self.assertLess(calls.index(("terminate_user", "bob")), calls.index(("add_to_group", "bob", "gridcoin-gui")))
        self.assertIn("wallet.dat", os.listdir(self.target()))

        # Processes that outlive the 30 s wait refuse the command.
        host = FakeHost(users=self.host.users, processes={self.uid: [4242]}, terminate_ends_processes=False)
        write_tree(os.path.join(self.home, ".GridcoinResearch"), {"wallet.dat": b"w"})
        svc_rmtree(self.target())
        os.unlink(self.record_path())
        rc, out, err = self.run_main("enable", "bob", "--terminate-sessions", host=host)
        self.assertEqual(rc, 1)
        self.assertGreaterEqual(host.clock, 30)
        self.assertLess(host.clock, 32)
        self.assertNotIn(("add_to_group", "bob", "gridcoin-gui"), host.calls)
        self.assertFalse(os.path.exists(self.target()))

    def test_enable_stops_user_instance_before_copy(self):
        write_tree(self.datadir, self.user_files())
        calls = self.host.calls
        patches = []
        for name in ("hold_datadir_lock", "copy_as", "stash_source"):
            original = getattr(svc, name)

            def wrapper(*args, _original=original, _name=name, **kwargs):
                calls.append((_name,))
                return _original(*args, **kwargs)

            patches.append(mock.patch.object(svc, name, wrapper))
        with contextlib.ExitStack() as stack:
            for p in patches:
                stack.enter_context(p)
            rc, out, err = self.run_main("enable", "bob")
        self.assertEqual(rc, 0, err)
        self.assertEqual(calls, [
            ("disable", "--now", "gridcoinresearchd@bob.service", "gridcoin-autounlock@bob.socket"),
            ("stop", "gridcoinresearchd.service"),
            ("hold_datadir_lock",),
            ("copy_as",),
            ("stash_source",),
            ("add_to_group", "bob", "gridcoin-gui"),
            ("enable", "--now", "gridcoinresearchd.service"),
        ])

    def test_enable_stops_on_a_failed_systemctl_step(self):
        write_tree(self.datadir, self.user_files())
        for verb in ("disable", "stop", "enable"):
            with self.subTest(verb=verb):
                host = FakeHost(users=self.host.users, fail=(verb,))
                rc, out, err = self.run_main("enable", "bob", host=host)
                self.assertEqual(rc, 1)
                self.assertIn(verb, err)
                self.assertNotIn("drop-ins", err)
                if verb != "enable":
                    self.assertIn("nothing was copied", err)
                    self.assertNotIn(("add_to_group", "bob", "gridcoin-gui"), host.calls)
                    self.assertFalse(os.path.exists(self.target()))
                else:
                    self.assertIn("enable the service once the cause is fixed", err)

    def test_enable_refuses_when_lock_held(self):
        write_tree(self.datadir, self.user_files())
        hold_lock_in_other_process(self, os.path.join(self.datadir, ".lock"))
        rc, out, err = self.run_main("enable", "bob")
        self.assertEqual(rc, 1)
        self.assertIn("lock", err)
        self.assertIn("gridcoinresearchd@bob.service is disabled and gridcoinresearchd.service stopped; nothing was "
                      "copied", err)
        self.assertNotIn(("add_to_group", "bob", "gridcoin-gui"), self.host.calls)
        self.assertEqual(os.listdir(self.target()) if os.path.exists(self.target()) else [], [])
        self.assertEqual(read_tree(self.datadir), read_tree_from(self.user_files()))

    def test_mainnet_enable_leaves_testnet_subdir(self):
        files = self.user_files()
        files["testnet/wallet.dat"] = b"testnet wallet"
        files["testnet.in-service-20260101T000000Z/old"] = b"old stash"
        write_tree(self.datadir, files)
        before = os.stat(os.path.join(self.datadir, "testnet", "wallet.dat"))
        rc, out, err = self.run_main("enable", "bob")
        self.assertEqual(rc, 0, err)
        after = os.stat(os.path.join(self.datadir, "testnet", "wallet.dat"))
        self.assertEqual((before.st_ino, before.st_dev), (after.st_ino, after.st_dev))
        # The conf at the root stays too, as testnet reads it.
        self.assertEqual(sorted(os.listdir(self.datadir)),
                         ["gridcoinresearch.conf", "testnet", "testnet.in-service-20260101T000000Z"])
        self.assertNotIn("testnet/", read_tree(self.target()))
        self.assertFalse(any(k.startswith("testnet") for k in read_tree(self.target())))
        [stash] = self.stashes(self.home, ".GridcoinResearch.in-service-")
        self.assertFalse(any(n.startswith("testnet") for n in os.listdir(stash)))

    def test_testnet_enable_moves_only_testnet(self):
        files = {"gridcoinresearch.conf": b"testnet=1\n", "wallet.dat": b"main wallet"}
        files.update({"testnet/" + k: v for k, v in self.user_files().items()})
        write_tree(self.datadir, files)
        rc, out, err = self.run_main("enable", "bob", "--network=test")
        self.assertEqual(rc, 0, err)
        self.assertIn(("disable", "--now", "gridcointestnetd@bob.service", "gridcointestnet-autounlock@bob.socket"),
                      self.host.calls)
        self.assertEqual(self.host.calls[-1], ("enable", "--now", "gridcointestnetd.service"))
        self.assertIn(("add_to_group", "bob", "gridcointestnet-gui"), self.host.calls)
        self.assertEqual(read_tree(self.target("test"))["wallet.dat"], b"wallet")
        self.assertFalse(os.path.lexists(os.path.join(self.datadir, "testnet")))
        [stash] = self.stashes(self.datadir, "testnet.in-service-")
        self.assertEqual(os.stat(stash).st_mode & 0o777, 0o700)
        # The datadir root, the conf there included, belongs to mainnet and stays.
        self.assertEqual(read_tree(self.datadir)["gridcoinresearch.conf"], b"testnet=1\n")
        self.assertEqual(read_tree(self.datadir)["wallet.dat"], b"main wallet")
        [q] = self.quarantine_dirs()
        self.assertTrue(os.path.basename(q).startswith("bob-test-"))

    def test_enable_refuses_symlinked_datadir_and_special_files(self):
        real = os.path.join(self.home, "real")
        write_tree(real, self.user_files())
        os.symlink(real, self.datadir)
        rc, out, err = self.run_main("enable", "bob")
        self.assertEqual(rc, 1)
        self.assertEqual(self.host.calls, [])
        os.unlink(self.datadir)

        files = self.user_files()
        write_tree(self.datadir, files)
        os.mkfifo(os.path.join(self.datadir, "blocks", "evil"))
        rc, out, err = self.run_main("enable", "bob")
        self.assertEqual(rc, 1)
        self.assertIn(os.path.join("blocks", "evil"), err)
        self.assertNotIn(("enable", "--now", "gridcoinresearchd.service"), self.host.calls)
        self.assertEqual(os.listdir(self.target()), [])
        self.assertIn("wallet.dat", os.listdir(self.datadir))

        os.unlink(os.path.join(self.datadir, "blocks", "evil"))
        os.unlink(os.path.join(self.datadir, "gridcoinsettings.json"))
        os.symlink(os.path.join(self.tmp.name, "login.defs"), os.path.join(self.datadir, "gridcoinsettings.json"))
        rc, out, err = self.run_main("enable", "bob")
        self.assertEqual(rc, 1)
        self.assertIn("gridcoinsettings.json", err)
        self.assertEqual(os.listdir(self.target()), [])

    def test_settings_the_core_cannot_load_refuse_before_the_copy(self):
        cases = {
            "not UTF-8": b'{"rpcpassword": "\xffSECRETBYTES"}',
            "not JSON": b'{"rpcpassword": SECRETBYTES',
            "duplicate": b'{"staking": true, "staking": false}',
            "infinite": b'{"staking": 1e999}',
            "too deep": b'{"a": ' + b"[" * 512 + b"]" * 512 + b"}",
            "recursion": b'{"a": ' + b"[" * 100000 + b"]" * 100000 + b"}",
        }
        copies = []
        original = svc.copy_as
        # One host for all, so that each run's quarantine gets a name of its own.
        host = FakeHost(users=self.host.users)
        for name, text in cases.items():
            with self.subTest(name=name):
                svc_rmtree(self.datadir)
                files = self.user_files()
                files["gridcoinsettings.json"] = text
                write_tree(self.datadir, files)
                with mock.patch.object(svc, "copy_as", lambda *a, **k: copies.append(a) or original(*a, **k)):
                    rc, out, err = self.run_main("enable", "bob", host=host)
                self.assertEqual(rc, 1)
                self.assertIn("gridcoinsettings.json", err)
                self.assertNotIn("SECRETBYTES", out + err)
                self.assertNotIn("0xff", out + err)
                self.assertEqual(copies, [])
                self.assertNotIn(("add_to_group", "bob", "gridcoin-gui"), host.calls)
                self.assertEqual(os.listdir(self.target()), [])
                self.assertEqual(read_tree(self.datadir), read_tree_from(files))

    def test_enable_and_disable_refuse_a_run_from_the_users_own_login(self):
        write_tree(self.datadir, self.user_files())
        host = FakeHost(users=self.host.users, login=self.uid)
        for argv in (("enable", "bob"), ("enable", "bob", "--terminate-sessions"), ("disable", "bob")):
            rc, out, err = self.run_main(*argv, host=host)
            self.assertEqual(rc, 1, argv)
            self.assertIn("run this from another account's login (a root console, ssh as root, or another "
                          "administrator)", err)
        self.assertEqual(host.calls, [])
        self.assertEqual(read_tree(self.datadir), read_tree_from(self.user_files()))
        # Another account's login (root's, here) is fine.
        rc, out, err = self.run_main("enable", "bob", host=FakeHost(users=self.host.users, login=0))
        self.assertEqual(rc, 0, err)

    def test_switch_refuses_while_the_other_networks_user_core_runs(self):
        # Both networks' per-user cores use the same datadir root and conf; the tool never stops
        # the other one itself.
        write_tree(self.datadir, self.user_files())
        write_tree(os.path.join(self.datadir, "testnet"), {"wallet.dat": b"t"})
        for network, other in (("main", "gridcointestnetd@bob.service"), ("test", "gridcoinresearchd@bob.service")):
            with self.subTest(network=network):
                host = FakeHost(users=self.host.users, active=(other,))
                rc, out, err = self.run_main("enable", "bob", "--network=" + network, host=host)
                self.assertEqual(rc, 1)
                self.assertIn("systemctl stop " + other, err)
                self.assertEqual(host.calls, [])
        self.write_record()
        write_tree(self.target(), {"wallet.dat": b"service wallet"})
        svc_rmtree(self.datadir)
        host = FakeHost(users=self.host.users, active=("gridcointestnetd@bob.service",))
        rc, out, err = self.run_main("disable", "bob", host=host)
        self.assertEqual(rc, 1)
        self.assertIn("systemctl stop gridcointestnetd@bob.service", err)
        self.assertEqual(host.calls, [])

    def test_mainnet_enable_refuses_a_node_that_runs_testnet(self):
        files = self.user_files()
        files["gridcoinresearch.conf"] = b"staking=1\ntestnet=1\n"
        write_tree(self.datadir, files)
        rc, out, err = self.run_main("enable", "bob")
        self.assertEqual(rc, 1)
        self.assertIn("runs testnet", err)
        self.assertEqual(self.host.calls, [])
        rc, out, err = self.run_main("enable", "bob", "--network=test")
        self.assertEqual(rc, 1)
        self.assertNotIn("runs testnet", err)

    def test_mainnet_enable_keeps_a_conf_testnet_shares(self):
        files = self.user_files()
        files["testnet/wallet.dat"] = b"testnet wallet"
        write_tree(self.datadir, files)
        conf = os.path.join(self.datadir, "gridcoinresearch.conf")
        before = os.stat(conf)
        rc, out, err = self.run_main("enable", "bob")
        self.assertEqual(rc, 0, err)
        after = os.stat(conf)
        self.assertEqual((before.st_ino, before.st_dev), (after.st_ino, after.st_dev))
        self.assertEqual(sorted(os.listdir(self.datadir)), ["gridcoinresearch.conf", "testnet"])
        [stash] = self.stashes(self.home, ".GridcoinResearch.in-service-")
        self.assertEqual(read_tree(stash)["gridcoinresearch.conf"], self.ORIGINAL_CONF)
        self.assertNotEqual(os.stat(os.path.join(stash, "gridcoinresearch.conf")).st_ino, after.st_ino)
        [q] = self.quarantine_dirs()
        self.assertEqual(read_tree(q)["gridcoinresearch.conf"], self.ORIGINAL_CONF)
        self.assertIn("testnet", out)
        self.assertNotIn("gridcoinresearch.conf", os.listdir(self.target()))

        # disable takes a datadir holding only testnet's entries and that conf, and keeps the conf.
        with open(conf, "ab") as f:
            f.write(b"addnode=1.2.3.4\n")
        rc, out, err = self.run_main("disable", "bob")
        self.assertEqual(rc, 0, err)
        self.assertEqual(read_tree(self.datadir)["gridcoinresearch.conf"], self.ORIGINAL_CONF + b"addnode=1.2.3.4\n")
        self.assertIn("Kept the gridcoinresearch.conf", out)
        self.assertEqual(read_tree(self.datadir)["wallet.dat"], b"wallet")

    def test_shared_conf_removed_meanwhile_comes_back_from_the_stash(self):
        files = self.user_files()
        files["testnet/wallet.dat"] = b"testnet wallet"
        write_tree(self.datadir, files)
        rc, out, err = self.run_main("enable", "bob")
        self.assertEqual(rc, 0, err)
        [stash] = self.stashes(self.home, ".GridcoinResearch.in-service-")
        # The copy belongs to the user, like the entries moved in beside it.
        self.assertEqual(os.stat(os.path.join(stash, "gridcoinresearch.conf")).st_uid, self.uid)
        os.unlink(os.path.join(self.datadir, "gridcoinresearch.conf"))
        rc, out, err = self.run_main("disable", "bob")
        self.assertEqual(rc, 0, err)
        self.assertEqual(read_tree(self.datadir)["gridcoinresearch.conf"], self.ORIGINAL_CONF)
        self.assertIn("restored", out)

    def test_mainnet_disable_refuses_more_than_testnet_and_the_conf(self):
        self.write_record()
        write_tree(self.target(), {"wallet.dat": b"service wallet"})
        write_tree(self.datadir, {"testnet/wallet.dat": b"t", "gridcoinresearch.conf": b"x=1\n", "peers.dat": b"p"})
        rc, out, err = self.run_main("disable", "bob")
        self.assertEqual(rc, 1)
        self.assertEqual(self.host.calls, [])

    def test_enable_refuses_a_per_user_unit_with_its_own_datadir(self):
        write_tree(self.datadir, self.user_files())
        elsewhere = os.path.join(self.home, "elsewhere")
        os.makedirs(elsewhere)
        rc, out, err = self.run_main("enable-user", "bob", "--datadir=" + elsewhere)
        self.assertEqual(rc, 0, err)
        self.host.calls.clear()
        rc, out, err = self.run_main("enable", "bob")
        self.assertEqual(rc, 1)
        self.assertIn(os.path.join(self.paths.etc_systemd, "gridcoinresearchd@bob.service.d",
                                   "50-gridcoin-service.conf"), err)
        self.assertEqual(self.host.calls, [])
        # Without -datadir in it, the drop-in is no obstacle.
        rc, out, err = self.run_main("enable-user", "bob")
        self.assertEqual(rc, 0, err)
        rc, out, err = self.run_main("enable", "bob")
        self.assertEqual(rc, 0, err)

    def test_every_failure_after_the_stop_reports_what_it_left(self):
        def run(host=None):
            svc_rmtree(self.datadir)
            svc_rmtree(self.target())
            for path in self.stashes(self.home, ".GridcoinResearch.in-service-"):
                svc_rmtree(path)
            if os.path.exists(self.record_path()):
                os.unlink(self.record_path())
            write_tree(self.datadir, self.user_files())
            return self.run_main("enable", "bob", host=host or self.host)

        # The writer dies mid-copy: what it made is removed, nothing else changed.
        def dies(inp, dst_dir, created=None):
            os.mkdir("partial", dir_fd=dst_dir)
            created.append("partial")
            os._exit(9)

        with mock.patch.object(svc, "extract_stream", dies):
            rc, out, err = run()
        self.assertEqual(rc, 1)
        self.assertIn("without a report", err)
        self.assertIn("nothing was copied", err)
        self.assertEqual(os.listdir(self.target()), [])
        self.assertIn("wallet.dat", os.listdir(self.datadir))

        # A rename into the stash fails halfway: the message names both places.
        real_rename = os.rename
        renames = []

        def rename(*args, **kwargs):
            renames.append(args)
            if len(renames) == 3:
                raise OSError(5, "Input/output error")
            return real_rename(*args, **kwargs)

        with mock.patch.object(svc.os, "rename", rename):
            rc, out, err = run()
        self.assertEqual(rc, 1)
        [stash] = self.stashes(self.home, ".GridcoinResearch.in-service-")
        self.assertIn("split between %s and %s" % (self.datadir, stash), err)
        self.assertEqual(len(os.listdir(stash)), 2)

        # gpasswd fails: the wallet is in the service datadir, the source in the stash. The same
        # host keeps the clock going, so each run's quarantine has a name of its own.
        self.host.fail.add("add_to_group")
        self.host.calls.clear()
        rc, out, err = run()
        self.assertEqual(rc, 1)
        [stash] = self.stashes(self.home, ".GridcoinResearch.in-service-")
        self.assertIn("the wallet is in %s and the source in %s" % (self.target(), stash), err)
        self.assertIn("not enabled", err)
        self.assertNotIn(("enable", "--now", "gridcoinresearchd.service"), self.host.calls)

    def test_disable_failures_report_what_they_left(self):
        write_tree(self.datadir, self.user_files())
        rc, out, err = self.run_main("enable", "bob")
        self.assertEqual(rc, 0, err)
        self.host.fail.add("remove_from_group")
        rc, out, err = self.run_main("disable", "bob")
        self.assertEqual(rc, 1)
        self.assertIn("the wallet is in %s and %s is empty" % (self.datadir, self.target()), err)
        self.assertIn("gridcoin-gui", err)

    def test_enable_holds_the_lock_for_the_whole_command(self):
        write_tree(self.datadir, self.user_files())
        lock = os.path.join(self.datadir, ".lock")
        seen = []
        originals = {name: getattr(svc, name) for name in ("copy_as", "stash_source", "write_owner_record")}

        def probe(name):
            def wrapper(*args, **kwargs):
                # .lock moves into the stash with the rest of the source.
                path = lock
                if not os.path.exists(lock):
                    path = os.path.join(self.stashes(self.home, ".GridcoinResearch.in-service-")[0], ".lock")
                seen.append((name, lock_is_held_elsewhere(path)))
                return originals[name](*args, **kwargs)
            return wrapper

        with contextlib.ExitStack() as stack:
            for name in originals:
                stack.enter_context(mock.patch.object(svc, name, probe(name)))
            rc, out, err = self.run_main("enable", "bob")
        self.assertEqual(rc, 0, err)
        self.assertEqual(seen, [("copy_as", True), ("stash_source", True), ("write_owner_record", True)])

    def test_enable_refuses_a_datadir_on_its_own_mount(self):
        write_tree(self.datadir, self.user_files())
        inode = os.stat(self.datadir).st_ino
        real_fstat = os.fstat

        def mounted(fd):
            st = real_fstat(fd)
            return changed_stat(st, st_dev=st.st_dev + 1) if st.st_ino == inode else st

        with mock.patch.object(svc.os, "fstat", mounted):
            rc, out, err = self.run_main("enable", "bob")
        self.assertEqual(rc, 1)
        self.assertIn("mount point", err)
        self.assertEqual(self.host.calls, [])

    def test_quarantined_files_over_16_mib_refuse(self):
        write_tree(self.datadir, self.user_files())
        os.truncate(os.path.join(self.datadir, "gridcoinresearch.conf"), 16 * 1024 * 1024 + 1)
        rc, out, err = self.run_main("enable", "bob")
        self.assertEqual(rc, 1)
        self.assertIn("larger than", err)
        self.assertEqual(self.host.calls, [])
        os.truncate(os.path.join(self.datadir, "gridcoinresearch.conf"), 16 * 1024 * 1024)
        os.truncate(os.path.join(self.datadir, "DB_CONFIG"), 16 * 1024 * 1024 + 1)
        rc, out, err = self.run_main("enable", "bob")
        self.assertEqual(rc, 1)
        self.assertIn("DB_CONFIG", err)
        self.assertIn("larger than", err)
        self.assertEqual(os.listdir(self.target()), [])

    def test_stash_refuses_a_swapped_stash_directory(self):
        write_tree(self.datadir, self.user_files())
        real_mkdir = os.mkdir

        def swapped(name, mode=0o777, *, dir_fd=None):
            real_mkdir(name, mode, dir_fd=dir_fd)
            if name.startswith(".GridcoinResearch.in-service-"):
                os.rename(name, name + ".moved", src_dir_fd=dir_fd, dst_dir_fd=dir_fd)
                real_mkdir(name, 0o700, dir_fd=dir_fd)
                fd = os.open(name + "/planted", os.O_WRONLY | os.O_CREAT, 0o600, dir_fd=dir_fd)
                os.close(fd)

        with mock.patch.object(svc.os, "mkdir", swapped):
            rc, out, err = self.run_main("enable", "bob")
        self.assertEqual(rc, 1)
        self.assertIn("replaced", err)
        self.assertEqual(read_tree(self.datadir), read_tree_from(self.user_files()))

    def test_enable_refuses_a_service_account_that_is_not_a_system_account(self):
        write_tree(self.datadir, self.user_files())
        # The plain login.defs: the shared test uid is not below UID_MIN.
        with mock.patch.object(svc, "service_account", self.real_service_account):
            rc, out, err = self.run_main("enable", "bob")
        self.assertEqual(rc, 1)
        self.assertIn("system account", err)
        self.assertEqual(self.host.calls, [])

    def test_enable_refuses_unknown_user_or_missing_datadir(self):
        rc, out, err = self.run_main("enable", "nobody")
        self.assertEqual(rc, 1)
        rc, out, err = self.run_main("enable", "bob")
        self.assertEqual(rc, 1)
        self.assertIn(".GridcoinResearch", err)
        self.assertEqual(self.host.calls, [])


def read_tree_from(files):
    """read_tree's view of a write_tree argument."""
    found = {}
    for rel, data in files.items():
        found[os.path.normpath(rel)] = data
        parent = os.path.dirname(rel)
        while parent:
            found[parent + "/"] = None
            parent = os.path.dirname(parent)
    return found


class DisableTests(SwitchTestBase):
    def test_disable_refuses_existing_user_datadir(self):
        self.write_record()
        self.write_record("test")
        write_tree(self.target(), {"wallet.dat": b"service wallet"})
        write_tree(self.datadir, {"wallet.dat": b"made by a plain GUI meanwhile"})
        rc, out, err = self.run_main("disable", "bob")
        self.assertEqual(rc, 1)
        self.assertIn("exists", err)
        self.assertEqual(self.host.calls, [])
        self.assertEqual(read_tree(self.datadir), {"wallet.dat": b"made by a plain GUI meanwhile"})
        self.assertEqual(read_tree(self.target()), {"wallet.dat": b"service wallet"})

        write_tree(self.target("test"), {"wallet.dat": b"service testnet wallet"})
        os.unlink(os.path.join(self.datadir, "wallet.dat"))
        os.makedirs(os.path.join(self.datadir, "testnet"))
        rc, out, err = self.run_main("disable", "bob", "--network=test")
        self.assertEqual(rc, 1)
        self.assertEqual(self.host.calls, [])
        self.assertEqual(read_tree(self.target("test")), {"wallet.dat": b"service testnet wallet"})

    def test_disable_refuses_without_a_service_datadir(self):
        self.write_record()
        rc, out, err = self.run_main("disable", "bob")
        self.assertEqual(rc, 1)
        self.assertEqual(self.host.calls, [])

    def test_disable_restores_original_conf(self):
        # Without testnet's datadir beside it the conf goes into the stash, and comes back from it.
        write_tree(self.datadir, self.user_files())
        rc, out, err = self.run_main("enable", "bob")
        self.assertEqual(rc, 0, err)
        [stash] = self.stashes(self.home, ".GridcoinResearch.in-service-")
        # The service ran meanwhile and wrote a wallet, settings, a conf and a DB_CONFIG of its own.
        write_tree(self.target(), {
            "wallet.dat": b"wallet v2",
            "gridcoinsettings.json": json.dumps({"guilogfile": "/x", "staking": False}).encode(),
            "gridcoinresearch.conf": b"blocknotify=/bin/evil\n",
            "DB_CONFIG": b"set_lg_dir /x\n",
        })
        self.host.calls.clear()
        rc, out, err = self.run_main("disable", "bob")
        self.assertEqual(rc, 0, err)
        s = read_tree(self.datadir)
        self.assertEqual(s["wallet.dat"], b"wallet v2")
        self.assertEqual(s["walletbackups/wallet-1.dat"], b"backup")
        self.assertEqual(s["gridcoinresearch.conf"], self.ORIGINAL_CONF)
        self.assertEqual(json.loads(s["gridcoinsettings.json"]), {"staking": False})
        self.assertNotIn("DB_CONFIG", s)
        self.assertIn("restored", out)
        self.assertEqual(os.stat(self.datadir).st_mode & 0o777, 0o700)
        self.assertEqual(os.listdir(self.target()), [])
        quarantined = read_tree(self.quarantine_dirs()[-1])
        self.assertEqual(quarantined["gridcoinresearch.conf"], b"blocknotify=/bin/evil\n")
        self.assertEqual(quarantined["DB_CONFIG"], b"set_lg_dir /x\n")
        self.assertEqual(self.host.calls[0], ("disable", "--now", "gridcoinresearchd.service",
                                              "gridcoin-autounlock.socket"))
        self.assertEqual(self.host.calls[-1], ("remove_from_group", "bob", "gridcoin-gui"))
        self.assertIn(stash, out)
        self.assertIn("gridcoin-service enable-user bob", out)
        self.assertIn("guilogfile", out + err)
        self.assertNotIn("evil", out + err)
        # The stash is left for the user to verify against.
        self.assertTrue(os.path.isdir(stash))

    def test_testnet_round_trip(self):
        files = {"gridcoinresearch.conf": b"testnet=1\n", "wallet.dat": b"main wallet"}
        files.update({"testnet/" + k: v for k, v in self.user_files().items()})
        write_tree(self.datadir, files)
        rc, out, err = self.run_main("enable", "bob", "--network=test")
        self.assertEqual(rc, 0, err)
        rc, out, err = self.run_main("disable", "bob", "--network=test")
        self.assertEqual(rc, 0, err)
        s = read_tree(os.path.join(self.datadir, "testnet"))
        # Created again by the tool, owner-only.
        self.assertEqual(os.stat(os.path.join(self.datadir, "testnet")).st_mode & 0o777, 0o700)
        self.assertEqual(s["wallet.dat"], b"wallet")
        self.assertEqual(s["gridcoinresearch.conf"], self.ORIGINAL_CONF)
        self.assertEqual(json.loads(s["gridcoinsettings.json"]), {"staking": True})
        self.assertEqual(os.listdir(self.target("test")), [])
        self.assertEqual(read_bytes(os.path.join(self.datadir, "wallet.dat")), b"main wallet")
        self.assertIn(("remove_from_group", "bob", "gridcointestnet-gui"), self.host.calls)
        self.assertIn("gridcointestnet-service enable-user bob", out)

    def test_failed_disable_leaves_the_user_side_as_it_was(self):
        # A FIFO in the service datadir stops the copy; the directories disable made go again,
        # so a rerun works once root removes it.
        self.write_record("test")
        write_tree(self.target("test"), {"wallet.dat": b"service testnet wallet"})
        write_tree(self.datadir, {"gridcoinresearch.conf": b"x=1\n"})
        fifo = os.path.join(self.target("test"), "evil")
        os.mkfifo(fifo)
        rc, out, err = self.run_main("disable", "bob", "--network=test")
        self.assertEqual(rc, 1)
        self.assertIn("evil", err)
        self.assertEqual(sorted(os.listdir(self.datadir)), ["gridcoinresearch.conf"])
        os.unlink(fifo)
        rc, out, err = self.run_main("disable", "bob", "--network=test")
        self.assertEqual(rc, 0, err)
        self.assertEqual(read_tree(os.path.join(self.datadir, "testnet")), {"wallet.dat": b"service testnet wallet"})

    def test_enable_records_the_user_and_disable_needs_that_record(self):
        write_tree(self.datadir, self.user_files())
        rc, out, err = self.run_main("enable", "bob")
        self.assertEqual(rc, 0, err)
        [stash] = self.stashes(self.home, ".GridcoinResearch.in-service-")
        st = os.stat(self.record_path())
        self.assertEqual(st.st_mode & 0o777, 0o600)
        with open(self.record_path(), encoding="utf8") as f:
            self.assertEqual(json.load(f), {"uid": self.uid, "user": "bob", "stash": stash})
        # A second enable, for anyone, is refused while the record is there.
        write_tree(self.datadir, {"wallet.dat": b"w"})
        self.host.calls.clear()
        rc, out, err = self.run_main("enable", "bob")
        self.assertEqual(rc, 1)
        self.assertIn(self.record_path(), err)
        self.assertEqual(self.host.calls, [])
        svc_rmtree(self.datadir)

        # Another user cannot take the wallet out.
        alice_home = os.path.join(self.tmp.name, "home", "alice")
        os.makedirs(alice_home)
        self.host.users["alice"] = Passwd("alice", self.uid, alice_home)
        rc, out, err = self.run_main("disable", "alice")
        self.assertEqual(rc, 1)
        self.assertIn("bob", err)
        self.assertEqual(self.host.calls, [])
        self.assertEqual(os.listdir(alice_home), [])
        self.assertIn("wallet.dat", os.listdir(self.target()))

        rc, out, err = self.run_main("disable", "bob")
        self.assertEqual(rc, 0, err)
        self.assertFalse(os.path.exists(self.record_path()))

    def test_disable_refuses_without_the_record(self):
        write_tree(self.target(), {"wallet.dat": b"service wallet"})
        rc, out, err = self.run_main("disable", "bob")
        self.assertEqual(rc, 1)
        self.assertIn(self.record_path(), err)
        self.assertEqual(self.host.calls, [])
        # A record that is not root's own 0600 file is not trusted.
        self.write_record()
        os.chmod(self.record_path(), 0o644)
        rc, out, err = self.run_main("disable", "bob")
        self.assertEqual(rc, 1)
        self.assertEqual(self.host.calls, [])
        # Nor is one for a different uid under the same name.
        os.unlink(self.record_path())
        self.write_record(uid=self.uid + 1)
        rc, out, err = self.run_main("disable", "bob")
        self.assertEqual(rc, 1)
        self.assertEqual(self.host.calls, [])

    def test_disable_opens_the_quarantine_before_making_the_users_directories(self):
        self.write_record("test")
        write_tree(self.target("test"), {"wallet.dat": b"service testnet wallet"})

        seen = []

        def fails(*args, **kwargs):
            seen.append(os.path.lexists(self.datadir))
            raise OSError(28, "No space left on device")

        with mock.patch.object(svc, "open_quarantine", fails):
            rc, out, err = self.run_main("disable", "bob", "--network=test")
        self.assertEqual(rc, 1)
        self.assertIn("nothing was copied", err)
        # Nothing was made for the user before the quarantine was there.
        self.assertEqual(seen, [False])
        self.assertFalse(os.path.lexists(self.datadir))

    def test_disable_refuses_running_user_processes(self):
        self.write_record()
        write_tree(self.target(), {"wallet.dat": b"service wallet"})
        self.host.processes = {self.uid: [4242]}
        rc, out, err = self.run_main("disable", "bob")
        self.assertEqual(rc, 1)
        self.assertIn("--terminate-sessions", err)
        self.assertEqual(self.host.calls, [])
        self.assertFalse(os.path.exists(self.datadir))


if __name__ == "__main__":
    unittest.main()
