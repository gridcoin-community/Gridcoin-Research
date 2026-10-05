#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""changesettings with an empty value erases the setting, it does not set it.

An empty value (name=) removes the setting from gridcoinsettings.json. It used
to also force an empty string into the running args, where it outranked every
other source and read as true for a boolean setting, so "enablesidestaking="
switched side staking ON until the next restart.

Side staking is the probe because getstakinginfo reports the live
-enablesidestaking value (local_side_staking_enabled), and the setting takes
effect without a restart.

  - node0, nothing set anywhere: erasing leaves side staking off.
  - node0, set to 1 by changesettings: erasing turns it off again and removes it
    from the settings file.
  - node1, -enablesidestaking=0 on the command line: a changesettings value
    still overrides it, and erasing that value hands the setting back to the
    command line.
  - node0, a side-stake allocation changed a second time: the side-stake
    reload reads the new value, not the one the first change forced into the
    running args.
  - node0, a GUI option (lang, suppressnetworkgraph): refused as not a node
    setting and not stored, while an empty value (an erase) is accepted and a
    node setting the GUI reads (showorphans) is still stored.
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error


def side_staking_enabled(node):
    return node.getstakinginfo()["side_staking"]["local_side_staking_enabled"]


def stored_settings(node):
    stored = set()
    for entry in node.listsettings()["setting_file_args"]:
        stored.update(key for key in entry if key != "changeable_without_restart")
    return stored


def local_allocation_pct(node, address):
    for entry in node.getstakinginfo()["side_staking"]["side_staking_allocations"]:
        if entry["address"] == address:
            return round(entry["allocation_pct"], 6)
    return None


class ChangeSettingsEraseTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0"], ["-staking=0", "-enablesidestaking=0"]]

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def run_test(self):
        node0, node1 = self.nodes

        self.log.info("Erasing a setting that is not set leaves the default in place")
        assert_equal(side_staking_enabled(node0), False)
        node0.changesettings("enablesidestaking=")
        assert_equal(side_staking_enabled(node0), False)

        self.log.info("Erasing a stored setting reverts it and removes it from the settings file")
        node0.changesettings("enablesidestaking=1")
        assert_equal(side_staking_enabled(node0), True)
        assert "enablesidestaking" in stored_settings(node0)

        node0.changesettings("enablesidestaking=")
        assert_equal(side_staking_enabled(node0), False)
        assert "enablesidestaking" not in stored_settings(node0)

        self.log.info("Erasing hands the setting back to the command line")
        assert_equal(side_staking_enabled(node1), False)
        node1.changesettings("enablesidestaking=1")
        assert_equal(side_staking_enabled(node1), True)

        node1.changesettings("enablesidestaking=")
        assert_equal(side_staking_enabled(node1), False)

        self.log.info("A side-stake setting changed again is reloaded at its new value")
        address = node0.getnewaddress()
        node0.changesettings("enablesidestaking=1")
        node0.changesettings(f"sidestakeaddresses={address}", "sidestakeallocations=10")
        assert_equal(local_allocation_pct(node0, address), 10)

        # The first call forced "10" into the running args. The side-stake reload
        # that this call triggers must read the "20" it stores.
        node0.changesettings("sidestakeallocations=20")
        assert_equal(local_allocation_pct(node0, address), 20)

        self.log.info("A GUI option is refused and not stored")
        for setting in ("lang=de", "suppressnetworkgraph=true"):
            assert_raises_rpc_error(-8, "Not a node setting", node0.changesettings, setting)
            assert setting.split("=")[0] not in stored_settings(node0)
        # An erase is let through, so an entry an older build stored can still be removed.
        node0.changesettings("lang=")

        node0.changesettings("showorphans=1")
        assert "showorphans" in stored_settings(node0)
        node0.changesettings("showorphans=")


if __name__ == "__main__":
    ChangeSettingsEraseTest().main()
