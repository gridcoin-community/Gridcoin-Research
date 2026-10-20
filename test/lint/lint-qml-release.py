#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

"""Fail when a release build configuration enables the QML front end.

No official binary is a QML build until the QML front end's checklist is
drained, so every release build configuration must leave ENABLE_QML off.
The lint checks values, not mentions: a line fails when it gives ENABLE_QML
a value other than a literal off (OFF, FALSE or 0, in any case) in a -D
definition, set(), option(), a shell or make assignment, or a YAML or JSON
key. An indirect value, such as a variable or a workflow expression, cannot
be proved off, so it fails as well. The top-level CMakeLists.txt must hold
exactly one option(), defaulting to OFF, and a cmake_dependent_option()
fails whatever its value. A whole-line comment is skipped; a setting in a
trailing comment is still read.

The files read are the tracked files under CHECKED_PATHSPECS, less vendored
code (VENDORED) and prose (PROSE). The paths in REQUIRED must be tracked and
present, and any other checked file that is tracked but cannot be read is
reported the same way. debian/rules and debian-testnet/rules are built by
OBS and by no CI job, so nothing else would notice a QML-on line there.

In build_targets.sh, ENABLE_QML may be assigned only by its one top-level
default, which must be false (none or two fail the lint), and by the
argument parser; a -DENABLE_QML other than off may appear only on the ON
line of the ENABLE_QML=true branch of the mapping, and an off -D passes
anywhere.

cmake_qml.yml builds the module with -DENABLE_QML=ON, and its values are not
read. That exemption holds while every uses: key (a step's action or a job's
reusable workflow, in any key spelling, several on one line included) names
an action in ALLOWED_ACTIONS (actions/checkout, actions/cache,
actions/cache/restore, actions/cache/save, actions/upload-artifact), no line
names a network publisher of RUN_DENY_RE outside its uses: keys, and it
uploads no artifact while it can run on a tag ref; and while no
release-capable workflow calls it, names it, triggers on another workflow's
run or reads another run's artifacts. tag_reach holds the ways a workflow can
run on a tag ref. A workflow is release-capable when it can run on a tag ref
or holds a RELEASE_RE line.

The QML variants of the packaging (VARIANT_FILES) are not read, provided the
two rules files include rules.qml only inside RULES_GUARD, no other workflow,
build_targets.sh, packaging or debian file names a variant, and none of these
files adds qml to DEB_BUILD_OPTIONS. cmake_qml.yml, exempt above, is read for
neither.

When the QML build is ready to ship, one file is lifted by adding its path,
and its variant's, to LIFTED; at the cut-over this lint is deleted.

What it cannot see: settings outside the repository, such as an OBS project
configuration (prjconf) or a Launchpad recipe that sets
DEB_BUILD_OPTIONS=qml or extra CMake flags, and an artifact route inside a
script outside the checked set that a release step runs, and two in-repo
spellings the per-line matchers miss: a JSON key with no blank after its
colon ("ENABLE_QML":"ON") and a set( call split before the variable name.
Nor does tag_reach read an on: written as a flow mapping on the line after
on: ({push: ...}), the names on the later lines of a flow list opened on the
on: line and wrapped (only the on: line itself is read), or YAML anchors and
aliases; and it reads an inline push flow mapping with a branch filter
(push: {branches: [main]}) as a push without a branch filter, a false red
rather than a miss. The variant-name and DEB_BUILD_OPTIONS checks do not
read cmake_qml.yml, the CMake files outside depends, or CMake presets. A
workflow that only reads releases counts as release-capable (gh release
view or download, or gh api on a releases path), so a cross-run download or
a cmake_qml.yml mention in it goes red although it publishes nothing. The
network-publisher check reads cmake_qml.yml one line at a time, and
RUN_DENY_RE is a named list of commands, each matched only as spelled there:
a listed command spelled otherwise (docker image push, git -C <dir> push,
aws s3api), an unlisted publisher (ssh, gh gist, gcloud, az), a command
split with a backslash so that its name sits on a later line, one reached
through a variable, alias or function, an interpreter one-liner, a script a
step runs, or a command glued to a uses: target with no blank between them
(actions/checkout@v5;curl ...), which is read as part of the target, is not
seen. A separate workflow that cannot itself run on a tag ref (a
workflow_run follower), downloads cmake_qml.yml's artifact and publishes it
with an action RELEASE_RE does not list is not release-capable, so the lint
stays green. The lint guards against
accidental workflow changes; it is not a security boundary.

There is no override marker: lifting a file is an edit of LIFTED.
"""

