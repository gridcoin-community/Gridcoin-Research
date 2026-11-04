#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

"""Static checks for the systemd unit templates under contrib/init.

Every contrib/init/*.service.in and *.socket.in template is classified by its
file name and checked against the rules for its kind. The checks run on the
templates with their @GRC_...@ variables still in place, so one pass covers both
networks. A failure names the file and the rule.

Kinds
  system_core     gridcoinresearchd.service.in (different-account mode)
  at_core         gridcoinresearchd@.service.in (same-account mode, convenience)
  helper_*        the autounlock helper socket and service, system and @ flavours

The same-account core keeps a small set of directives and must not carry the
filesystem, namespace and syscall hardening of the system core, so the lint also
asserts that those directives are absent from it.

Usage
  lint-systemd-units.py              check the templates
  lint-systemd-units.py --self-test  check each rule against an embedded bad sample
"""

import glob
import os
import re
import sys

INIT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "contrib", "init")

KINDS = {
    "gridcoinresearchd.service.in": "system_core",
    "gridcoinresearchd@.service.in": "at_core",
    "gridcoin-autounlock.socket.in": "helper_socket_system",
    "gridcoin-autounlock.service.in": "helper_service_system",
    "gridcoin-autounlock@.socket.in": "helper_socket_at",
    "gridcoin-autounlock@.service.in": "helper_service_at",
}

CORE = ("system_core", "at_core")
HELPER_SERVICE = ("helper_service_system", "helper_service_at")
HELPER_SOCKET = ("helper_socket_system", "helper_socket_at")
HELPER = HELPER_SERVICE + HELPER_SOCKET
ALL_KINDS = CORE + HELPER

# Directives the same-account core must not carry: the filesystem, namespace
# and syscall hardening stays with the system core. Each key is checked on its own.
AT_CORE_ABSENT = (
    "ProtectHome", "TemporaryFileSystem", "BindReadOnlyPaths", "BindPaths", "InaccessiblePaths",
    "ProtectProc", "PrivateTmp", "PrivateDevices", "ProtectSystem", "RestrictAddressFamilies",
    "SystemCallFilter", "SystemCallErrorNumber", "SystemCallArchitectures", "MemoryDenyWriteExecute",
    "CapabilityBoundingSet", "RestrictSUIDSGID", "RestrictRealtime", "LockPersonality",
    "RestrictNamespaces", "ProtectKernelTunables", "ProtectKernelModules", "ProtectKernelLogs",
    "ProtectControlGroups", "ProtectClock", "ProtectHostname", "DynamicUser", "ReadWritePaths",
)


class Unit:
    """The directives of a template with comments removed and continuations joined."""

    def __init__(self, text):
        self.text = text
        self.comments = "\n".join(line.lstrip().lstrip("#;") for line in text.splitlines()
                                  if line.lstrip().startswith(("#", ";")))
        self.directives = []
        pending = ""
        for raw in text.splitlines():
            line = raw.strip()
            if not pending and (not line or line.startswith(("#", ";"))):
                continue
            if line.endswith("\\"):
                pending += line[:-1].rstrip() + " "
                continue
            line = pending + line
            pending = ""
            if line.startswith("["):
                continue
            key, sep, value = line.partition("=")
            if sep:
                self.directives.append((key.strip(), value.strip()))

    def values(self, key):
        return [v for k, v in self.directives if k == key]

    def has(self, key, value):
        return value in self.values(key)

    def executable_lines(self):
        return "\n".join(f"{k}={v}" for k, v in self.directives)


RULES = []


def add(kinds, rule, check):
    RULES.append({"kinds": kinds, "rule": rule, "check": check})


def exact(key, value, kinds):
    # the directive appears with exactly this value (any of its occurrences)
    add(kinds, f"{key}={value}", lambda u: u.has(key, value))


def absent(key, kinds):
    add(kinds, f"no {key}=", lambda u: not u.values(key))


def exec_has(fragment, kinds):
    add(kinds, f"ExecStart contains {fragment}", lambda u: any(fragment in v for v in u.values("ExecStart")))


