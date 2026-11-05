#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Tests for gridcoin-service, same-account commands.

Every side effect goes through a FakeHost and a Paths rooted in a temporary
directory. No test runs systemctl, usermod, loginctl or any other host command.
"""

import collections
import contextlib
import dataclasses
import importlib.machinery
import importlib.util
import io
import os
import re
import tempfile
import unittest

TOOL_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "gridcoin_service.py.in")
SYSTEMD_UNITS_CMAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "cmake",
                                   "systemd_units.cmake")


def load_tool(path=TOOL_PATH):
    loader = importlib.machinery.SourceFileLoader("gridcoin_service", path)
    spec = importlib.util.spec_from_loader(loader.name, loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


svc = load_tool()

Passwd = collections.namedtuple("Passwd", "pw_name pw_uid pw_dir")
CONFIGURED = {"bindir": "/usr/bin", "default_network": "main", "python3": "/usr/bin/python3",
              "libexec_subdir_main": "gridcoin", "libexec_subdir_test": "gridcointestnet"}


class FakeHost:
    def __init__(self, users=None, groups=None, root=True, enabled=(), active=(), fail=()):
        self.users = users if users is not None else {}
        self.groups = groups if groups is not None else {}
        self.root = root
        self.fail = set(fail)
        self.enabled = set(enabled)
        self.active = set(active)
        self.calls = []

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


if __name__ == "__main__":
    unittest.main()
