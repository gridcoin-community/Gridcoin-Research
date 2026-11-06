#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

"""Tests of the signed SHA256SUMS tools. Run by test/lint/lint-release-signature.sh."""

import base64
import importlib.util
import os
import tempfile
import unittest

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.asymmetric.utils import encode_dss_signature

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.join(HERE, "testdata")


def load(name):
    spec = importlib.util.spec_from_file_location(name.replace("-", "_"), os.path.join(HERE, name + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


SIGN = load("sign-sha256sums")
VERIFY = load("verify-sha256sums")

SUMS = b"%s  gridcoin-9.9.9-win64-setup.exe\n" % (b"ab" * 32)


def read(path):
    with open(path, encoding="utf-8") as handle:
        return handle.read()


def read_bytes(path):
    with open(path, "rb") as handle:
        return handle.read()


def write(directory, name, data):
    path = os.path.join(directory, name)
    with open(path, "wb") as handle:
        handle.write(data)
    return path


def new_key():
    return ec.generate_private_key(ec.SECP256R1())


def pub_pem(key):
    return key.public_key().public_bytes(serialization.Encoding.PEM,
                                         serialization.PublicFormat.SubjectPublicKeyInfo)


class ReleaseSignatureTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.key = new_key()
        self.sums = write(self.tmp.name, "SHA256SUMS", SUMS)
        self.sig = write(self.tmp.name, "SHA256SUMS.sig", SIGN.sign_bytes(self.key, SUMS).encode())
        self.keys = write(self.tmp.name, "keys.txt",
                          b"# comment\n" + SIGN.keyline(self.key.public_key()).encode() + b"\n")

    def check(self, sig=None, keys=None, sums=None):
        return VERIFY.verify_files(keys or self.keys, sums or self.sums, sig or self.sig)

    def test_roundtrip(self):
        self.check()
        self.assertEqual(VERIFY.hash_for(SUMS, "gridcoin-9.9.9-win64-setup.exe"), "ab" * 32)
        with self.assertRaises(VERIFY.VerifyError):
            VERIFY.hash_for(SUMS, "other.exe")

    def test_tampered_sums_rejected(self):
        bad = write(self.tmp.name, "T", SUMS.replace(b"ab", b"ac", 1))
        with self.assertRaises(VERIFY.VerifyError):
            self.check(sums=bad)

    def test_wrong_key_rejected(self):
        other = new_key()
        # same keyid claimed but another key signed
        line = read(self.sig).split()
        forged = SIGN.sign_bytes(other, SUMS).split()
        sig = write(self.tmp.name, "F", ("%s %s %s\n" % (forged[0], line[1], forged[2])).encode())
        with self.assertRaises(VERIFY.VerifyError):
            self.check(sig=sig)

    def test_unknown_keyid_rejected(self):
        other = new_key()
        sig = write(self.tmp.name, "U", SIGN.sign_bytes(other, SUMS).encode())
        with self.assertRaises(VERIFY.VerifyError):
            self.check(sig=sig)

    def test_no_key_pinned(self):
        keys = write(self.tmp.name, "empty.txt", b"# no key\n")
        with self.assertRaises(VERIFY.VerifyError) as ctx:
            self.check(keys=keys)
        self.assertEqual(str(ctx.exception), "no release key is pinned in %s" % keys)

    def test_keys_file_duplicate_keyid(self):
        line = SIGN.keyline(self.key.public_key())
        keys = write(self.tmp.name, "dup.txt", (line + "\n" + line + "\n").encode())
        self.assertEqual(len(VERIFY.read_keys(keys)), 1)
        self.check(keys=keys)

    def test_keys_file_keyid_mismatch(self):
        fields = SIGN.keyline(self.key.public_key()).split()
        other = SIGN.keyline(new_key().public_key()).split()
        # a keyid that belongs to another key, and a key that does not match its keyid
        for text in ("%s %s x\n" % (other[0], fields[1]), "%s %s x\n" % (fields[0], other[1])):
            keys = write(self.tmp.name, "bad.txt", text.encode())
            with self.assertRaises(VERIFY.VerifyError, msg=text):
                self.check(keys=keys)

    def test_malformed_sig_lines(self):
        good = read(self.sig).split()
        raw = base64.b64decode(good[2])
        cases = [
            "gridcoin-sha256sums-sig-v2 %s %s\n" % (good[1], good[2]),
            "gridcoin-sha256sums-sig-v1 %s !!!notbase64\n" % good[1],
            "gridcoin-sha256sums-sig-v1 %s %s\n" % (good[1], base64.b64encode(raw[:63]).decode()),
            "gridcoin-sha256sums-sig-v1 %s %s extra\n" % (good[1], good[2]),
            "gridcoin-sha256sums-sig-v1 %s %s" % (good[1], good[2]),
            "",
        ]
        for text in cases:
            sig = write(self.tmp.name, "M", text.encode())
            with self.assertRaises(VERIFY.VerifyError, msg=repr(text)):
                self.check(sig=sig)

    def test_der_to_p1363_leading_zeros(self):
        self.assertEqual(SIGN.der_to_p1363(encode_dss_signature(1, 2)), b"\x00" * 31 + b"\x01" + b"\x00" * 31 + b"\x02")
        high = (1 << 255) | 5
        out = SIGN.der_to_p1363(encode_dss_signature(high, high))
        self.assertEqual(out, high.to_bytes(32, "big") * 2)
        # wrap a DER signature from the key and verify it
        der = self.key.sign(SUMS, ec.ECDSA(SIGN.hashes.SHA256()))
        pub = write(self.tmp.name, "pub.pem", pub_pem(self.key))
        wrapped = SIGN.wrap_text(der, self.key.public_key())
        sig = write(self.tmp.name, "W", wrapped.encode())
        self.check(sig=sig)
        self.assertTrue(os.path.exists(pub))

    def test_keyid_vector(self):
        key = serialization.load_pem_private_key(read_bytes(os.path.join(DATA, "test-key.pem")), None)
        lines = [line for line in read(os.path.join(DATA, "test-key.pub.txt")).splitlines()
                 if line.strip() and not line.startswith("#")]
        self.assertEqual(len(lines), 1)
        self.assertEqual(SIGN.keyline(key.public_key(), "TEST-ONLY"), lines[0])
        self.assertEqual(len(lines[0].split()[0]), 16)

    def test_testdata_vectors(self):
        keys = os.path.join(DATA, "test-release-keys.txt")
        VERIFY.verify_files(keys, os.path.join(DATA, "SHA256SUMS"), os.path.join(DATA, "SHA256SUMS.sig"))
        with self.assertRaises(VERIFY.VerifyError):
            VERIFY.verify_files(keys, os.path.join(DATA, "SHA256SUMS.tampered"), os.path.join(DATA, "SHA256SUMS.sig"))
        with self.assertRaises(VERIFY.VerifyError):
            VERIFY.verify_files(keys, os.path.join(DATA, "SHA256SUMS"), os.path.join(DATA, "SHA256SUMS.sig.wrongkey"))


if __name__ == "__main__":
    unittest.main()
