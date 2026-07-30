#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Stake-only wallet autounlock helper.

External autounlock engine for the Linux systemd service and the Windows
scheduled task (see doc/multiprocess.md). Reads RPC connection details from
the config file (gridcoinresearch.conf, or gridcoin.conf), takes the passphrase
from a file the platform credential store populates (never from argv), and
sends `walletpassphrase <pass> <timeout> true` whenever it sees a fresh core
instance. Python 3 stdlib only.

Security-critical behaviour (mirrors the mainnet-validated prototype in the
gridcoinresearchd-autounlock.service design lineage):

  * Listener-ownership gate. On a normal core start the RPC port is UNBOUND for
    minutes -- LoadBlockIndex (init.cpp) runs long before StartRPCThreads -- and
    the RPC port is unprivileged, so any local account can bind it first. HTTP
    Basic authenticates the client to the server, never the reverse, so a naive
    readiness poll would hand rpcuser:rpcpassword (and then the passphrase) to
    whatever answers. Before sending anything over the wire we require that every
    LISTEN socket on the port belongs to our own uid (parsed from /proc/net/tcp
    and /proc/net/tcp6). A foreign owner, or a /proc that cannot be fully read,
    fails closed. The gate needs a POSIX uid model + /proc (Linux). When the
    target is loopback but the platform cannot verify ownership (e.g. Windows),
    the helper REFUSES to send unless --allow-unverified-listener is given. When
    the target is a non-loopback (remote) host the gate cannot apply at all and
    the helper proceeds, having warned at startup -- that is the operator opting
    into network-exposed RPC. A Windows-native owner check (GetExtendedTcpTable)
    is a follow-up that will let the gate run there too.

  * Proxy-free opener. urllib's default opener honours *_proxy from the
    environment and proxy_bypass_environment() does not exempt loopback unless
    no_proxy is set, so an inherited http_proxy could divert the passphrase POST
    -- body and Authorization header -- to a remote host in cleartext. We build a
    proxy-free opener so that cannot happen regardless of the environment.

  * Redaction. The passphrase and rpcpassword are scrubbed out of every log line,
    and a top-level catch-all ensures no unexpected exception prints an
    unredacted traceback or kills the resident poll loop.

Exit status (returned by main(); meaningful mainly in --once mode, where the
systemd oneshot keys RestartPreventExitStatus on it):
  0 -- unlocked, or nothing to do (already unlocked / not encrypted)
  1 -- transient; retrying may help (core not up, RPC not bound yet, transport)
  2 -- unrecoverable; retrying cannot help (wrong passphrase, bad RPC
       credentials/HTTP 401, a foreign process holding the RPC port, or a
       loopback target whose owner cannot be verified on this platform)