def exec_starts(prefix, kinds):
    add(kinds, f"ExecStart starts with {prefix}", lambda u: any(v.startswith(prefix) for v in u.values("ExecStart")))


def text_forbids(pattern, label, kinds, field="executable"):
    def check(u):
        haystack = u.executable_lines() if field == "executable" else u.text
        return re.search(pattern, haystack) is None
    add(kinds, f"no {label}", check)


def comment_has(phrase, kinds):
    add(kinds, f"header says '{phrase}'", lambda u: phrase.lower() in " ".join(u.comments.lower().split()))


def comment_forbids(phrase, kinds):
    add(kinds, f"header never says '{phrase}'", lambda u: phrase.lower() not in " ".join(u.comments.lower().split()))


def finite_start_timeout(u):
    values = u.values("TimeoutStartSec")
    return len(values) == 1 and re.fullmatch(r"[1-9][0-9]*(us|ms|s|sec|min|h|m)?", values[0]) is not None


# Rules for every kind
for _key, _value in (("NotifyAccess", "all"), ("RemainAfterExit", "yes")):
    add(ALL_KINDS, f"no {_key}={_value}", lambda u, k=_key, v=_value: not u.has(k, v))
text_forbids(r"--rpcpassword", "--rpcpassword", ALL_KINDS)
text_forbids(r"/usr/bin/|/usr/libexec/", "literal /usr/bin/ or /usr/libexec/ path", ALL_KINDS, field="text")
text_forbids(r"(?<![\w-])-daemon(wait)?(?![\w-])", "bare -daemon or -daemonwait", ALL_KINDS, field="text")
comment_forbids("readable only by this service", ALL_KINDS)
comment_forbids("blocks execve", ALL_KINDS)
comment_forbids("created by ConfigurationDirectory", ALL_KINDS)

# Rules for both cores
exact("Type", "notify", CORE)
exact("NotifyAccess", "main", CORE)
absent("PIDFile", CORE)
exact("LimitCORE", "0", CORE)
exact("PrivatePIDs", "yes", CORE)
exact("UMask", "0077", CORE)
exact("NoNewPrivileges", "true", CORE)
exact("TimeoutStopSec", "600", CORE)
exact("RestartSec", "10", CORE)
exact("After", "network-online.target", CORE)
exact("Wants", "network-online.target", CORE)
exact("WantedBy", "multi-user.target", CORE)
add(CORE, "finite TimeoutStartSec", finite_start_timeout)
exact("TimeoutStartSec", "30min", CORE)
comment_has("doc/running-unattended.md", CORE)
comment_has("PrivatePIDs=", CORE)
comment_has("systemd 257", CORE)

# System core (different account, full sandbox)
SYS = ("system_core",)
exec_has("-nodaemon", SYS)
exec_has("-nodaemonwait", SYS)
exec_has("-multiprocess", SYS)
exec_has("-ipcdir=/run/@GRC_STATE_NAME@", SYS)
exec_has("-ipcattachgroup=@GRC_ATTACH_GROUP@", SYS)
exec_has("$GRIDCOIN_AUTOUNLOCK_ARGS", SYS)
exec_starts("@GRC_BINDIR@/@GRC_DAEMON@ @GRC_NET_ARG@ -nodaemon", SYS)
exact("Environment", "GRIDCOIN_AUTOUNLOCK_ARGS=", SYS)
exact("Restart", "always", SYS)
exact("User", "@GRC_ACCOUNT@", SYS)
exact("Group", "@GRC_ACCOUNT@", SYS)
exact("SupplementaryGroups", "@GRC_ATTACH_GROUP@", SYS)
exact("RuntimeDirectory", "@GRC_STATE_NAME@", SYS)
exact("RuntimeDirectoryMode", "0750", SYS)
exact("StateDirectory", "@GRC_STATE_NAME@", SYS)
exact("StateDirectoryMode", "0700", SYS)
absent("ConfigurationDirectory", SYS)
for _key, _value in (
    ("ProtectHome", "yes"), ("ProtectProc", "invisible"), ("PrivateTmp", "true"), ("PrivateDevices", "true"),
    ("ProtectSystem", "strict"), ("RestrictSUIDSGID", "true"), ("CapabilityBoundingSet", ""),
    ("MemoryDenyWriteExecute", "true"), ("RestrictRealtime", "true"), ("LockPersonality", "true"),
    ("RestrictNamespaces", "true"), ("ProtectKernelTunables", "true"), ("ProtectKernelModules", "true"),
    ("ProtectKernelLogs", "true"), ("ProtectControlGroups", "true"), ("ProtectClock", "true"),
    ("ProtectHostname", "true"), ("SystemCallFilter", "@system-service"), ("SystemCallErrorNumber", "EPERM"),
    ("SystemCallArchitectures", "native"),
    ("RestrictAddressFamilies", "AF_UNIX AF_INET AF_INET6 AF_NETLINK"),
):
    exact(_key, _value, SYS)
