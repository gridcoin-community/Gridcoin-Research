#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

"""Sign a SHA256SUMS release list with a detached ECDSA P-256 signature.

  sign-sha256sums.py sign --key key.pem SHA256SUMS
  sign-sha256sums.py wrap --der-signature sig.der --pubkey pub.pem SHA256SUMS
  sign-sha256sums.py keyline --pubkey pub.pem

sign writes SHA256SUMS.sig next to the list. wrap does the same for a DER signature
that a hardware token produced. keyline prints the release-keys.txt line for a public key.

SHA256SUMS.sig is one line: gridcoin-sha256sums-sig-v1 <keyid> <base64 of r||s, 64 bytes>.
The signature is ECDSA P-256 over the SHA-256 of the exact bytes of SHA256SUMS. The keyid is
the lowercase hex of the first 8 bytes of SHA-256(X||Y), X and Y as 32-byte big-endian.
"""

import argparse
import base64
import hashlib
import sys

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.asymmetric.utils import decode_dss_signature

SIG_PREFIX = "gridcoin-sha256sums-sig-v1"


def public_xy(public_key):
    numbers = public_key.public_numbers()
    if not isinstance(public_key.curve, ec.SECP256R1):
        raise ValueError("the key is not on P-256")
    return numbers.x.to_bytes(32, "big") + numbers.y.to_bytes(32, "big")


def keyid_of(xy):
    return hashlib.sha256(xy).digest()[:8].hex()


def keyline(public_key, label="gridcoin-release"):
    xy = public_xy(public_key)
    return "%s %s %s" % (keyid_of(xy), base64.b64encode(xy).decode(), label)


def der_to_p1363(der):
    r, s = decode_dss_signature(der)
    return r.to_bytes(32, "big") + s.to_bytes(32, "big")


def sig_line(public_key, p1363):
    return "%s %s %s\n" % (SIG_PREFIX, keyid_of(public_xy(public_key)), base64.b64encode(p1363).decode())


def sign_bytes(private_key, data):
    der = private_key.sign(data, ec.ECDSA(hashes.SHA256()))
    return sig_line(private_key.public_key(), der_to_p1363(der))


def wrap_text(der, public_key):
    return sig_line(public_key, der_to_p1363(der))


def load_public(path):
    with open(path, "rb") as handle:
        data = handle.read()
    try:
        return serialization.load_pem_public_key(data)
    except ValueError:
        return serialization.load_pem_private_key(data, None).public_key()


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    p_sign = sub.add_parser("sign")
    p_sign.add_argument("--key", required=True)
    p_sign.add_argument("sums")
    p_wrap = sub.add_parser("wrap")
    p_wrap.add_argument("--der-signature", required=True)
    p_wrap.add_argument("--pubkey", required=True)
    p_wrap.add_argument("sums")
    p_line = sub.add_parser("keyline")
    p_line.add_argument("--pubkey", required=True)
    p_line.add_argument("--label", default="gridcoin-release")
    args = parser.parse_args()

    if args.command == "keyline":
        print(keyline(load_public(args.pubkey), args.label))
        return 0
    if args.command == "sign":
        with open(args.key, "rb") as handle:
            key = serialization.load_pem_private_key(handle.read(), None)
        with open(args.sums, "rb") as handle:
            text = sign_bytes(key, handle.read())
    else:
        with open(args.der_signature, "rb") as handle:
            der = handle.read()
        public_key = load_public(args.pubkey)
        with open(args.sums, "rb") as handle:
            data = handle.read()
        # check the token's signature before it is written down
        public_key.verify(der, data, ec.ECDSA(hashes.SHA256()))
        text = wrap_text(der, public_key)
    with open(args.sums + ".sig", "w", encoding="ascii", newline="\n") as handle:
        handle.write(text)
    print("wrote %s.sig" % args.sums)
    return 0


if __name__ == "__main__":
    sys.exit(main())