In the resident (default, non---once) mode the loop never exits on these; it
logs and keeps polling so an admin can clear the condition without a restart.
"""

import argparse
import base64
import http.client
import json
import os
import sys
import time
import urllib.error
import urllib.request

EXIT_OK = 0
EXIT_TRANSIENT = 1
EXIT_UNRECOVERABLE = 2

# Mainnet RPC port (CBaseChainParams::MAIN, src/chainparamsbase.cpp). The daemon
# defaults to this when rpcport is unset, and a packaged gridcoinresearch.conf
# commonly omits it, so default to it here too (matching contrib/api-proxy). A
# non-mainnet node must set rpcport in the config or pass --rpcport.
DEFAULT_RPC_PORT = 15715

# RPC error codes we react to (src/rpc/protocol.h).
RPC_INVALID_PARAMETER = -8
RPC_WALLET_PASSPHRASE_INCORRECT = -14
RPC_WALLET_WRONG_ENC_STATE = -15
RPC_WALLET_ALREADY_UNLOCKED = -17

# urllib's module-level urlopen() uses the default opener, whose ProxyHandler is
# built from getproxies(); an inherited http_proxy would silently divert the
# walletpassphrase POST off-box in cleartext. This opener carries no proxy.
_OPENER = urllib.request.build_opener(urllib.request.ProxyHandler({}))

_SECRETS = []


def register_secret(value):
    """Record a secret so redact() can scrub it from any later log line."""
    if value:
        _SECRETS.append(value)


def redact(text):
    """Strip any known secret out of text before it reaches a log."""
    out = str(text)
    for secret in _SECRETS:
        if secret:
            out = out.replace(secret, "<REDACTED>")
    return out


def log(msg):
    sys.stdout.write("gridcoin_autounlock: " + redact(msg) + "\n")
    sys.stdout.flush()


def warn(msg):
    sys.stderr.write("gridcoin_autounlock: WARNING: " + redact(msg) + "\n")
    sys.stderr.flush()


def parse_conf(text):
    """Parse a Gridcoin config blob the way the core does: strip an inline '#'
    comment (everything from the first '#', which also drops full-line comments),
    then key=value with whitespace trimmed. On a repeated key the FIRST value wins
    -- matching the core, whose config-file settings use reverse_precedence and
    "take the first assigned value instead of last" (src/util/settings.cpp). (The
    core disallows '#' in rpcpassword, so stripping from the first '#' is safe.)
    """
    conf = {}
    for raw in text.splitlines():
        line = raw.split("#", 1)[0].strip()
        if not line or "=" not in line:
            continue
        key, _, value = line.partition("=")
        conf.setdefault(key.strip(), value.strip())  # first occurrence wins
    return conf


def resolve_connection(conf, args):
    """Merge conf + CLI args into a connection dict. Args win. Validate."""
    user = getattr(args, "rpcuser", None) or conf.get("rpcuser")
    password = getattr(args, "rpcpassword", None) or conf.get("rpcpassword")
    host = getattr(args, "rpcconnect", None) or conf.get("rpcconnect") or "127.0.0.1"
    port = getattr(args, "rpcport", None) or conf.get("rpcport") or DEFAULT_RPC_PORT
    if not user or not password:
        raise ValueError("rpcuser and rpcpassword must be set (in the config file or via --rpcuser/--rpcpassword)")
    try:
        port = int(port)
    except (TypeError, ValueError):
        raise ValueError("rpcport must be an integer (got %r)" % (port,))
    return {"host": str(host), "port": port, "user": str(user), "password": str(password)}


def is_loopback(host):
    """True if host addresses this machine's loopback (so the /proc gate applies)."""
    h = str(host).strip().lower()
    return h in ("127.0.0.1", "::1", "localhost") or h.startswith("127.")


def _platform_can_gate():
    """True if this platform can verify RPC-listener ownership: it needs a POSIX
    uid model (os.getuid). /proc availability is handled separately -- a missing
    /proc yields 'unreadable' (fail closed), which is correct on e.g. macOS."""
    return hasattr(os, "getuid")


def parse_listen_uids(text, port):
    """UIDs owning LISTEN sockets on `port` in one /proc/net/tcp{,6} blob.

    Fields (whitespace-split): local_address is index 1 as HEX_IP:HEX_PORT; st is
    index 3 ('0A' == TCP_LISTEN); uid is index 7 (the kernel joins tx_queue:rx_queue
    and tr:tm->when into single colon-tokens, so four header columns collapse to
    two data fields). The bind address is deliberately ignored -- requiring every
    listener on the port to be ours is simpler and strictly more conservative than
    decoding v4-mapped IPv6 / v6_only, which /proc does not expose.
    """
    uids = set()
    for line in text.splitlines()[1:]:  # skip the header row
        fields = line.split()
        if len(fields) < 8 or fields[3] != "0A":
            continue
        try:
            if int(fields[1].rsplit(":", 1)[1], 16) != port:
                continue
            uids.add(int(fields[7]))
        except (IndexError, ValueError):
            continue
    return uids


def listening_uids(port):
    """Return (uids, readable, unreadable): UIDs owning LISTEN sockets on `port`
    across IPv4 and IPv6, how many of the two /proc tables were read successfully,
    and whether any table was present but could NOT be read.

    FileNotFoundError means that address family is simply absent (e.g. IPv6
    disabled) and is not an error. Any other OSError means the table exists but we
    could not read it -- which must fail closed (see check_listener_ownership),
    because a listener we cannot see could be a foreign one on the port.
    """
    uids = set()
    readable = 0
    unreadable = False
    for path in ("/proc/net/tcp", "/proc/net/tcp6"):
        try:
            with open(path, "r", encoding="ascii", errors="replace") as handle:
                text = handle.read()
        except FileNotFoundError:
            continue  # this family is absent -- not an error
        except OSError:
            unreadable = True  # present but unreadable -- must fail closed
            continue
        readable += 1
        uids |= parse_listen_uids(text, port)
    return uids, readable, unreadable