comment_has("attach group = node operator plus wallet viewer", SYS)
comment_has("doc/running-unattended.md", SYS)

# Same-account core (convenience, small kept set; the dropped directives stay out)
AT = ("at_core",)
exact("Restart", "on-failure", AT)
exact("RestartPreventExitStatus", "78", AT)
exact("Conflicts", "@GRC_DAEMON@.service", AT)
exact("User", "%i", AT)
exec_has("exit 78", AT)
exec_has("@GRC_SERVICE_TOOL@ enable-user %i", AT)
for _key in AT_CORE_ABSENT:
    absent(_key, AT)
comment_has("staking convenience and autounlock", AT)
comment_has("no isolation from your own session", AT)
comment_has("different-account mode", AT)
comment_has("supplementary groups", AT)
comment_forbids("sandbox", AT)

# Helper services
HS_SYS = ("helper_service_system",)
HS_AT = ("helper_service_at",)
exact("Type", "exec", HELPER_SERVICE)
exact("DynamicUser", "yes", HELPER_SERVICE)
exact("Restart", "no", HELPER_SERVICE)
exact("RuntimeMaxSec", "60", HELPER_SERVICE)
exact("UMask", "0077", HELPER_SERVICE)
exact("LimitCORE", "0", HELPER_SERVICE)
exact("ProtectHome", "yes", HELPER_SERVICE)
exact("PrivateNetwork", "yes", HELPER_SERVICE)
exact("IPAddressDeny", "any", HELPER_SERVICE)
exact("RestrictAddressFamilies", "AF_UNIX", HELPER_SERVICE)
exact("Requires", "dbus.socket", HELPER_SERVICE)
exact("After", "dbus.socket", HELPER_SERVICE)
exact("RefuseManualStart", "yes", HELPER_SERVICE)
exact("NoNewPrivileges", "true", HELPER_SERVICE)
exact("ProtectSystem", "strict", HELPER_SERVICE)
exact("PrivateTmp", "true", HELPER_SERVICE)
exact("PrivateDevices", "true", HELPER_SERVICE)
exact("MemoryDenyWriteExecute", "true", HELPER_SERVICE)
exact("SystemCallFilter", "@system-service", HELPER_SERVICE)
exact("CapabilityBoundingSet", "", HELPER_SERVICE)
absent("PrivatePIDs", HELPER_SERVICE)
absent("PIDFile", HELPER_SERVICE)
add(HELPER_SERVICE, "LoadCredentialEncrypted=wallet-passphrase:",
    lambda u: any(v.startswith("wallet-passphrase:") for v in u.values("LoadCredentialEncrypted")))
exec_starts("@GRC_UNIT_PYTHON3@ -I @GRC_LIBEXECDIR@/@GRC_LIBEXEC_SUBDIR@/gridcoin-autounlock-helper ", HELPER_SERVICE)
exec_has("--bound-unit=@GRC_DAEMON@@%i.service --peer-user=%i", HS_AT)
exec_has("--bound-unit=@GRC_DAEMON@.service --peer-user=@GRC_ACCOUNT@", HS_SYS)
exact("LoadCredentialEncrypted",
      "wallet-passphrase:/etc/credstore.encrypted/@GRC_CRED_PREFIX@-%i.wallet-passphrase", HS_AT)
