#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Tests for the decision in ci-gate.py. They need no network."""

import copy
import glob
import importlib.util
import os
import re
import unittest

_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "ci-gate.py")
_SPEC = importlib.util.spec_from_file_location("ci_gate", _PATH)
assert _SPEC is not None and _SPEC.loader is not None
gate = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(gate)

REPO = "o/r"
MERGE = "m" * 40
HEAD = "h" * 40
TREE = "t" * 40
QUALITY = ".github/workflows/cmake_quality.yml"
DISTROS = ".github/workflows/cmake_distros.yml"

PUSH_ENV = {
    "GITHUB_EVENT_NAME": "push",
    "GITHUB_REF": "refs/heads/development",
    "GITHUB_REPOSITORY": REPO,
    "GITHUB_SHA": MERGE,
    "GITHUB_WORKFLOW_REF": "o/r/%s@refs/heads/development" % QUALITY,
}


def run_entry(path, run_id, number, status="completed", conclusion="success"):
    return {"path": path, "id": run_id, "run_number": number,
            "status": status, "conclusion": conclusion}


def replies():
    """The API replies of the case that skips."""
    return {
        "repos/o/r/commits/%s/pulls" % MERGE: [{
            "number": 7, "merged_at": "2026-01-01T00:00:00Z", "merge_commit_sha": MERGE,
            "base": {"ref": "development"}, "head": {"sha": HEAD}}],
        "repos/o/r/pulls/7/files?per_page=100&page=1": [{"filename": "src/init.cpp"}],
        "repos/o/r/actions/runs?head_sha=%s&event=pull_request&per_page=100&page=1" % HEAD: {
            "workflow_runs": [run_entry(QUALITY, 11, 5), run_entry(DISTROS, 12, 9)]},
        "repos/o/r/git/commits/%s" % MERGE: {"tree": {"sha": TREE}},
        "repos/o/r/actions/runs/11/artifacts?per_page=100&page=1": {
            "artifacts": [{"name": "ci-tested-tree-" + TREE, "expired": False},
                          {"name": "unrelated", "expired": False}]},
    }


def api_from(table):
    def api(path):
        return copy.deepcopy(table[path])
    return api


def decide(env=None, table=None):
    return gate.decide(PUSH_ENV if env is None else env,
                       api_from(replies() if table is None else table))


RUNS_KEY = "repos/o/r/actions/runs?head_sha=%s&event=pull_request&per_page=100&page=1" % HEAD
ARTIFACTS_KEY = "repos/o/r/actions/runs/11/artifacts?per_page=100&page=1"
PULLS_KEY = "repos/o/r/commits/%s/pulls" % MERGE
FILES_KEY = "repos/o/r/pulls/7/files?per_page=100&page=1"


DEPENDS_CACHE = "os-depends-mp-host-" + "d" * 64
FLATPAK_CACHE = "flatpak-builder-" + "f" * 64
CACHES_ENV = dict(PUSH_ENV, CI_GATE_SEEDED_CACHES="%s\n  %s\n" % (DEPENDS_CACHE, FLATPAK_CACHE))


def cache_key(key):
    return ("repos/o/r/actions/caches?key=%s&ref=refs%%2Fheads%%2Fdevelopment"
            "&per_page=100&page=1" % key)


def cache_entry(key, ref="refs/heads/development"):
    return {"key": key, "ref": ref}


def replies_with_caches():
    """The API replies of the case that skips with both seeded caches present."""
    table = replies()
    table[cache_key(DEPENDS_CACHE)] = {"actions_caches": [cache_entry(DEPENDS_CACHE)]}
    table[cache_key(FLATPAK_CACHE)] = {"actions_caches": [cache_entry(FLATPAK_CACHE)]}
    return table


class SkipTests(unittest.TestCase):
    def test_identical_tree_and_green_runs_skip(self):
        run, reason, tested = decide()
        self.assertFalse(run, reason)
        self.assertIsNone(tested)

    def test_skips_when_the_branch_holds_every_seeded_cache(self):
        run, reason, tested = decide(CACHES_ENV, replies_with_caches())
        self.assertFalse(run, reason)
        self.assertIsNone(tested)

    def test_no_seeded_caches_named_asks_for_none(self):
        # replies() has no cache listing, so a query would raise.
        self.assertFalse(decide(dict(PUSH_ENV, CI_GATE_SEEDED_CACHES=" \n"))[0])