def check_listener_ownership(port):
    """Classify who is listening on `port` before any credential goes on the wire.

    Returns one of:
      'ours'       -- one or more LISTEN sockets, all owned by our uid: proceed
      'none'       -- nothing is listening yet (core not up): transient
      'foreign'    -- a uid other than ours owns a LISTEN socket: refuse
      'unreadable' -- a /proc table could not be read, or none could: refuse.
                      This is fail-closed: an unreadable table might hide a foreign
                      listener (e.g. an attacker on ::1 when tcp6 is unreadable).
    """
    uids, readable, unreadable = listening_uids(port)
    if unreadable or readable == 0:
        return "unreadable"
    me = os.getuid()
    if uids - {me}:
        return "foreign"
    if uids:
        return "ours"
    return "none"


class RpcError(Exception):
    """A JSON-RPC or transport failure. `code` is the JSON-RPC error code when one
    was returned (else None); `http_status` is the HTTP status when relevant."""

    def __init__(self, message, code=None, http_status=None):
        super().__init__(message)
        self.code = code
        self.http_status = http_status


class RpcClient:
    def __init__(self, conn, allow_unverified=False):
        self.host = conn["host"]
        self.port = conn["port"]
        # Bracket an IPv6 literal host so the URL is valid: http://[::1]:port/,
        # not the ambiguous http://::1:port/. is_loopback() accepts ::1, so an
        # IPv6 loopback target would otherwise fail every poll.
        url_host = "[%s]" % self.host if (":" in self.host and not self.host.startswith("[")) else self.host
        self._url = "http://%s:%d/" % (url_host, self.port)
        token = base64.b64encode(("%s:%s" % (conn["user"], conn["password"])).encode()).decode()
        self._auth = "Basic " + token
        register_secret(token)  # so an accidental echo of the auth header is scrubbed
        # Whether the target is our own loopback (the /proc gate applies) and
        # whether the operator has accepted proceeding without a verifiable owner.
        self.loopback = is_loopback(self.host)
        self.allow_unverified = allow_unverified

    def call(self, method, params):
        body = json.dumps({"jsonrpc": "1.0", "id": "autounlock",
                           "method": method, "params": params}).encode()
        req = urllib.request.Request(self._url, data=body,
                                     headers={"Authorization": self._auth,
                                              "Content-Type": "application/json"})
        try:
            with _OPENER.open(req, timeout=10) as resp:
                payload = json.loads(resp.read().decode())
        except urllib.error.HTTPError as e:
            # Gridcoin returns application-level JSON-RPC errors with an HTTP 500
            # status and the error object in the body; auth failure is 401. Parse
            # the body so the real code/message surfaces. (HTTPError is a URLError
            # subclass, so this except must precede the URLError one.)
            if e.code == 401:
                raise RpcError("HTTP 401 Unauthorized: rpcuser/rpcpassword rejected by the daemon",
                               http_status=401)
            try:
                err = json.loads(e.read().decode()).get("error")
            except (ValueError, OSError):
                err = None
            if isinstance(err, dict):
                raise RpcError(str(err.get("message") or err), code=err.get("code"), http_status=e.code)
            raise RpcError("HTTP %d error" % e.code, http_status=e.code)
        except (urllib.error.URLError, http.client.HTTPException, OSError, ValueError) as e:
            # Transport-level (connection refused, timeout, malformed HTTP such as
            # BadStatusLine, non-JSON body). No JSON-RPC code; retry may help.
            raise RpcError(str(e))
        if not isinstance(payload, dict):
            raise RpcError("RPC returned a non-object JSON payload for %r" % method)
        err = payload.get("error")
        if err:
            if isinstance(err, dict):
                raise RpcError(str(err.get("message") or err), code=err.get("code"))
            raise RpcError(str(err))
        return payload.get("result")


def should_unlock(prev_uptime, cur_uptime):
    """First contact, or the core restarted (uptime went backwards)."""
    return prev_uptime is None or cur_uptime < prev_uptime


