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
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <system_error>
#include <unistd.h>

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

//! Connect a fresh AF_UNIX SOCK_STREAM socket to \p addr. Returns the connected
//! fd, or -1 with \p out_errno set on failure (caller decides how to react).
int TryConnect(const struct sockaddr_un& addr, int& out_errno)
{
    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        out_errno = errno;
        return -1;
    }
    if (::connect(fd, (const struct sockaddr*)&addr, sizeof(addr)) == 0) {
        out_errno = 0;
        return fd;
    }
    out_errno = errno;
    ::close(fd);
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
            throw std::system_error(connect_errno, std::system_category());
        }
        return fd;
    }

    int bind(const fs::path& data_dir, std::string& address) override
    {
        struct sockaddr_un addr;
        const fs::path path = ParseAddress(address, data_dir, addr);

        // Socket directory 0700 (design section 4.3).
        if (path.has_parent_path()) {
            fs::create_directories(path.parent_path());
            ::chmod(path.parent_path().string().c_str(), 0700);
        }

        // Anti-hijack (design section 4.3): if the socket path already exists,
        // connect-probe it. A live listener means the node is already running --
        // refuse to start rather than displace it. A refused/absent connection
        // means a stale path left by a crash -- unlink and rebind. Never unlink a
        // live socket.
        if (fs::symlink_status(path).type() == fs::socket_file) {
            int probe_errno = 0;
            int probe_fd = TryConnect(addr, probe_errno);
            if (probe_fd != -1) {
                ::close(probe_fd);
                throw std::runtime_error(strprintf(
                    "Another process is already listening on the IPC socket '%s'; refusing to start.",
                    path.string()));
            }
            // Stale socket (connection refused / gone): safe to remove.
            fs::remove(path);
        }

        int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd == -1) {
            throw std::system_error(errno, std::system_category());
        }
        if (::bind(fd, (const struct sockaddr*)&addr, sizeof(addr)) != 0) {
            int bind_error = errno;
            ::close(fd);
            throw std::system_error(bind_error, std::system_category());
        }
        // Socket 0600 (design section 4.3).
        ::chmod(path.string().c_str(), 0600);
        return fd;
    }
};
} // namespace

std::unique_ptr<Process> MakeProcess() { return std::make_unique<ProcessImpl>(); }
} // namespace ipc