class SeededCacheTests(unittest.TestCase):
    def assert_runs(self, table, needle):
        run, reason, _ = decide(CACHES_ENV, table)
        self.assertTrue(run, reason)
        self.assertIn(needle, reason)

    def test_missing_cache_runs(self):
        for key in (DEPENDS_CACHE, FLATPAK_CACHE):
            table = replies_with_caches()
            table[cache_key(key)] = {"actions_caches": []}
            self.assert_runs(table, "holds no cache " + key)

    def test_longer_key_with_the_same_prefix_does_not_count(self):
        table = replies_with_caches()
        table[cache_key(FLATPAK_CACHE)] = {"actions_caches": [cache_entry(FLATPAK_CACHE + "-x")]}
        self.assert_runs(table, "holds no cache " + FLATPAK_CACHE)

    def test_cache_of_another_ref_does_not_count(self):
        table = replies_with_caches()
        table[cache_key(DEPENDS_CACHE)] = {
            "actions_caches": [cache_entry(DEPENDS_CACHE, "refs/pull/7/merge")]}
        self.assert_runs(table, "holds no cache " + DEPENDS_CACHE)

    def test_run_decided_earlier_asks_for_no_cache(self):
        # No cache listing in the table: a query would raise.
        table = replies()
        table[ARTIFACTS_KEY]["artifacts"][0]["name"] = "ci-tested-tree-" + "u" * 40
        self.assert_runs(table, "recorded tree " + "u" * 40)

    def test_cache_listing_error_is_not_swallowed_by_decide(self):
        table = replies_with_caches()
        del table[cache_key(FLATPAK_CACHE)]
        with self.assertRaises(KeyError):
            decide(CACHES_ENV, table)


class AlwaysRunTests(unittest.TestCase):
    def assert_runs(self, env=None, table=None, needle=""):
        run, reason, _ = decide(env, table)
        self.assertTrue(run, reason)
        self.assertIn(needle, reason)

    def test_tag_runs_without_asking_the_api(self):
        env = dict(PUSH_ENV, GITHUB_REF="refs/tags/5.5.3.0")
        self.assert_runs(env, {}, "refs/tags/5.5.3.0")

    def test_other_branch_runs(self):
        self.assert_runs(dict(PUSH_ENV, GITHUB_REF="refs/heads/master"), {}, "refs/heads/master")

    def test_manual_and_scheduled_runs_run(self):
        for event in ("workflow_dispatch", "schedule", ""):
            self.assert_runs(dict(PUSH_ENV, GITHUB_EVENT_NAME=event), {}, "event is")

    def test_local_run_runs(self):
        self.assert_runs(dict(PUSH_ENV, ACT="true"), {}, "local run")

    def test_direct_push_runs(self):
        table = replies()
        table[PULLS_KEY] = []
        self.assert_runs(table=table, needle="0 merged pull requests")

    def test_commit_inside_an_open_pull_request_runs(self):
        table = replies()
        table[PULLS_KEY][0]["merged_at"] = None
        self.assert_runs(table=table, needle="0 merged pull requests")

    def test_commit_that_is_not_the_merge_commit_runs(self):
        table = replies()
        table[PULLS_KEY][0]["merge_commit_sha"] = "x" * 40
        self.assert_runs(table=table, needle="0 merged pull requests")

    def test_pull_request_into_another_base_runs(self):
        table = replies()
        table[PULLS_KEY][0]["base"]["ref"] = "master"
        self.assert_runs(table=table, needle="0 merged pull requests")

    def test_cache_seed_paths_run(self):
        for entry in ({"filename": "depends/packages/qt.mk"},
                      {"filename": "contrib/flatpak/us.gridcoin.GridcoinResearch.yml"},
                      {"filename": "doc/x.mk", "previous_filename": "depends/x.mk"}):
            table = replies()
            table[FILES_KEY].append(entry)
            self.assert_runs(table=table, needle="whose cache this branch seeds")

    def test_any_run_not_green_runs(self):
        for status, conclusion in (("completed", "cancelled"), ("completed", "failure"),
                                   ("completed", "timed_out"), ("in_progress", None),
                                   ("queued", None)):
            table = replies()
            table[RUNS_KEY]["workflow_runs"][1] = run_entry(DISTROS, 12, 9, status, conclusion)
            self.assert_runs(table=table, needle=DISTROS)

    def test_latest_run_of_a_workflow_decides(self):
        table = replies()
        table[RUNS_KEY]["workflow_runs"].append(run_entry(DISTROS, 13, 10, "completed", "failure"))
        self.assert_runs(table=table, needle=DISTROS)

        table = replies()
        table[RUNS_KEY]["workflow_runs"].append(run_entry(DISTROS, 10, 8, "completed", "failure"))
        self.assertFalse(decide(table=table)[0])

    def test_missing_own_run_runs(self):
        table = replies()
        table[RUNS_KEY]["workflow_runs"] = [run_entry(DISTROS, 12, 9)]
        self.assert_runs(table=table, needle="has no run of " + QUALITY)

    def test_no_runs_at_all_runs(self):
        table = replies()
        table[RUNS_KEY]["workflow_runs"] = []
        self.assert_runs(table=table, needle="has no run of")

    def test_different_tree_runs(self):
        table = replies()
        table[ARTIFACTS_KEY]["artifacts"][0]["name"] = "ci-tested-tree-" + "u" * 40
        self.assert_runs(table=table, needle="recorded tree " + "u" * 40)

    def test_missing_record_runs(self):
        table = replies()
        table[ARTIFACTS_KEY]["artifacts"] = [{"name": "unrelated", "expired": False}]
        self.assert_runs(table=table, needle="recorded tree none")

    def test_expired_record_runs(self):
        table = replies()
        table[ARTIFACTS_KEY]["artifacts"][0]["expired"] = True
        self.assert_runs(table=table, needle="recorded tree none")

    def test_two_different_records_run(self):
        table = replies()
        table[ARTIFACTS_KEY]["artifacts"].append(
            {"name": "ci-tested-tree-" + "u" * 40, "expired": False})
        self.assert_runs(table=table, needle="recorded tree")

    def test_api_error_is_not_swallowed_by_decide(self):
        table = replies()
        del table[ARTIFACTS_KEY]
        with self.assertRaises(KeyError):
            decide(table=table)