def gate(client):
    """Run the listener-ownership gate for `client`. Returns an EXIT_* code to
    stop this poll (nothing was sent), or None to proceed with RPC calls."""
    if not getattr(client, "loopback", False):
        return None  # remote target: the /proc gate cannot apply (warned at startup)
    if not _platform_can_gate():
        # Loopback target, but this platform has no way to verify who owns the
        # port. Fail closed unless the operator explicitly accepted the risk.
        if getattr(client, "allow_unverified", False):
            return None
        warn("cannot verify the owner of the loopback RPC port %d on this platform; "
             "refusing to send credentials. Use --allow-unverified-listener on a "
             "trusted single-user host to override." % client.port)
        return EXIT_UNRECOVERABLE
    status = check_listener_ownership(client.port)
    if status == "foreign":
        warn("REFUSING TO SEND CREDENTIALS: a process owned by another uid is "
             "listening on the RPC port %d. Either the core runs as a different "
             "user than expected, or a local account has bound the RPC port. "
             "Investigate before relying on autounlock." % client.port)
        return EXIT_UNRECOVERABLE
    if status == "unreadable":
        warn("cannot read /proc/net/tcp{,6} to verify who owns the RPC port %d; "
             "refusing to send credentials to an unverified listener" % client.port)
        return EXIT_UNRECOVERABLE
    if status == "none":
        return EXIT_TRANSIENT  # nothing listening yet; the core is still coming up
    return None  # 'ours': proceed


def run_once(client, passphrase, timeout, prev_uptime):
    """One poll: gate on listener ownership, then unlock (stake-only) if a fresh
    core instance is seen. Returns (new_uptime, exit_code). The exit_code reflects
    this poll's outcome; the resident loop ignores it, --once returns it.
    """
    gate_code = gate(client)
    if gate_code is not None:
        return prev_uptime, gate_code

    try:
        info = client.call("getinfo", [])
    except RpcError as e:
        if e.http_status == 401:
            # Wrong rpcuser/rpcpassword: retrying with the same credentials cannot
            # help, so this is unrecoverable (the oneshot then parks in "failed").
            warn("RPC rejected the credentials (HTTP 401): rpcuser/rpcpassword are "
                 "wrong -- retrying cannot fix this")
            return prev_uptime, EXIT_UNRECOVERABLE
        return prev_uptime, EXIT_TRANSIENT  # core down or RPC not bound yet
    if not isinstance(info, dict):
        return prev_uptime, EXIT_TRANSIENT
    try:
        cur_uptime = int(info.get("uptime", 0))
    except (TypeError, ValueError):
        return prev_uptime, EXIT_TRANSIENT
    if not should_unlock(prev_uptime, cur_uptime):
        return cur_uptime, EXIT_OK

    try:
        client.call("walletpassphrase", [passphrase, timeout, True])  # stake-only
    except RpcError as e:
        if e.http_status == 401:
            warn("RPC rejected the credentials (HTTP 401) -- retrying cannot fix this")
            return cur_uptime, EXIT_UNRECOVERABLE
        code = e.code
        if code == RPC_WALLET_ALREADY_UNLOCKED:
            # -17 is thrown before the stake-only flag is assigned, so the wallet
            # is unlocked but by another path and the staking-only restriction was
            # NOT re-asserted -- if that path was a full GUI unlock it is spendable.
            # No RPC exposes the flag, so we can only report it.
            warn("wallet was already unlocked by another path (-17); the staking-only "
                 "restriction was NOT re-asserted and may not be in effect")
            return cur_uptime, EXIT_OK
        if code == RPC_WALLET_WRONG_ENC_STATE:
            warn("wallet is not encrypted (-15); there is nothing to unlock")
            return cur_uptime, EXIT_OK
        if code in (RPC_WALLET_PASSPHRASE_INCORRECT, RPC_INVALID_PARAMETER):
            warn("walletpassphrase was rejected (%s): %s -- retrying cannot fix this"
                 % (code, e))
            return cur_uptime, EXIT_UNRECOVERABLE
        # Unknown/transport error: log and let the next poll (or systemd) retry.
        warn("walletpassphrase failed: %s" % e)
        return cur_uptime, EXIT_TRANSIENT
    return cur_uptime, EXIT_OK


