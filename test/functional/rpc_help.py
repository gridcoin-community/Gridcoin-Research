#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Exercise the help RPC and RPCHelpMan arity validation across every
converted RPC the running daemon advertises.

This test is the motivating use-case for the functional-test framework: the
RPCHelpMan rollout tracked at #2922 changes RPC help rendering and moves
argument-count validation from ad-hoc params.size() checks into
`RPCHelpMan::IsValidNumArgs()`. Per-command conversion PRs need automated
coverage so a regression is caught before merge.

Discovery model
---------------
Rather than maintaining a hand-curated list of converted commands (which
goes stale the moment a new conversion PR merges), the test walks the help
RPC's categories to enumerate every command the daemon advertises, then
classifies each as RPCHelpMan-converted or legacy by inspecting its
`help <name>` output. Converted commands -- the ones whose help contains
both `Result:` and `Examples:` sections -- get the full pattern check.
Legacy commands are logged and skipped.

That means a reviewer validating a pending RPCHelpMan PR (#2958-#2975 as
of this writing, plus future ones) needs only:

  git checkout <the-pr-branch>
  cmake --build build
  python3 test/functional/test_runner.py rpc_help.py

The newly-converted commands are auto-discovered as `RPCHelpMan` and the
pattern check runs against them. No edits to this file are required.

Regression guard
----------------
`MIN_CONVERTED_COMMANDS` is a floor on how many converted commands the
test expects to find. If it ever drops below that, a previously-converted
command has reverted to legacy (or discovery itself is broken). Bump the
floor whenever a new tier merges.

What it catches per converted command
-------------------------------------
  - `help <cmd>` includes Result:/Examples: sections (format regression).
  - Wrong-arity call is rejected. Most converted commands fail IsValidNumArgs
    and surface RPC_MISC_ERROR (-1) with help.ToString() in the message.
    Variadic commands accept the 100 args and fall through to per-argument
    validation, raising RPC_INVALID_PARAMETER (-8) from the function body.
    Either constitutes "the command rejected the call", which is enough to
    verify dispatcher + arity-or-arg-validation behavior end-to-end.

What it does NOT catch (deferred to richer tests once regtest lands)
-------------------------------------------------------------------
  - Per-field return-shape drift on OBJ-returning RPCs (only the
    representative getblockchaininfo spot-check is asserted here).
  - Happy-path return values for commands that require wallet/chain/peer
    state -- those need Phase 2A regtest to set up deterministically.
