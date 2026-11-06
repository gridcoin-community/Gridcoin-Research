#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

"""Static checks of the Windows scripts under contrib/windows and the NSIS packaging.

The scripts register the core's tasks without a stored password and run, in part, as
SYSTEM, so what they may contain is fixed here. Nothing is run: the files are read as
text. A failure names the rule and the file.

Rules
  1  no task registration with a password anywhere in contrib/windows or build-aux/cpack
  2  the resident autounlock helper is gone and no longer packaged
  3  every .ps1 starts with the version requirement and sets strict mode and stop-on-error
  4  inside a generated .cmd block, every system tool is named by its full System32 path
  5  the NSIS template runs PowerShell only from $SYSDIR
  6  the installer and the GUI launcher refuse a datadir that ends in a backslash
  7  the scripts do not mention the retired Python autounlock helper

Usage
  lint-windows-scripts.py              check the tree
  lint-windows-scripts.py --self-test  check each rule against an embedded bad sample
"""

import glob
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")

WIN_DIR = "contrib/windows"
CPACK_DIR = "build-aux/cpack"
NSIS = "build-aux/cpack/NSIS.template.in"
CMAKE = "src/CMakeLists.txt"
COMMON = "contrib/windows/GridcoinCommon.ps1"
INSTALL = "contrib/windows/Install-GridcoinCoreTask.ps1"
GUI = "contrib/windows/Start-GridcoinGui.ps1"
SHUTDOWN = "contrib/windows/Set-GridcoinShutdownFlush.ps1"

HELPERS = ("Set-GridcoinAutounlock.ps1", "Invoke-GridcoinAutounlock.ps1")
HELPER_NAME_ALLOWED_IN = (COMMON, "contrib/windows/Invoke-GridcoinInstallStep.ps1")

PASSWORD_RE = re.compile(r"-Password\b|TASK_LOGON_PASSWORD|LogonType\s+Password", re.IGNORECASE)
SYSTEM_TOOLS = ("ping", "tasklist", "find", "schtasks", "powershell", "cmd", "netsh", "icacls")
SYSTEM32 = "%systemroot%\\system32\\"
TOKEN_RE = re.compile(r"[^\s\"'|<>&()]+")
POWERSHELL_RE = re.compile(r"\bpowershell(?:\.exe)?\b", re.IGNORECASE)
SYSDIR_POWERSHELL = "$SYSDIR\\WindowsPowerShell\\v1.0\\"


def load_tree(root):
    """The files the rules read, as {path relative to the repository: text}."""
    files = {}
    paths = glob.glob(os.path.join(root, WIN_DIR, "*")) + glob.glob(os.path.join(root, CPACK_DIR, "*"))
    paths.append(os.path.join(root, CMAKE))
    for path in paths:
        if not os.path.isfile(path):
            continue
        with open(path, encoding="utf-8", errors="replace") as handle:
            files[os.path.relpath(path, root).replace(os.sep, "/")] = handle.read()
    return files


def under(files, directory):
    return {path: text for path, text in files.items() if path.startswith(directory + "/")}


def ps1_files(files):
    return {path: text for path, text in under(files, WIN_DIR).items() if path.endswith(".ps1")}


def rule1(files):
    failures = []
    for directory in (WIN_DIR, CPACK_DIR):
        for path, text in sorted(under(files, directory).items()):
            for number, line in enumerate(text.splitlines(), 1):
                if PASSWORD_RE.search(line):
                    failures.append(f"rule 1: {path}:{number}: task registration with a password: {line.strip()}")
    return failures


def rule2(files):
    failures = []
    for name in HELPERS:
        if f"{WIN_DIR}/{name}" in files:
            failures.append(f"rule 2: {WIN_DIR}/{name} must not exist")
    for path, text in sorted(files.items()):
        if "Invoke-GridcoinAutounlock" in text and path not in HELPER_NAME_ALLOWED_IN:
            failures.append(f"rule 2: {path}: names Invoke-GridcoinAutounlock")
    for path in (CMAKE, NSIS):
        for name in HELPERS:
            if name in files.get(path, ""):
                failures.append(f"rule 2: {path}: names {name}")
    return failures