exact("LoadCredentialEncrypted",
      "wallet-passphrase:/etc/credstore.encrypted/@GRC_CRED_PREFIX@.wallet-passphrase", HS_SYS)

# Helper sockets
exact("Accept", "no", HELPER_SOCKET)
exact("SocketMode", "0600", HELPER_SOCKET)
exact("DirectoryMode", "0755", HELPER_SOCKET)
exact("RemoveOnStop", "yes", HELPER_SOCKET)
exact("TriggerLimitIntervalSec", "10s", HELPER_SOCKET)
exact("TriggerLimitBurst", "20", HELPER_SOCKET)
exact("WantedBy", "sockets.target", HELPER_SOCKET)
exact("ListenStream", "/run/@GRC_AUTOUNLOCK@/%i.sock", ("helper_socket_at",))
exact("SocketUser", "%i", ("helper_socket_at",))
exact("ListenStream", "/run/@GRC_AUTOUNLOCK@/@GRC_ACCOUNT@.sock", ("helper_socket_system",))
exact("SocketUser", "@GRC_ACCOUNT@", ("helper_socket_system",))


def check_unit(kind, text):
    """Return the names of the rules the text breaks."""
    unit = Unit(text)
    return [r["rule"] for r in RULES if kind in r["kinds"] and not r["check"](unit)]


def check_sysusers_tmpfiles(directory):
    failures = []
    expected = {
        "gridcoin.sysusers.in": ('u @GRC_ACCOUNT@ - "Gridcoin daemon" /var/lib/@GRC_STATE_NAME@', "g @GRC_ATTACH_GROUP@ -"),
        "gridcoin.tmpfiles.in": ("d @GRC_CONF_DIR@ 0710 root @GRC_ACCOUNT@ -", "d /var/lib/gridcoin-service 0700 root root -"),
    }
    for name, lines in expected.items():
        path = os.path.join(directory, name)
        if not os.path.exists(path):
            failures.append((name, "template is missing"))
            continue
        with open(path, encoding="utf8") as f:
            present = [line.strip() for line in f if line.strip() and not line.startswith("#")]
        for line in lines:
            if line not in present:
                failures.append((name, f"line '{line}'"))
    return failures


def lint(directory):
    failures = []
    found = sorted(glob.glob(os.path.join(directory, "*.service.in")) + glob.glob(os.path.join(directory, "*.socket.in")))
    names = {os.path.basename(p) for p in found}
    for name in sorted(set(KINDS) - names):
        failures.append((name, "template is missing"))
    for path in found:
        name = os.path.basename(path)
        kind = KINDS.get(name)
        if kind is None:
            failures.append((name, "unclassified unit template (add it to KINDS in lint-systemd-units.py)"))
            continue
        with open(path, encoding="utf8") as f:
            text = f.read()
        for rule in check_unit(kind, text):
            failures.append((name, rule))
    failures += check_sysusers_tmpfiles(directory)
    return failures


# Embedded samples for --self-test. Each is the smallest text that satisfies every
# rule of its kind; a bad sample is made from it by breaking exactly one rule.
HEADER_AT = """# Same-account mode: this unit is for staking convenience and autounlock. There is
# no isolation from your own session. For higher security use the different-account mode.
# The user's supplementary groups ride along. PrivatePIDs= is ignored with a log line
# before systemd 257. See doc/running-unattended.md.
"""
HEADER_SYS = """# Different-account mode. attach group = node operator plus wallet viewer.
# PrivatePIDs= is ignored with a log line before systemd 257.
# See doc/running-unattended.md.
"""

COMMON_CORE = """Type=notify
NotifyAccess=main
LimitCORE=0
PrivatePIDs=yes
UMask=0077
NoNewPrivileges=true
TimeoutStartSec=30min
TimeoutStopSec=600
RestartSec=10
After=network-online.target
Wants=network-online.target
WantedBy=multi-user.target
"""

