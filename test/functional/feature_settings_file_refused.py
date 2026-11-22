#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Refused keys in gridcoinsettings.json are ignored at startup.

A key of the refused class (a command to run, a file or directory to use, an RPC or
IPC option) is honoured only in gridcoinresearch.conf or on the command line. When
the settings file holds one, the node:

  - ignores it and logs "ignored <key> from gridcoinsettings.json; set it in
    gridcoinresearch.conf if you want it";
  - lists it under setting_file_args_ignored in listsettings;
  - keeps it in the file, until changesettings erases it;
  - still reads the other keys in the file.
"""
import json
import os

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal


class SettingsFileRefusedTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-connect=0", "-listen=0"]]
        self.supports_cli = False

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def settings_file(self):
        return os.path.join(self.nodes[0].datadir, self.chain, "gridcoinsettings.json")

    def run_test(self):
        node = self.nodes[0]
        backup_dir = os.path.join(self.options.tmpdir, "backups")
        os.makedirs(backup_dir)

        self.log.info("Stop the node and put refused keys in its settings file")
        self.stop_node(0)
        written = {
            "backupdir": backup_dir,
            "walletbackupintervalsecs": "60",
            "rpcport": "1",
            "staking": "0",
        }
        with open(self.settings_file(), "w", encoding="utf8") as f:
            json.dump(written, f)

        refused = ["backupdir", "walletbackupintervalsecs", "rpcport"]
        expected = ["ignored %s from gridcoinsettings.json; set it in gridcoinresearch.conf if you want it" % key
                    for key in refused]
        with node.assert_debug_log(expected, timeout=30):
            self.start_node(0)

        self.log.info("The node works, so the ignored rpcport was not used")
        node.getblockcount()

        settings = node.listsettings()
        ignored = {}
        for entry in settings["setting_file_args_ignored"]:
            for key, value in entry.items():
                ignored[key] = value
        assert_equal(sorted(ignored), sorted(refused))
        in_use = [key for entry in settings["setting_file_args"] for key in entry]
        assert "staking" in in_use
        for key in refused:
            assert key not in in_use

        self.log.info("The ignored keys stay in the file")
        with open(self.settings_file(), encoding="utf8") as f:
            on_disk = json.load(f)
        for key in refused:
            assert key in on_disk

        self.log.info("changesettings erases an ignored key from the file and the ignored list")
        node.changesettings("backupdir=")
        with open(self.settings_file(), encoding="utf8") as f:
            on_disk = json.load(f)
        assert "backupdir" not in on_disk
        assert "walletbackupintervalsecs" in on_disk
        ignored_now = [key for entry in node.listsettings()["setting_file_args_ignored"] for key in entry]
        assert "backupdir" not in ignored_now
        assert_equal(sorted(ignored_now), ["rpcport", "walletbackupintervalsecs"])


if __name__ == '__main__':
    SettingsFileRefusedTest().main()