def rule3(files):
    failures = []
    for path, text in sorted(ps1_files(files).items()):
        if not text.startswith("#Requires -Version 5.1"):
            failures.append(f"rule 3: {path}: does not start with '#Requires -Version 5.1'")
        if "Set-StrictMode -Version Latest" not in text:
            failures.append(f"rule 3: {path}: lacks 'Set-StrictMode -Version Latest'")
        if "$ErrorActionPreference = 'Stop'" not in text:
            failures.append(f"rule 3: {path}: lacks \"$ErrorActionPreference = 'Stop'\"")
    return failures


def generated_lines(text):
    """(line number, line) for each line inside a BEGIN GENERATED / END GENERATED block."""
    inside = False
    for number, line in enumerate(text.splitlines(), 1):
        if "BEGIN GENERATED" in line:
            inside = True
        elif "END GENERATED" in line:
            inside = False
        elif inside:
            yield number, line


def rule4(files):
    failures = []
    for path in (COMMON, SHUTDOWN):
        if path in files and "BEGIN GENERATED" not in files[path]:
            failures.append(f"rule 4: {path}: has no BEGIN GENERATED / END GENERATED block")
    for path, text in sorted(ps1_files(files).items()):
        for number, line in generated_lines(text):
            body = line.strip().lstrip("'\"").strip()
            if body.lower().startswith("rem ") or body.startswith("#"):
                continue
            for token in TOKEN_RE.findall(body):
                name = token.replace("/", "\\").split("\\")[-1].lower()
                if name.endswith(".exe"):
                    name = name[:-4]
                if name in SYSTEM_TOOLS and not token.lower().startswith(SYSTEM32):
                    failures.append(f"rule 4: {path}:{number}: '{token}' is not a %SystemRoot%\\System32\\ path")
    return failures


def rule5(files):
    failures = []
    text = files.get(NSIS, "")
    for match in POWERSHELL_RE.finditer(text):
        if not text[:match.start()].endswith(SYSDIR_POWERSHELL):
            failures.append(f"rule 5: {NSIS}: PowerShell is not run from {SYSDIR_POWERSHELL}")
    return failures


def rule6(files):
    failures = []
    for path in (INSTALL, GUI):
        if "'\\\\$'" not in files.get(path, ""):
            failures.append(f"rule 6: {path}: no trailing-backslash refusal ('\\\\$' match on the datadir)")
    return failures


def rule7(files):
    failures = []
    for path, text in sorted(under(files, WIN_DIR).items()):
        for needle in ("gridcoin_autounlock.py", "contrib/wallettools"):
            if needle in text:
                failures.append(f"rule 7: {path}: mentions {needle}")
    return failures


RULES = (rule1, rule2, rule3, rule4, rule5, rule6, rule7)

GOOD_PS1 = "#Requires -Version 5.1\nSet-StrictMode -Version Latest\n$ErrorActionPreference = 'Stop'\n"
GOOD_FILES = {
    COMMON: GOOD_PS1 + "if ($t.Principal.LogonType -eq 1) { }\n'rem BEGIN GENERATED'\n'rem END GENERATED'\n",
    INSTALL: GOOD_PS1 + "if ($DataDir -match '\\\\$') { throw 'x' }\n",
    SHUTDOWN: GOOD_PS1 + "'rem BEGIN GENERATED'\n'rem END GENERATED'\n",
    GUI: GOOD_PS1 + "if ($DataDir -match '\\\\$') { throw 'x' }\n",
    "contrib/windows/Other.ps1": GOOD_PS1 + (
        "$l = @(\n    'rem BEGIN GENERATED',\n    '\"%SystemRoot%\\System32\\PING.EXE\" -n 121 127.0.0.1 >nul',\n"
        "    'rem ping and find in a comment',\n    'rem END GENERATED'\n)\n"),
    NSIS: 'ExecWait \'"$SYSDIR\\WindowsPowerShell\\v1.0\\powershell.exe" -File x\'\n',
    CMAKE: "install(FILES Install-GridcoinCoreTask.ps1)\n",
}