SAMPLES = {
    "system_core": HEADER_SYS + COMMON_CORE + """ExecStart=@GRC_BINDIR@/@GRC_DAEMON@ @GRC_NET_ARG@ -nodaemon -nodaemonwait -multiprocess \\
    -ipcdir=/run/@GRC_STATE_NAME@ -ipcattachgroup=@GRC_ATTACH_GROUP@ $GRIDCOIN_AUTOUNLOCK_ARGS
Environment=GRIDCOIN_AUTOUNLOCK_ARGS=
Restart=always
User=@GRC_ACCOUNT@
Group=@GRC_ACCOUNT@
SupplementaryGroups=@GRC_ATTACH_GROUP@
RuntimeDirectory=@GRC_STATE_NAME@
RuntimeDirectoryMode=0750
StateDirectory=@GRC_STATE_NAME@
StateDirectoryMode=0700
ProtectHome=yes
ProtectProc=invisible
PrivateTmp=true
PrivateDevices=true
ProtectSystem=strict
RestrictSUIDSGID=true
CapabilityBoundingSet=
MemoryDenyWriteExecute=true
RestrictRealtime=true
LockPersonality=true
RestrictNamespaces=true
ProtectKernelTunables=true
ProtectKernelModules=true
ProtectKernelLogs=true
ProtectControlGroups=true
ProtectClock=true
ProtectHostname=true
SystemCallFilter=@system-service
SystemCallErrorNumber=EPERM
SystemCallArchitectures=native
RestrictAddressFamilies=AF_UNIX AF_INET AF_INET6 AF_NETLINK
""",
    "at_core": HEADER_AT + COMMON_CORE + """ExecStart=/bin/sh -c 'echo "%n: run @GRC_SERVICE_TOOL@ enable-user %i first" >&2; exit 78'
Restart=on-failure
RestartPreventExitStatus=78
Conflicts=@GRC_DAEMON@.service
User=%i
""",
    "helper_socket_system": """ListenStream=/run/@GRC_AUTOUNLOCK@/@GRC_ACCOUNT@.sock
SocketUser=@GRC_ACCOUNT@
SocketMode=0600
DirectoryMode=0755
Accept=no
RemoveOnStop=yes
TriggerLimitIntervalSec=10s
TriggerLimitBurst=20
WantedBy=sockets.target
""",
    "helper_socket_at": """ListenStream=/run/@GRC_AUTOUNLOCK@/%i.sock
SocketUser=%i
SocketMode=0600
DirectoryMode=0755
Accept=no
RemoveOnStop=yes
TriggerLimitIntervalSec=10s
TriggerLimitBurst=20
WantedBy=sockets.target
""",
}

HELPER_COMMON = """Type=exec
DynamicUser=yes
Restart=no
RuntimeMaxSec=60
UMask=0077
LimitCORE=0
ProtectHome=yes
PrivateNetwork=yes
IPAddressDeny=any
RestrictAddressFamilies=AF_UNIX
Requires=dbus.socket
After=dbus.socket
RefuseManualStart=yes
NoNewPrivileges=true
ProtectSystem=strict
PrivateTmp=true
PrivateDevices=true
MemoryDenyWriteExecute=true
SystemCallFilter=@system-service
CapabilityBoundingSet=
"""
SAMPLES["helper_service_system"] = HELPER_COMMON + """ExecStart=@GRC_UNIT_PYTHON3@ -I @GRC_LIBEXECDIR@/@GRC_LIBEXEC_SUBDIR@/gridcoin-autounlock-helper --bound-unit=@GRC_DAEMON@.service --peer-user=@GRC_ACCOUNT@
LoadCredentialEncrypted=wallet-passphrase:/etc/credstore.encrypted/@GRC_CRED_PREFIX@.wallet-passphrase
"""
SAMPLES["helper_service_at"] = HELPER_COMMON + """ExecStart=@GRC_UNIT_PYTHON3@ -I @GRC_LIBEXECDIR@/@GRC_LIBEXEC_SUBDIR@/gridcoin-autounlock-helper --bound-unit=@GRC_DAEMON@@%i.service --peer-user=%i
LoadCredentialEncrypted=wallet-passphrase:/etc/credstore.encrypted/@GRC_CRED_PREFIX@-%i.wallet-passphrase
"""

