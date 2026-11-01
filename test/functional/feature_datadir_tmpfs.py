#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""A data directory on tmpfs: refused when it is the default, recorded when it is chosen.

The node checks whether its data directory is on tmpfs or ramfs. When that
directory is the DEFAULT one, nobody chose it -- it is what the old Flatpak
fallback produced -- so the node names it on stderr before the log file opens
and then refuses to start. When it comes from an explicit -datadir, the operator
chose it and the node carries on.

The stderr line used to be printed in both cases. The framework passes -datadir
for every node and checks at stop that the node wrote nothing to stderr, so on
any host whose /tmp is tmpfs every functional test failed. An explicit -datadir
on tmpfs now gets its warning in debug.log instead.

Both halves are pinned, because the fix is a predicate and each side of it can
regress on its own:

- explicit -datadir on tmpfs: stop_node() finds stderr empty, and debug.log
  carries the warning. The warning is the load-bearing assertion. It can only
  be written by a node that looked at its directory and found it volatile, so
  it is what proves the node really was on tmpfs. Clean stderr on its own would
  also pass for a node that never checked.
- default data directory on tmpfs: started with HOME on tmpfs and no -datadir,
  the node names the directory on stderr and refuses to start. So it does with
  an empty -datadir, which resolves to the default directory.