"""

from test_framework.authproxy import JSONRPCException
from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal


# JSON-RPC error codes (src/rpc/protocol.h).
#
# RPC_MISC_ERROR (-1) is emitted by the dispatcher when an RPC handler throws
# std::runtime_error -- exactly what `throw runtime_error(help.ToString())`
# does on an arity violation. RPC_INVALID_PARAMETER (-8) is what *some*
# RPCs throw when they reach the function body with the wrong shape (e.g.,
# variadic commands that accept the 100 args and then fail on individual
# argument validation). Either constitutes "the command rejected the bad
# call", which is the per-command invariant we want to assert.
RPC_MISC_ERROR = -1
RPC_INVALID_PARAMETER = -8
RPC_ARITY_REJECTION_CODES = (RPC_MISC_ERROR, RPC_INVALID_PARAMETER)

# Categories advertised by the help RPC (see src/rpc/server.cpp). The help
# RPC accepts each of these as a single-word category filter and returns a
# listing of commands in that category.
HELP_CATEGORIES = ("wallet", "staking", "developer", "network", "voting")

# Universal arity-violation argument list. RPCHelpMan::IsValidNumArgs
# rejects calls where params.size() > m_args.size(); passing far more args
# than any sensible RPC declares is the simplest way to trip it without
# knowing each command's signature.
ARITY_VIOLATION_ARGS = ["x"] * 100

# Floor for the converted-command count. Bump when a new tier merges. If
# discovery ever returns fewer than this, a previously-converted command
# has reverted to legacy (regression) or the discovery walk itself broke.
# Tier 1a (#2954, 21 commands) and Tier 1b (#2956, 17 commands) are merged
# as of 2026-05-25 along with a handful of earlier conversions; 30 is a
# conservative lower bound that still catches significant regressions.
MIN_CONVERTED_COMMANDS = 30

# RPCHelpMan output markers. Both must appear in `help <cmd>` for the
# command to be considered converted -- RPCHelpMan::ToString always emits
# both sections, while legacy throw-runtime_error help strings do not.
HELPMAN_MARKERS = ("Result", "Examples")

# Representative OBJ-returning commands worth spot-checking for top-level
# field drift. Each entry maps command name -> set of top-level fields
# that must be present in the result. Restricted to commands that work
# against a fresh -testnet datadir (genesis-only, no peers, no wallet
# activity).
#
# Schema notes (Gridcoin diverges from Bitcoin Core intentionally):
#   - getblockchaininfo emits `testnet: bool` (driven by fTestNet) instead
#     of `chain: str`, no bestblockhash, no softforks. The fields below are
#     all unconditional pushKVs in src/rpc/blockchain.cpp:getblockchaininfo
#     -- stable enough to assert on without flapping.
OBJ_SHAPE_CHECKS = {
    "getblockchaininfo": {
        "blocks", "in_sync", "moneysupply", "difficulty", "testnet", "errors",
    },
}


class RpcHelpTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "test"
        self.setup_clean_chain = True
        self.extra_args = [["-connect=0", "-listen=0"]]

    def setup_network(self):
        # Single isolated node is enough for help RPCs; no P2P, no sync.
        self.setup_nodes()

    # --- System-level checks (not per command) ------------------------------

    def check_help_overview(self, node):
        """`help` with no args returns the help-on-help category overview."""
        text = node.help()
        assert isinstance(text, str)
        for token in ("help", "Categories", "wallet", "staking", "network"):
            assert token in text, f"help() output missing '{token}'\n---\n{text}"

    def check_help_unknown(self, node):
        """`help <unknown cmd>` responds sanely rather than crashing."""
        text = node.help("zzz_definitely_not_a_real_rpc")
        assert isinstance(text, str)
        # CRPCTable::help returns "help: unknown command: <name>" when no
        # command matches; assert tolerantly so wording tweaks don't break us.
        assert "unknown command" in text.lower() or text.strip() == "", (
            f"help(unknown) didn't report unknown-command sanely: {text!r}"
        )

    # --- Discovery ----------------------------------------------------------

    def discover_commands(self, node):
        """Enumerate every RPC the daemon advertises across all help categories."""
        names = set()
        for category in HELP_CATEGORIES:
            try:
                text = node.help(category)
            except Exception as e:  # noqa: BLE001 - help on category should not fail
                self.log.warning("help(%s) failed: %s", category, e)
                continue
            for line in text.splitlines():
                line = line.strip()
                if not line:
                    continue
                tokens = line.split()
                if not tokens:
                    continue
                cand = tokens[0]
                # RPC command names are lowercase identifiers (a-z, 0-9, _).
                # Filter out section headers, blank lines, prose, etc.
                if cand and cand[0].islower() and all(
                    c.isalnum() or c == "_" for c in cand
                ):
                    names.add(cand)
        return sorted(names)

    def is_converted(self, node, name):
        """Return (True, help_text) if `help <name>` looks RPCHelpMan-formatted."""
        try:
            text = node.help(name)
        except Exception:
            return False, ""
        if not isinstance(text, str):
            return False, ""
        return all(marker in text for marker in HELPMAN_MARKERS), text

    # --- Per-command pattern check -----------------------------------------

    def check_helpman_pattern(self, node, name, help_text):
        """Validate one RPCHelpMan-converted command end-to-end.

        Asserts the help text carries the RPCHelpMan structure and that
        IsValidNumArgs() rejects a clearly-too-many call.
        """
        # 1. Help format: command name + RPCHelpMan section markers must
        #    all be present. Already implicit in discovery, but assert
        #    here so the failure points at the specific command.
        for marker in (name,) + HELPMAN_MARKERS:
            assert marker in help_text, (
                f"help({name!r}) missing marker '{marker}'\n---\n{help_text}"
            )

        # 2. Wrong-arity call: passing 100 dummy positional args exceeds
        #    every realistic m_args.size(). Most converted commands reject
        #    this via IsValidNumArgs -> runtime_error -> RPC_MISC_ERROR (-1).
        #    Variadic commands (e.g. changesettings) accept the args and
        #    fail downstream with RPC_INVALID_PARAMETER (-8) on individual
        #    arg validation. Either outcome satisfies "the command rejected
        #    the bad call", which is the per-command invariant we want.
        method = getattr(node, name)
        try:
            method(*ARITY_VIOLATION_ARGS)
        except JSONRPCException as e:
            assert e.error['code'] in RPC_ARITY_REJECTION_CODES, (
                f"{name}: unexpected JSONRPC error code {e.error['code']} "
                f"on 100-arg call (expected one of {RPC_ARITY_REJECTION_CODES})"
            )
        else:
            raise AssertionError(
                f"{name}: 100-arg call did not raise -- the command "
                f"accepted clearly-bogus args without rejection"
            )

    # --- Test driver -------------------------------------------------------

    def run_test(self):
        node = self.nodes[0]

        self.log.info("help (no args) returns the category overview")
        self.check_help_overview(node)

        self.log.info("help (unknown command) responds gracefully")
        self.check_help_unknown(node)

        self.log.info("Discovering all advertised RPCs across categories")
        all_names = self.discover_commands(node)
        assert all_names, "Discovery returned no commands; help categories broken?"

        converted = []
        legacy = []
        for name in all_names:
            ok, text = self.is_converted(node, name)
            if ok:
                converted.append((name, text))
            else:
                legacy.append(name)

        self.log.info(
            "Discovered %d total RPCs: %d RPCHelpMan-converted, %d legacy",
            len(all_names), len(converted), len(legacy),
        )

        # Regression guard: fail loudly if we lost coverage on previously-
        # converted commands (catches accidental reverts to legacy or a
        # discovery walk that broke).
        assert len(converted) >= MIN_CONVERTED_COMMANDS, (
            f"Only {len(converted)} converted commands discovered; "
            f"expected at least {MIN_CONVERTED_COMMANDS}. "
            f"Either a conversion was reverted or discovery is broken.\n"
            f"Legacy commands seen: {legacy}"
        )

        # Pattern check every converted command.
        self.log.info("Validating RPCHelpMan pattern on %d converted commands",
                      len(converted))
        for name, help_text in converted:
            self.log.info("  -> %s", name)
            self.check_helpman_pattern(node, name, help_text)

        # Spot-check representative OBJ-returning commands for field drift.
        self.log.info("Spot-checking OBJ result shapes")
        for name, expected_fields in OBJ_SHAPE_CHECKS.items():
            self.log.info("  -> %s expects fields %s", name, sorted(expected_fields))
            result = getattr(node, name)()
            assert isinstance(result, dict), (
                f"{name} returned non-dict: {type(result).__name__}"
            )
            missing = expected_fields - set(result.keys())
            assert not missing, (
                f"{name} result missing expected fields: {sorted(missing)}\n"
                f"got: {sorted(result.keys())}"
            )

        # Final genesis-only sanity: prove the dispatcher answers real data,
        # not just error paths. getblockcount and getbestblockhash are
        # converted and don't need any chain state beyond genesis.
        assert_equal(node.getblockcount(), 0)
        assert_equal(node.getbestblockhash(), node.getblockhash(0))


if __name__ == "__main__":
    RpcHelpTest().main()