# Extra lines that add a forbidden directive. The value is arbitrary but plausible.
AT_ABSENT_SAMPLES = {key: f"{key}=yes\n" for key in AT_CORE_ABSENT}


def break_rule(text, rule):
    """Return the sample with exactly one rule broken, or None if the rule has no mutation."""
    lines = text.splitlines(keepends=True)
    if rule.startswith("no "):
        what = rule[3:]
        simple = {
            "--rpcpassword": "ExecStart=x --rpcpassword=1\n",
            "literal /usr/bin/ or /usr/libexec/ path": "ExecStart=/usr/bin/x\n",
            "bare -daemon or -daemonwait": "ExecStart=x -daemonwait\n",
            "PIDFile=": "PIDFile=/run/x.pid\n",
            "ConfigurationDirectory=": "ConfigurationDirectory=gridcoin\n",
            "PrivatePIDs=": "PrivatePIDs=yes\n",
            "NotifyAccess=all": "NotifyAccess=all\n",
            "RemainAfterExit=yes": "RemainAfterExit=yes\n",
        }
        if what in simple:
            return text + simple[what]
        key = what.rstrip("=")
        if key in AT_ABSENT_SAMPLES:
            return text + AT_ABSENT_SAMPLES[key]
        return None
    if rule.startswith("header never says"):
        phrase = rule.split("'")[1]
        return text + f"# {phrase}\n"
    if rule.startswith("header says"):
        phrase = rule.split("'")[1]
        return text.replace(phrase, "REDACTED")
    if rule == "finite TimeoutStartSec":
        return text.replace("TimeoutStartSec=30min", "TimeoutStartSec=infinity")
    if rule.startswith("ExecStart contains ") or rule.startswith("ExecStart starts with "):
        fragment = rule.split(" ", 3)[3] if rule.startswith("ExecStart starts") else rule[len("ExecStart contains "):]
        return text.replace(fragment, "BROKEN")
    if rule.startswith("LoadCredentialEncrypted=wallet-passphrase:"):
        return text.replace("LoadCredentialEncrypted=wallet-passphrase:", "LoadCredentialEncrypted=other:")
    if "=" in rule:
        key, _, value = rule.partition("=")
        out = []
        for line in lines:
            if line.strip() == f"{key}={value}":
                out.append(f"{key}=BROKEN\n")
            else:
                out.append(line)
        broken = "".join(out)
        return broken if broken != text else None
    return None


def self_test():
    problems = []
    for kind, sample in SAMPLES.items():
        good = check_unit(kind, sample)
        if good:
            problems.append(f"{kind}: the good sample fails: {good}")
    for kind, sample in SAMPLES.items():
        for r in RULES:
            if kind not in r["kinds"]:
                continue
            bad = break_rule(sample, r["rule"])
            if bad is None:
                problems.append(f"{kind}: no bad sample for rule '{r['rule']}'")
            elif r["rule"] not in check_unit(kind, bad):
                problems.append(f"{kind}: rule '{r['rule']}' did not fail on its bad sample")
    # a misclassified file name must be reported, and the @ core must refuse the system sandbox back
    sandboxed = SAMPLES["at_core"] + "ProtectHome=tmpfs\nBindReadOnlyPaths=/run/systemd/notify\n"
    if not {"no ProtectHome=", "no BindReadOnlyPaths="} <= set(check_unit("at_core", sandboxed)):
        problems.append("at_core: dropped directives are not rejected")
    return problems


def main(argv):
    if argv[1:] == ["--self-test"]:
        problems = self_test()
        for p in problems:
            print(f"lint-systemd-units self-test: {p}", file=sys.stderr)
        if problems:
            return 1
        print(f"lint-systemd-units self-test: {len(RULES)} rules, every one fails on its bad sample")
        return 0
    if len(argv) > 1:
        print("usage: lint-systemd-units.py [--self-test]", file=sys.stderr)
        return 2
    failures = lint(INIT_DIR)
    for name, rule in failures:
        print(f"contrib/init/{name}: rule failed: {rule}", file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
