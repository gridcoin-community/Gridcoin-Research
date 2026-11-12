#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Socket-activated stake-only wallet passphrase helper.

Started by systemd on a connection to the helper socket (Accept=no, LISTEN_FDS=1,
fd 3) as a service under a fixed system account, whose LoadCredential= holds the wallet
passphrase.
It serves the passphrase to exactly one client, the MainPID of the bound core unit,
and exits. Python 3 standard library only.

Before serving, it sends a challenge of CHALLENGE_BYTES random bytes, which the client
sends back. The kernel stamps that answer with the pid of the process that sent it, when
it was sent, so the answer ties the connection to a process that existed after the
challenge was taken.

The decision order in decide() is the security property. The passphrase file is
read only after decide() has allowed the peer, and a failed systemd query ends the
run without opening the credential. Cgroup membership is never consulted, because a
child of the core is in the same cgroup and must not be served.

Exit status
  0 served, 3 refused, 4 no connection within the accept wait, 5 credential
  unusable, 6 systemd query failed, 64 usage, 70 internal error.
"""

import hmac
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
ANSWER_WAIT_SECONDS = 10
SEND_TIMEOUT_SECONDS = 10
# The challenge's length; CHALLENGE_BYTES in src/wallet/autounlock_socket.h is the same.
CHALLENGE_BYTES = 16
UCRED = struct.Struct("3i")
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


def read_answer(conn, size, timeout):
    # The answer to the challenge as (bytes, pid, uid), the pid and uid those the kernel stamped on it for the process
    # that sent it (SO_PASSCRED is set on conn). None when fewer than size bytes arrive within timeout seconds, or
    # when what arrives carries no stamp, or the stamps of more than one process.
    deadline = time.monotonic() + timeout
    data = b""
    sender = None
    while len(data) < size:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            return None
        conn.settimeout(remaining)
        try:
            chunk, ancdata, _, _ = conn.recvmsg(size - len(data), socket.CMSG_SPACE(UCRED.size))
        except OSError:
            return None
        if not chunk:
            return None
        stamp = None
        for level, kind, payload in ancdata:
            if level == socket.SOL_SOCKET and kind == socket.SCM_CREDENTIALS and len(payload) >= UCRED.size:
                stamp = UCRED.unpack(payload[:UCRED.size])
        if stamp is None or stamp[0] <= 0 or (sender is not None and stamp != sender):
            return None
        sender = stamp
        data += chunk
    return data, sender[0], sender[1]


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
    close = text.rfind(")")
    if close < 0:
        return None
    rest = text[close + 1:].split()
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
    # str.isdigit() is true for some non-ASCII digits that int() may reject or misread
    if not (text.isascii() and text.isdigit()):
        raise SystemdQueryError("unexpected systemctl output")
    try:
        return int(text)
    except (ValueError, UnicodeDecodeError):
        raise SystemdQueryError("unexpected systemctl output") from None


def decide(peer_pid, peer_uid, answer_pid, answer_uid, expected_uid, pidfd, challenge_ticks, main_pid_fn,
           start_ticks_fn, pidfd_pid_fn):
    # The process that connected must be the one that answered the challenge, by the kernel's stamp on the answer.
    # With a pidfd (kernels from 6.5) the pidfd names that process for good. Without one the peer is its pid and
    # start time: a process with that pid that started before the challenge was taken (challenge_ticks) and still
    # runs is the one that sent the answer, which came after the challenge. A pid recycled since is refused.
    if peer_uid != expected_uid or answer_uid != expected_uid:
        return False, "uid"
    if peer_pid <= 0:
        return False, "pid unknown"
    if answer_pid != peer_pid:
        return False, "the answer came from another process"
    s1 = None
    if pidfd is None:
        s1 = start_ticks_fn(peer_pid)
        if s1 is None:
            return False, "start time unreadable (is /proc mounted with hidepid?)"
        if s1 >= challenge_ticks:
            return False, "started after the challenge"
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
        "answer_timeout": ANSWER_WAIT_SECONDS,
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
        peer_pid, peer_uid, _ = peer_credentials(conn)
        pidfd = deps["pidfd"](conn)
        try:
            expected_uid = deps["uid_of_user"](peer_user)
        except KeyError:
            log_line(deps, "refused peer pid %d uid %d: unknown peer user" % (peer_pid, peer_uid))
            return EXIT_REFUSED
        # The stamps are asked for before the challenge is sent, so the answer, which comes after it, carries one.
        conn.setsockopt(socket.SOL_SOCKET, socket.SO_PASSCRED, 1)
        challenge_ticks = deps["boottime_ticks"]()
        challenge = os.urandom(CHALLENGE_BYTES)
        try:
            conn.settimeout(SEND_TIMEOUT_SECONDS)
            conn.sendall(challenge)
            answer = read_answer(conn, CHALLENGE_BYTES, deps["answer_timeout"])
        except OSError:
            answer = None
        if answer is None or not hmac.compare_digest(answer[0], challenge):
            log_line(deps, "refused peer pid %d uid %d: the challenge was not answered" % (peer_pid, peer_uid))
            return EXIT_REFUSED
        try:
            allowed, reason = decide(peer_pid, peer_uid, answer[1], answer[2], expected_uid, pidfd, challenge_ticks,
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
