#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Tests for the daemon packages' maintainer scripts (debian/, debian-testnet/).

Each script runs under /bin/sh against a fake root in a temporary directory: every /etc, /var,
/run and /usr path in it is rewritten into that root, and the run is refused if any other absolute
path is left. systemctl, dpkg, chown, chgrp, systemd-sysusers, systemd-tmpfiles and the service
tool are stub executables that log their arguments; PATH holds only those and a few coreutils, so
nothing reaches the host's systemd, dpkg or system directories.
"""

import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

SOURCE_ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
NETWORKS = {
    "main": {"tree": "debian", "package": "gridcoinresearchd", "daemon": "gridcoinresearchd",
             "old_autounlock": "gridcoinresearchd-autounlock", "account": "gridcoin", "conf_dir": "etc/gridcoin",
             "tool": "gridcoin-service", "autounlock": "gridcoin-autounlock"},
    "test": {"tree": "debian-testnet", "package": "gridcointestnetd", "daemon": "gridcointestnetd",
             "old_autounlock": "gridcointestnetd-autounlock", "account": "gridcointestnet",
             "conf_dir": "etc/gridcoin-testnet", "tool": "gridcointestnet-service",
             "autounlock": "gridcointestnet-autounlock"},
}
OLD = "5.5.2.0-1"
NEW = "5.5.2.1-1"
LATER = "5.5.3.0-1"
# The absolute paths a rewritten script may still name: devices it reads or writes to.
ALLOWED_ABSOLUTE = {"/dev/null", "/dev/urandom"}
# The top-level directories of a host: a path into one of them that the rewrite did not take into
# the fake root is refused wherever it stands, after a word character, "-", "." or "$" too
# (cp -t/etc/x).
HOST_TOP = r"(?:etc|var|run|usr|home|root|tmp|srv|opt|lib|lib32|lib64|bin|sbin|boot|proc|sys|mnt|media|dev)"
COREUTILS = ("mkdir", "chmod", "rm", "cat", "mv", "od", "tr", "mktemp", "grep", "ln", "ls", "rmdir", "sed")

# Debian version comparison (dpkg's verrevcmp), for the dpkg stub.
DPKG_COMPARE = r'''
import sys


def order(c):
    if c.isdigit():
        return 0
    if c.isalpha():
        return ord(c)
    if c == "~":
        return -1
    return ord(c) + 256


def verrevcmp(a, b):
    i = j = 0
    while i < len(a) or j < len(b):
        while (i < len(a) and not a[i].isdigit()) or (j < len(b) and not b[j].isdigit()):
            ac = order(a[i]) if i < len(a) else 0
            bc = order(b[j]) if j < len(b) else 0
            if ac != bc:
                return ac - bc
            i += 1
            j += 1
        while i < len(a) and a[i] == "0":
            i += 1
        while j < len(b) and b[j] == "0":
            j += 1
        first = 0
        while i < len(a) and a[i].isdigit() and j < len(b) and b[j].isdigit():
            first = first or ord(a[i]) - ord(b[j])
            i += 1
            j += 1
        if i < len(a) and a[i].isdigit():
            return 1
        if j < len(b) and b[j].isdigit():
            return -1
        if first:
            return first
    return 0


def split(v):
    epoch, _, rest = v.rpartition(":") if ":" in v else ("0", "", v)
    upstream, _, revision = rest.rpartition("-") if "-" in rest else (rest, "", "")
    return int(epoch or 0), upstream, revision


def compare(a, b):
    ea, ua, ra = split(a)
    eb, ub, rb = split(b)
    return (ea > eb) - (ea < eb) or verrevcmp(ua, ub) or verrevcmp(ra, rb)


a, op, b = sys.argv[1:4]
c = compare(a, b)
ok = {"lt": c < 0, "le": c <= 0, "eq": c == 0, "ne": c != 0, "ge": c >= 0, "gt": c > 0}[op]
sys.exit(0 if ok else 1)
'''

# A stub logs its name and arguments to $LOG, fails when its name or one of its arguments is in
# $FAIL (space separated), and otherwise exits 0. systemctl list-units prints $ROOT/active-units.
STUB = '''#!/bin/sh
printf '%%s\\n' "%(name)s $*" >> "$LOG"
for word in %(name)s "$@"; do
    case " $FAIL " in *" $word "*) exit 1 ;; esac
done
%(extra)s
exit 0
'''
STAT_STUB = '''#!/bin/sh
# stat -c %%u <path>: 0 for the paths in $ROOT_OWNED, as if root owned them, and 1000 for every
# other path, whoever really owns it (the suite runs as root in CI images).
if [ "$1" = -c ] && [ "$2" = %%u ]; then
    case " $ROOT_OWNED " in
        *" $3 "*) echo 0 ;;
        *) echo 1000 ;;
    esac
    exit 0
fi
exec %(real)s "$@"
'''
# cp, failing when one of its arguments is in $FAIL.
CP_STUB = '''#!/bin/sh
for word in "$@"; do
    case " $FAIL " in *" $word "*) exit 1 ;; esac
done
exec %(real)s "$@"
'''
# What a real stat answers for %%u when root owns every file: the suite must not depend on it.
ROOT_OWNER_STAT = '''#!/bin/sh
if [ "$1" = -c ] && [ "$2" = %%u ]; then
    echo 0
    exit 0
fi
exec %(real)s "$@"
'''


def outside_paths(text):
    """The absolute paths a rewritten script names outside the fake root, by line: any path into a
    host directory wherever it stands, and any other absolute path after a separator, apart from
    ALLOWED_ABSOLUTE. Comment lines are skipped; paths in the fake root are @ROOT@/..."""
    found = []
    for number, line in enumerate(text.splitlines(), start=1):
        if line.lstrip().startswith("#"):
            continue
        line = re.sub(r"@ROOT@/[^\s\"';|)]*", "@P@", line)
        paths = [re.match(r"/[^\s\"';|)]*", line[m.start():]).group(0)
                 for m in re.finditer(r"/%s(?=/|\b)" % HOST_TOP, line)]
        paths += re.findall(r"(?:^|[\s\"'=<>(|])(/[A-Za-z][^\s\"';|)]*)", line)
        found += [(number, path) for path in paths if path not in ALLOWED_ABSOLUTE]
    return found


class FakeRoot:
    def __init__(self, test, network, real_owner_root=False):
        self.test = test
        self.net = NETWORKS[network]
        self.network = network
        self.tmp = tempfile.TemporaryDirectory()
        test.addCleanup(self.tmp.cleanup)
        self.root = os.path.join(self.tmp.name, "root")
        self.bin = os.path.join(self.tmp.name, "bin")
        self.log = os.path.join(self.tmp.name, "calls.log")
        for d in (self.root, self.bin, self.path("etc/systemd/system"), self.path("var/lib"), self.path("usr/sbin"),
                  self.path(self.net["conf_dir"])):
            os.makedirs(d, exist_ok=True)
        for tool in COREUTILS:
            real = shutil.which(tool, path="/usr/bin:/bin")
            if real is None:
                test.skipTest("%s is not installed" % tool)
            os.symlink(real, os.path.join(self.bin, tool))
        helper = os.path.join(self.tmp.name, "dpkg_compare.py")
        with open(helper, "w", encoding="utf8") as f:
            f.write(DPKG_COMPARE)
        dpkg_extra = '[ "$1" = --compare-versions ] && exec "%s" -I "%s" "$2" "$3" "$4"' % (sys.executable, helper)
        stubs = {
            "systemctl": 'if [ "$1" = list-units ]; then cat "$ROOT/active-units" 2>/dev/null; fi',
            "dpkg": dpkg_extra,
            "chown": "", "chgrp": "", "systemd-sysusers": "", "systemd-tmpfiles": "",
        }
        for name, extra in stubs.items():
            self.write_executable(os.path.join(self.bin, name), STUB % {"name": name, "extra": extra})
        real_stat = shutil.which("stat", path="/usr/bin:/bin")
        if real_owner_root or os.environ.get("GRC_HARNESS_SIMULATE_ROOT"):
            # Behind the stub, a stat that says root owns everything, as it does for a suite run as root.
            stand_in = os.path.join(self.tmp.name, "root-owner-stat")
            self.write_executable(stand_in, ROOT_OWNER_STAT % {"real": real_stat})
            real_stat = stand_in
        self.write_executable(os.path.join(self.bin, "stat"), STAT_STUB % {"real": real_stat})
        self.write_executable(os.path.join(self.bin, "cp"),
                              CP_STUB % {"real": shutil.which("cp", path="/usr/bin:/bin")})
        self.set_tool(0)

    def path(self, rel):
        return os.path.join(self.root, rel)

    @staticmethod
    def write_executable(path, text):
        with open(path, "w", encoding="utf8") as f:
            f.write(text)
        os.chmod(path, 0o755)

    def set_tool(self, rc, removes_state=None):
        """The service tool: logs, and exits rc. With rc 0 it removes the state file, as the real one does
        on success."""
        state = self.state_path()
        removes = (rc == 0) if removes_state is None else removes_state
        body = 'printf "%%s\\n" "%s $*" >> "$LOG"\n' % self.net["tool"]
        if removes:
            body += 'rm -f "%s"\n' % state
        self.write_executable(self.path("usr/sbin/" + self.net["tool"]), "#!/bin/sh\n%sexit %d\n" % (body, rc))

    def state_path(self):
        return self.path("var/lib/gridcoin-service/switchover-%s.state" % self.network)

    def systemd_running(self, running=True):
        d = self.path("run/systemd/system")
        if running:
            os.makedirs(d, exist_ok=True)
        elif os.path.isdir(d):
            os.rmdir(d)

    def active(self, *units):
        with open(self.path("active-units"), "w", encoding="utf8") as f:
            for unit in units:
                f.write("%s loaded active running Gridcoin daemon\n" % unit)

    def link(self, target, unit):
        d = self.path("etc/systemd/system/%s.wants" % target)
        os.makedirs(d, exist_ok=True)
        os.symlink("/usr/lib/systemd/system/" + unit, os.path.join(d, unit))

    def rewrite(self, script):
        source = os.path.join(SOURCE_ROOT, self.net["tree"], "%s.%s" % (self.net["package"], script))
        with open(source, encoding="utf8") as f:
            text = f.read()
        text = re.sub(r"(?<![\w.$/-])/(etc|var|run|usr)/", "@ROOT@/\\1/", text)
        for number, path in outside_paths(text):
            self.test.fail("%s:%d names %s outside the fake root" % (script, number, path))
        return text.replace("@ROOT@", self.root)

    def run(self, script, *args, fail=(), root_owned=()):
        path = os.path.join(self.tmp.name, script)
        with open(path, "w", encoding="utf8") as f:
            f.write(self.rewrite(script))
        if os.path.exists(self.log):
            os.unlink(self.log)
        env = {"PATH": self.bin, "LOG": self.log, "ROOT": self.root, "FAIL": " ".join(fail), "LC_ALL": "C",
               "ROOT_OWNED": " ".join(self.path(p) for p in root_owned), "HOME": self.tmp.name}
        proc = subprocess.run(["/bin/sh", path, *args], env=env, cwd=self.tmp.name, capture_output=True,
                              encoding="utf8")
        return proc.returncode, proc.stdout, proc.stderr

    def calls(self, prefix=""):
        if not os.path.exists(self.log):
            return []
        with open(self.log, encoding="utf8") as f:
            return [line.rstrip("\n") for line in f if line.startswith(prefix)]

    def read(self, rel):
        with open(self.path(rel), encoding="utf8") as f:
            return f.read()


class HarnessTests(unittest.TestCase):
    def test_no_real_command_or_path_is_reachable(self):
        fake = FakeRoot(self, "main")
        names = set(os.listdir(fake.bin))
        self.assertIn("systemctl", names)
        for name in names:
            target = os.path.join(fake.bin, name)
            if name in ("systemctl", "dpkg", "chown", "chgrp", "systemd-sysusers", "systemd-tmpfiles", "stat"):
                self.assertFalse(os.path.islink(target), name)
        for net in NETWORKS:
            for script in ("preinst", "postinst", "prerm", "postrm"):
                text = FakeRoot(self, net).rewrite(script)
                self.assertNotRegex(text, r"(?m)^[^#]*(?<![\w.$/-])/(etc|var|run|usr)/", script)


class GuardTests(unittest.TestCase):
    def test_the_guard_flags_host_paths_wherever_they_stand(self):
        for line in ("cp -t/etc/x y", "a=x/var/lib/y", "echo .$/run/x", "ls $X/usr/bin", "touch /srv/x",
                     "cat \"/home/x\"", "rm -f /tmp/x"):
            self.assertTrue(outside_paths(line + "\n"), line)
        for line in ("echo \"$STATE_DIR/switchover-main.state\"", "rm -f @ROOT@/etc/x @ROOT@/var/lib/y",
                     "# cp -t/etc/x", "cat /dev/null", "od -N 32 /dev/urandom", "for f in \"$UNIT_DIR\"/*.wants/x; do",
                     "rmdir \"${dropin%/*}\""):
            self.assertEqual(outside_paths(line + "\n"), [], line)


class OwnerTests(unittest.TestCase):
    def test_ownership_does_not_come_from_the_real_owner(self):
        # As root, every file of the fake root is root's; the conf is still not "root-owned" unless
        # the test says so.
        fake = FakeRoot(self, "main", real_owner_root=True)
        conf = fake.path("etc/gridcoin/gridcoin.conf")
        with open(conf, "w", encoding="utf8") as f:
            f.write("rpcuser=x\n")
        os.chmod(conf, 0o604)
        rc, out, err = fake.run("postinst", "configure", OLD)
        self.assertEqual(rc, 0, err)
        self.assertEqual(os.stat(conf).st_mode & 0o777, 0o604)
        self.assertEqual(fake.calls("chgrp"), [])


class PreinstTests(unittest.TestCase):
    def test_records_the_old_setup_and_turns_the_old_autounlock_off(self):
        for network in NETWORKS:
            with self.subTest(network=network):
                fake = FakeRoot(self, network)
                net = fake.net
                fake.systemd_running()
                fake.link("multi-user.target", "%s@alice.service" % net["daemon"])
                fake.link("multi-user.target", "%s@.service" % net["daemon"])
                fake.link("%s@alice.service" % net["daemon"], "%s@alice.service" % net["old_autounlock"])
                if network == "main":
                    fake.link("%s.service" % net["daemon"], "%s.service" % net["old_autounlock"])
                os.makedirs(fake.path("etc/systemd/system/%s.service.d" % net["daemon"]))
                override = fake.path("etc/systemd/system/%s.service.d/override.conf" % net["daemon"])
                with open(override, "w", encoding="utf8") as f:
                    f.write("[Service]\nUser=gridcoin\n")
                fake.active("%s@alice.service" % net["daemon"], "%s@carol.service" % net["daemon"])
                rc, out, err = fake.run("preinst", "upgrade", OLD)
                self.assertEqual(rc, 0, err)
                expected = ["autounlock alice", "core alice", "core-active alice", "core-active carol",
                            "system-core"] + (["system-autounlock"] if network == "main" else [])
                state = fake.state_path()
                self.assertEqual(fake.read(state).splitlines(), expected)
                self.assertEqual(os.stat(state).st_mode & 0o777, 0o600)
                self.assertEqual(os.stat(os.path.dirname(state)).st_mode & 0o777, 0o700)
                recorded = state.replace(".state", ".recorded")
                self.assertEqual(fake.read(recorded).splitlines(), ["before no"] + expected)
                disabled = ["systemctl disable %s@alice.service" % net["old_autounlock"]]
                if network == "main":
                    disabled.append("systemctl disable %s.service" % net["old_autounlock"])
                self.assertEqual(fake.calls("systemctl disable"), disabled)
                self.assertEqual(len(fake.calls("systemctl stop")), 1)
                # A second run, a retried upgrade, appends to what is still pending.
                rc, out, err = fake.run("preinst", "upgrade", OLD)
                self.assertEqual(rc, 0, err)
                self.assertEqual(fake.read(state).splitlines(), expected + expected)
                self.assertEqual(fake.read(recorded).splitlines(), ["before yes"] + expected)

    def test_a_failed_copy_of_the_earlier_state_keeps_it(self):
        # The copy of a pending state file fails: the preinst stops before it records or turns
        # anything off, and the abort that follows leaves the pending state as it is.
        fake = FakeRoot(self, "main")
        fake.systemd_running()
        fake.link("gridcoinresearchd@alice.service", "gridcoinresearchd-autounlock@alice.service")
        os.makedirs(os.path.dirname(fake.state_path()))
        with open(fake.state_path(), "w", encoding="utf8") as f:
            f.write("core-active dave\n")
        before = fake.state_path().replace(".state", ".before")
        rc, out, err = fake.run("preinst", "upgrade", OLD, fail=(before,))
        self.assertNotEqual(rc, 0)
        self.assertEqual(fake.calls("systemctl disable"), [])
        rc, out, err = fake.run("postrm", "abort-upgrade", OLD)
        self.assertEqual(rc, 0, err)
        self.assertEqual(fake.read(fake.state_path()), "core-active dave\n")
        self.assertEqual(fake.calls("systemctl"), [])

    def test_a_link_without_its_unit_file_is_removed(self):
        fake = FakeRoot(self, "main")
        fake.link("gridcoinresearchd@bob.service", "gridcoinresearchd-autounlock@bob.service")
        link = fake.path("etc/systemd/system/gridcoinresearchd@bob.service.wants/"
                         "gridcoinresearchd-autounlock@bob.service")
        rc, out, err = fake.run("preinst", "install", OLD, fail=("disable",))
        self.assertEqual(rc, 0, err)
        self.assertFalse(os.path.lexists(link))
        # systemd is not running: nothing is asked of it beyond the files.
        self.assertEqual(fake.calls("systemctl"), ["systemctl disable gridcoinresearchd-autounlock@bob.service"])

    def test_nothing_is_recorded_from_a_new_version_or_on_a_fresh_install(self):
        for network, args in [(n, a) for n in NETWORKS for a in (("upgrade", NEW), ("upgrade", LATER), ("install",))]:
            with self.subTest(network=network, args=args):
                fake = FakeRoot(self, network)
                fake.systemd_running()
                fake.link("%s@alice.service" % fake.net["daemon"], "%s@alice.service" % fake.net["old_autounlock"])
                rc, out, err = fake.run("preinst", *args)
                self.assertEqual(rc, 0, err)
                self.assertFalse(os.path.exists(fake.state_path()))
                self.assertEqual(fake.calls("systemctl"), [])

    def test_nothing_to_record_leaves_no_file(self):
        fake = FakeRoot(self, "main")
        fake.systemd_running()
        rc, out, err = fake.run("preinst", "upgrade", OLD)
        self.assertEqual(rc, 0, err)
        self.assertEqual(os.listdir(fake.path("var/lib/gridcoin-service")), [])


class PostinstTests(unittest.TestCase):
    def conf(self, fake):
        return fake.path(fake.net["conf_dir"] + "/gridcoin.conf")

    def test_creates_the_conf_root_and_service_group_only(self):
        for network in NETWORKS:
            with self.subTest(network=network):
                fake = FakeRoot(self, network)
                rc, out, err = fake.run("postinst", "configure", "")
                self.assertEqual(rc, 0, err)
                conf = self.conf(fake)
                self.assertEqual(os.stat(conf).st_mode & 0o777, 0o640)
                text = fake.read(conf)
                self.assertEqual(len(re.findall(r"(?m)^rpcpassword=[0-9a-f]{64}$", text)), 1)
                self.assertIn("\nrpcuser=gridcoinrpc\n", text)
                self.assertEqual(len(re.findall(r"(?m)^addnode=", text)), 6)
                chgrp = fake.calls("chgrp")
                self.assertEqual(len(chgrp), 1)
                self.assertTrue(chgrp[0].startswith("chgrp %s %s." % (fake.net["account"], conf)), chgrp)
                self.assertEqual(os.listdir(os.path.dirname(conf)), ["gridcoin.conf"])
                self.assertIn("systemd-sysusers %s" % fake.path("usr/lib/sysusers.d/%s.conf" % fake.net["daemon"]),
                              fake.calls("systemd-sysusers"))
                # systemd is not running: no systemctl call.
                self.assertEqual(fake.calls("systemctl"), [])

    def test_a_failure_while_creating_the_conf_leaves_no_temporary_file(self):
        for network in NETWORKS:
            with self.subTest(network=network):
                fake = FakeRoot(self, network)
                rc, out, err = fake.run("postinst", "configure", "", fail=("chgrp",))
                self.assertNotEqual(rc, 0)
                self.assertEqual(os.listdir(os.path.dirname(self.conf(fake))), [])

    def test_regroups_only_a_conf_of_roots(self):
        fake = FakeRoot(self, "test")
        conf = self.conf(fake)
        with open(conf, "w", encoding="utf8") as f:
            f.write("rpcuser=x\n")
        os.chmod(conf, 0o604)
        rc, out, err = fake.run("postinst", "configure", OLD)
        self.assertEqual(rc, 0, err)
        self.assertEqual(os.stat(conf).st_mode & 0o777, 0o604)
        self.assertEqual(fake.calls("chgrp"), [])
        rc, out, err = fake.run("postinst", "configure", OLD, root_owned=(fake.net["conf_dir"] + "/gridcoin.conf",))
        self.assertEqual(rc, 0, err)
        self.assertEqual(os.stat(conf).st_mode & 0o777, 0o640)
        self.assertEqual(fake.calls("chgrp"), ["chgrp gridcointestnet %s" % conf])
        self.assertEqual(fake.read(conf), "rpcuser=x\n")

    def write_state(self, fake, text):
        os.makedirs(os.path.dirname(fake.state_path()), exist_ok=True)
        with open(fake.state_path(), "w", encoding="utf8") as f:
            f.write(text)

    def test_a_failed_switch_over_does_not_fail_the_package(self):
        fake = FakeRoot(self, "main")
        fake.systemd_running()
        fake.set_tool(1)
        self.write_state(fake, "core-active alice\n")
        rc, out, err = fake.run("postinst", "configure", OLD)
        self.assertEqual(rc, 0, err)
        self.assertEqual(fake.calls("gridcoin-service"),
                         ["gridcoin-service switch-over --state=%s" % fake.state_path()])
        self.assertIn("switch-over --state=%s" % fake.state_path(), err)
        self.assertTrue(os.path.exists(fake.state_path()))
        # A daemon-reload that fails does not stop the configuration either.
        rc, out, err = fake.run("postinst", "configure", OLD, fail=("daemon-reload",))
        self.assertEqual(rc, 0, err)

    def test_later_upgrades_restart_running_cores_except_unfinished_ones(self):
        for network in NETWORKS:
            with self.subTest(network=network):
                fake = FakeRoot(self, network)
                fake.systemd_running()
                units = ["%s@%s.service" % (fake.net["daemon"], user) for user in ("alice", "bob", "carol")]
                fake.active(*units)
                restart = ["systemctl try-restart --no-block " + unit for unit in units]
                # bob's switch-over is still unfinished; it is not restarted.
                fake.set_tool(1)
                self.write_state(fake, "core bob\ncore-active bob\n")
                rc, out, err = fake.run("postinst", "configure", NEW)
                self.assertEqual(rc, 0, err)
                self.assertEqual(fake.calls("systemctl try-restart"), [restart[0], restart[2]])
                # A core the switch-over has just handled is not restarted a second time.
                fake.set_tool(0)
                rc, out, err = fake.run("postinst", "configure", LATER)
                self.assertEqual(rc, 0, err)
                self.assertEqual(fake.calls("systemctl try-restart"), [restart[0], restart[2]])
                # No state file: every running core.
                rc, out, err = fake.run("postinst", "configure", LATER)
                self.assertEqual(rc, 0, err)
                self.assertEqual(fake.calls("systemctl try-restart"), restart)
                # Upgrading from a version before the switch-over leaves the restarts to it, and a
                # fresh install restarts nothing.
                for args in (("configure", OLD), ("configure", "")):
                    self.write_state(fake, "core-active alice\n")
                    rc, out, err = fake.run("postinst", *args)
                    self.assertEqual(rc, 0, err)
                    self.assertEqual(fake.calls("systemctl try-restart"), [])

    def test_without_systemd_the_rerun_is_named(self):
        fake = FakeRoot(self, "test")
        self.write_state(fake, "core alice\n")
        rc, out, err = fake.run("postinst", "configure", OLD)
        self.assertEqual(rc, 0, err)
        self.assertEqual(fake.calls("systemctl"), [])
        self.assertEqual(fake.calls(fake.net["tool"]), [])
        self.assertIn("%s switch-over --state=%s" % (fake.net["tool"], fake.state_path()), err)
        self.assertTrue(os.path.exists(fake.state_path()))

    def test_a_failed_configure_drops_the_preinst_record_too(self):
        for network in NETWORKS:
            with self.subTest(network=network):
                fake = FakeRoot(self, network)
                state_dir = fake.path("var/lib/gridcoin-service")
                os.makedirs(state_dir)
                for name in ("recorded", "before"):
                    with open(os.path.join(state_dir, "switchover-%s.%s" % (network, name)), "w", encoding="utf8") as f:
                        f.write("before no\n")
                rc, out, err = fake.run("postinst", "configure", OLD, fail=("chgrp",))
                self.assertNotEqual(rc, 0)
                self.assertEqual(os.listdir(state_dir), [])
                self.assertEqual(os.listdir(fake.path(fake.net["conf_dir"])), [])

    def test_a_finished_configure_drops_the_preinst_record(self):
        fake = FakeRoot(self, "main")
        fake.systemd_running()
        self.write_state(fake, "core alice\n")
        recorded = fake.state_path().replace(".state", ".recorded")
        with open(recorded, "w", encoding="utf8") as f:
            f.write("core alice\n")
        rc, out, err = fake.run("postinst", "configure", OLD)
        self.assertEqual(rc, 0, err)
        self.assertFalse(os.path.exists(recorded))


class AbortTests(unittest.TestCase):
    def test_an_aborted_upgrade_turns_the_old_autounlock_back_on(self):
        for network in NETWORKS:
            with self.subTest(network=network):
                fake = FakeRoot(self, network)
                net = fake.net
                fake.systemd_running()
                fake.link("%s@alice.service" % net["daemon"], "%s@alice.service" % net["old_autounlock"])
                fake.link("multi-user.target", "%s@alice.service" % net["daemon"])
                if network == "main":
                    fake.link("%s.service" % net["daemon"], "%s.service" % net["old_autounlock"])
                # An entry still pending from before stays.
                os.makedirs(os.path.dirname(fake.state_path()))
                with open(fake.state_path(), "w", encoding="utf8") as f:
                    f.write("core-active dave\n")
                os.chmod(fake.state_path(), 0o600)
                rc, out, err = fake.run("preinst", "upgrade", OLD)
                self.assertEqual(rc, 0, err)
                rc, out, err = fake.run("postrm", "abort-upgrade", OLD)
                self.assertEqual(rc, 0, err)
                enabled = ["systemctl enable %s@alice.service" % net["old_autounlock"]]
                if network == "main":
                    enabled.append("systemctl enable %s.service" % net["old_autounlock"])
                self.assertEqual(fake.calls("systemctl enable"), enabled)
                self.assertEqual(fake.read(fake.state_path()), "core-active dave\n")
                self.assertEqual(os.stat(fake.state_path()).st_mode & 0o777, 0o600)
                self.assertEqual(os.listdir(os.path.dirname(fake.state_path())),
                                 [os.path.basename(fake.state_path())])

    def test_an_aborted_install_says_what_was_left_off(self):
        for network in NETWORKS:
            with self.subTest(network=network):
                fake = FakeRoot(self, network)
                fake.systemd_running()
                old = "%s@alice.service" % fake.net["old_autounlock"]
                fake.link("%s@alice.service" % fake.net["daemon"], old)
                rc, out, err = fake.run("preinst", "install", OLD)
                self.assertEqual(rc, 0, err)
                rc, out, err = fake.run("postrm", "abort-install", OLD, fail=("enable",))
                self.assertEqual(rc, 0, err)
                self.assertIn(old, err)
                self.assertIn("left off", err)
                self.assertEqual(os.listdir(fake.path("var/lib/gridcoin-service")), [])

    def test_an_abort_never_drops_a_pending_state_it_cannot_restore(self):
        for network, recorded in [(n, r) for n in NETWORKS for r in ("before yes\n", "")]:
            with self.subTest(network=network, recorded=recorded):
                fake = FakeRoot(self, network)
                fake.systemd_running()
                state_dir = os.path.dirname(fake.state_path())
                os.makedirs(state_dir)
                with open(fake.state_path(), "w", encoding="utf8") as f:
                    f.write("core alice\n")
                # The record says a state file was there before, but its copy is missing; or the
                # record does not say.
                with open(os.path.join(state_dir, "switchover-%s.recorded" % network), "w", encoding="utf8") as f:
                    f.write(recorded + "autounlock alice\n")
                rc, out, err = fake.run("postrm", "abort-upgrade", OLD)
                self.assertEqual(rc, 0, err)
                self.assertEqual(fake.read(fake.state_path()), "core alice\n")
                self.assertIn(fake.state_path(), err)

    def test_an_abort_without_a_record_changes_nothing(self):
        fake = FakeRoot(self, "main")
        fake.systemd_running()
        rc, out, err = fake.run("postrm", "abort-upgrade", OLD)
        self.assertEqual(rc, 0, err)
        self.assertEqual(fake.calls("systemctl"), [])


class RemovalTests(unittest.TestCase):
    def test_prerm_stops_the_per_user_units(self):
        fake = FakeRoot(self, "test")
        fake.systemd_running()
        rc, out, err = fake.run("prerm", "remove")
        self.assertEqual(rc, 0, err)
        self.assertEqual(fake.calls("systemctl"), [
            "systemctl stop gridcointestnetd@*.service gridcointestnet-autounlock@*.socket "
            "gridcointestnet-autounlock@*.service gridcointestnet-autounlock.socket"])
        rc, out, err = fake.run("prerm", "upgrade", NEW)
        self.assertEqual(fake.calls("systemctl"), [])

    def test_purge_removes_only_the_tools_drop_ins_and_state(self):
        for network in NETWORKS:
            with self.subTest(network=network):
                fake = FakeRoot(self, network)
                net = fake.net
                other = "main" if network == "test" else "test"
                state_dir = "var/lib/gridcoin-service/"
                units = "etc/systemd/system/"
                ours = [units + "%s@alice.service.d/50-gridcoin-service.conf" % net["daemon"],
                        units + "%s@alice.service.d/60-gridcoin-autounlock.conf" % net["daemon"],
                        units + "%s@bob.service.d/50-gridcoin-service.conf" % net["daemon"],
                        units + "%s.service.d/60-gridcoin-autounlock.conf" % net["daemon"],
                        units + "%s.service.d/50-gridcoin-service.conf" % net["daemon"],
                        units + "%s.service.d/50-existing-credential.conf" % net["autounlock"],
                        state_dir + "switchover-%s.state" % network,
                        state_dir + "switchover-%s.recorded" % network]
                kept = [units + "%s@bob.service.d/override.conf.disabled-by-gridcoin-switchover" % net["daemon"],
                        units + "%s.service.d/override.conf" % net["daemon"],
                        state_dir + "switchover-%s.state" % other,
                        state_dir + "%s.owner" % network,
                        state_dir + "quarantine/alice-%s-x/gridcoinresearch.conf" % network,
                        net["conf_dir"] + "/gridcoin.conf",
                        net["conf_dir"] + "/wallet-passphrase.cred",
                        "var/lib/%s/wallet.dat" % net["daemon"],
                        "etc/credstore.encrypted/x.wallet-passphrase"]
                for rel in ours + kept:
                    os.makedirs(os.path.dirname(fake.path(rel)), exist_ok=True)
                    with open(fake.path(rel), "w", encoding="utf8") as f:
                        f.write("x")
                rc, out, err = fake.run("postrm", "purge")
                self.assertEqual(rc, 0, err)
                for rel in ours:
                    self.assertFalse(os.path.exists(fake.path(rel)), rel)
                for rel in kept:
                    self.assertTrue(os.path.exists(fake.path(rel)), rel)
                # Emptied drop-in directories go; ones with other files stay.
                self.assertFalse(os.path.exists(fake.path(units + "%s@alice.service.d" % net["daemon"])))
                self.assertFalse(os.path.exists(fake.path(units + "%s.service.d" % net["autounlock"])))
                self.assertTrue(os.path.isdir(fake.path(units + "%s@bob.service.d" % net["daemon"])))
                self.assertTrue(os.path.isdir(fake.path(units + "%s.service.d" % net["daemon"])))
                self.assertEqual(fake.calls("systemctl"), [])


class VersionTests(unittest.TestCase):
    def test_the_first_new_version_is_the_same_everywhere(self):
        # Set at the release cut, together: the preinst and postinst constants and the NEWS entry.
        for net in NETWORKS.values():
            values = set()
            for script in ("preinst", "postinst"):
                source = os.path.join(SOURCE_ROOT, net["tree"], "%s.%s" % (net["package"], script))
                with open(source, encoding="utf8") as f:
                    values.update(re.findall(r"(?m)^FIRST_NEW_VERSION=(\S+)$", f.read()))
            self.assertEqual(len(values), 1, (net["tree"], values))
            first = values.pop()
            self.assertTrue(first.endswith("~"), first)
            with open(os.path.join(SOURCE_ROOT, net["tree"], "%s.NEWS" % net["package"]), encoding="utf8") as f:
                version = re.match(r"\S+ \(([^)]+)\)", f.readline()).group(1)
            self.assertEqual(version.rsplit("-", 1)[0], first[:-1], net["tree"])


class NewsTests(unittest.TestCase):
    def test_news_says_the_system_service_keeps_its_data(self):
        for net in NETWORKS.values():
            with open(os.path.join(SOURCE_ROOT, net["tree"], "%s.NEWS" % net["package"]), encoding="utf8") as f:
                news = " ".join(f.read().split())
            self.assertIn("-datadir", news, net["tree"])
            self.assertIn("never starts on a new datadir", news, net["tree"])


def maintainer_scripts():
    for net in NETWORKS.values():
        for script in ("preinst", "postinst", "prerm", "postrm"):
            yield os.path.join(SOURCE_ROOT, net["tree"], "%s.%s" % (net["package"], script))


class SyntaxTests(unittest.TestCase):
    def test_scripts_parse(self):
        for path in maintainer_scripts():
            with self.subTest(path=os.path.relpath(path, SOURCE_ROOT)):
                self.assertEqual(subprocess.run(["/bin/sh", "-n", path]).returncode, 0)


if __name__ == "__main__":
    unittest.main()
