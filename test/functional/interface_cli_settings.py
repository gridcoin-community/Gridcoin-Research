#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""The RPC client reads the node's read-write settings file and never writes it.

gridcoinresearchd given a non-switch argv acts as the RPC client. It used to run
InitSettings, which reads gridcoinsettings.json and writes it back through a fixed
temporary name, before it found out it was a client. Clients calling the same node
concurrently raced on that temporary file and could fail with "Error initializing
settings", and a client could write back a setting the node had just changed.

The client still has to read the file: a connection setting stored there with
changesettings outranks the config file, and the client must honour it.

  - client calls leave gridcoinsettings.json untouched (same inode, same mtime);
  - a client-only setting stored with changesettings (rpcconnect, pointed at a name
    that cannot resolve) is honoured by the client, and the command line still
    outranks it;
  - erasing the setting restores the default.
"""
import os

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_process_error,
)


class InterfaceCLISettingsTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0", "-connect=0", "-listen=0"]]
        # Under --usecli every RPC, including the changesettings that erases the stored rpcconnect, would go through
        # the client this test points at an unresolvable host.
        self.supports_cli = False

    def skip_test_if_missing_module(self):
        self.skip_if_no_cli()

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def settings_file(self):
        return os.path.join(self.nodes[0].datadir, self.chain, "gridcoinsettings.json")

    def file_identity(self):
        st = os.stat(self.settings_file())
        return (st.st_ino, st.st_mtime_ns, st.st_size)

    def run_test(self):
        node = self.nodes[0]
        expected_height = node.getblockcount()

        self.log.info("Client calls leave the settings file untouched")
        assert os.path.exists(self.settings_file())
        before = self.file_identity()
        for _ in range(3):
            assert_equal(node.cli.getblockcount(), expected_height)
        assert_equal(self.file_identity(), before)

        self.log.info("The client honours a connection setting stored with changesettings")
        node.changesettings("rpcconnect=nonexistent.invalid")
        stored = self.file_identity()
        # The client exits with 1 when it cannot reach the server; ignoring the stored value would succeed instead.
        assert_raises_process_error(1, "couldn't connect to server", node.cli.getblockcount)
        # The command line outranks the read-write settings.
        assert_equal(node.cli("-rpcconnect=127.0.0.1").getblockcount(), expected_height)
        # Neither client call rewrote the file.
        assert_equal(self.file_identity(), stored)

        self.log.info("Erasing the setting restores the default")
        node.changesettings("rpcconnect=")
        assert_equal(node.cli.getblockcount(), expected_height)


if __name__ == '__main__':
    InterfaceCLISettingsTest().main()
