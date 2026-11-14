#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Tests for the decision in ci-gate.py. They need no network."""

import copy
import importlib.util
import os
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


class SkipTests(unittest.TestCase):
    def test_identical_tree_and_green_runs_skip(self):
        run, reason, tested = decide()
        self.assertFalse(run, reason)
        self.assertIsNone(tested)


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
