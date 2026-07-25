// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "ipc/process.h"

#include "fs.h"
#include "tinyformat.h"
#include "util.h"
#include "util/syserror.h"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <system_error>
#ifndef WIN32
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#else
// winsock2.h must precede afunix.h/ws2tcpip.h. AF_UNIX for Windows lives in
// <afunix.h> (Windows 10 1803+); sockaddr_un has the same sun_family/sun_path
// layout ParseAddress relies on.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <afunix.h>
#endif

namespace ipc {
namespace {

//! Default socket filename inside the (network-specific) data directory:
//! <datadir>/<network>/node.sock (doc/multiprocess_design.md section 4.3).
const char* const DEFAULT_SOCKET_FILE = "node.sock";

//! Resolve \p address ("unix" -> <data_dir>/node.sock, "unix:<path>" -> a custom
//! path) into a sockaddr_un. Rewrites \p address to its canonical "unix:<path>"
//! form. Throws std::invalid_argument on an unrecognized address or an
//! over-length path.
fs::path ParseAddress(std::string& address, const fs::path& data_dir, struct sockaddr_un& addr)
{
    fs::path path;
    if (address == "unix" || address == "auto") {
        path = data_dir / DEFAULT_SOCKET_FILE;
    } else if (address.rfind("unix:", 0) == 0 && address.size() > 5) {
        path = fs::path(address.substr(5));
        if (!path.is_absolute()) path = data_dir / path;
    } else {
        throw std::invalid_argument(strprintf("Unrecognized IPC address '%s'", address));
    }

    const std::string path_str = path.string();
    if (path_str.size() >= sizeof(addr.sun_path)) {
        throw std::invalid_argument(
            strprintf("Unix socket path '%s' exceeds the maximum socket path length", path_str));
    }
    address = strprintf("unix:%s", path_str);

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path_str.c_str(), sizeof(addr.sun_path) - 1);
    return path;
}

//! Platform-uniform socket helpers. Gridcoin/libmultiprocess carry socket
//! descriptors as int (SocketId); on Windows a SOCKET is uintptr_t but AF_UNIX
//! handles fit in the low 32 bits, so int is used here and converted only at the
//! winsock boundary. INVALID_SOCKET is represented as -1 in int space.
// compat.h provides a cross-platform SOCKET type and closesocket() macro
// (myclosesocket: real closesocket() on Windows, close() on POSIX). It takes a
// SOCKET& lvalue, so hand it one rather than an rvalue cast.
void CloseSocket(int fd) { SOCKET s = static_cast<SOCKET>(fd); closesocket(s); }
#ifdef WIN32
int LastSocketError() { return ::WSAGetLastError(); }

//! Translate a winsock error into a std::error_code whose comparison against
//! std::errc still works: interfaces.cpp classifies a connect failure as
//! connection_refused / no_such_file_or_directory / not_a_directory to detect a
//! not-running daemon, and libstdc++'s system_category() does not map the WSA
//! range. Map the connect-relevant codes to their POSIX errc; keep the raw code
//! under system_category() otherwise so the message is still meaningful.
//! TODO(win-mp W4): confirm on-device which WSA/Win32 code Windows AF_UNIX
//! actually returns for a missing vs. unlistened socket path.
std::error_code SocketErrorCode(int err)
{
    switch (err) {
    case WSAECONNREFUSED:      return std::make_error_code(std::errc::connection_refused);
    case WSAEACCES:            return std::make_error_code(std::errc::permission_denied);
    case WSAEADDRINUSE:        return std::make_error_code(std::errc::address_in_use);
    case WSAENETDOWN:
    case WSAENETUNREACH:       return std::make_error_code(std::errc::network_unreachable);
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND: return std::make_error_code(std::errc::no_such_file_or_directory);
    default:                   return std::error_code(err, std::system_category());
    }
}
#else
int LastSocketError() { return errno; }
std::error_code SocketErrorCode(int err) { return std::error_code(err, std::system_category()); }
#endif

//! Create an AF_UNIX SOCK_STREAM socket that is not inherited by child processes,
//! so the IPC fd cannot leak into an exec'd child. POSIX uses SOCK_CLOEXEC
//! atomically where available (Linux, the BSDs) and falls back to
//! fcntl(FD_CLOEXEC) (notably macOS); Windows uses WSA_FLAG_NO_HANDLE_INHERIT.
//! Returns -1 with the platform socket error set on failure.
int MakeCloexecStreamSocket()
{
#ifdef WIN32
    SOCKET s = ::WSASocketW(AF_UNIX, SOCK_STREAM, 0, nullptr, 0, WSA_FLAG_NO_HANDLE_INHERIT);
    return s == INVALID_SOCKET ? -1 : static_cast<int>(s);
#elif defined(SOCK_CLOEXEC)
    return ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
#else
    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) return -1;
    const int flags = ::fcntl(fd, F_GETFD);
    if (flags == -1 || ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1) {
        const int saved = errno;
        ::close(fd);
        errno = saved;
        return -1;
    }
    return fd;
#endif
}