# (rule, a file that breaks it) pairs, applied to GOOD_FILES
BAD_SAMPLES = (
    (1, {"contrib/windows/Other.ps1": GOOD_PS1 + "Register-ScheduledTask -User u -Password p\n"}),
    (1, {"contrib/windows/Other.ps1": GOOD_PS1 + "$x = [TASK_LOGON_PASSWORD]\n"}),
    (1, {"build-aux/cpack/x.in": "$p.LogonType  Password\n"}),
    (2, {"contrib/windows/Set-GridcoinAutounlock.ps1": GOOD_PS1}),
    (2, {"contrib/windows/Other.ps1": GOOD_PS1 + "& .\\Invoke-GridcoinAutounlock.ps1\n"}),
    (2, {CMAKE: "install(FILES Set-GridcoinAutounlock.ps1)\n"}),
    (2, {NSIS: "File Invoke-GridcoinAutounlock.ps1\n"}),
    (3, {"contrib/windows/Other.ps1": "Set-StrictMode -Version Latest\n$ErrorActionPreference = 'Stop'\n"}),
    (3, {"contrib/windows/Other.ps1": "#Requires -Version 5.1\n$ErrorActionPreference = 'Stop'\n"}),
    (3, {"contrib/windows/Other.ps1": "#Requires -Version 5.1\nSet-StrictMode -Version Latest\n"}),
    (4, {SHUTDOWN: GOOD_PS1}),
    (4, {COMMON: GOOD_PS1}),
    (4, {"contrib/windows/Other.ps1": GOOD_PS1 + "'rem BEGIN GENERATED'\n'ping -n 121 127.0.0.1 >nul'\n'rem END GENERATED'\n"}),
    (4, {"contrib/windows/Other.ps1": GOOD_PS1 + "'rem BEGIN GENERATED'\n'x | find /i y'\n'rem END GENERATED'\n"}),
    (4, {"contrib/windows/Other.ps1": GOOD_PS1 + "'rem BEGIN GENERATED'\n'\"C:\\Windows\\System32\\cmd.exe\" /c x'\n'rem END GENERATED'\n"}),
    (5, {NSIS: "ExecWait 'powershell.exe -File x'\n"}),
    (5, {NSIS: "ExecWait '\"$WINDIR\\System32\\WindowsPowerShell\\v1.0\\powershell.exe\" x'\n"}),
    (6, {INSTALL: GOOD_PS1}),
    (6, {GUI: GOOD_PS1}),
    (7, {"contrib/windows/README.md": "see contrib/wallettools/gridcoin_autounlock.py\n"}),
)


def self_test():
    ok = True
    for rule in RULES:
        failures = rule(GOOD_FILES)
        if failures:
            print(f"self-test: {rule.__name__} fails the good sample: {failures[0]}")
            ok = False
    for number, patch in BAD_SAMPLES:
        files = dict(GOOD_FILES)
        files.update(patch)
        if not RULES[number - 1](files):
            print(f"self-test: rule {number} does not detect {sorted(patch)}")
            ok = False
    return ok


def main():
    if "--self-test" in sys.argv[1:]:
        if not self_test():
            return 1
        print("lint-windows-scripts: self-test ok")
        return 0
    files = load_tree(ROOT)
    failures = []
    for rule in RULES:
        failures.extend(rule(files))
    for failure in failures:
        print(failure)
    if failures:
        print(f"lint-windows-scripts: {len(failures)} failure(s)")
        return 1
    print("lint-windows-scripts: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
