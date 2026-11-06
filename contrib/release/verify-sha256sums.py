#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

"""Verify a SHA256SUMS list against its detached signature and a pinned key file.

  verify-sha256sums.py --keys release-keys.txt SHA256SUMS SHA256SUMS.sig [--file <name>]

Exit 0 only when the signature verifies under a key pinned in the keys file. With --file,
the SHA-256 the list gives for that name is printed. The formats are in sign-sha256sums.py.
"""

import argparse
import base64
import binascii
import importlib.util
import os
import re
import sys

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.asymmetric.utils import encode_dss_signature


def _load_signer():
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sign-sha256sums.py")
    spec = importlib.util.spec_from_file_location("sign_sha256sums", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


SIGNER = _load_signer()
SUMS_LINE = re.compile(r"^([0-9a-f]{64})  ([^/\\\s][^/\\]*)$")


class VerifyError(Exception):
    pass


def read_keys(path):
    """{keyid: X||Y} from a release-keys.txt."""
    keys = {}
    with open(path, encoding="utf-8") as handle:
        for line in handle:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            fields = line.split(None, 2)
            if len(fields) < 2:
                raise VerifyError("malformed key line in %s" % path)
            try:
                xy = base64.b64decode(fields[1], validate=True)
            except (binascii.Error, ValueError):
                raise VerifyError("malformed key line in %s" % path)
            if len(xy) != 64 or SIGNER.keyid_of(xy) != fields[0]:
                raise VerifyError("key line %s in %s does not match its key id" % (fields[0], path))
            keys[fields[0]] = xy
    if not keys:
        raise VerifyError("no release key is pinned in %s" % path)
    return keys


def parse_sig(text):
    if not text.endswith("\n") or text.count("\n") != 1:
        raise VerifyError("the signature file must be exactly one LF-terminated line")
    fields = text[:-1].split(" ")
    if len(fields) != 3 or fields[0] != SIGNER.SIG_PREFIX:
        raise VerifyError("the signature line is malformed")
    if not re.fullmatch(r"[0-9a-f]{16}", fields[1]):
        raise VerifyError("the signature key id is malformed")
    try:
        raw = base64.b64decode(fields[2], validate=True)
    except (binascii.Error, ValueError):
        raise VerifyError("the signature is not valid base64")
    if len(raw) != 64:
        raise VerifyError("the signature is %d bytes, not 64" % len(raw))
    return fields[1], raw


def verify_files(keys_path, sums_path, sig_path):
    keys = read_keys(keys_path)
    with open(sig_path, "rb") as handle:
        try:
            text = handle.read().decode("ascii")
        except UnicodeDecodeError:
            raise VerifyError("the signature file is not ASCII")
    keyid, raw = parse_sig(text)
    if keyid not in keys:
        raise VerifyError("the signature key %s is not pinned in %s" % (keyid, keys_path))
    xy = keys[keyid]
    numbers = ec.EllipticCurvePublicNumbers(int.from_bytes(xy[:32], "big"), int.from_bytes(xy[32:], "big"), ec.SECP256R1())
    try:
        public_key = numbers.public_key()
    except ValueError:
        raise VerifyError("the pinned key %s is not a point on P-256" % keyid)
    with open(sums_path, "rb") as handle:
        data = handle.read()
    der = encode_dss_signature(int.from_bytes(raw[:32], "big"), int.from_bytes(raw[32:], "big"))
    try:
        public_key.verify(der, data, ec.ECDSA(hashes.SHA256()))
    except InvalidSignature:
        raise VerifyError("the signature does not match %s" % sums_path)
    return data


def hash_for(data, name):
    found = None
    for line in data.decode("utf-8").split("\n"):
        if not line:
            continue
        match = SUMS_LINE.match(line)
        if not match:
            raise VerifyError("malformed line in the checksum list")
        if match.group(2) == name:
            if found is not None:
                raise VerifyError("%s is listed twice" % name)
            found = match.group(1)
    if found is None:
        raise VerifyError("%s is not listed" % name)
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--keys", required=True)
    parser.add_argument("--file")
    parser.add_argument("sums")
    parser.add_argument("sig")
    args = parser.parse_args()
    try:
        data = verify_files(args.keys, args.sums, args.sig)
        if args.file:
            print(hash_for(data, args.file))
        else:
            print("signature ok")
    except VerifyError as err:
        print("verify-sha256sums: %s" % err, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