//! Connect a fresh AF_UNIX SOCK_STREAM socket to \p addr. Returns the connected
//! fd, or -1 with \p out_errno set on failure (caller decides how to react).
int TryConnect(const struct sockaddr_un& addr, int& out_errno)
{
    int fd = MakeCloexecStreamSocket();
    if (fd == -1) {
        out_errno = LastSocketError();
        return -1;
    }
    if (::connect(fd, (const struct sockaddr*)&addr, sizeof(addr)) == 0) {
        out_errno = 0;
        return fd;
    }
    out_errno = LastSocketError();
    CloseSocket(fd);
    return -1;
}

class ProcessImpl : public Process
{
public:
    int connect(const fs::path& data_dir, std::string& address) override
    {
        struct sockaddr_un addr;
        ParseAddress(address, data_dir, addr);
        int connect_errno = 0;
        int fd = TryConnect(addr, connect_errno);
        if (fd == -1) {
            throw std::system_error(SocketErrorCode(connect_errno));
        }
        return fd;
    }

    int bind(const fs::path& data_dir, std::string& address) override
    {
        struct sockaddr_un addr;
        const fs::path path = ParseAddress(address, data_dir, addr);

        // Socket directory access control (design section 4.3). On POSIX we chmod
        // the parent 0700 and fail closed: if we cannot restrict it, refuse to
        // listen rather than expose the socket in a world-accessible directory.
        // Windows has no chmod/umask for AF_UNIX; the socket instead inherits the
        // NTFS ACL of the per-user-profile data directory (owner-only by default) --
        // the same reliance Bitcoin Core has on the datadir ACL. Explicit DACLs are
        // a hardening follow-up (design decision 3 / Windows hardening).
        if (path.has_parent_path()) {
            fs::create_directories(path.parent_path());
#ifndef WIN32
            if (::chmod(path.parent_path().string().c_str(), 0700) != 0) {
                throw std::system_error(errno, std::system_category());
            }
#endif
        }

        // Try to bind; only if the path already exists (EADDRINUSE) do we probe it.
        // A live listener means the node is already running (refuse to start rather
        // than displace it); a refused/absent connection means a stale path from a
        // crash (unlink and retry once). Binding first -- rather than pre-emptively
        // unlinking -- avoids a window where we remove a socket another starting
        // node just bound. (A residual race with a live-but-backlog-full listener
        // remains; a lock file is the future hardening.)
        for (int attempt = 0; attempt < 2; ++attempt) {
            int fd = MakeCloexecStreamSocket();
            if (fd == -1) {
                throw std::system_error(SocketErrorCode(LastSocketError()));
            }
#ifndef WIN32
            // Create the socket node with 0600 from the start: bind() honors the
            // umask, so a permissive umask would otherwise leave a world-accessible
            // window before an after-the-fact chmod. (No umask on Windows; the
            // datadir ACL governs access -- see the directory comment above.)
            const mode_t old_umask = ::umask(0177);
#endif
            const int rc = ::bind(fd, (const struct sockaddr*)&addr, sizeof(addr));
            const int bind_errno = LastSocketError();
#ifndef WIN32
            ::umask(old_umask);
#endif
            if (rc == 0) {
#ifndef WIN32
                // Belt-and-suspenders after the umask above. Fail closed: if the
                // socket cannot be confirmed 0600, refuse to serve rather than
                // leave the wallet IPC socket potentially accessible.
                if (::chmod(path.string().c_str(), 0600) != 0) {
                    const int chmod_errno = errno;
                    CloseSocket(fd);
                    throw std::system_error(chmod_errno, std::system_category());
                }
#endif
                return fd;
            }
            CloseSocket(fd);
            const std::error_code bind_ec = SocketErrorCode(bind_errno);
            if (bind_ec != std::errc::address_in_use || attempt == 1) {
                throw std::system_error(bind_ec);
            }
            int probe_errno = 0;
            int probe_fd = TryConnect(addr, probe_errno);
            if (probe_fd != -1) {
                CloseSocket(probe_fd);
                throw std::runtime_error(strprintf(
                    "Another process is already listening on the IPC socket '%s'; refusing to start.",
                    path.string()));
            }
            // Stale socket path from a crashed node: the liveness probe above just
            // confirmed nothing is listening, so remove it and retry the bind. On
            // POSIX we additionally confirm it is a socket file before unlinking;
            // on Windows an AF_UNIX socket is a reparse point that fs::symlink_status
            // does not report as socket_file and bind() cannot overwrite an existing
            // path, so remove any leftover path (best-effort).
#ifndef WIN32
            if (fs::symlink_status(path).type() == fs::socket_file) {
                fs::remove(path); // stale: safe to remove, then retry the bind
            }
#else
            try {
                fs::remove(path);
            } catch (const fs::filesystem_error&) {
                // already gone -- fine, retry the bind
            }
#endif
        }
        throw std::runtime_error("Failed to bind the IPC socket"); // unreachable
    }
};
} // namespace

std::unique_ptr<Process> MakeProcess() { return std::make_unique<ProcessImpl>(); }
} // namespace ipc