Linux only, and only where /dev/shm is tmpfs with room for a node: the
volatile-filesystem check is Linux-only, and the test needs somewhere it can put
a data directory that the check will call volatile.
"""

import os
import re
import shutil
import subprocess
import sys
import tempfile

from test_framework.test_framework import GridcoinTestFramework, SkipTest
from test_framework.util import assert_equal, chain_subdir

TMPFS_ROOT = "/dev/shm"
VOLATILE_FSTYPES = ("tmpfs", "ramfs")

# The node shuts down at startup when its data directory has less than
# nMinDiskSpace (50 MiB, src/node/blockstorage.cpp) free. A container's /dev/shm
# is 64 MiB by default and can have less; 1 MiB more covers what this test writes.
MIN_FREE_BYTES = (50 + 1) * 1024 * 1024

# Shared by every message the node writes about a volatile data directory.
VOLATILE = "is on a temporary filesystem (tmpfs/ramfs)"

# The stderr line for the default directory, and the refusal that follows it.
DEFAULT_STDERR = "WARNING: the default data directory"
DEFAULT_REFUSAL = "The default data directory is on a temporary filesystem"


def mount_fstype(path):
    """Filesystem type of the mount holding path: the longest matching mount point."""
    real = os.path.realpath(path)
    best_point, best_type = "", None
    with open("/proc/self/mounts", encoding="utf-8") as mounts:
        for line in mounts:
            fields = line.split()
            # The kernel escapes whitespace in mount points as octal, e.g. \040.
            point = re.sub(r"\\([0-7]{3})", lambda m: chr(int(m.group(1), 8)), fields[1])
            inside = point == "/" or real == point or real.startswith(point.rstrip("/") + "/")
            if inside and len(point) >= len(best_point):
                best_point, best_type = point, fields[2]
    return best_type


class DatadirTmpfsTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0"]]
        self.tmpfs_root = None

    def skip_test_if_missing_module(self):
        if not sys.platform.startswith("linux"):
            raise SkipTest("the volatile-filesystem check is Linux-only")
        if not os.path.isdir(TMPFS_ROOT) or mount_fstype(TMPFS_ROOT) not in VOLATILE_FSTYPES:
            raise SkipTest("{} is not tmpfs on this host".format(TMPFS_ROOT))
        # The same figure the node checks: fs::space().available is f_bavail.
        free = shutil.disk_usage(TMPFS_ROOT).free
        if free < MIN_FREE_BYTES:
            raise SkipTest("{} has {} bytes free, less than the {} a node needs".format(
                TMPFS_ROOT, free, MIN_FREE_BYTES))

    def setup_chain(self):
        super().setup_chain()
        # Move node0's directory onto tmpfs and leave a symlink where the
        # framework expects it. The framework still passes -datadir with its own
        # path, which is the case under test, and statfs follows the link to tmpfs.
        self.tmpfs_root = tempfile.mkdtemp(prefix="grc_datadir_tmpfs_", dir=TMPFS_ROOT)
        node_dir = os.path.join(self.options.tmpdir, "node0")
        tmpfs_node_dir = os.path.join(self.tmpfs_root, "node0")
        shutil.move(node_dir, tmpfs_node_dir)
        os.symlink(tmpfs_node_dir, node_dir)
        # Named here because shutdown() cannot: the framework has removed its log
        # handlers by then. A failed or --nocleanup run keeps this half in RAM.
        self.log.info("node0's data directory is on tmpfs at {}".format(tmpfs_node_dir))

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def debug_log(self):
        path = os.path.join(self.nodes[0].datadir, chain_subdir(self.chain), "debug.log")
        with open(path, encoding="utf-8") as log_file:
            return log_file.read()

    def run_test(self):
        self.test_explicit_datadir()
        self.test_default_datadir()

    def test_explicit_datadir(self):
        self.log.info("an explicit -datadir on tmpfs runs, warns in debug.log, and leaves stderr empty")
        assert mount_fstype(self.nodes[0].datadir) in VOLATILE_FSTYPES

        # stop_node() fails on any stderr output, which is the reported failure.
        # Stopping first also means debug.log is complete when it is read below.
        self.stop_node(0)

        warnings = [line for line in self.debug_log().splitlines()
                    if "WARNING:" in line and VOLATILE in line]
        assert_equal(len(warnings), 1)

    def test_default_datadir(self):
        self.log.info("the default data directory on tmpfs is named on stderr and refused")
        self.check_default_refused([])

        # -datadir= is no choice of directory: the node resolves an empty value to
        # the default, as a wrapper's -datadir=$DIR produces when DIR is unset.
        self.log.info("an empty -datadir is the default data directory, and refused the same way")
        self.check_default_refused(["-datadir="])

    def check_default_refused(self, extra_args):
        # A HOME of its own per case, so one refused start leaves nothing behind
        # for the next.
        home = tempfile.mkdtemp(prefix="home_", dir=self.tmpfs_root)

        # Off Windows and macOS the default directory is $HOME/.GridcoinResearch,
        # unless the container variable says Flatpak; drop it so the plain branch
        # is the one under test. LIBC_FATAL_STDERR_ as TestNode.start() sets it, so
        # a libc abort message lands in the captured stderr, not on the terminal.
        env = dict(os.environ, HOME=home, LIBC_FATAL_STDERR_="1")
        env.pop("container", None)

        # No usable -datadir: that is the point. -listen=0 and -server=0 only limit
        # the damage if the refusal ever regresses and the node comes up; the
        # timeout then kills it. The timeout is capped because --timeout-factor 0
        # becomes 99999 in the framework, and a regressed node would then run for
        # days. cwd is the tmpfs HOME, much as TestNode runs its nodes in the
        # test's directory, so anything the child writes relative to its working
        # directory stays in the tree this test removes.
        #
        # The refused node currently aborts (see below), so lower the core-file
        # limit for the child, or every run would leave a core dump behind. It is
        # set in this process and inherited, rather than through preexec_fn, which
        # is not safe with the framework's network thread running; lowering the
        # soft limit and putting it back needs no privilege. resource is Unix-only,
        # so it is imported here, past the Linux-only skip, rather than at the top,
        # where it would fail the script on Windows instead of skipping it.
        import resource
        core_soft, core_hard = resource.getrlimit(resource.RLIMIT_CORE)
        resource.setrlimit(resource.RLIMIT_CORE, (0, core_hard))
        try:
            result = subprocess.run(
                [self.options.gridcoind, "-regtest", "-listen=0", "-server=0"] + extra_args,
                env=env, cwd=home, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                timeout=60 * min(self.options.timeout_factor, 10), check=False)
        finally:
            resource.setrlimit(resource.RLIMIT_CORE, (core_soft, core_hard))
        stderr = result.stderr.decode("utf-8", "replace")

        # Non-zero rather than a specific code. An early InitError in AppInit2
        # currently ends in an uncaught std::runtime_error from init_blockindex()
        # on the way out -- SIGABRT, so returncode -6 -- instead of a clean exit.
        # That is a separate, pre-existing defect of every early-init error path;
        # what this test pins is that the node does not start.
        assert result.returncode != 0, "the node started on a default data directory on tmpfs"
        assert DEFAULT_STDERR in stderr, stderr
        assert VOLATILE in stderr, stderr
        assert DEFAULT_REFUSAL in stderr, stderr
        # The directory it names must be the one under the tmpfs HOME.
        assert os.path.join(home, ".GridcoinResearch") in stderr, stderr

    def shutdown(self):
        exit_code = super().shutdown()
        # Follow the framework's own policy: the tmpfs half of the tree goes
        # when the rest does, and stays beside it when a failure keeps it.
        if self.tmpfs_root and not os.path.exists(self.options.tmpdir):
            shutil.rmtree(self.tmpfs_root, ignore_errors=True)
        return exit_code


if __name__ == '__main__':
    DatadirTmpfsTest().main()