class WorkflowTests(unittest.TestCase):
    """Every cache a workflow keys by a hash of files is named to its gate."""

    def test_gate_names_every_hash_keyed_cache(self):
        workflows = os.path.join(os.path.dirname(_PATH), "..", "..", ".github", "workflows")
        paths = sorted(glob.glob(os.path.join(workflows, "*.yml")))
        self.assertTrue(paths)
        named_somewhere = False
        for path in paths:
            with open(path, encoding="utf8") as handle:
                lines = handle.read().splitlines()
            hosts = sorted({match.group(1) for match in
                            (re.match(r"\s+host: (\S+)$", line) for line in lines) if match})
            expected = set()
            for line in lines:
                match = re.match(r"\s+key: (.*hashFiles\(.*)$", line)
                if not match:
                    continue
                key = match.group(1)
                if "${{ matrix.host }}" in key:
                    self.assertTrue(hosts, path)
                    expected.update(key.replace("${{ matrix.host }}", host) for host in hosts)
                else:
                    expected.add(key)
            named = set()
            for index, line in enumerate(lines):
                if line.strip() != "CI_GATE_SEEDED_CACHES: |":
                    continue
                indent = len(line) - len(line.lstrip())
                for entry in lines[index + 1:]:
                    if len(entry) - len(entry.lstrip()) <= indent:
                        break
                    named.add(entry.strip())
            self.assertEqual(named, expected, path)
            named_somewhere = named_somewhere or bool(named)
        self.assertTrue(named_somewhere)


class PullRequestTests(unittest.TestCase):
    def test_pull_request_run_reports_its_tree(self):
        env = dict(PUSH_ENV, GITHUB_EVENT_NAME="pull_request", GITHUB_REF="refs/pull/7/merge")
        run, _, tested = decide(env, {"repos/o/r/git/commits/%s" % MERGE: {"tree": {"sha": TREE}}})
        self.assertTrue(run)
        self.assertEqual(tested, TREE)


class HelperTests(unittest.TestCase):
    def test_workflow_path(self):
        self.assertEqual(gate.workflow_path("o/r/%s@refs/pull/7/merge" % QUALITY), QUALITY)

    def test_paged_follows_full_pages(self):
        pages = {"x?per_page=100&page=1": list(range(100)), "x?per_page=100&page=2": [100]}
        self.assertEqual(gate.paged(api_from(pages), "x"), list(range(101)))

    def test_main_runs_on_error(self):
        saved = dict(os.environ)
        try:
            os.environ.clear()
            self.assertEqual(gate.main(), 0)
        finally:
            os.environ.update(saved)


if __name__ == "__main__":
    unittest.main()