import os
import re
import subprocess
import sys

REQUIRED = (
    "CMakeLists.txt",
    "build_targets.sh",
    "depends/toolchain.cmake.in",
    "contrib/flatpak/us.gridcoin.GridcoinResearch.yml",
    "debian/rules",
    "debian-testnet/rules",
    ".github/workflows/cmake_production.yml",
)
EXEMPT_WORKFLOW = ".github/workflows/cmake_qml.yml"
QML_FLATPAK_MANIFEST = "contrib/flatpak/us.gridcoin.GridcoinResearch.qml.yml"
VARIANT_FILES = (
    "debian/rules.qml",
    "debian-testnet/rules.qml",
    QML_FLATPAK_MANIFEST,
)
RULES_FILES = ("debian/rules", "debian-testnet/rules")
RULES_GUARD = "ifneq (,$(filter qml,$(DEB_BUILD_OPTIONS)))"
LIFTED = ()
VENDORED = (
    ":!src/leveldb/",
    ":!src/secp256k1/",
    ":!src/univalue/",
    ":!src/bdb53/",
    ":!src/crc32c/",
    ":!src/ipc/libmultiprocess/",
)
PROSE = (":!*.md", ":!*/changelog", ":!*/copyright")
CHECKED_PATHSPECS = (
    ".github/workflows/*.yml",
    ".github/workflows/*.yaml",
    "build_targets.sh",
    "CMakeLists.txt",
    "**/CMakeLists.txt",
    "*.cmake",
    "*.cmake.in",
    "depends/",
    "contrib/flatpak/",
    "contrib/obs/",
    "debian/",
    "debian-testnet/",
    "CMakePresets.json",
    "CMakeUserPresets.json",
)
OFF_VALUES = {"off", "false", "0"}
ARTIFACT_RE = re.compile(r"actions/upload-artifact")
ALLOWED_ACTIONS = (
    "actions/checkout",
    "actions/cache",
    "actions/cache/restore",
    "actions/cache/save",
    "actions/upload-artifact",
)
USES_RE = re.compile(
    r"""(?:^|[\s{,])["']?uses["']?\s*:[ \t]*["']?([^\s"'#,}]*)"""
)
RUN_DENY_RE = re.compile(
    r"\bgh\s+(release|api)\b|\bcurl\b|\bwget\b|\bdocker\s+push\b"
    r"|\bgit\s+push\b|\btwine\b|\bnpm\s+publish\b|\bosc\b|push-to-obs\.sh"
    r"|uploads\.github\.com|api\.github\.com|\b(scp|rsync|sftp|ftp)\b"
    r"|\baws\s+s3\b|\bgsutil\b"
)
RELEASE_RE = re.compile(
    r"softprops/action-gh-release|actions/upload-release-asset"
    r"|ncipollo/release-action|\bgh\s+release\b|docker/build-push-action"
    r"|push-to-obs\.sh"
    r"|actions/upload-pages-artifact|actions/deploy-pages"
    r"|\bgh\s+api\b(?=.*\breleases\b)|uploads\.github\.com"
    r"""|uses:\s*["']?(\./\.github/workflows/|[^@\s"']+/\.github/workflows/)"""
)
REUSE_RE = re.compile(r"cmake_qml\.ya?ml|CMake QML Module")
WORKFLOW_RUN_KEY_RE = re.compile(r"^\s*workflow_run\s*:")
CROSS_RUN_RE = re.compile(
    r"\brun-id\s*:|\bgh\s+run\s+download\b|action-download-artifact"
    r"|/actions/(runs/[^/\s]+/)?artifacts|downloadArtifact"
    r"|listWorkflowRunArtifacts"
)
DFLAG_RE = re.compile(r"-D\s*ENABLE_QML(?::\w+)?\s*=")
SET_RE = re.compile(r"\bset\s*\(\s*ENABLE_QML\b\s*")
OPTION_RE = re.compile(r'\boption\s*\(\s*ENABLE_QML\s+"[^"]*"\s*')
DEPENDENT_OPTION_RE = re.compile(
    r"\bcmake_dependent_option\s*\(\s*ENABLE_QML\b"
)
ASSIGN_RE = re.compile(r"(?<![\w$-])ENABLE_QML(?::\w+)?\s*[:?+]?=(?!=)")
KEY_RE = re.compile(r"""(?<![\w$-])["']?ENABLE_QML["']?\s*:(?=\s|$)""")
DEB_OPTS_RE = re.compile(r"DEB_BUILD_OPTIONS\s*[:+?]?=.*\bqml\b")
BT_DEFAULT_RE = re.compile(r"^ENABLE_QML=")
BT_ECHO_RE = re.compile(r'^\s*(echo|printf)\s+"[^"$`]*"\s*(>&2)?\s*$')
BT_NOEQ_SET_RE = re.compile(
    r"\bunset\s+(-v\s+)?ENABLE_QML\b|\bprintf\s+-v\s*ENABLE_QML\b"
    r"|\bread\b[^;&|]*\bENABLE_QML\b|\bdeclare\s+-n\b.*\bENABLE_QML\b"
)
BT_PARSER_PATTERN = "ENABLE_QML=*)"
BT_PARSER_ASSIGN = 'ENABLE_QML="${arg#*=}"'
BT_GUARD_LINE = 'if [ "$ENABLE_QML" = "true" ]; then'
BT_MAPPING_LINE = 'QML_CMAKE_FLAG="-DENABLE_QML=ON"'
TAG_REACH_ORDER = (
    "inline on",
    "on list",
    "no on block",
    "release",
    "workflow_call",
    "create",
    "deployment",
    "deployment_status",
    "registry_package",
    "push: tags",
    "push: tags-ignore",
    "push without a branch filter",
    "workflow_dispatch",
)
TAG_EVENT_NAMES = (
    "push",
    "release",
    "workflow_call",
    "create",
    "deployment",
    "deployment_status",
    "registry_package",
    "workflow_dispatch",
)


