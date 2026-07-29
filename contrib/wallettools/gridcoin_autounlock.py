#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Stake-only wallet autounlock helper.

External autounlock engine for the Linux systemd service and the Windows
scheduled task (see doc/multiprocess.md). Reads RPC connection details from
gridcoin.conf, takes the passphrase from a file the platform credential store
populates (never from argv), and sends `walletpassphrase <pass> <timeout> true`
whenever it sees a fresh core instance. Python 3 stdlib only.
"""

def parse_conf(text):
    """Parse a gridcoin.conf-style key=value blob. '#' comments, last wins."""
    conf = {}
    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            continue
        key, _, value = line.partition("=")
        conf[key.strip()] = value.strip()
    return conf


def resolve_connection(conf, args):
    """Merge conf + CLI args into a connection dict. Args win. Validate."""
    user = getattr(args, "rpcuser", None) or conf.get("rpcuser")
    password = getattr(args, "rpcpassword", None) or conf.get("rpcpassword")
    host = getattr(args, "rpcconnect", None) or conf.get("rpcconnect") or "127.0.0.1"
    port = getattr(args, "rpcport", None) or conf.get("rpcport")
    if not user or not password:
        raise ValueError("rpcuser and rpcpassword must be set (gridcoin.conf or --rpcuser/--rpcpassword)")
    if not port:
        raise ValueError("rpcport must be set (gridcoin.conf or --rpcport)")
    return {"host": str(host), "port": int(port), "user": str(user), "password": str(password)}
