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

import argparse
import base64
import json
import os
import sys
import time
import urllib.error
import urllib.request


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


class RpcError(Exception):
    pass


class RpcClient:
    def __init__(self, conn):
        self._url = "http://%s:%d/" % (conn["host"], conn["port"])
        token = base64.b64encode(("%s:%s" % (conn["user"], conn["password"])).encode()).decode()
        self._auth = "Basic " + token

    def call(self, method, params):
        body = json.dumps({"jsonrpc": "1.0", "id": "autounlock",
                           "method": method, "params": params}).encode()
        req = urllib.request.Request(self._url, data=body,
                                     headers={"Authorization": self._auth,
                                              "Content-Type": "application/json"})
        try:
            with urllib.request.urlopen(req, timeout=10) as resp:
                payload = json.loads(resp.read().decode())
        except (urllib.error.URLError, OSError, ValueError) as e:
            raise RpcError(str(e))
        if payload.get("error"):
            raise RpcError(str(payload["error"]))
        return payload.get("result")


def should_unlock(prev_uptime, cur_uptime):
    """First contact, or the core restarted (uptime went backwards)."""
    return prev_uptime is None or cur_uptime < prev_uptime


def run_once(client, passphrase, timeout, prev_uptime):
    """One poll: unlock (stake-only) if a fresh core instance is seen.

    Returns the new uptime baseline, or prev_uptime unchanged if the core is
    unreachable / RPC not up yet.
    """
    try:
        info = client.call("getinfo", [])
    except RpcError:
        return prev_uptime  # core down or RPC not bound yet; try again next poll
    cur_uptime = int(info.get("uptime", 0))
    if should_unlock(prev_uptime, cur_uptime):
        client.call("walletpassphrase", [passphrase, timeout, True])  # stake-only
    return cur_uptime


def read_passphrase(path):
    with open(path, "r", encoding="utf8") as f:
        pw = f.read()
    pw = pw.rstrip("\n")
    if not pw:
        raise ValueError("passphrase file %s is empty" % path)
    return pw


def _load_conf(args):
    if args.conf:
        conf_path = args.conf
    elif args.datadir:
        conf_path = os.path.join(args.datadir, "gridcoin.conf")
    else:
        return {}
    try:
        with open(conf_path, "r", encoding="utf8") as f:
            return parse_conf(f.read())
    except OSError:
        return {}


def build_arg_parser():
    p = argparse.ArgumentParser(description="Stake-only Gridcoin wallet autounlock helper.")
    p.add_argument("--datadir", help="Datadir containing gridcoin.conf.")
    p.add_argument("--conf", help="Explicit path to gridcoin.conf (overrides --datadir).")
    p.add_argument("--passphrase-file", required=True, dest="passphrase_file",
                   help="File the platform credential store populates with the wallet passphrase. "
                        "Never pass the passphrase on the command line.")
    p.add_argument("--rpcconnect", default=None)
    p.add_argument("--rpcport", type=int, default=None)
    p.add_argument("--rpcuser", default=None)
    p.add_argument("--rpcpassword", default=None)
    p.add_argument("--timeout", type=int, default=99999999,
                   help="walletpassphrase timeout seconds (node clamps > 100000000).")
    p.add_argument("--interval", type=int, default=20, help="Poll interval seconds.")
    p.add_argument("--once", action="store_true", help="Poll once and exit (testing / one-shot).")
    return p


def main(argv=None):
    args = build_arg_parser().parse_args(argv)
    passphrase = read_passphrase(args.passphrase_file)
    conn = resolve_connection(_load_conf(args), args)
    client = RpcClient(conn)
    prev_uptime = None
    while True:
        prev_uptime = run_once(client, passphrase, args.timeout, prev_uptime)
        if args.once:
            return 0
        time.sleep(args.interval)


if __name__ == "__main__":
    sys.exit(main())