def read_passphrase(path):
    # utf-8-sig transparently drops a UTF-8 BOM, which Windows editors / PowerShell
    # redirection routinely prepend -- an unstripped BOM would make walletpassphrase
    # reject the passphrase (-14).
    with open(path, "r", encoding="utf-8-sig") as f:
        pw = f.read()
    pw = pw.rstrip("\r\n")  # strip trailing CR/LF only (do not touch chosen spaces)
    if not pw.strip():
        raise ValueError("passphrase file %s is empty or whitespace-only" % path)
    return pw


def _load_conf(args):
    if args.conf:
        candidates = [args.conf]
    elif args.datadir:
        # Core default is gridcoinresearch.conf (GRIDCOIN_CONF_FILENAME); gridcoin.conf
        # is a common packaging alias. Prefer the default, then fall back to the alias.
        candidates = [os.path.join(args.datadir, "gridcoinresearch.conf"),
                      os.path.join(args.datadir, "gridcoin.conf")]
    else:
        return {}
    for conf_path in candidates:
        try:
            with open(conf_path, "r", encoding="utf8") as f:
                return parse_conf(f.read())
        except OSError:
            continue
    return {}


def build_arg_parser():
    p = argparse.ArgumentParser(description="Stake-only Gridcoin wallet autounlock helper.")
    p.add_argument("--datadir",
                   help="Data directory containing gridcoinresearch.conf (or gridcoin.conf) -- the "
                        "main configuration file, alongside the wallet database and the blockchain.")
    p.add_argument("--conf",
                   help="Explicit path to the configuration file (overrides the --datadir lookup of "
                        "gridcoinresearch.conf / gridcoin.conf).")
    p.add_argument("--passphrase-file", required=True, dest="passphrase_file",
                   help="File the platform credential store populates with the wallet passphrase. "
                        "Never pass the passphrase on the command line.")
    p.add_argument("--rpcconnect", default=None)
    p.add_argument("--rpcport", type=int, default=None,
                   help="RPC port (defaults to the config's rpcport, else %d -- the mainnet "
                        "default; a non-mainnet node must set it here or in the config)." % DEFAULT_RPC_PORT)
    p.add_argument("--rpcuser", default=None)
    p.add_argument("--rpcpassword", default=None,
                   help="RPC password (visible in the process list; prefer setting it in the config file).")
    p.add_argument("--timeout", type=int, default=99999999,
                   help="walletpassphrase timeout seconds (node clamps > 100000000).")
    p.add_argument("--interval", type=int, default=20, help="Poll interval seconds.")
    p.add_argument("--once", action="store_true", help="Poll once and exit (testing / one-shot service).")
    p.add_argument("--allow-unverified-listener", action="store_true", dest="allow_unverified_listener",
                   help="Proceed even when the RPC listener's owner cannot be verified on this platform "
                        "(e.g. Windows, which lacks the /proc-based gate). INSECURE unless the host is "
                        "single-user and trusted: without owner verification the passphrase could be sent "
                        "to a local process that squatted the RPC port during core startup.")
    return p


def main(argv=None):
    args = build_arg_parser().parse_args(argv)
    passphrase = read_passphrase(args.passphrase_file)
    register_secret(passphrase)
    conn = resolve_connection(_load_conf(args), args)
    register_secret(conn["password"])
    client = RpcClient(conn, allow_unverified=args.allow_unverified_listener)
    if not client.loopback:
        warn("RPC target %s is not loopback; the listener-ownership gate cannot "
             "apply and credentials will be sent to the configured host." % conn["host"])
    elif not _platform_can_gate():
        if args.allow_unverified_listener:
            warn("the listener-ownership gate is unavailable on this platform; proceeding "
                 "because --allow-unverified-listener was given (trusted single-user host assumed)")
        else:
            warn("the listener-ownership gate is unavailable on this platform (no POSIX uid "
                 "model); credentials will NOT be sent until --allow-unverified-listener is set "
                 "or native listener verification is added")
    prev_uptime = None
    while True:
        try:
            prev_uptime, code = run_once(client, passphrase, args.timeout, prev_uptime)
        except Exception as e:
            # The resident loop must never die and must never print an unredacted
            # traceback. Treat anything unexpected as a transient poll failure.
            warn("unexpected error during poll: %s: %s" % (type(e).__name__, e))
            code = EXIT_TRANSIENT
        if args.once:
            return code
        time.sleep(args.interval)


if __name__ == "__main__":
    sys.exit(main())
