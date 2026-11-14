#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Decide whether a CI run has to do its work.

A pull request run tests the pull request merged into its base. When that pull
request is then merged and the base has not moved, the push run on the base
branch builds and tests exactly the same tree a second time. This script lets
the "CI Gate" job of each workflow skip that second run.

On a pull request run it reports the tree being tested, and the job records
it as an artifact named ci-tested-tree-<tree>.

On a push to the gated branch it answers "skip" only when all of these hold:

  - the pushed commit is the merge commit of exactly one merged pull request,
  - the pull request did not touch a path whose build cache only a run on the
    base branch can seed for later pull requests,
  - the latest pull request run of every workflow on the pull request's head
    commit completed with success (a cancelled, failed, timed out or still
    running one does not count), and this workflow is among them,
  - this workflow's pull request run recorded a tree, the record has not
    expired, and it equals the tree of the pushed commit.

Everything else runs: tags, manual and scheduled runs, other branches, direct
pushes, local runs, and any error or doubt while deciding.

The decision is in decide(), which takes the environment and a function that
performs a GitHub API GET, so test-ci-gate.py exercises it without a network.
"""

import json
import os
import subprocess
import sys
from typing import Any, Callable, Dict, List, Mapping, Optional, Tuple

GATED_REF = "refs/heads/development"
ARTIFACT_PREFIX = "ci-tested-tree-"

# Caches keyed by a hash of these paths are readable by later pull requests
# only when a run on the base branch saved them.
CACHE_SEED_PREFIXES = ("depends/", "contrib/flatpak/")

PER_PAGE = 100
MAX_PAGES = 50

Api = Callable[[str], Any]
Decision = Tuple[bool, str, Optional[str]]


def paged(api: Api, path: str, key: Optional[str] = None) -> List[Any]:
    """Collect every page of a list endpoint. key names the list in an object reply."""
    items: List[Any] = []
    separator = "&" if "?" in path else "?"
    for page in range(1, MAX_PAGES + 1):
        reply = api("%s%sper_page=%d&page=%d" % (path, separator, PER_PAGE, page))
        chunk = reply[key] if key is not None else reply
        items.extend(chunk)
        if len(chunk) < PER_PAGE:
            return items
    raise RuntimeError("too many pages from %s" % path)


def workflow_path(workflow_ref: str) -> str:
    """'owner/repo/.github/workflows/x.yml@refs/heads/y' -> '.github/workflows/x.yml'."""
    path = workflow_ref.split("@", 1)[0]
    return path.split("/", 2)[2]


def touches_cache_seed(files: List[Mapping[str, Any]]) -> Optional[str]:
    for entry in files:
        for name in (entry.get("filename"), entry.get("previous_filename")):
            if name and name.startswith(CACHE_SEED_PREFIXES):
                return name
    return None


def latest_run_per_workflow(runs: List[Mapping[str, Any]]) -> Dict[str, Mapping[str, Any]]:
    latest: Dict[str, Mapping[str, Any]] = {}
    for run in runs:
        path = run["path"].split("@", 1)[0]
        if path not in latest or run["run_number"] > latest[path]["run_number"]:
            latest[path] = run
    return latest


def decide(env: Mapping[str, str], api: Api) -> Decision:
    """Return (run, reason, tested_tree). tested_tree is set on pull request runs only."""
    if env.get("ACT"):
        return True, "local run", None

    event = env.get("GITHUB_EVENT_NAME", "")
    repo = env["GITHUB_REPOSITORY"]
    sha = env["GITHUB_SHA"]

    if event == "pull_request":
        tree = api("repos/%s/git/commits/%s" % (repo, sha))["tree"]["sha"]
        return True, "pull request run, testing tree %s" % tree, tree

    if event != "push":
        return True, "event is %s" % (event or "unknown"), None
    if env.get("GITHUB_REF") != GATED_REF:
        return True, "ref is %s" % env.get("GITHUB_REF"), None

    base = GATED_REF[len("refs/heads/"):]
    pulls = [pull for pull in api("repos/%s/commits/%s/pulls" % (repo, sha))
             if pull.get("merged_at")
             and pull.get("merge_commit_sha") == sha
             and pull["base"]["ref"] == base]
    if len(pulls) != 1:
        return True, "%d merged pull requests have this merge commit" % len(pulls), None
    number = pulls[0]["number"]
    head = pulls[0]["head"]["sha"]

    seed = touches_cache_seed(paged(api, "repos/%s/pulls/%d/files" % (repo, number)))
    if seed is not None:
        return True, "#%d changes %s, whose cache this branch seeds" % (number, seed), None

    runs = paged(api, "repos/%s/actions/runs?head_sha=%s&event=pull_request" % (repo, head),
                 "workflow_runs")
    latest = latest_run_per_workflow(runs)
    for path, run in sorted(latest.items()):
        if run.get("status") != "completed" or run.get("conclusion") != "success":
            return True, "#%d run of %s is %s/%s" % (
                number, path, run.get("status"), run.get("conclusion")), None

    own = workflow_path(env["GITHUB_WORKFLOW_REF"])
    if own not in latest:
        return True, "#%d has no run of %s on %s" % (number, own, head), None

    tree = api("repos/%s/git/commits/%s" % (repo, sha))["tree"]["sha"]
    artifacts = paged(api, "repos/%s/actions/runs/%d/artifacts" % (repo, latest[own]["id"]),
                      "artifacts")
    recorded = sorted({artifact["name"][len(ARTIFACT_PREFIX):] for artifact in artifacts
                       if artifact["name"].startswith(ARTIFACT_PREFIX)
                       and not artifact.get("expired")})
    if recorded != [tree]:
        return True, "#%d run recorded tree %s, this commit has %s" % (
            number, ",".join(recorded) or "none", tree), None

    return False, "#%d run %d tested this tree (%s) and every run on %s succeeded" % (
        number, latest[own]["id"], tree, head), None


def gh_api(path: str) -> Any:
    result = subprocess.run(["gh", "api", path], stdout=subprocess.PIPE, check=True, text=True)
    return json.loads(result.stdout)


def main() -> int:
    try:
        run, reason, tested_tree = decide(os.environ, gh_api)
    except Exception as error:  # Any doubt means a full run.
        run, reason, tested_tree = True, "could not decide (%s: %s)" % (
            type(error).__name__, error), None

    print("ci-gate: %s: %s" % ("run" if run else "skip", reason))
    output = os.environ.get("GITHUB_OUTPUT")
    if output:
        with open(output, "a", encoding="utf8") as handle:
            handle.write("run=%s\n" % ("true" if run else "false"))
            handle.write("tested-tree=%s\n" % (tested_tree or ""))
    record = os.environ.get("CI_GATE_RECORD")
    if record and tested_tree:
        with open(record, "w", encoding="utf8") as handle:
            handle.write("%s\n" % tested_tree)
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary and not run:
        with open(summary, "a", encoding="utf8") as handle:
            handle.write("Skipped: %s\n" % reason)
    return 0


if __name__ == "__main__":
    sys.exit(main())
