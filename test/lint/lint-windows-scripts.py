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
  5  the NSIS template runs PowerShell only from $SYSDIR, and runs every step of
     Invoke-GridcoinInstallStep.ps1 through it: StopCore and HardenInstallDir before the
     first file is installed, PreUninstall before the first file is deleted
  6  the installer and the GUI launcher refuse a datadir that ends in a backslash
  7  the scripts do not mention the retired Python autounlock helper
  8  the access list the installer gives a custom install directory is owned by
     Administrators, protected, full control for Administrators and SYSTEM, and read and
     execute for Users, which holds READ_CONTROL and FILE_READ_ATTRIBUTES (a service reads
     the access lists of its program's directories through Users)
  9  the update script checks the Authenticode signature (Test-GridcoinReleaseSignature, which
     calls Get-AuthenticodeSignature) before it runs the installer, and the shipped
     release-signer.txt pins nothing (no uncommented line) until the release signer is known

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
STEPS = "contrib/windows/Invoke-GridcoinInstallStep.ps1"
UPDATE = "contrib/windows/Update-GridcoinCore.ps1"
RELEASE_TEST = "contrib/windows/Test-GridcoinRelease.ps1"
SIGNER = "contrib/windows/release-signer.txt"

HELPERS = ("Set-GridcoinAutounlock.ps1", "Invoke-GridcoinAutounlock.ps1")
HELPER_NAME_ALLOWED_IN = (COMMON, STEPS)

PASSWORD_RE = re.compile(r"-Password\b|TASK_LOGON_PASSWORD|LogonType\s+Password", re.IGNORECASE)
SYSTEM_TOOLS = ("ping", "tasklist", "find", "schtasks", "powershell", "cmd", "netsh", "icacls")
SYSTEM32 = "%systemroot%\\system32\\"
TOKEN_RE = re.compile(r"[^\s\"'|<>&()]+")
POWERSHELL_RE = re.compile(r"\bpowershell(?:\.exe)?\b", re.IGNORECASE)
SYSDIR_POWERSHELL = "$SYSDIR\\WindowsPowerShell\\v1.0\\"

# The steps of Invoke-GridcoinInstallStep.ps1 that the template runs, and the one form of the
# line that runs each: from $PLUGINSDIR (copied there before anything is installed) or from the
# installed $INSTDIR\windows, with the install directory and, optionally, the state file.
INSTALL_STEPS = ("StopCore", "HardenInstallDir", "SwitchOver", "StartCore", "ReportPermissions",
                 "UninstallCheck", "PreUninstall")
STEP_LINE_RE = re.compile(
    r"^\s*nsExec::Exec(?:ToLog|ToStack)? '\"\$SYSDIR\\WindowsPowerShell\\v1\.0\\powershell\.exe\" "
    r"-NoProfile -NonInteractive -ExecutionPolicy Bypass "
    r"-File \"(?:\$PLUGINSDIR|\$INSTDIR\\windows)\\Invoke-GridcoinInstallStep\.ps1\" "
    r"-Step (\w+) -InstallDir \"\$INSTDIR\"(?: -StateFile \"\$PLUGINSDIR\\upgrade-state\.json\")?'\s*$")
STEP_COPY_RE = re.compile(r"^\s*File /oname=\$PLUGINSDIR\\(Invoke-GridcoinInstallStep|GridcoinCommon)\.ps1 ")
INSTALLED_FILE_RE = re.compile(r"^\s*File (?!/oname=\$PLUGINSDIR\\)")
DELETION_RE = re.compile(r"^\s*(?:Delete|RMDir)\b", re.IGNORECASE)

# The access list of a custom install directory (rule 8), as SDDL.
SDDL_RE = re.compile(r"^\$script:InstallDirSddl = '([^']*)'\s*$", re.MULTILINE)
SDDL_PARTS_RE = re.compile(r"^O:(\w+)D:([A-Z]*)((?:\([^()]*\))+)$")
ACE_RE = re.compile(r"\(([^()]*)\)")
FILE_ALL_ACCESS = 0x1F01FF
SDDL_RIGHTS = {"FA": FILE_ALL_ACCESS, "FR": 0x120089, "FX": 0x1200A0}
READ_CONTROL = 0x20000
FILE_READ_ATTRIBUTES = 0x80
# FILE_WRITE_DATA, FILE_APPEND_DATA, FILE_WRITE_EA, FILE_DELETE_CHILD, FILE_WRITE_ATTRIBUTES,
# DELETE, WRITE_DAC, WRITE_OWNER, GENERIC_ALL and GENERIC_WRITE.
WRITE_RIGHTS = 0x2 | 0x4 | 0x10 | 0x40 | 0x100 | 0x10000 | 0x40000 | 0x80000 | 0x10000000 | 0x40000000


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


def first_line(lines, pattern):
    """The index of the first of the lines that the pattern matches, or None."""
    return next((index for index, line in enumerate(lines) if pattern.search(line)), None)


def rule5(files):
    failures = []
    text = files.get(NSIS, "")
    for match in POWERSHELL_RE.finditer(text):
        if not text[:match.start()].endswith(SYSDIR_POWERSHELL):
            failures.append(f"rule 5: {NSIS}: PowerShell is not run from {SYSDIR_POWERSHELL}")

    lines = text.splitlines()
    runs = {}
    for index, line in enumerate(lines):
        if "Invoke-GridcoinInstallStep.ps1" not in line or "nsExec::" not in line:
            continue
        match = STEP_LINE_RE.match(line)
        if not match:
            failures.append(f"rule 5: {NSIS}:{index + 1}: a step is not run in the one form: {line.strip()}")
            continue
        runs.setdefault(match.group(1), []).append(index)
    for step in INSTALL_STEPS:
        if step not in runs:
            failures.append(f"rule 5: {NSIS}: the step {step} of Invoke-GridcoinInstallStep.ps1 is not run")
    for step in sorted(set(runs) - set(INSTALL_STEPS)):
        failures.append(f"rule 5: {NSIS}: {step} is not a step of Invoke-GridcoinInstallStep.ps1")

    installed = first_line(lines, INSTALLED_FILE_RE)
    copied = [index for index, line in enumerate(lines) if STEP_COPY_RE.match(line)]
    for step in ("StopCore", "HardenInstallDir"):
        for index in runs.get(step, []):
            if installed is not None and index > installed:
                failures.append(f"rule 5: {NSIS}:{index + 1}: {step} runs after the first file is installed")
            if len(copied) < 2 or index < max(copied):
                failures.append(f"rule 5: {NSIS}:{index + 1}: {step} runs before the step script and "
                                "GridcoinCommon.ps1 are copied to $PLUGINSDIR")
    deleted = first_line(lines, DELETION_RE)
    for index in runs.get("PreUninstall", []):
        if deleted is not None and index > deleted:
            failures.append(f"rule 5: {NSIS}:{index + 1}: PreUninstall runs after the first file is deleted")
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


def sddl_rights(text):
    """The access mask an SDDL ACE's rights field names, or None when it is not one rule 8 reads."""
    if re.fullmatch(r"0x[0-9a-fA-F]+", text):
        return int(text, 16)
    if not text or len(text) % 2 or any(text[i:i + 2] not in SDDL_RIGHTS for i in range(0, len(text), 2)):
        return None
    mask = 0
    for i in range(0, len(text), 2):
        mask |= SDDL_RIGHTS[text[i:i + 2]]
    return mask


def rule8(files):
    text = files.get(STEPS)
    if text is None:
        return [f"rule 8: {STEPS} does not exist"]
    match = SDDL_RE.search(text)
    if not match:
        return [f"rule 8: {STEPS}: no \"$script:InstallDirSddl = '<SDDL>'\" line"]
    sddl = match.group(1)
    parts = SDDL_PARTS_RE.match(sddl)
    if not parts:
        return [f"rule 8: {STEPS}: '{sddl}' is not an owner and a DACL"]

    failures = []
    owner, flags, aces = parts.groups()
    if owner != "BA":
        failures.append(f"rule 8: {STEPS}: the owner is {owner}, not BA (Administrators)")
    if "P" not in flags:
        failures.append(f"rule 8: {STEPS}: the DACL is not protected (flags '{flags}')")
    granted = {}
    for ace in ACE_RE.findall(aces):
        fields = ace.split(";")
        if len(fields) != 6 or fields[0] != "A" or fields[1] != "OICI" or fields[3] or fields[4]:
            failures.append(f"rule 8: {STEPS}: ({ace}) is not an allow entry inherited by files and directories")
            continue
        if fields[5] in granted:
            failures.append(f"rule 8: {STEPS}: {fields[5]} has two entries")
        granted[fields[5]] = sddl_rights(fields[2])
    if sorted(granted) != ["BA", "BU", "SY"]:
        failures.append(f"rule 8: {STEPS}: the entries are for {sorted(granted)}, not BA, BU and SY")
    for trustee in ("BA", "SY"):
        if trustee in granted and granted[trustee] != FILE_ALL_ACCESS:
            failures.append(f"rule 8: {STEPS}: {trustee} does not have full control")
    users = granted.get("BU")
    if users is None:
        failures.append(f"rule 8: {STEPS}: BU (Users) has no access mask rule 8 reads")
    else:
        if users & READ_CONTROL == 0 or users & FILE_READ_ATTRIBUTES == 0:
            failures.append(f"rule 8: {STEPS}: BU (Users) lacks READ_CONTROL or FILE_READ_ATTRIBUTES "
                            f"(0x{users:x}), which a service needs to check its program's directories")
        if users & WRITE_RIGHTS:
            failures.append(f"rule 8: {STEPS}: BU (Users) can change the directory (0x{users:x})")
    return failures


def rule9(files):
    failures = []
    update = files.get(UPDATE)
    if update is None:
        failures.append(f"rule 9: {UPDATE} does not exist")
    else:
        # comments do not count: only code lines are searched
        code = [line for line in update.splitlines() if not line.lstrip().startswith("#")]
        check = first_line(code, re.compile(r"\bTest-GridcoinReleaseSignature\b"))
        start = first_line(code, re.compile(r"\bStart-Process\b"))
        if check is None:
            failures.append(f"rule 9: {UPDATE}: does not call Test-GridcoinReleaseSignature")
        if start is None:
            failures.append(f"rule 9: {UPDATE}: has no Start-Process of the installer")
        if check is not None and start is not None and check > start:
            failures.append(f"rule 9: {UPDATE}: runs the installer before it checks the signature")
    verifier = files.get(RELEASE_TEST)
    if verifier is None:
        failures.append(f"rule 9: {RELEASE_TEST} does not exist")
    elif "Get-AuthenticodeSignature" not in verifier:
        failures.append(f"rule 9: {RELEASE_TEST}: does not call Get-AuthenticodeSignature")
    signer = files.get(SIGNER)
    if signer is None:
        failures.append(f"rule 9: {SIGNER} does not exist")
    else:
        for number, line in enumerate(signer.splitlines(), 1):
            if line.strip() and not line.lstrip().startswith("#"):
                failures.append(f"rule 9: {SIGNER}:{number}: pins a value ({line.strip()}); it ships with comments only")
    return failures


RULES = (rule1, rule2, rule3, rule4, rule5, rule6, rule7, rule8, rule9)

GOOD_PS1 = "#Requires -Version 5.1\nSet-StrictMode -Version Latest\n$ErrorActionPreference = 'Stop'\n"


def step_line(step, where="$PLUGINSDIR", state=True, run="ExecToLog"):
    """A template line that runs one step of Invoke-GridcoinInstallStep.ps1, in rule 5's form."""
    line = (f"    nsExec::{run} '\"$SYSDIR\\WindowsPowerShell\\v1.0\\powershell.exe\" -NoProfile -NonInteractive "
            f"-ExecutionPolicy Bypass -File \"{where}\\Invoke-GridcoinInstallStep.ps1\" -Step {step} "
            f"-InstallDir \"$INSTDIR\"")
    if state:
        line += " -StateFile \"$PLUGINSDIR\\upgrade-state.json\""
    return line + "'\n"


GOOD_COPIES = ("    File /oname=$PLUGINSDIR\\Invoke-GridcoinInstallStep.ps1 x\\Invoke-GridcoinInstallStep.ps1\n"
               "    File /oname=$PLUGINSDIR\\GridcoinCommon.ps1 x\\GridcoinCommon.ps1\n")
GOOD_MAIN_HEAD = "Section -Main\n" + GOOD_COPIES + step_line("StopCore") + step_line("HardenInstallDir")
GOOD_MAIN_TAIL = ("    File x\\gridcoinresearch.exe\n"
                  + step_line("SwitchOver", "$INSTDIR\\windows") + step_line("StartCore", "$INSTDIR\\windows")
                  + step_line("ReportPermissions", "$INSTDIR\\windows") + "SectionEnd\n")
GOOD_UNINSTALL = ("Section -un.Main\n" + step_line("PreUninstall", "$INSTDIR\\windows", False)
                  + "    Delete x\n    RMDir /r y\nSectionEnd\n"
                  + "Function un.onInit\n" + step_line("UninstallCheck", "$INSTDIR\\windows", False, "ExecToStack")
                  + "FunctionEnd\n")
GOOD_NSIS = GOOD_MAIN_HEAD + GOOD_MAIN_TAIL + GOOD_UNINSTALL
GOOD_SDDL = "O:BAD:PAI(A;OICI;FA;;;BA)(A;OICI;FA;;;SY)(A;OICI;0x1200a9;;;BU)"
GOOD_FILES = {
    COMMON: GOOD_PS1 + "if ($t.Principal.LogonType -eq 1) { }\n'rem BEGIN GENERATED'\n'rem END GENERATED'\n",
    INSTALL: GOOD_PS1 + "if ($DataDir -match '\\\\$') { throw 'x' }\n",
    SHUTDOWN: GOOD_PS1 + "'rem BEGIN GENERATED'\n'rem END GENERATED'\n",
    GUI: GOOD_PS1 + "if ($DataDir -match '\\\\$') { throw 'x' }\n",
    "contrib/windows/Other.ps1": GOOD_PS1 + (
        "$l = @(\n    'rem BEGIN GENERATED',\n    '\"%SystemRoot%\\System32\\PING.EXE\" -n 121 127.0.0.1 >nul',\n"
        "    'rem ping and find in a comment',\n    'rem END GENERATED'\n)\n"),
    NSIS: GOOD_NSIS,
    STEPS: GOOD_PS1 + "$script:InstallDirSddl = '" + GOOD_SDDL + "'\n",
    CMAKE: "install(FILES Install-GridcoinCoreTask.ps1)\n",
    UPDATE: GOOD_PS1 + "# Start-Process in a comment\nTest-GridcoinReleaseSignature -Path $p -SignerFile $s\nStart-Process $p\n",
    RELEASE_TEST: GOOD_PS1 + "Get-AuthenticodeSignature -LiteralPath $Path\n",
    SIGNER: "# comments only\n\n",
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
    (5, {NSIS: GOOD_NSIS + "ExecWait 'powershell.exe -File x'\n"}),
    (5, {NSIS: GOOD_NSIS + "ExecWait '\"$WINDIR\\System32\\WindowsPowerShell\\v1.0\\powershell.exe\" x'\n"}),
    # a step that is not run, or not in the one form, or that is no step of the script
    (5, {NSIS: GOOD_NSIS.replace(step_line("StartCore", "$INSTDIR\\windows"), "")}),
    (5, {NSIS: GOOD_NSIS.replace(step_line("UninstallCheck", "$INSTDIR\\windows", False, "ExecToStack"), "")}),
    (5, {NSIS: GOOD_NSIS.replace(" -NonInteractive", "", 1)}),
    (5, {NSIS: GOOD_NSIS.replace("-InstallDir \"$INSTDIR\"", "-InstallDir $INSTDIR", 1)}),
    (5, {NSIS: GOOD_NSIS + step_line("StopService")}),
    # StopCore or HardenInstallDir after the first installed file, or before the copies
    (5, {NSIS: "Section -Main\n" + GOOD_COPIES + step_line("StopCore") + "    File x\\a.exe\n"
               + step_line("HardenInstallDir") + GOOD_MAIN_TAIL + GOOD_UNINSTALL}),
    (5, {NSIS: "Section -Main\n" + step_line("StopCore") + GOOD_COPIES + step_line("HardenInstallDir")
               + GOOD_MAIN_TAIL + GOOD_UNINSTALL}),
    # PreUninstall after the first deletion
    (5, {NSIS: GOOD_MAIN_HEAD + GOOD_MAIN_TAIL + "Section -un.Main\n    RMDir /r y\n"
               + step_line("PreUninstall", "$INSTDIR\\windows", False) + "SectionEnd\n"
               + "Function un.onInit\n" + step_line("UninstallCheck", "$INSTDIR\\windows", False, "ExecToStack")
               + "FunctionEnd\n"}),
    (6, {INSTALL: GOOD_PS1}),
    (6, {GUI: GOOD_PS1}),
    (7, {"contrib/windows/README.md": "see contrib/wallettools/gridcoin_autounlock.py\n"}),
    (8, {STEPS: None}),
    (8, {STEPS: GOOD_PS1}),
    # Users without READ_CONTROL, without FILE_READ_ATTRIBUTES, or with a right that changes the directory
    (8, {STEPS: GOOD_PS1 + "$script:InstallDirSddl = '" + GOOD_SDDL.replace("0x1200a9", "0x1000a9") + "'\n"}),
    (8, {STEPS: GOOD_PS1 + "$script:InstallDirSddl = '" + GOOD_SDDL.replace("0x1200a9", "0x120029") + "'\n"}),
    (8, {STEPS: GOOD_PS1 + "$script:InstallDirSddl = '" + GOOD_SDDL.replace("0x1200a9", "0x1201bf") + "'\n"}),
    (8, {STEPS: GOOD_PS1 + "$script:InstallDirSddl = '" + GOOD_SDDL.replace("0x1200a9", "FA") + "'\n"}),
    # not protected, another owner, an entry not inherited, another account, an entry missing
    (8, {STEPS: GOOD_PS1 + "$script:InstallDirSddl = '" + GOOD_SDDL.replace("D:PAI", "D:AI") + "'\n"}),
    (8, {STEPS: GOOD_PS1 + "$script:InstallDirSddl = '" + GOOD_SDDL.replace("O:BA", "O:SY") + "'\n"}),
    (8, {STEPS: GOOD_PS1 + "$script:InstallDirSddl = '" + GOOD_SDDL.replace("(A;OICI;FA;;;SY)", "(A;CI;FA;;;SY)") + "'\n"}),
    (8, {STEPS: GOOD_PS1 + "$script:InstallDirSddl = '" + GOOD_SDDL + "(A;OICI;0x1200a9;;;AU)'\n"}),
    (8, {STEPS: GOOD_PS1 + "$script:InstallDirSddl = '" + GOOD_SDDL.replace("(A;OICI;FA;;;SY)", "") + "'\n"}),
    (8, {STEPS: GOOD_PS1 + "$script:InstallDirSddl = '" + GOOD_SDDL.replace("(A;OICI;FA;;;SY)", "(A;OICI;FR;;;SY)") + "'\n"}),
    # the installer run before the check, no check, a verifier without Get-AuthenticodeSignature,
    # a pinned value in the shipped file, and a missing file
    (9, {UPDATE: GOOD_PS1 + "Start-Process $p\nTest-GridcoinReleaseSignature -Path $p -SignerFile $s\n"}),
    (9, {UPDATE: GOOD_PS1 + "# Test-GridcoinReleaseSignature\nStart-Process $p\n"}),
    (9, {UPDATE: GOOD_PS1 + "Test-GridcoinReleaseSignature -Path $p -SignerFile $s\n"}),
    (9, {RELEASE_TEST: GOOD_PS1 + "Get-FileHash $Path\n"}),
    (9, {SIGNER: "# comments\nidentity-eku=1.3.6.1.4.1.311.97.1\n"}),
    (9, {SIGNER: None}),
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
        for path, sample in patch.items():
            # None stands for a file that does not exist.
            if sample is None:
                files.pop(path, None)
            else:
                files[path] = sample
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