def git(*args):
    """Run git with the given arguments and return its standard output."""
    result = subprocess.run(["git", *args], capture_output=True)
    if result.returncode != 0:
        print("lint-qml-release: git {} failed (exit {})".format(
            " ".join(args), result.returncode))
        sys.exit(1)
    return result.stdout


def tracked_files():
    """The checked set: tracked files under the pathspecs, less the variants
    and the lifted files."""
    out = git("ls-files", "-z", "--", *CHECKED_PATHSPECS, *VENDORED, *PROSE)
    listed = set(out.decode("utf8", errors="replace").split("\0"))
    listed.discard("")
    return sorted(listed - set(VARIANT_FILES) - set(LIFTED))


def _add(problems, path, number, rank, text):
    """Record a problem at a line, or at the file when number is None (after
    the file's line problems).

    On one line the problems print by rank: the value rules (0), then the
    reuse (1) and cross-run (2) rules, then the variant rules (3 to 5)."""
    line_key = float("inf") if number is None else number
    where = path if number is None else "{}:{}".format(path, number)
    key = (1, path, line_key, rank, len(problems))
    problems.append((key, "lint-qml-release: {}: {}".format(where, text)))


def _missing(problems, path):
    """Record a missing file once per path, ahead of every other problem."""
    text = "lint-qml-release: {}: missing; the checked set is out of date".format(path)
    if any(entry[1] == text for entry in problems):
        return
    order = REQUIRED.index(path) if path in REQUIRED else len(REQUIRED)
    problems.append(((0, order, path), text))


