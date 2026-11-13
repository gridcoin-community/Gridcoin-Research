#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Check what a deployed Gridcoin.app is signed with and where it loads code from.

    check-macos-bundle.py --app PATH [--release --team-id ID] [--allow-gui-jit]

Every Mach-O file in the bundle is measured with codesign and otool, so this
runs on macOS only. It fails when:

  * a file carries an entitlement that relaxes the hardened runtime, or one
    that needs a provisioning profile the bundle does not have;
  * a file names a library, or a library search path, that resolves outside
    the bundle and the system;
  * a file's signature does not verify;
  * with --release, a file is not signed by the given team or lacks the
    hardened runtime.

The rules are plain functions over values, tested by
test-check-macos-bundle.py without Apple's tools.
"""

import argparse
import os
import plistlib
import re
import subprocess
import sys
from typing import Dict, List, Optional, Tuple

#: Entitlements that relax the hardened runtime. No file in the bundle may
#: carry one.
FORBIDDEN = ("com.apple.security.cs.disable-library-validation",
             "com.apple.security.cs.allow-dyld-environment-variables",
             "com.apple.security.cs.allow-unsigned-executable-memory",
             "com.apple.security.get-task-allow",
             "com.apple.security.cs.debugger",
             "com.apple.security.cs.disable-executable-page-protection")

#: Entitlements the system honours only with a matching provisioning profile.
#: The bundle embeds none, and a program that claims one without it is killed
#: at launch.
RESTRICTED = ("com.apple.application-identifier",
              "com.apple.developer.team-identifier",
              "keychain-access-groups")

JIT = "com.apple.security.cs.allow-jit"

GUI_PATH = "Contents/MacOS/gridcoinresearch"
DAEMON_PATH = "Contents/Helpers/gridcoinresearchd"

#: Where an absolute load command may point.
SYSTEM_PREFIXES = ("/usr/lib/", "/System/Library/")

#: The directories of the bundle's executables, which @executable_path can
#: stand for in a library either of them loads.
EXECUTABLE_DIRS = (os.path.dirname(GUI_PATH), os.path.dirname(DAEMON_PATH))

MACHO_MAGICS = (b"\xfe\xed\xfa\xce", b"\xce\xfa\xed\xfe", b"\xfe\xed\xfa\xcf", b"\xcf\xfa\xed\xfe",
                b"\xca\xfe\xba\xbe", b"\xbe\xba\xfe\xca")


def role_of(app: str, macho: str) -> str:
    """Return "gui", "daemon" or "other" for a Mach-O file of the bundle."""
    relative = os.path.relpath(macho, app)
    if relative == GUI_PATH:
        return "gui"
    if relative == DAEMON_PATH:
        return "daemon"
    return "other"


def check_entitlements(role: str, ents: Dict[str, object], allow_gui_jit: bool) -> List[str]:
    failures = []
    for key in FORBIDDEN:
        if ents.get(key):
            failures.append("%s carries the entitlement %s" % (role, key))
    for key in RESTRICTED:
        if key in ents:
            failures.append("%s claims %s, which needs a provisioning profile" % (role, key))
    if ents.get(JIT) and not (role == "gui" and allow_gui_jit):
        failures.append("%s carries the entitlement %s" % (role, JIT))
    return failures


def is_inside(root: str, path: str) -> bool:
    root = os.path.normpath(root)
    path = os.path.normpath(path)
    return path == root or path.startswith(root + "/")


def resolve(app: str, macho: str, path: str) -> Optional[List[str]]:
    """Return the places a @loader_path or @executable_path reference can name.

    None means the path starts with neither. @loader_path is the directory of
    the file itself. @executable_path is the directory of the executable that
    started the process: the file's own for an executable, either one's for a
    library.
    """
    role = role_of(app, macho)
    if role == "other":
        executable_dirs = [os.path.join(app, directory) for directory in EXECUTABLE_DIRS]
    else:
        executable_dirs = [os.path.dirname(macho)]
    for token, bases in (("@loader_path", [os.path.dirname(macho)]), ("@executable_path", executable_dirs)):
        if path == token or path.startswith(token + "/"):
            return [os.path.normpath(base + path[len(token):]) for base in bases]
    return None


def check_rpaths(app: str, macho: str, rpaths: List[str]) -> List[str]:
    failures = []
    for rpath in rpaths:
        places = resolve(app, macho, rpath)
        if places is None or not all(is_inside(app, place) for place in places):
            failures.append("%s has the library search path %s, which is outside the bundle" % (macho, rpath))
    return failures


def check_load_commands(app: str, macho: str, libs: List[str]) -> List[str]:
    failures = []
    for lib in libs:
        places = resolve(app, macho, lib)
        if lib.startswith("@rpath/"):
            # The search paths are held inside the bundle by check_rpaths, so
            # the rest must not climb out of one.
            rest = os.path.normpath(lib[len("@rpath/"):])
            good = not (rest == ".." or rest.startswith("../") or rest.startswith("/"))
        elif places is not None:
            good = all(is_inside(app, place) for place in places)
        else:
            good = os.path.normpath(lib).startswith(SYSTEM_PREFIXES)
        if not good:
            failures.append("%s loads %s, which is outside the bundle and the system" % (macho, lib))
    return failures


def check_verified(macho: str, code: int, text: str) -> List[str]:
    """Judge the result of `codesign --verify` on one file."""
    if code != 0:
        lines = text.strip().splitlines()
        return ["the signature of %s does not verify: %s" % (macho, lines[-1] if lines else "no output")]
    return []


def check_team(macho: str, found: Optional[str], expected: str) -> List[str]:
    if found != expected:
        return ["%s is signed by team %s, not %s" % (macho, found or "(none)", expected)]
    return []


def parse_codesign_display(text: str) -> Tuple[Optional[str], bool]:
    """Return (team identifier or None, hardened runtime) from `codesign -dvv`."""
    team = None
    match = re.search(r"^TeamIdentifier=(\S+)\s*$", text, re.MULTILINE)
    if match and match.group(1) != "not":
        team = match.group(1)
    flags = re.search(r"^CodeDirectory .*flags=\S*\(([^)]*)\)", text, re.MULTILINE)
    runtime = flags is not None and "runtime" in flags.group(1).split(",")
    return team, runtime


def parse_otool_load_commands(text: str) -> Tuple[List[str], List[str]]:
    """Return (libraries loaded, library search paths) from `otool -l`."""
    libs: List[str] = []
    rpaths: List[str] = []
    cmd = None
    for line in text.splitlines():
        fields = line.split()
        if len(fields) >= 2 and fields[0] == "cmd":
            cmd = fields[1]
        elif len(fields) >= 2 and fields[0] == "name" and cmd in (
                "LC_LOAD_DYLIB", "LC_LOAD_WEAK_DYLIB", "LC_REEXPORT_DYLIB", "LC_LAZY_LOAD_DYLIB",
                "LC_LOAD_UPWARD_DYLIB"):
            libs.append(line.split("name", 1)[1].rsplit(" (offset", 1)[0].strip())
        elif len(fields) >= 2 and fields[0] == "path" and cmd == "LC_RPATH":
            rpaths.append(line.split("path", 1)[1].rsplit(" (offset", 1)[0].strip())
    return libs, rpaths


def is_macho(path: str) -> bool:
    try:
        with open(path, "rb") as f:
            return f.read(4) in MACHO_MAGICS
    except OSError:
        return False


def find_machos(app: str) -> List[str]:
    found = []
    for dirpath, _dirnames, filenames in os.walk(app):
        for filename in filenames:
            path = os.path.join(dirpath, filename)
            if not os.path.islink(path) and is_macho(path):
                found.append(path)
    return sorted(found)


def run(cmd: List[str]) -> Tuple[int, str]:
    done = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False)
    return done.returncode, done.stdout.decode("utf-8", "replace")


def read_entitlements(macho: str) -> Dict[str, object]:
    done = subprocess.run(["codesign", "-d", "--entitlements", "-", "--xml", macho],
                          stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, check=False)
    if done.returncode != 0 or not done.stdout.strip():
        return {}
    return plistlib.loads(done.stdout)


def main(argv: List[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--app", required=True, help="path of the deployed Gridcoin.app")
    parser.add_argument("--release", action="store_true",
                        help="also require the team's signature and the hardened runtime on every file")
    parser.add_argument("--team-id", help="the signing team's identifier (with --release)")
    parser.add_argument("--allow-gui-jit", action="store_true",
                        help="accept %s on the GUI executable" % JIT)
    args = parser.parse_args(argv)

    if args.release and not args.team_id:
        parser.error("--release needs --team-id")

    machos = find_machos(args.app)
    roles = {macho: role_of(args.app, macho) for macho in machos}
    failures: List[str] = []

    for wanted in ("gui", "daemon"):
        if wanted not in roles.values():
            failures.append("the bundle has no %s executable" % wanted)

    for macho in machos:
        failures += check_entitlements(roles[macho], read_entitlements(macho), args.allow_gui_jit)

        code, text = run(["otool", "-l", macho])
        if code != 0:
            failures.append("otool could not read %s" % macho)
            continue
        libs, rpaths = parse_otool_load_commands(text)
        failures += check_load_commands(args.app, macho, libs)
        failures += check_rpaths(args.app, macho, rpaths)

        # A signature's details mean nothing unless the signature verifies.
        code, text = run(["codesign", "--verify", "--strict", "--all-architectures", macho])
        failures += check_verified(macho, code, text)

        if args.release:
            code, text = run(["codesign", "-dvv", macho])
            team, runtime = parse_codesign_display(text) if code == 0 else (None, False)
            failures += check_team(macho, team, args.team_id)
            if not runtime:
                failures.append("%s is signed without the hardened runtime" % macho)

    print("check-macos-bundle: %d Mach-O files in %s (%s mode)"
          % (len(machos), args.app, "release" if args.release else "structure"))
    for failure in failures:
        print("FAIL: " + failure)
    if not failures:
        print("PASS")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
