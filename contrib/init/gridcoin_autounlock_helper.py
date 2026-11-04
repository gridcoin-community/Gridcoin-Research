#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Socket-activated stake-only wallet passphrase helper.

Started by systemd on a connection to the helper socket (Accept=no, LISTEN_FDS=1,
fd 3) as a DynamicUser= service whose LoadCredential= holds the wallet passphrase.
It serves the passphrase to exactly one client, the MainPID of the bound core unit,
and exits. Python 3 standard library only.

The decision order in decide() is the security property. The passphrase file is
read only after decide() has allowed the peer, and a failed systemd query ends the
run without opening the credential. Cgroup membership is never consulted, because a
child of the core is in the same cgroup and must not be served.

Exit status
  0 served, 3 refused, 4 no connection within the accept wait, 5 credential
  unusable, 6 systemd query failed, 64 usage, 70 internal error.
"""

import os
import pwd
import select
import socket
import struct
import subprocess
import sys
import time

EXIT_SERVED, EXIT_REFUSED, EXIT_NO_CONNECTION, EXIT_CREDENTIAL, EXIT_SYSTEMD, EXIT_USAGE, EXIT_INTERNAL = 0, 3, 4, 5, 6, 64, 70

ACCEPT_WAIT_SECONDS = 5
SEND_TIMEOUT_SECONDS = 10
MAX_PASSPHRASE_BYTES = 1024
CREDENTIAL_NAME = "wallet-passphrase"
SYSTEMCTL_PATH = "/usr/bin:/bin"

SO_PEERPIDFD = getattr(socket, "SO_PEERPIDFD", 77)
# architectures where the generic SO_PEERPIDFD number is known to apply
PEERPIDFD_MACHINES = {"x86_64", "i686", "aarch64", "armv7l", "riscv64", "ppc64le", "s390x"}


class UsageError(Exception):
    pass


class SystemdQueryError(Exception):
    pass


class CredentialError(Exception):
    pass


def listen_fd_from_environment(env, pid):
    # The socket-activation protocol: LISTEN_PID names the process the descriptors
    # were meant for, LISTEN_FDS counts them starting at fd 3.
    if env.get("LISTEN_PID") != str(pid):
        raise UsageError("LISTEN_PID does not name this process")
    if env.get("LISTEN_FDS") != "1":
        raise UsageError("expected exactly one activation socket")
    return 3


def peer_credentials(conn):
    raw = conn.getsockopt(socket.SOL_SOCKET, socket.SO_PEERCRED, struct.calcsize("3i"))
    pid, uid, gid = struct.unpack("3i", raw)
    return pid, uid, gid


def peer_pidfd(conn):
    # A pidfd for the peer, or None when the kernel cannot supply a trustworthy one.
    if os.uname().machine not in PEERPIDFD_MACHINES:
        return None
    try:
        raw = conn.getsockopt(socket.SOL_SOCKET, SO_PEERPIDFD, struct.calcsize("i"))
        fd = struct.unpack("i", raw)[0]
    except (OSError, struct.error):
        return None
    if fd < 0:
        return None
    try:
        with open("/proc/self/fdinfo/%d" % fd, encoding="utf8") as f:
            has_pid = any(line.startswith("Pid:") for line in f)
        target = os.readlink("/proc/self/fd/%d" % fd)
        if has_pid and (target == "anon_inode:[pidfd]" or target.startswith("pidfd:[")):
            return fd
    except (OSError, UnicodeDecodeError):
        pass
    os.close(fd)
    return None


def pidfd_pid(pidfd, proc="/proc"):
    # The pid a pidfd refers to, or -1 once that process has exited.
    with open("%s/self/fdinfo/%d" % (proc, pidfd), encoding="utf8") as f:
        for line in f:
            if line.startswith("Pid:"):
                return int(line.split()[1])
    raise OSError("no Pid line in fdinfo")


def process_start_ticks(pid, proc="/proc"):
    try:
        with open("%s/%d/stat" % (proc, pid), encoding="utf8", errors="replace") as f:
            text = f.read()
    except OSError:
        return None
    # comm may contain spaces and parentheses, so parse after the last ')'.
    # The remainder starts at field 3 (state); field 22 is index 19 of it.
    rest = text[text.rfind(")") + 1:].split()
    try:
        return int(rest[19])
    except (IndexError, ValueError):
        return None


def boottime_ticks():
    return int(time.clock_gettime(time.CLOCK_BOOTTIME) * os.sysconf("SC_CLK_TCK"))


def query_main_pid(unit, run=subprocess.run):
    argv = ["systemctl", "show", "--property=MainPID", "--value", "--", unit]
    try:
        result = run(argv, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, universal_newlines=True,
                     timeout=10, env={"PATH": SYSTEMCTL_PATH}, check=False)
    except (OSError, subprocess.SubprocessError):
        raise SystemdQueryError("systemctl could not be run")
    if result.returncode != 0:
        raise SystemdQueryError("systemctl failed")
    text = result.stdout.strip()
    if not text.isdigit():
        raise SystemdQueryError("unexpected systemctl output")
    return int(text)


def decide(peer_pid, peer_uid, expected_uid, pidfd, accept_ticks, main_pid_fn, start_ticks_fn, pidfd_pid_fn):
    if peer_uid != expected_uid:
        return False, "uid"
    if peer_pid <= 0:
        return False, "pid unknown"
    s1 = None
    if pidfd is None:
        s1 = start_ticks_fn(peer_pid)
        if s1 is None or s1 > accept_ticks:
            return False, "started after accept"
    # SystemdQueryError propagates to the caller
    m = main_pid_fn()
    if m <= 0:
        return False, "no main process"
    if pidfd is not None:
        p = pidfd_pid_fn(pidfd)
        if not (p == peer_pid == m):
            return False, "not the main process"
    else:
        if m != peer_pid or start_ticks_fn(peer_pid) != s1:
            return False, "not the main process"
    return True, "main process"


def load_passphrase(cred_dir):
    path = os.path.join(cred_dir, CREDENTIAL_NAME)
    try:
        with open(path, "rb") as f:
            data = f.read(MAX_PASSPHRASE_BYTES + 3)
    except OSError:
        raise CredentialError("missing")
    if data.endswith(b"\r\n"):
        data = data[:-2]
    elif data.endswith(b"\n"):
        data = data[:-1]
    if not data:
        raise CredentialError("empty")
    if len(data) > MAX_PASSPHRASE_BYTES:
        raise CredentialError("too long")
    try:
        data.decode("utf8")
    except UnicodeDecodeError:
        raise CredentialError("not valid UTF-8") from None
    return data


def default_deps():
    return {
        "main_pid": query_main_pid,
        "start_ticks": process_start_ticks,
        "boottime_ticks": boottime_ticks,
        "pidfd": peer_pidfd,
        "pidfd_pid": pidfd_pid,
        "load_passphrase": load_passphrase,
        "uid_of_user": lambda name: pwd.getpwnam(name).pw_uid,
        "log": sys.stderr,
        "accept_timeout": ACCEPT_WAIT_SECONDS,
    }


def log_line(deps, text):
    deps["log"].write("gridcoin-autounlock-helper: %s\n" % text)
    deps["log"].flush()


def serve_once(listen_fd, bound_unit, peer_user, env, deps):
    os.set_inheritable(listen_fd, False)
    listener = socket.socket(fileno=listen_fd)
    pidfd = None
    try:
        ready, _, _ = select.select([listener], [], [], deps["accept_timeout"])
        if not ready:
            log_line(deps, "no connection within %s s" % deps["accept_timeout"])
            return EXIT_NO_CONNECTION
        conn, _ = listener.accept()
    finally:
        listener.close()
    try:
        accept_ticks = deps["boottime_ticks"]()
        peer_pid, peer_uid, _ = peer_credentials(conn)
        pidfd = deps["pidfd"](conn)
        try:
            expected_uid = deps["uid_of_user"](peer_user)
        except KeyError:
            log_line(deps, "refused peer pid %d uid %d: unknown peer user" % (peer_pid, peer_uid))
            return EXIT_REFUSED
        try:
            allowed, reason = decide(peer_pid, peer_uid, expected_uid, pidfd, accept_ticks,
                                     lambda: deps["main_pid"](bound_unit), deps["start_ticks"],
                                     deps["pidfd_pid"])
        except SystemdQueryError as e:
            log_line(deps, "systemd query failed for %s: %s" % (bound_unit, e))
            return EXIT_SYSTEMD
        if not allowed:
            log_line(deps, "refused peer pid %d uid %d: %s" % (peer_pid, peer_uid, reason))
            return EXIT_REFUSED
        cred_dir = env.get("CREDENTIALS_DIRECTORY")
        try:
            if not cred_dir:
                raise CredentialError("missing")
            passphrase = deps["load_passphrase"](cred_dir)
        except CredentialError as e:
            log_line(deps, "credential unusable: %s" % e)
            return EXIT_CREDENTIAL
        conn.settimeout(SEND_TIMEOUT_SECONDS)
        conn.sendall(passphrase)
        conn.shutdown(socket.SHUT_WR)
        log_line(deps, "served %s main pid %d" % (bound_unit, peer_pid))
        return EXIT_SERVED
    finally:
        if pidfd is not None:
            os.close(pidfd)
        conn.close()


def parse_args(argv):
    values = {}
    for arg in argv:
        for name in ("--bound-unit", "--peer-user"):
            if arg.startswith(name + "="):
                values[name] = arg[len(name) + 1:]
                break
        else:
            raise UsageError("unknown argument")
    if not values.get("--bound-unit") or not values.get("--peer-user"):
        raise UsageError("--bound-unit and --peer-user are required")
    return values["--bound-unit"], values["--peer-user"]


def main(argv, stderr=None):
    err = stderr if stderr is not None else sys.stderr
    try:
        bound_unit, peer_user = parse_args(argv)
        fd = listen_fd_from_environment(os.environ, os.getpid())
    except UsageError as e:
        err.write("gridcoin-autounlock-helper: usage: %s\n" % e)
        return EXIT_USAGE
    deps = default_deps()
    deps["log"] = err
    try:
        return serve_once(fd, bound_unit, peer_user, dict(os.environ), deps)
    except Exception as e:
        # only the exception class is logged, so no secret can reach the log
        err.write("gridcoin-autounlock-helper: internal error: %s\n" % type(e).__name__)
        return EXIT_INTERNAL


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