def read_lines(path, problems):
    """The file's lines, or None for a binary or unreadable file."""
    try:
        with open(path, "rb") as handle:
            data = handle.read()
    except OSError:
        _missing(problems, path)
        return None
    if b"\0" in data[:8192]:
        return None
    return data.decode("utf8", errors="replace").replace("\r", "").split("\n")


def is_comment(line):
    """True when the first non-blank character is a hash."""
    return line.lstrip().startswith("#")


def value_after(line, end):
    """The value after position end: a quoted string or a bare word."""
    rest = line[end:].lstrip()
    if rest[:1] in ('"', "'"):
        close = rest.find(rest[0], 1)
        return rest[1:] if close < 0 else rest[1:close]
    return re.match(r"""[^\s\\)\]},;"']*""", rest).group(0)


def is_off(value):
    """True only for a literal off: OFF, FALSE or 0, in any case."""
    return value.casefold() in OFF_VALUES


def _shown(value):
    """The value as a message prints it."""
    return value if value else "(empty)"


def _blank(text, matches):
    """The text with each match's span replaced by spaces of the same length."""
    for match in matches:
        start, end = match.span()
        text = text[:start] + " " * (end - start) + text[end:]
    return text


def check_generic(path, lines, problems):
    """The value forms, applied in order, each span blanked before the next."""
    forms = (DFLAG_RE, SET_RE, OPTION_RE, DEPENDENT_OPTION_RE, ASSIGN_RE, KEY_RE)
    options = 0
    for number, line in enumerate(lines, 1):
        if is_comment(line):
            continue
        work = line
        for form in forms:
            matches = list(form.finditer(work))
            for match in matches:
                value = value_after(line, match.end())
                if form is DEPENDENT_OPTION_RE:
                    _add(problems, path, number, 0,
                         "ENABLE_QML declared with cmake_dependent_option "
                         "(must be a plain option(... OFF))")
                elif form is OPTION_RE:
                    options += 1
                    if not is_off(value):
                        _add(problems, path, number, 0,
                             "option(ENABLE_QML) defaults to {} "
                             "(must be OFF)".format(_shown(value)))
                elif not is_off(value):
                    _add(problems, path, number, 0,
                         "ENABLE_QML set to {}; a release build configuration "
                         "must leave it OFF".format(_shown(value)))
            work = _blank(work, matches)
    if path == "CMakeLists.txt" and options != 1:
        _add(problems, path, None, 0,
             'expected one option(ENABLE_QML "..." OFF), found {}'.format(options))


def _parser_lines(lines):
    """The indexes of the allowed parser pair of build_targets.sh: the first
    pattern line, and the next non-blank line when it is the assignment."""
    for index, line in enumerate(lines):
        if line.strip() != BT_PARSER_PATTERN:
            continue
        for following in range(index + 1, len(lines)):
            if lines[following].strip():
                if lines[following].strip() == BT_PARSER_ASSIGN:
                    return index, following
                return index, None
        return index, None
    return None, None


