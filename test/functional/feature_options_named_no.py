#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Options whose own names begin with "no".

"-no" in front of an option's name negates that option: -nolisten is -listen=0.
Three options have names of their own that begin with "no": -noncruncher,
-nonewprivs and, for the GUI, -nobuildwarn. Each is taken by its name on the
command line and in gridcoinresearch.conf, with or without a value, and a
second "no" negates it. A name that is no option is still refused.
"""
import os
import platform

from test_framework.test_framework import GridcoinTestFramework
from test_framework.test_node import ErrorMatch

NONCRUNCHER = "Non-cruncher mode configured. Skipping CPID import."
NO_EMAIL = "Please set 'email=<your BOINC account email>' in gridcoinresearch.conf or 'noncruncher=1'"
HARDENED = "HardenProcess: NO_NEW_PRIVS set"
EMAIL_OFF = "noncruncher is set and switches off the email that is also set"
UNKNOWN_IN_CONFIG = ["Ignoring unknown configuration value noncruncher",
                     "Ignoring unknown configuration value nonewprivs"]


class OptionsNamedNoTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-connect=0", "-listen=0"]]
        self.supports_cli = False

    def setup_network(self):
        # An empty BOINC client state inside the test directory, so the node does
        # not read the BOINC installation of the host.
        boinc_dir = os.path.join(self.options.tmpdir, "boinc")
        os.makedirs(boinc_dir, exist_ok=True)
        with open(os.path.join(boinc_dir, "client_state.xml"), "w", encoding="utf-8") as client_state:
            client_state.write("<client_state>\n</client_state>\n")
        self.extra_args[0].append("-boincdatadir=%s" % boinc_dir)

        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def network_config(self):
        """The config file a regtest node takes its settings from."""
        return os.path.join(self.nodes[0].datadir, self.chain, "gridcoinresearch.conf")

    def restart(self, args, expected, unexpected):
        """Restart the node with args and check its log for the start."""
        node = self.nodes[0]
        self.stop_node(0)
        with node.assert_debug_log(expected, unexpected_msgs=unexpected, timeout=60):
            self.start_node(0, extra_args=self.extra_args[0] + args)

    def refused(self, arg):
        self.nodes[0].assert_start_raises_init_error(
            extra_args=self.extra_args[0] + [arg],
            expected_msg="Error parsing command line arguments: Invalid parameter %s" % arg,
            match=ErrorMatch.PARTIAL_REGEX)

    def run_test(self):
        linux = platform.system() == "Linux"
        hardened = [HARDENED] if linux else []

        self.log.info("with none of the options: a node with no email, hardened on Linux")
        self.restart([], [NO_EMAIL] + hardened, [NONCRUNCHER])

        self.log.info("-noncruncher on the command line, bare and with a value")
        self.restart(["-noncruncher"], [NONCRUNCHER], [NO_EMAIL, EMAIL_OFF])
        self.restart(["-noncruncher=1"], [NONCRUNCHER], [NO_EMAIL])
        self.restart(["-noncruncher=0"], [NO_EMAIL], [NONCRUNCHER])

        self.log.info("a second no negates it")
        self.restart(["-nononcruncher"], [NO_EMAIL], [NONCRUNCHER])

        self.log.info("-nonewprivs=0 leaves the process as it was started")
        self.restart(["-nonewprivs=0"], [NO_EMAIL], [HARDENED])
        self.restart(["-nononewprivs"], [NO_EMAIL], [HARDENED])
        self.restart(["-nonewprivs"], [NO_EMAIL] + hardened, [])

        self.log.info("-nobuildwarn is a GUI option the node accepts and does not use")
        self.restart(["-nobuildwarn"], [NO_EMAIL], [NONCRUNCHER])

        self.log.info("the negation of another option is unchanged")
        self.restart(["-nostaking", "-nodnsseed", "-nolisten"], [NO_EMAIL], [NONCRUNCHER])

        self.log.info("a name that is no option is refused")
        self.stop_node(0)
        for arg in ("-nofoo", "-noncrunche", "-nononewprivsx"):
            self.refused(arg)
        self.start_node(0)

        self.log.info("in gridcoinresearch.conf")
        self.stop_node(0)
        with open(self.network_config(), "a", encoding="utf-8") as config:
            config.write("noncruncher=1\nnonewprivs=0\n")
        with self.nodes[0].assert_debug_log([NONCRUNCHER],
                                            unexpected_msgs=[NO_EMAIL, HARDENED, EMAIL_OFF] + UNKNOWN_IN_CONFIG,
                                            timeout=60):
            self.start_node(0)

        self.log.info("the command line is ahead of the config file")
        self.restart(["-noncruncher=0", "-nonewprivs=1"], [NO_EMAIL] + hardened, [NONCRUNCHER])

        self.log.info("a config file with noncruncher and an email: the node says the email is not used")
        self.stop_node(0)
        with open(self.network_config(), "a", encoding="utf-8") as config:
            config.write("email=researcher@example.com\n")
        with self.nodes[0].assert_debug_log([NONCRUNCHER, EMAIL_OFF], unexpected_msgs=[NO_EMAIL], timeout=60):
            self.start_node(0)

        self.log.info("with noncruncher off the node is a researcher and says nothing of it")
        self.restart(["-noncruncher=0"], [], [NONCRUNCHER, NO_EMAIL, EMAIL_OFF])


if __name__ == "__main__":
    OptionsNamedNoTest().main()
