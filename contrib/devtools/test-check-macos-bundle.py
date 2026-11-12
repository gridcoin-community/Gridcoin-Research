#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Tests for the rules in check-macos-bundle.py. They need no Apple tools."""

import importlib.util
import os
import unittest

_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "check-macos-bundle.py")
_SPEC = importlib.util.spec_from_file_location("check_macos_bundle", _PATH)
assert _SPEC is not None and _SPEC.loader is not None
bundle = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(bundle)

APP = "/Volumes/X/Gridcoin.app"
GUI = APP + "/Contents/MacOS/gridcoinresearch"
DAEMON = APP + "/Contents/Helpers/gridcoinresearchd"
DYLIB = APP + "/Contents/Frameworks/libcrypto.3.dylib"


class RoleTests(unittest.TestCase):
    def test_roles(self):
        self.assertEqual(bundle.role_of(APP, GUI), "gui")
        self.assertEqual(bundle.role_of(APP, DAEMON), "daemon")
        self.assertEqual(bundle.role_of(APP, DYLIB), "other")


class EntitlementTests(unittest.TestCase):
    def test_forbidden_entitlement_fails_for_every_role(self):
        for key in bundle.FORBIDDEN:
            for role in ("gui", "daemon", "other"):
                failures = bundle.check_entitlements(role, {key: True}, allow_gui_jit=False)
                self.assertEqual(len(failures), 1, (key, role, failures))
                self.assertIn(key, failures[0])

    def test_forbidden_entitlement_set_false_passes(self):
        key = bundle.FORBIDDEN[0]
        self.assertEqual(bundle.check_entitlements("gui", {key: False}, allow_gui_jit=False), [])

    def test_restricted_entitlement_fails_for_every_role(self):
        for key in bundle.RESTRICTED:
            for role in ("gui", "daemon", "other"):
                failures = bundle.check_entitlements(role, {key: ["ABCDE12345.x"]}, allow_gui_jit=False)
                self.assertEqual(len(failures), 1, (key, role, failures))
                self.assertIn(key, failures[0])

    def test_gui_jit_only_when_allowed(self):
        jit = {"com.apple.security.cs.allow-jit": True}
        self.assertEqual(len(bundle.check_entitlements("gui", jit, allow_gui_jit=False)), 1)
        self.assertEqual(bundle.check_entitlements("gui", jit, allow_gui_jit=True), [])
        self.assertEqual(len(bundle.check_entitlements("daemon", jit, allow_gui_jit=True)), 1)
        self.assertEqual(len(bundle.check_entitlements("other", jit, allow_gui_jit=True)), 1)

    def test_the_shipped_gui_entitlements_pass(self):
        ents = {"com.apple.security.network.client": True}
        self.assertEqual(bundle.check_entitlements("gui", ents, allow_gui_jit=False), [])
        self.assertEqual(bundle.check_entitlements("daemon", {}, allow_gui_jit=False), [])

    def test_the_entitlement_file_in_the_tree_passes(self):
        import plistlib
        path = os.path.join(os.path.dirname(_PATH), "..", "macdeploy", "entitlements-gui.plist")
        with open(path, "rb") as f:
            ents = plistlib.load(f)
        self.assertEqual(bundle.check_entitlements("gui", ents, allow_gui_jit=False), [])


class LoadPathTests(unittest.TestCase):
    def test_rpath_outside_bundle_fails(self):
        self.assertEqual(len(bundle.check_rpaths(DAEMON, ["/opt/homebrew/lib"])), 1)
        self.assertEqual(len(bundle.check_rpaths(DAEMON, ["/usr/local/lib", "@loader_path/../Frameworks"])), 1)
        self.assertEqual(bundle.check_rpaths(DAEMON, ["@loader_path/../Frameworks"]), [])
        self.assertEqual(bundle.check_rpaths(GUI, ["@executable_path/../Frameworks"]), [])

    def test_homebrew_load_command_fails(self):
        self.assertEqual(
            len(bundle.check_load_commands(GUI, ["/usr/local/opt/openssl@3/lib/libcrypto.3.dylib"])), 1)
        self.assertEqual(
            len(bundle.check_load_commands(GUI, ["/opt/homebrew/opt/qtbase/lib/QtCore.framework/Versions/A/QtCore"])), 1)
        self.assertEqual(bundle.check_load_commands(GUI, [
            "/usr/lib/libc++.1.dylib",
            "/System/Library/Frameworks/AppKit.framework/Versions/C/AppKit",
            "@executable_path/../Frameworks/x.dylib",
            "@rpath/QtCore.framework/Versions/A/QtCore",
        ]), [])

    def test_load_command_under_a_home_directory_fails(self):
        self.assertEqual(len(bundle.check_load_commands(GUI, ["/Users/runner/work/lib/libfoo.dylib"])), 1)


class SignatureTests(unittest.TestCase):
    def test_team_mismatch_fails(self):
        self.assertEqual(len(bundle.check_team(GUI, "OTHERTEAM1", "ABCDE12345")), 1)
        self.assertEqual(len(bundle.check_team(GUI, None, "ABCDE12345")), 1)
        self.assertEqual(bundle.check_team(GUI, "ABCDE12345", "ABCDE12345"), [])

    def test_parse_codesign_display(self):
        text = ("Executable=/x/Gridcoin.app/Contents/MacOS/gridcoinresearch\n"
                "Identifier=us.gridcoin.Gridcoin-Qt\n"
                "CodeDirectory v=20500 size=1 flags=0x10000(runtime) hashes=1+7 location=embedded\n"
                "TeamIdentifier=ABCDE12345\n")
        self.assertEqual(bundle.parse_codesign_display(text), ("ABCDE12345", True))
        adhoc = ("CodeDirectory v=20400 size=1 flags=0x2(adhoc) hashes=1+7 location=embedded\n"
                 "TeamIdentifier=not set\n")
        self.assertEqual(bundle.parse_codesign_display(adhoc), (None, False))

    def test_parse_otool_load_commands(self):
        text = ("Load command 12\n"
                "          cmd LC_LOAD_DYLIB\n"
                "      cmdsize 56\n"
                "         name /usr/lib/libc++.1.dylib (offset 24)\n"
                "Load command 13\n"
                "          cmd LC_RPATH\n"
                "      cmdsize 48\n"
                "         path @loader_path/../Frameworks (offset 12)\n"
                "Load command 14\n"
                "          cmd LC_LOAD_WEAK_DYLIB\n"
                "      cmdsize 56\n"
                "         name @rpath/libweak.dylib (offset 24)\n"
                "Load command 15\n"
                "          cmd LC_ID_DYLIB\n"
                "      cmdsize 56\n"
                "         name /opt/homebrew/lib/libself.dylib (offset 24)\n")
        libs, rpaths = bundle.parse_otool_load_commands(text)
        self.assertEqual(libs, ["/usr/lib/libc++.1.dylib", "@rpath/libweak.dylib"])
        self.assertEqual(rpaths, ["@loader_path/../Frameworks"])


if __name__ == "__main__":
    unittest.main()