def check_build_targets(lines, problems):
    """The default, the argument parser and the mapping of build_targets.sh."""
    path = "build_targets.sh"
    pattern_at, assign_at = _parser_lines(lines)
    defaults = 0
    mapping_seen = False
    previous = ""
    for index, line in enumerate(lines):
        number = index + 1
        if is_comment(line):
            continue
        prior = previous
        if line.strip():
            previous = line.strip()
        if BT_ECHO_RE.match(line):
            continue
        flags = list(DFLAG_RE.finditer(line))
        for match in flags:
            value = value_after(line, match.end())
            if is_off(value):
                continue
            if (
                line.strip() == BT_MAPPING_LINE
                and prior == BT_GUARD_LINE
                and not mapping_seen
            ):
                mapping_seen = True
                continue
            _add(problems, path, number, 0,
                 "-DENABLE_QML={} outside the ENABLE_QML=true branch of the "
                 "mapping".format(_shown(value)))
        reassigned = False
        for match in ASSIGN_RE.finditer(_blank(line, flags)):
            if match.start() == 0 and BT_DEFAULT_RE.match(line):
                defaults += 1
                value = value_after(line, match.end())
                if not is_off(value):
                    _add(problems, path, number, 0,
                         "ENABLE_QML defaults to {} (must be false)".format(_shown(value)))
            elif index not in (pattern_at, assign_at):
                reassigned = True
        if BT_NOEQ_SET_RE.search(line):
            reassigned = True
        if reassigned:
            _add(problems, path, number, 0,
                 "ENABLE_QML assigned outside its default and the argument parser")
    if defaults != 1:
        _add(problems, path, None, 0,
             "expected one top-level ENABLE_QML default, found {}".format(defaults))


def on_block(lines):
    """(start, end) of the workflow's on block, end exclusive, or None.

    The block ends at the next line that starts with a non-space character
    and is neither a comment nor a block-sequence item: a list that is the
    value of on: may sit at the indentation of on: itself."""
    start = None
    for index, line in enumerate(lines):
        if re.match(r"""["']?on["']?:""", line):
            start = index
            break
    if start is None:
        return None
    end = len(lines)
    for index in range(start + 1, len(lines)):
        line = lines[index]
        if line[:1].strip() and not is_comment(line) and not re.match(r"-(\s|$)", line):
            end = index
            break
    return start, end


def push_subblock(lines, index, end):
    """The lines after lines[index], before end, indented deeper than it."""
    indent = len(lines[index]) - len(lines[index].lstrip())
    sub = []
    for line in lines[index + 1:end]:
        if not line.strip() or is_comment(line):
            continue
        if len(line) - len(line.lstrip()) <= indent:
            break
        sub.append(line)
    return sub


def _key_of(text):
    """The key normaliser: the key of a stripped key line, read with one pair
    of matching quotes removed and blanks allowed before the colon, or None
    for any other line."""
    match = re.match(r"""(?:"([^"]*)"|'([^']*)'|([^\s"'#:][^\s:]*))\s*:""", text)
    if match is None:
        return None
    return next(group for group in match.groups() if group is not None)


def _has_key(body, key):
    """True when a non-comment line of body, stripped, is the key followed by
    its colon, read through the key normaliser."""
    return any(_key_of(line.strip()) == key for line in body if not is_comment(line))


def _uncommented(text):
    """The text without a trailing comment."""
    return re.sub(r"(^|\s)#.*", "", text)


def _events_named(text):
    """The names of TAG_EVENT_NAMES that the text names as words."""
    return set(re.findall(r"\b({})\b".format("|".join(TAG_EVENT_NAMES)), text))


def on_list_events(body):
    """The event names of the on block's YAML list: the sequence items at the
    block's first indentation level, or the names in a flow list that opens
    the block. Items nested under a key are never event names."""
    lines = [_uncommented(line) for line in body
             if line.strip() and not is_comment(line)]
    if not lines:
        return set()
    if lines[0].strip().startswith("["):
        names = set()
        for line in lines:
            names |= _events_named(line)
            if "]" in line:
                break
        return names
    level = min(len(line) - len(line.lstrip()) for line in lines)
    names = set()
    for line in lines:
        if len(line) - len(line.lstrip()) != level:
            continue
        match = re.match(r"""-\s*["']?([A-Za-z_]+)["']?\s*$""", line.strip())
        if match:
            names.add(match.group(1))
    return names


