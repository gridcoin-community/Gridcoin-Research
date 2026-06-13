#!/usr/bin/env python3
# Copyright (c) 2014-2020 The Bitcoin Core developers
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Run regression test suite.

This module calls down into individual test cases via subprocess. It will
forward all unrecognized arguments onto the individual test scripts.

For a description of arguments recognized by test scripts, see
`test/functional/test_framework/test_framework.py:GridcoinTestFramework.main`.

"""

import argparse
from collections import deque
import configparser
import datetime
import os
import time
import shutil
import subprocess
import sys
import tempfile
import re
import logging
import unittest

# Formatting. Default colors to empty strings.
BOLD, GREEN, RED, GREY = ("", ""), ("", ""), ("", ""), ("", "")
try:
    # Make sure python thinks it can write unicode to its stdout
    "\u2713".encode("utf_8").decode(sys.stdout.encoding)
    TICK = "✓ "
    CROSS = "✖ "
    CIRCLE = "○ "
except UnicodeDecodeError:
    TICK = "P "
    CROSS = "x "
    CIRCLE = "o "

if os.name != 'nt' or sys.getwindowsversion() >= (10, 0, 14393):  # type: ignore[attr-defined]
    if os.name == 'nt':
        import ctypes
        kernel32 = ctypes.windll.kernel32  # type: ignore
        ENABLE_VIRTUAL_TERMINAL_PROCESSING = 4
        STD_OUTPUT_HANDLE = -11
        STD_ERROR_HANDLE = -12
        # Enable ascii color control to stdout
        stdout = kernel32.GetStdHandle(STD_OUTPUT_HANDLE)
        stdout_mode = ctypes.c_int32()
        kernel32.GetConsoleMode(stdout, ctypes.byref(stdout_mode))
        kernel32.SetConsoleMode(stdout, stdout_mode.value | ENABLE_VIRTUAL_TERMINAL_PROCESSING)
        # Enable ascii color control to stderr
        stderr = kernel32.GetStdHandle(STD_ERROR_HANDLE)
        stderr_mode = ctypes.c_int32()
        kernel32.GetConsoleMode(stderr, ctypes.byref(stderr_mode))
        kernel32.SetConsoleMode(stderr, stderr_mode.value | ENABLE_VIRTUAL_TERMINAL_PROCESSING)
    # primitive formatting on supported
    # terminal via ANSI escape sequences:
    BOLD = ('\033[0m', '\033[1m')
    GREEN = ('\033[0m', '\033[0;32m')
    RED = ('\033[0m', '\033[0;31m')
    GREY = ('\033[0m', '\033[1;30m')

TEST_EXIT_PASSED = 0
TEST_EXIT_SKIPPED = 77

TEST_FRAMEWORK_MODULES = [
    # Phase 1 only has util.py. Phase 3 will add address / blocktools / key /
    # script / messages once messages.py and p2p.py land.
    "util",
]

EXTENDED_SCRIPTS = [
    # These tests are not run by default (only with --extended).
    # Longest test should go first, to favor running tests in parallel.
    #
    # feature_stakelimit.py drives the background ThreadStakeMiner to verify the
    # regtest stakelimit height ceiling pauses and resumes staking. It is
    # wall-clock bound (staked blocks are ~16s apart via STAKE_TIMESTAMP_MASK),
    # so it takes tens of seconds and is opt-in rather than in the default suite.
    'feature_stakelimit.py',
    #
    # feature_reorg.py exercises a competing-proof-of-stake reorg between two
    # nodes. It is intermittently flaky on the current regtest stack and is NOT
    # in the default CI suite: both nodes share the same deterministic, stakeable
    # premine, so their independently-staked block-1s can draw the same coinstake
    # kernel. When the kernel (proof-of-stake hash = stake modifier + prevout +
    # 16s-masked time) collides, the duplicate-proof-of-stake guard
    # (validation.cpp) makes each node reject the other's block, so the shorter
    # node never reorganizes and sync_blocks times out (~13% of runs locally).
    # It cannot be made deterministic from Python without one of: a setmocktime
    # RPC (to put the two nodes in different stake-time slots), invalidateblock
    # (to force a reorg from a single staker), or disconnectnode (to split a
    # shared chain). Re-promote to BASE_SCRIPTS once one of those lands.
    'feature_reorg.py',
]

BASE_SCRIPTS = [
    # Scripts that are run by default.
    # Phase 1 ships:
    #   - feature_hello.py: framework smoke test (start node, getblockchaininfo)
    #   - rpc_help.py: RPCHelpMan rollout coverage (the motivating use case
    #     from #2922 — exercises `help`, structured help format, and
    #     IsValidNumArgs() arity validation on converted commands)
    # Phase 2A adds:
    #   - feature_regtest_staking.py: regtest premine discovery, deterministic
    #     PoS block production (generatetoaddress), and the stakelimit RPC
    # Phase 3 adds (P2P wire protocol):
    #   - p2p_version_handshake.py: version/verack + ping/pong against a node
    #   - p2p_block_tx_relay.py: relay a node-produced tx and block over P2P
    # Phase 4A adds the investor-mode starter suite (no beacon/CPID required):
    #   - wallet_basic.py: raw-tx + sendtoaddress spend, balance, confirmations
    #   - wallet_backup.py: backupwallet + dumpprivkey/importprivkey round-trip
    #   - mempool_accept.py: sendrawtransaction accept + double-spend rejection
    #   - rpc_net.py: two-node getpeerinfo/addnode + block propagation
    #   - feature_sidestake.py: local sidestaking config + reward split
    # (feature_reorg.py is in EXTENDED_SCRIPTS — flaky on the shared-premine
    #  regtest stack; see the note there.)
    # Phase 4A.2 adds RPCHelpMan-surface tests (investor-mode, no beacon/CPID):
    #   - rpc_signmessage.py: signmessage/verifymessage/validateaddress
    #   - rpc_rawtransaction.py: createrawtransaction/decode/decodescript/sign
    #   - rpc_psgt.py: PSGT create/decode/convert/combine/process/finalize
    #   - rpc_htlc.py: createhtlc + decodescript of the redeem script
    #   - wallet_keypool.py: keypoolrefill/getnewaddress/dumpprivkey
    #   - rpc_blockchain.py: getblock*/getblockchaininfo/getdifficulty
    #   - p2p_ping.py: ping/pong keepalive over the P2P wire protocol
    #   - rpc_netinfo.py: getnetworkinfo/getnettotals/getconnectioncount/getpeerinfo
    #   - rpc_multisig.py: addmultisigaddress -> validateaddress
    #   - wallet_listtransactions.py: listtransactions/gettransaction/listsinceblock
    'feature_hello.py',
    'feature_regtest_staking.py',
    'p2p_version_handshake.py',
    'p2p_block_tx_relay.py',
    'p2p_ping.py',
    'rpc_help.py',
    'rpc_signmessage.py',
    'rpc_rawtransaction.py',
    'rpc_psgt.py',
    'rpc_htlc.py',
    'rpc_blockchain.py',
    'rpc_netinfo.py',
    'rpc_multisig.py',
    'wallet_basic.py',
    'wallet_backup.py',
    'wallet_keypool.py',
    'wallet_listtransactions.py',
    'mempool_accept.py',
    'rpc_net.py',
    'feature_sidestake.py',
]

# Place EXTENDED_SCRIPTS first since longer tests benefit from being scheduled
# earlier when parallelism is available.
ALL_SCRIPTS = EXTENDED_SCRIPTS + BASE_SCRIPTS

NON_SCRIPTS = [
    # These are python files that live in the functional tests directory, but are not test scripts.
    "combine_logs.py",
    "create_cache.py",
    "test_runner.py",
]

def main():
    # Parse arguments and pass through unrecognised args
    parser = argparse.ArgumentParser(add_help=False,
                                     usage='%(prog)s [test_runner.py options] [script options] [scripts]',
                                     description=__doc__,
                                     epilog='''
    Help text and arguments for individual test script:''',
                                     formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument('--ansi', action='store_true', default=sys.stdout.isatty(), help="Use ANSI colors and dots in output (enabled by default when standard output is a TTY)")
    parser.add_argument('--combinedlogslen', '-c', type=int, default=0, metavar='n', help='On failure, print a log (of length n lines) to the console, combined from the test framework and all test nodes.')
    parser.add_argument('--coverage', action='store_true', help='generate a basic coverage report for the RPC interface')
    parser.add_argument('--ci', action='store_true', help='Run checks and code that are usually only enabled in a continuous integration environment')
    parser.add_argument('--exclude', '-x', help='specify a comma-separated-list of scripts to exclude.')
    parser.add_argument('--extended', action='store_true', help='run the extended test suite in addition to the basic tests')
    parser.add_argument('--help', '-h', '-?', action='store_true', help='print help text and exit')
    parser.add_argument('--jobs', '-j', type=int, default=4, help='how many test scripts to run in parallel. Default=4.')
    parser.add_argument('--keepcache', '-k', action='store_true', help='the default behavior is to flush the cache directory on startup. --keepcache retains the cache from the previous testrun.')
    parser.add_argument('--quiet', '-q', action='store_true', help='only print dots, results summary and failure logs')
    parser.add_argument('--tmpdirprefix', '-t', default=tempfile.gettempdir(), help="Root directory for datadirs")
    parser.add_argument('--failfast', action='store_true', help='stop execution after the first test failure')
    parser.add_argument('--filter', help='filter scripts to run by regular expression')
    parser.add_argument('--configfile', default=os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "config.ini")),
                        help='Location of the config file generated by CMake (default: %(default)s). CMake writes it to <builddir>/test/config.ini.')

    args, unknown_args = parser.parse_known_args()
    if not args.ansi:
        global BOLD, GREEN, RED, GREY
        BOLD = ("", "")
        GREEN = ("", "")
        RED = ("", "")
        GREY = ("", "")

    # args to be passed on always start with two dashes; tests are the remaining unknown args
    tests = [arg for arg in unknown_args if arg[:2] != "--"]
    passon_args = [arg for arg in unknown_args if arg[:2] == "--"]

    # Read config generated by CMake (test/config.ini), located via --configfile.
    config = configparser.ConfigParser()
    configfile = os.path.abspath(args.configfile)
    config.read_file(open(configfile, encoding="utf8"))

    passon_args.append("--configfile=%s" % configfile)

    # Set up logging
    logging_level = logging.INFO if args.quiet else logging.DEBUG
    logging.basicConfig(format='%(message)s', level=logging_level)

    # Create base test directory
    tmpdir = "%s/test_runner_%s" % (args.tmpdirprefix, datetime.datetime.now().strftime("%Y%m%d_%H%M%S"))

    os.makedirs(tmpdir)

    logging.debug("Temporary test directory at %s" % tmpdir)

    enable_gridcoind = config["components"].getboolean("ENABLE_GRIDCOIND")

    if not enable_gridcoind:
        print("No functional tests to run.")
        print("Rerun cmake with -DENABLE_GRIDCOIND=ON and rebuild")
        sys.exit(0)

    # Build list of tests
    test_list = []
    if tests:
        # Individual tests have been specified. Run specified tests that exist
        # in the ALL_SCRIPTS list. Accept names with or without a .py extension.
        # Specified tests can contain wildcards, but in that case the supplied
        # paths should be coherent, e.g. the same path as that provided to call
        # test_runner.py. Examples:
        #   `test/functional/test_runner.py test/functional/wallet*`
        #   `test/functional/test_runner.py ./test/functional/wallet*`
        #   `test_runner.py wallet*`
        #   but not:
        #   `test/functional/test_runner.py wallet*`
        # Multiple wildcards can be passed:
        #   `test_runner.py tool* mempool*`
        for test in tests:
            script = test.split("/")[-1]
            script = script + ".py" if ".py" not in script else script
            if script in ALL_SCRIPTS:
                test_list.append(script)
            else:
                print("{}WARNING!{} Test '{}' not found in full test list.".format(BOLD[1], BOLD[0], test))
    elif args.extended:
        # Include extended tests
        test_list += ALL_SCRIPTS
    else:
        # Run base tests only
        test_list += BASE_SCRIPTS

    # Remove the test cases that the user has explicitly asked to exclude.
    if args.exclude:
        exclude_tests = [test.split('.py')[0] for test in args.exclude.split(',')]
        for exclude_test in exclude_tests:
            # Remove <test_name>.py and <test_name>.py --arg from the test list
            exclude_list = [test for test in test_list if test.split('.py')[0] == exclude_test]
            for exclude_item in exclude_list:
                test_list.remove(exclude_item)
            if not exclude_list:
                print("{}WARNING!{} Test '{}' not found in current test list.".format(BOLD[1], BOLD[0], exclude_test))

    if args.filter:
        test_list = list(filter(re.compile(args.filter).search, test_list))

    if not test_list:
        print("No valid test scripts specified. Check that your test is in one "
              "of the test lists in test_runner.py, or run test_runner.py with no arguments to run all tests")
        sys.exit(0)

    if args.help:
        # Print help for test_runner.py, then print help of the first script (with args removed) and exit.
        parser.print_help()
        subprocess.check_call([sys.executable, os.path.join(config["environment"]["SRCDIR"], 'test', 'functional', test_list[0].split()[0]), '-h'])
        sys.exit(0)

    check_script_list(src_dir=config["environment"]["SRCDIR"], fail_on_warn=args.ci)
    check_script_prefixes()

    if not args.keepcache:
        shutil.rmtree("%s/test/cache" % config["environment"]["BUILDDIR"], ignore_errors=True)

    run_tests(
        test_list=test_list,
        src_dir=config["environment"]["SRCDIR"],
        build_dir=config["environment"]["BUILDDIR"],
        tmpdir=tmpdir,
        jobs=args.jobs,
        enable_coverage=args.coverage,
        args=passon_args,
        combined_logs_len=args.combinedlogslen,
        failfast=args.failfast,
        use_term_control=args.ansi,
    )

def run_tests(*, test_list, src_dir, build_dir, tmpdir, jobs=1, enable_coverage=False, args=None, combined_logs_len=0, failfast=False, use_term_control):
    args = args or []

    # Warn if gridcoinresearchd is already running
    try:
        # pgrep exits with code zero when one or more matching processes found
        if subprocess.run(["pgrep", "-x", "gridcoinresearchd"], stdout=subprocess.DEVNULL).returncode == 0:
            print("%sWARNING!%s There is already a gridcoinresearchd process running on this system. Tests may fail unexpectedly due to resource contention!" % (BOLD[1], BOLD[0]))
    except OSError:
        # pgrep not supported
        pass

    # Warn if there is a cache directory
    cache_dir = "%s/test/cache" % build_dir
    if os.path.isdir(cache_dir):
        print("%sWARNING!%s There is a cache directory here: %s. If tests fail unexpectedly, try deleting the cache directory." % (BOLD[1], BOLD[0], cache_dir))

    # Test Framework Tests
    print("Running Unit Tests for Test Framework Modules")
    test_framework_tests = unittest.TestSuite()
    for module in TEST_FRAMEWORK_MODULES:
        test_framework_tests.addTest(unittest.TestLoader().loadTestsFromName("test_framework.{}".format(module)))
    result = unittest.TextTestRunner(verbosity=1, failfast=True).run(test_framework_tests)
    if not result.wasSuccessful():
        logging.debug("Early exiting after failure in TestFramework unit tests")
        # `sys.exit(False)` exits with code 0 (False == 0). Use 1 so CI
        # actually reports the framework unit-test failure rather than
        # swallowing it as a success.
        sys.exit(1)

    tests_dir = src_dir + '/test/functional/'

    flags = ['--cachedir={}'.format(cache_dir)] + args

    if enable_coverage:
        coverage = RPCCoverage()
        flags.append(coverage.flag)
        logging.debug("Initializing coverage directory at %s" % coverage.dir)
    else:
        coverage = None

    if len(test_list) > 1 and jobs > 1:
        create_cache_path = tests_dir + 'create_cache.py'
        if os.path.isfile(create_cache_path):
            # Populate cache
            try:
                subprocess.check_output([sys.executable, create_cache_path] + flags + ["--tmpdir=%s/cache" % tmpdir])
            except subprocess.CalledProcessError as e:
                sys.stdout.buffer.write(e.output)
                raise
        else:
            # create_cache.py hasn't been ported to this PR. Force
            # sequential execution so tests don't race on a missing
            # shared cache.
            logging.warning("create_cache.py not present; forcing jobs=1 (no shared cache available).")
            jobs = 1

    #Run Tests
    job_queue = TestHandler(
        num_tests_parallel=jobs,
        tests_dir=tests_dir,
        tmpdir=tmpdir,
        test_list=test_list,
        flags=flags,
        use_term_control=use_term_control,
    )
    start_time = time.time()
    test_results = []

    max_len_name = len(max(test_list, key=len))
    test_count = len(test_list)
    for i in range(test_count):
        test_result, testdir, stdout, stderr = job_queue.get_next()
        test_results.append(test_result)
        done_str = "{}/{} - {}{}{}".format(i + 1, test_count, BOLD[1], test_result.name, BOLD[0])
        if test_result.status == "Passed":
            logging.debug("%s passed, Duration: %s s" % (done_str, test_result.time))
        elif test_result.status == "Skipped":
            logging.debug("%s skipped" % (done_str))
        else:
            print("%s failed, Duration: %s s\n" % (done_str, test_result.time))
            print(BOLD[1] + 'stdout:\n' + BOLD[0] + stdout + '\n')
            print(BOLD[1] + 'stderr:\n' + BOLD[0] + stderr + '\n')
            if combined_logs_len and os.path.isdir(testdir):
                combine_logs_path = os.path.join(tests_dir, 'combine_logs.py')
                if os.path.isfile(combine_logs_path):
                    # Print the final `combinedlogslen` lines of the combined logs
                    print('{}Combine the logs and print the last {} lines ...{}'.format(BOLD[1], combined_logs_len, BOLD[0]))
                    print('\n============')
                    print('{}Combined log for {}:{}'.format(BOLD[1], testdir, BOLD[0]))
                    print('============\n')
                    combined_logs_args = [sys.executable, combine_logs_path, testdir]
                    if BOLD[0]:
                        combined_logs_args += ['--color']
                    combined_logs, _ = subprocess.Popen(combined_logs_args, universal_newlines=True, stdout=subprocess.PIPE).communicate()
                    print("\n".join(deque(combined_logs.splitlines(), combined_logs_len)))
                else:
                    # combine_logs.py hasn't been ported. Fall back to the
                    # captured stdout/stderr already printed above, so the
                    # caller still sees actionable failure context.
                    print("(combine_logs.py not present; printing per-process stdout/stderr only)")

            if failfast:
                logging.debug("Early exiting after test failure")
                break

    print_results(test_results, max_len_name, (int(time.time() - start_time)))

    if coverage:
        coverage_passed = coverage.report_rpc_coverage()

        logging.debug("Cleaning up coverage data")
        coverage.cleanup()
    else:
        coverage_passed = True

    # Clear up the temp directory if all subdirectories are gone
    if not os.listdir(tmpdir):
        os.rmdir(tmpdir)

    all_passed = all(map(lambda test_result: test_result.was_successful, test_results)) and coverage_passed

    # This will be a no-op unless failfast is True in which case there may be dangling
    # processes which need to be killed.
    job_queue.kill_and_join()

    sys.exit(not all_passed)

def print_results(test_results, max_len_name, runtime):
    results = "\n" + BOLD[1] + "%s | %s | %s\n\n" % ("TEST".ljust(max_len_name), "STATUS   ", "DURATION") + BOLD[0]

    test_results.sort(key=TestResult.sort_key)
    all_passed = True
    time_sum = 0

    for test_result in test_results:
        all_passed = all_passed and test_result.was_successful
        time_sum += test_result.time
        test_result.padding = max_len_name
        results += str(test_result)

    status = TICK + "Passed" if all_passed else CROSS + "Failed"
    if not all_passed:
        results += RED[1]
    results += BOLD[1] + "\n%s | %s | %s s (accumulated) \n" % ("ALL".ljust(max_len_name), status.ljust(9), time_sum) + BOLD[0]
    if not all_passed:
        results += RED[0]
    results += "Runtime: %s s\n" % (runtime)
    print(results)

class TestHandler:
    """
    Trigger the test scripts passed in via the list.
    """

    def __init__(self, *, num_tests_parallel, tests_dir, tmpdir, test_list, flags, use_term_control):
        assert num_tests_parallel >= 1
        self.num_jobs = num_tests_parallel
        self.tests_dir = tests_dir
        self.tmpdir = tmpdir
        self.test_list = test_list
        self.flags = flags
        self.num_running = 0
        self.jobs = []
        self.use_term_control = use_term_control

    def get_next(self):
        while self.num_running < self.num_jobs and self.test_list:
            # Add tests
            self.num_running += 1
            test = self.test_list.pop(0)
            portseed = len(self.test_list)
            portseed_arg = ["--portseed={}".format(portseed)]
            log_stdout = tempfile.SpooledTemporaryFile(max_size=2**16)
            log_stderr = tempfile.SpooledTemporaryFile(max_size=2**16)
            test_argv = test.split()
            testdir = "{}/{}_{}".format(self.tmpdir, re.sub(".py$", "", test_argv[0]), portseed)
            tmpdir_arg = ["--tmpdir={}".format(testdir)]
            self.jobs.append((test,
                              time.time(),
                              subprocess.Popen([sys.executable, self.tests_dir + test_argv[0]] + test_argv[1:] + self.flags + portseed_arg + tmpdir_arg,
                                               universal_newlines=True,
                                               stdout=log_stdout,
                                               stderr=log_stderr),
                              testdir,
                              log_stdout,
                              log_stderr))
        if not self.jobs:
            raise IndexError('pop from empty list')

        # Print remaining running jobs when all jobs have been started.
        if not self.test_list:
            print("Remaining jobs: [{}]".format(", ".join(j[0] for j in self.jobs)))

        dot_count = 0
        while True:
            # Return first proc that finishes
            time.sleep(.5)
            for job in self.jobs:
                (name, start_time, proc, testdir, log_out, log_err) = job
                if proc.poll() is not None:
                    log_out.seek(0), log_err.seek(0)
                    [stdout, stderr] = [log_file.read().decode('utf-8') for log_file in (log_out, log_err)]
                    log_out.close(), log_err.close()
                    if proc.returncode == TEST_EXIT_PASSED and stderr == "":
                        status = "Passed"
                    elif proc.returncode == TEST_EXIT_SKIPPED:
                        status = "Skipped"
                    else:
                        status = "Failed"
                    self.num_running -= 1
                    self.jobs.remove(job)
                    if self.use_term_control:
                        clearline = '\r' + (' ' * dot_count) + '\r'
                        print(clearline, end='', flush=True)
                    dot_count = 0
                    return TestResult(name, status, int(time.time() - start_time)), testdir, stdout, stderr
            if self.use_term_control:
                print('.', end='', flush=True)
            dot_count += 1

    def kill_and_join(self):
        """Send SIGKILL to all jobs and block until all have ended."""
        procs = [i[2] for i in self.jobs]

        for proc in procs:
            proc.kill()

        for proc in procs:
            proc.wait()


class TestResult():
    def __init__(self, name, status, time):
        self.name = name
        self.status = status
        self.time = time
        self.padding = 0

    def sort_key(self):
        if self.status == "Passed":
            return 0, self.name.lower()
        elif self.status == "Failed":
            return 2, self.name.lower()
        elif self.status == "Skipped":
            return 1, self.name.lower()

    def __repr__(self):
        if self.status == "Passed":
            color = GREEN
            glyph = TICK
        elif self.status == "Failed":
            color = RED
            glyph = CROSS
        elif self.status == "Skipped":
            color = GREY
            glyph = CIRCLE

        return color[1] + "%s | %s%s | %s s\n" % (self.name.ljust(self.padding), glyph, self.status.ljust(7), self.time) + color[0]

    @property
    def was_successful(self):
        return self.status != "Failed"


def check_script_prefixes():
    """Check that test scripts start with one of the allowed name prefixes."""

    good_prefixes_re = re.compile("^(example|feature|interface|mempool|mining|p2p|rpc|wallet|tool)_")
    bad_script_names = [script for script in ALL_SCRIPTS if good_prefixes_re.match(script) is None]

    if bad_script_names:
        print("%sERROR:%s %d tests not meeting naming conventions:" % (BOLD[1], BOLD[0], len(bad_script_names)))
        print("  %s" % ("\n  ".join(sorted(bad_script_names))))
        raise AssertionError("Some tests are not following naming convention!")


def check_script_list(*, src_dir, fail_on_warn):
    """Check scripts directory.

    Check that there are no scripts in the functional tests directory which are
    not being run by pull-tester.py."""
    script_dir = src_dir + '/test/functional/'
    python_files = set([test_file for test_file in os.listdir(script_dir) if test_file.endswith(".py")])
    missed_tests = list(python_files - set(map(lambda x: x.split()[0], ALL_SCRIPTS + NON_SCRIPTS)))
    if len(missed_tests) != 0:
        print("%sWARNING!%s The following scripts are not being run: %s. Check the test lists in test_runner.py." % (BOLD[1], BOLD[0], str(missed_tests)))
        if fail_on_warn:
            # On CI this warning is an error to prevent merging incomplete commits into master
            sys.exit(1)


class RPCCoverage():
    """
    Coverage reporting utilities for test_runner.

    Coverage calculation works by having each test script subprocess write
    coverage files into a particular directory. These files contain the RPC
    commands invoked during testing, as well as a complete listing of RPC
    commands per `gridcoin-cli help` (`rpc_interface.txt`).

    After all tests complete, the commands run are combined and diff'd against
    the complete list to calculate uncovered RPC commands.

    See also: test/functional/test_framework/coverage.py

    """
    def __init__(self):
        self.dir = tempfile.mkdtemp(prefix="coverage")
        self.flag = '--coveragedir=%s' % self.dir

    def report_rpc_coverage(self):
        """
        Print out RPC commands that were unexercised by tests.

        """
        uncovered = self._get_uncovered_rpc_commands()

        if uncovered:
            print("Uncovered RPC commands:")
            print("".join(("  - %s\n" % command) for command in sorted(uncovered)))
            return False
        else:
            print("All RPC commands covered.")
            return True

    def cleanup(self):
        return shutil.rmtree(self.dir)

    def _get_uncovered_rpc_commands(self):
        """
        Return a set of currently untested RPC commands.

        """
        # This is shared from `test/functional/test_framework/coverage.py`
        reference_filename = 'rpc_interface.txt'
        coverage_file_prefix = 'coverage.'

        coverage_ref_filename = os.path.join(self.dir, reference_filename)
        coverage_filenames = set()
        all_cmds = set()
        # Consider RPC generate covered, because it is overloaded in
        # test_framework/test_node.py and not seen by the coverage check.
        covered_cmds = set({'generate'})

        if not os.path.isfile(coverage_ref_filename):
            raise RuntimeError("No coverage reference found")

        with open(coverage_ref_filename, 'r', encoding="utf8") as coverage_ref_file:
            all_cmds.update([line.strip() for line in coverage_ref_file.readlines()])

        for root, _, files in os.walk(self.dir):
            for filename in files:
                if filename.startswith(coverage_file_prefix):
                    coverage_filenames.add(os.path.join(root, filename))

        for filename in coverage_filenames:
            with open(filename, 'r', encoding="utf8") as coverage_file:
                covered_cmds.update([line.strip() for line in coverage_file.readlines()])

        return all_cmds - covered_cmds


if __name__ == '__main__':
    main()