def tag_reach(lines):
    """The first reason of TAG_REACH_ORDER by which the workflow can run on a
    tag ref, or None."""
    span = on_block(lines)
    inline = False
    body = []
    unfiltered_push = False
    if span is not None:
        start, end = span
        inline = bool(_events_named(_uncommented(lines[start].split(":", 1)[1])))
        body = lines[start + 1:end]
        for index in range(start + 1, end):
            line = lines[index]
            if is_comment(line) or _key_of(line.strip()) != "push":
                continue
            sub = push_subblock(lines, index, end)
            if not _has_key(sub, "branches") and not _has_key(sub, "branches-ignore"):
                unfiltered_push = True
    holds = {
        "inline on": inline,
        "on list": bool(on_list_events(body) & set(TAG_EVENT_NAMES)),
        "no on block": span is None,
        "release": _has_key(body, "release"),
        "workflow_call": _has_key(body, "workflow_call"),
        "create": _has_key(body, "create"),
        "deployment": _has_key(body, "deployment"),
        "deployment_status": _has_key(body, "deployment_status"),
        "registry_package": _has_key(body, "registry_package"),
        "push: tags": _has_key(body, "tags"),
        "push: tags-ignore": _has_key(body, "tags-ignore"),
        "push without a branch filter": unfiltered_push,
        "workflow_dispatch": _has_key(body, "workflow_dispatch"),
    }
    for reason in TAG_REACH_ORDER:
        if holds[reason]:
            return reason
    return None


def release_capable(lines):
    """True when the workflow can run on a tag ref or holds a RELEASE_RE line:
    one of the actions it names (softprops/action-gh-release,
    actions/upload-release-asset, ncipollo/release-action,
    docker/build-push-action, actions/upload-pages-artifact,
    actions/deploy-pages), a gh release call or a gh api call on a releases
    path (read or write), push-to-obs.sh, an uploads.github.com URL, or a
    reusable-workflow call."""
    if tag_reach(lines) is not None:
        return True
    return any(RELEASE_RE.search(line) for line in lines if not is_comment(line))


def check_exempt_workflow(lines, problems):
    """cmake_qml.yml uses only allowed in-run actions, names no network
    publisher, and uploads no artifact while it can run on a tag ref."""
    path = EXEMPT_WORKFLOW
    allowed = "{} and {}".format(", ".join(ALLOWED_ACTIONS[:-1]), ALLOWED_ACTIONS[-1])
    publisher = None
    for number, line in enumerate(lines, 1):
        if is_comment(line):
            continue
        artifact = ARTIFACT_RE.search(line)
        if artifact and publisher is None:
            publisher = (number, artifact.group(0))
        rest = line
        for uses in USES_RE.finditer(line):
            target = uses.group(1)
            name = target.split("@", 1)[0]
            if name not in ALLOWED_ACTIONS:
                _add(problems, path, number, 0,
                     "uses {}, which is not an allowed in-run action; the "
                     "exemption allows only {}".format(_shown(target), allowed))
            start, end = uses.span()
            rest = rest[:start] + " " * (end - start) + rest[end:]
        deny = RUN_DENY_RE.search(rest)
        if deny:
            _add(problems, path, number, 0,
                 "reaches a network publisher ({}); the exemption covers "
                 "workflow artifacts only".format(deny.group(0)))
    if publisher is None:
        return
    reason = tag_reach(lines)
    if reason is not None:
        _add(problems, path, publisher[0], 0,
             "publishes ({}) and can run on a tag ref ({}); its exemption needs "
             "a workflow that never runs on a tag".format(publisher[1], reason))


def check_reuse(path, lines, problems):
    """A release-capable workflow neither names cmake_qml.yml nor triggers on
    another workflow's run."""
    for number, line in enumerate(lines, 1):
        if is_comment(line):
            continue
        if REUSE_RE.search(line) or WORKFLOW_RUN_KEY_RE.search(line):
            _add(problems, path, number, 1,
                 "names cmake_qml.yml or triggers on another workflow's run; its "
                 "exemption holds only while no release-capable workflow calls it "
                 "or triggers on it")


def check_cross_run(path, lines, problems):
    """A release-capable workflow reads no other run's artifacts."""
    for number, line in enumerate(lines, 1):
        if is_comment(line):
            continue
        if CROSS_RUN_RE.search(line):
            _add(problems, path, number, 2,
                 "downloads artifacts from another workflow run; cmake_qml.yml's "
                 "exemption assumes no release run can collect its artifacts")


def check_rules_guard(path, lines, problems):
    """A rules file includes rules.qml only inside RULES_GUARD, and not in its
    else branch."""
    stack = []
    for number, line in enumerate(lines, 1):
        if is_comment(line):
            continue
        text = line.strip()
        if "rules.qml" in text:
            if not stack or stack[-1][0] != RULES_GUARD or stack[-1][1]:
                _add(problems, path, number, 3,
                     'rules.qml referenced outside an "{}" block'.format(RULES_GUARD))
        if text.startswith(("ifeq", "ifneq", "ifdef", "ifndef")):
            stack.append([text, False])
        elif text == "else" or text.startswith("else "):
            if stack:
                stack[-1][1] = True
        elif text.split(None, 1)[:1] == ["endif"]:
            if stack:
                stack.pop()


def check_variant_refs(path, lines, problems):
    """No workflow but cmake_qml.yml, and no build_targets.sh, packaging or
    debian file but the two rules files, names a QML variant file."""
    manifest = os.path.basename(QML_FLATPAK_MANIFEST)
    for number, line in enumerate(lines, 1):
        if is_comment(line):
            continue
        for name in ("rules.qml", manifest):
            if name in line:
                _add(problems, path, number, 4,
                     "references the QML variant file {}".format(name))
                break


def check_deb_options(path, lines, problems):
    """No workflow but cmake_qml.yml, and no build_targets.sh, packaging or
    debian file, adds qml to DEB_BUILD_OPTIONS."""
    for number, line in enumerate(lines, 1):
        if is_comment(line):
            continue
        if DEB_OPTS_RE.search(line):
            _add(problems, path, number, 5, "adds qml to DEB_BUILD_OPTIONS")


def main():
    top = git("rev-parse", "--show-toplevel")
    os.chdir(top.decode("utf8", errors="replace").strip())
    files = tracked_files()
    problems = []
    listed = set(files) | set(LIFTED)
    for path in REQUIRED:
        if path not in listed or not os.path.isfile(path):
            _missing(problems, path)
    checked = 0
    for path in files:
        lines = read_lines(path, problems)
        if lines is None:
            continue
        checked += 1
        if path == EXEMPT_WORKFLOW:
            check_exempt_workflow(lines, problems)
            continue
        workflow = path.startswith(".github/workflows/")
        targets = path == "build_targets.sh"
        packaging = path.startswith(("depends/", "contrib/flatpak/", "contrib/obs/"))
        debian = path.startswith(("debian/", "debian-testnet/"))
        if targets:
            check_build_targets(lines, problems)
        else:
            check_generic(path, lines, problems)
        if workflow and release_capable(lines):
            check_reuse(path, lines, problems)
            check_cross_run(path, lines, problems)
        if path in RULES_FILES:
            check_rules_guard(path, lines, problems)
        elif workflow or targets or packaging or debian:
            check_variant_refs(path, lines, problems)
        if workflow or targets or packaging or debian:
            check_deb_options(path, lines, problems)
    if problems:
        for _key, text in sorted(problems, key=lambda entry: entry[0]):
            print(text)
        print("lint-qml-release: {} problem(s); no official build may enable "
              "QML before the cut-over".format(len(problems)))
        return 1
    print("lint-qml-release: OK ({} files checked)".format(checked))
    return 0


if __name__ == "__main__":
    sys.exit(main())
