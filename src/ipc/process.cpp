// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "ipc/process.h"
#include "ipc/peercred.h"
#include "util/dir_permissions.h"
#include "util/sock.h"

#include "fs.h"
#include "tinyformat.h"
#include "util/system.h"
#include "util.h"
#include "util/syserror.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>
#ifndef WIN32
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <unistd.h>
#else
#include "interfaces/ipc.h" // PeerRefusedError
#include "ipc/win_endpoint.h"

// winsock2.h must precede afunix.h/ws2tcpip.h. AF_UNIX for Windows lives in
// <afunix.h> (Windows 10 1803+); sockaddr_un has the same sun_family/sun_path
// layout ParseAddress relies on.
#include <climits>
#include <mutex>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <afunix.h>
#endif

namespace ipc {
namespace {

//! Default socket filename inside the IPC directory (-ipcdir, default the
//! network-specific data directory): <ipcdir>/node.sock (doc/multiprocess_design.md section 4.3).
const char* const DEFAULT_SOCKET_FILE = "node.sock";

//! Resolve \p address ("unix" -> <ipc_dir>/node.sock, "unix:<path>" -> a custom
//! path) into a sockaddr_un. Rewrites \p address to its canonical "unix:<path>"
//! form. Throws std::invalid_argument on an unrecognized address or an
//! over-length path.
fs::path ParseAddress(std::string& address, const fs::path& ipc_dir, struct sockaddr_un& addr)
{
    fs::path path;
    if (address == "unix" || address == "auto") {
        path = ipc_dir / DEFAULT_SOCKET_FILE;
    } else if (address.rfind("unix:", 0) == 0 && address.size() > 5) {
        path = fs::path(address.substr(5));
        if (!path.is_absolute()) path = ipc_dir / path;
    } else {
        throw std::invalid_argument(strprintf("Unrecognized IPC address '%s'", address));
    }

    // OPEN QUESTION (Windows, non-ASCII paths): sun_path is filled from
    // path.string(), which on Windows is UTF-8 (SetupEnvironment() makes it so),
    // while the cookie writer (handshake.cpp) uses path.wstring(). Microsoft's
    // AF_UNIX documentation does not state which narrow encoding the kernel expects
    // in sun_path (UTF-8 vs. the ANSI code page), and we have no on-device evidence
    // either way, so the behaviour is deliberately left alone rather than guessed
    // at: changing it blindly could break the ASCII path that is known to work.
    // A datadir containing non-ASCII characters may therefore fail to bind/connect
    // on Windows even once the cookie is readable; that needs an on-device
    // experiment before any encoding change here.
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
// Close an IPC socket descriptor with explicit per-platform dispatch: ::close()
// on POSIX; on Windows compat.h's closesocket() macro (myclosesocket -> the
// winsock closesocket(), which takes a SOCKET& lvalue, so hand it one).
void CloseSocket(int fd)
{
#ifdef WIN32
    SOCKET s = static_cast<SOCKET>(fd);
    closesocket(s);
#else
    ::close(fd);
#endif
}
#ifdef WIN32
int LastSocketError() { return ::WSAGetLastError(); }

//! Initialize winsock once before the first IPC socket call. The daemon already
//! calls WSAStartup for its P2P stack (net.cpp), but the -multiprocess GUI
//! process connects to the node without necessarily starting that stack, so the
//! IPC layer initializes winsock itself. WSAStartup is reference-counted, so this
//! composes safely with net.cpp's call; a process-lifetime init needs no cleanup.
void EnsureWinsock()
{
    static std::once_flag once;
    std::call_once(once, [] {
        WSADATA wsadata;
        const int ret = ::WSAStartup(MAKEWORD(2, 2), &wsadata);
        if (ret != 0) {
            throw std::system_error(ret, std::system_category(), "WSAStartup");
        }
    });
}

//! Translate a winsock error into a std::error_code whose comparison against
//! std::errc still works: interfaces.cpp classifies a connect failure as
//! connection_refused / no_such_file_or_directory / not_a_directory to detect a
//! not-running daemon, and libstdc++'s system_category() does not map the WSA
//! range. Map the connect-relevant codes to their POSIX errc; keep the raw code
//! under system_category() otherwise so the message is still meaningful.
//! On-device (W4): a missing socket path returns WSAECONNREFUSED (-> connection_refused,
//! the clean no-daemon fallback, confirmed live). The unlistened case (a stale socket
//! file with no listener) is still mapped generically -- not yet separately exercised,
//! which is exactly why IsPossiblyStaleSocketBindError below does not gate stale-socket
//! recovery on a single error code.
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
void EnsureWinsock() {} // no-op on POSIX
#endif

//! Could this bind() failure be a leftover socket path from a crashed node, i.e.
//! is it worth probing the path for a live listener before giving up?
//!
//! POSIX answers this unambiguously: bind() over an existing path is EADDRINUSE.
//! Windows does not: AF_UNIX bind() there goes through the filesystem to create a
//! reparse point over an existing name, and the failure surfaces as whichever of
//! WSAEADDRINUSE / WSAEACCES / ERROR_FILE_EXISTS / ERROR_ALREADY_EXISTS /
//! ERROR_ACCESS_DENIED the redirector hands back (see the SocketErrorCode note --
//! this case is not separately exercised on-device). Gating recovery on
//! address_in_use alone therefore risks a node that, after a single taskkill /F,
//! never listens again on *any* subsequent start -- a permanent, one-log-line
//! failure. Accepting the "exists"/"denied" family costs nothing: the caller
//! still refuses to touch the path when the liveness probe finds a listener.
bool IsPossiblyStaleSocketBindError(const std::error_code& ec)
{
#ifndef WIN32
    return ec == std::errc::address_in_use;
#else
    if (ec == std::errc::address_in_use || ec == std::errc::file_exists ||
        ec == std::errc::permission_denied) {
        return true;
    }
    // Raw Win32/winsock codes SocketErrorCode leaves under system_category().
    if (ec.category() == std::system_category()) {
        switch (ec.value()) {
        case ERROR_FILE_EXISTS:
        case ERROR_ALREADY_EXISTS:
        case ERROR_ACCESS_DENIED:
        case ERROR_SHARING_VIOLATION:
        case WSAEINVAL:
            return true;
        }
    }
    return false;
#endif
}

//! Create an AF_UNIX SOCK_STREAM socket that is not inherited by child processes,
//! so the IPC fd cannot leak into an exec'd child (see CreateCloexecSocket).
//! Returns -1 with the platform socket error set on failure.
int MakeCloexecStreamSocket()
{
    // Windows: WSA_FLAG_OVERLAPPED is required because libmultiprocess drives these
    // sockets with kj's IOCP async backend (overlapped AcceptEx / WSARecv).
    SOCKET s = CreateCloexecSocket(AF_UNIX, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) return -1;
#ifdef WIN32
    // The descriptor is carried as int (libmultiprocess SocketId) across the
    // transport. Windows AF_UNIX handles fit in the low 32 bits in practice, but
    // guard the assumption: a value that would not round-trip through int is
    // unusable, so fail loudly here rather than silently truncate to a bogus
    // (possibly negative, non-INVALID) fd downstream.
    if (s > static_cast<SOCKET>(INT_MAX)) {
        closesocket(s);
        ::WSASetLastError(WSAEMFILE);
        return -1;
    }
#endif
    return static_cast<int>(s);
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

#ifdef WIN32
//! The GUI's check of the endpoint at \p socket_path before it connects
//! (ipc::win::CheckServerEndpointBeforeConnect against ExpectedServerSid()),
//! returning what must stay open until the connect is made. A missing endpoint is
//! no server listening, and is passed on as such, so the GUI keeps waiting.
//! Anything else that stops the check is a refusal, thrown as
//! interfaces::PeerRefusedError: nothing has been connected or sent.
win::EndpointHold CheckEndpointBeforeConnect(const fs::path& socket_path)
{
    try {
        return win::CheckServerEndpointBeforeConnect(socket_path, win::ExpectedServerSid());
    } catch (const std::system_error& e) {
        if (e.code() == std::errc::no_such_file_or_directory) throw;
        throw interfaces::PeerRefusedError(
            strprintf("Refusing to connect: the IPC endpoint could not be checked (%s)", e.what()));
    } catch (const std::exception& e) {
        throw interfaces::PeerRefusedError(strprintf("Refusing to connect to the IPC endpoint: %s", e.what()));
    }
}
#endif

} // namespace

fs::path GetIpcDir()
{
    if (gArgs.IsArgSet("-ipcdir") && !gArgs.IsArgNegated("-ipcdir")) {
        return AbsPathForConfigVal(fs::path(gArgs.GetArg("-ipcdir", "")), /*net_specific=*/true);
    }
    return GetDataDir();
}

std::optional<std::string> AttachGroupIpcDirRefusal()
{
    if (ConfiguredOption("-ipcattachgroup").empty()) return std::nullopt;

    if (ConfiguredOption("-ipcdir").empty()) {
        return std::string{"-ipcattachgroup is set without -ipcdir. The attach group is given access to the IPC "
                           "directory, which without -ipcdir is the data directory. Set -ipcdir to a directory of "
                           "its own for the IPC socket and cookie."};
    }

    // Compared with the links resolved. A directory that does not exist yet, or
    // cannot be resolved, is not the data directory, which does exist. A spelling
    // this misses (on Windows, one that differs only in case) is refused where the
    // directory is opened, by its identity (DataDirectoryRefusal).
    const auto resolved = [](const fs::path& path) {
        boost::system::error_code ec;
        const fs::path canonical = fs::canonical(path, ec);
        return ec ? fs::path() : canonical;
    };

    const fs::path ipcdir = resolved(GetIpcDir());
    if (ipcdir.empty()) return std::nullopt;

    // The network's data directory, and on a test network also the base one
    // above it, which holds the configuration file.
    for (const bool net_specific : {true, false}) {
        if (ipcdir == resolved(GetDataDir(net_specific))) {
            return strprintf("-ipcattachgroup is set and -ipcdir names the data directory (%s). The attach group is "
                             "given access to the IPC directory. Set -ipcdir to a directory of its own for the IPC "
                             "socket and cookie.",
                             GetIpcDir().string());
        }
    }

    return std::nullopt;
}

std::optional<std::string> DataDirectoryRefusal(const fs::path& ipcdir, const FileId& ipcdir_id,
                                                const std::vector<std::pair<fs::path, FileId>>& datadirs)
{
    for (const auto& [datadir, id] : datadirs) {
        if (id == ipcdir_id) {
            return strprintf("-ipcattachgroup is set and -ipcdir (%s) is the data directory (%s). The attach group is "
                             "given access to the IPC directory. Set -ipcdir to a directory of its own for the IPC "
                             "socket and cookie.",
                             ipcdir.string(), datadir.string());
        }
    }

    return std::nullopt;
}

#ifndef WIN32
void ApplyPathAccessNoFollow(const fs::path& path, PathKind kind, mode_t mode, std::optional<gid_t> group,
                             const std::function<void(const struct stat&)>& check)
{
    const std::string p = path.string();
    const auto fail = [&](int err, const char* what) {
        throw std::system_error(err, std::system_category(), strprintf("%s %s", what, p));
    };
    const auto type_ok = [kind](mode_t m) {
        switch (kind) {
        case PathKind::Socket: return S_ISSOCK(m);
        case PathKind::Directory: return S_ISDIR(m);
        case PathKind::RegularFile: return S_ISREG(m);
        }
        return false;
    };

#ifdef __linux__
    const int fd = ::open(p.c_str(), O_PATH | O_NOFOLLOW | O_CLOEXEC);
    if (fd == -1) fail(errno, "cannot open");

    struct FdCloser {
        int fd{-1};
        ~FdCloser() { ::close(fd); }
    } closer{fd};

    struct stat st;
    if (::fstat(fd, &st) != 0) fail(errno, "cannot stat");
    // A symlink opens as the link itself under O_PATH|O_NOFOLLOW, so it fails
    // the type check here.
    if (!type_ok(st.st_mode)) fail(EINVAL, "unexpected file type at");
    if (st.st_uid != ::geteuid()) fail(EPERM, "not owned by this user:");
    if (check) check(st);

    const auto set_mode = [&](mode_t m) {
        bool mode_set = false;
#ifdef SYS_fchmodat2
        mode_set = ::syscall(SYS_fchmodat2, fd, "", m, AT_EMPTY_PATH) == 0;
#endif
        if (!mode_set) {
            const std::string proc_path = strprintf("/proc/self/fd/%d", fd);
            if (::chmod(proc_path.c_str(), m) != 0) fail(errno, "cannot set the mode of");
        }
    };

    if (group) {
        // Narrow first, so the new group never holds a bit the old mode had and
        // the final mode does not.
        set_mode(static_cast<mode_t>(st.st_mode & mode & 07777));
        if (::fchownat(fd, "", static_cast<uid_t>(-1), *group, AT_EMPTY_PATH) != 0) {
            fail(errno, "cannot set the group of");
        }
    }
    set_mode(mode);

    // Verify through the same fd.
    struct stat after;
    if (::fstat(fd, &after) != 0) fail(errno, "cannot stat");
    if (!type_ok(after.st_mode) || after.st_uid != ::geteuid() || (after.st_mode & 07777) != mode ||
        (group && after.st_gid != *group)) {
        fail(EPERM, "access was not applied to");
    }
#else
    struct stat st;
    if (::lstat(p.c_str(), &st) != 0) fail(errno, "cannot stat");
    if (!type_ok(st.st_mode)) fail(EINVAL, "unexpected file type at");
    if (st.st_uid != ::geteuid()) fail(EPERM, "not owned by this user:");
    if (check) check(st);
    if (group) {
        // Narrow first, so the new group never holds a bit the old mode had and
        // the final mode does not.
        if (::fchmodat(AT_FDCWD, p.c_str(), static_cast<mode_t>(st.st_mode & mode & 07777), AT_SYMLINK_NOFOLLOW) != 0) {
            fail(errno, "cannot set the mode of");
        }
        if (::fchownat(AT_FDCWD, p.c_str(), static_cast<uid_t>(-1), *group, AT_SYMLINK_NOFOLLOW) != 0) {
            fail(errno, "cannot set the group of");
        }
    }
    if (::fchmodat(AT_FDCWD, p.c_str(), mode, AT_SYMLINK_NOFOLLOW) != 0) fail(errno, "cannot set the mode of");
    struct stat after;
    if (::lstat(p.c_str(), &after) != 0) fail(errno, "cannot stat");
    if (!type_ok(after.st_mode) || after.st_uid != ::geteuid() || (after.st_mode & 07777) != mode ||
        (group && after.st_gid != *group)) {
        fail(EPERM, "access was not applied to");
    }
#endif
}

std::optional<std::string> IpcDirWarning(const std::string& dir, mode_t mode, uid_t owner, gid_t group,
                                         uid_t self_uid, std::optional<gid_t> attach_gid)
{
    if ((mode & (S_IRWXG | S_IRWXO)) == 0) return std::nullopt;
    if (attach_gid && owner == self_uid && group == *attach_gid && (mode & 07777) == static_cast<mode_t>(0750)) {
        return std::nullopt;
    }
    return strprintf("the IPC directory %s is accessible to other users (mode %03o). Leaving it as configured; "
                     "the IPC socket and cookie inside it are %s.",
                     dir, static_cast<unsigned>(mode & 07777),
                     attach_gid ? "accessible to the attach group" : "owner-only");
}

namespace {
//! Refuses, by throwing std::runtime_error, \p dir opened as \p st when it is
//! the network data directory or the base one above it: the same device and
//! inode, which a bind mount or a spelling with ".." keeps.
void RefuseDataDirectory(const fs::path& dir, const struct stat& st)
{
    std::vector<std::pair<fs::path, FileId>> datadirs;
    for (const bool net_specific : {true, false}) {
        const fs::path datadir = GetDataDir(net_specific);
        struct stat data_st{};

        if (::stat(datadir.string().c_str(), &data_st) != 0) continue;

        datadirs.emplace_back(datadir,
                              FileId{static_cast<uint64_t>(data_st.st_dev), static_cast<uint64_t>(data_st.st_ino)});
    }

    const FileId id{static_cast<uint64_t>(st.st_dev), static_cast<uint64_t>(st.st_ino)};
    if (std::optional<std::string> refusal = DataDirectoryRefusal(dir, id, datadirs)) {
        throw std::runtime_error(*refusal);
    }
}
} // namespace

void ApplyAttachGroupAccess(const fs::path& ipcdir)
{
    const std::optional<gid_t> gid = AttachGroupGid();
    if (!gid) throw std::logic_error("no -ipcattachgroup is configured");
    try {
        ApplyPathAccessNoFollow(ipcdir, PathKind::Directory, 0750, gid,
                                [&ipcdir](const struct stat& st) { RefuseDataDirectory(ipcdir, st); });
    } catch (const std::system_error& e) {
        if (e.code() != std::error_code(EPERM, std::system_category())) throw;
        // EPERM can also come from the ownership check, so say what to change
        // only when the account is really outside the group. A core with
        // CAP_CHOWN that is not a member never gets here, because the change
        // succeeds.
        bool member = ::getegid() == *gid;
        const int n = ::getgroups(0, nullptr);
        if (!member && n > 0) {
            std::vector<gid_t> groups(static_cast<size_t>(n));
            const int got = ::getgroups(n, groups.data());
            if (got >= 0) member = std::find(groups.begin(), groups.begin() + got, *gid) != groups.begin() + got;
        }
        if (member) throw;
        throw std::system_error(e.code(), strprintf("cannot set the group of %s (the core must run with the "
                                                    "attach group as a supplementary group)", ipcdir.string()));
    }
}
#endif // !WIN32

namespace {
//! Make sure \p dir exists. Returns whether this call created it, which the
//! caller then restricts. Throws std::system_error, naming the directory, when it
//! cannot be created.
bool MakeIpcDirectory(const fs::path& dir)
{
    boost::system::error_code ec;
    const bool created = fs::create_directories(dir, ec);
    if (ec) {
        throw std::system_error(ec.value(), std::system_category(),
                                "could not create the IPC directory " + dir.string());
    }

    return created;
}
} // namespace

#ifndef WIN32
void PrepareAttachGroupDirectory(const fs::path& dir)
{
    // "Created" also when only a part of a spelling such as "<datadir>/x/.." was
    // made, so the restriction refuses the data directory as the group's access
    // does.
    const bool created = MakeIpcDirectory(dir);
    if (created) {
        ApplyPathAccessNoFollow(dir, PathKind::Directory, 0700, std::nullopt,
                                [&dir](const struct stat& st) { RefuseDataDirectory(dir, st); });
    }

    // A directory made just now was this account's alone; only a change to one
    // that was there already is logged.
    struct stat before{};
    const bool existed = !created && ::stat(dir.string().c_str(), &before) == 0;

    ApplyAttachGroupAccess(dir);

    const std::optional<gid_t> gid = AttachGroupGid();
    if (existed && gid && ((before.st_mode & 07777) != static_cast<mode_t>(0750) || before.st_gid != *gid)) {
        LogPrintf("IPC: set the IPC directory %s to mode 750 in the attach group (gid %u); it was mode %03o, gid %u",
                  dir.string(), static_cast<unsigned>(*gid), static_cast<unsigned>(before.st_mode & 07777),
                  static_cast<unsigned>(before.st_gid));
    }
}
#else
void PrepareAttachGroupDirectory(const fs::path& dir)
{
    const std::optional<std::string> group_sid = win::AttachGroupSid();
    if (!group_sid) throw std::logic_error("no -ipcattachgroup is configured");
    const std::optional<util::winacl::Sid> group = util::winacl::ParseSidString(*group_sid);
    if (!group) throw std::logic_error(strprintf("the attach group's SID '%s' does not parse", *group_sid));

    // "Created" also when only a part of a spelling such as "<datadir>\x\.." was
    // made; the data directory is refused by its identity before anything is
    // changed either way.
    const bool created = MakeIpcDirectory(dir);
    win::ApplyAttachGroupDirectoryAccess(dir, *group, created);
}
#endif // !WIN32

void PrepareIpcDirectory(const fs::path& dir)
{
    // Socket directory access control (design section 4.3).
    //
    // The directory is hardened ONLY when this call creates it. If it already
    // exists its permissions are the operator's decision: an admin may have
    // deliberately widened them (a shared group, a second account, a backup
    // agent), and silently re-tightening on every start would undo that with
    // no warning and no way to make it stick. So a permissive existing
    // directory is warned about but never changed.
    //
    // The socket and the auth cookie are ours and are recreated on every start
    // with a fixed access that fails closed (see bind and handshake.cpp):
    // owner-only by default, and with -ipcattachgroup the socket is 0660 and the
    // cookie 0640 in the attach group (on Windows, the group's entries in
    // ipc/win_endpoint.h). A widened directory therefore does not change who can
    // read the cookie. In attach-group mode the daemon sets the directory itself
    // to 0750 in that group (ApplyAttachGroupAccess), which is the state this call
    // does not warn about.
    //
    // Windows is the exception for an -ipcdir the operator named: there an
    // existing directory where another account could add, remove or replace the
    // socket or cookie is refused rather than warned about, because on Windows the
    // directory's list, not its traverse right, is what keeps them in place. The
    // default, the data directory, keeps the warning: data directories made by
    // earlier releases inherit %APPDATA%'s list.
    //
    // When the directory is created here, restricting it fails closed: if we
    // cannot restrict a directory we just made, throw rather than expose the
    // socket in a world-accessible one. On POSIX that is an fd-based mode 0700;
    // Windows has no chmod/umask for AF_UNIX, so the equivalent is an explicit
    // owner-only protected DACL (ApplyOwnerOnlyDacl).
    //
    // The default -ipcdir is the data directory, which GetDataDir() has
    // normally created already (owner-only, through
    // util::CreateOwnerOnlyDirectory), so for it the create branch does not run.
    // It runs for an explicit -ipcdir, or a custom socket path, whose directory
    // does not exist yet.
    if (MakeIpcDirectory(dir)) {
#ifndef WIN32
        ApplyPathAccessNoFollow(dir, PathKind::Directory, 0700, std::nullopt);
#else
        util::ApplyOwnerOnlyDacl(dir, /*inheritable=*/true);
#endif
    } else {
#ifndef WIN32
        struct stat st;
        if (::stat(dir.string().c_str(), &st) == 0) {
            const auto warning = IpcDirWarning(dir.string(), st.st_mode, st.st_uid, st.st_gid, ::geteuid(),
                                               AttachGroupGid());
            if (warning) LogPrintf("WARN: %s: %s", __func__, *warning);
        }
#else
        if (!ConfiguredOption("-ipcdir").empty()) {
            if (const std::optional<std::string> reason = win::ServerIpcDirRefusal(dir)) {
                throw std::runtime_error(strprintf("Refusing to serve IPC: %s", *reason));
            }
            return;
        }

        try {
            util::VerifyOwnerOnlyDacl(dir);
        } catch (const std::exception&) {
            LogPrintf("WARN: %s: the IPC directory %s is not restricted to this account. "
                      "Leaving it as configured; the IPC socket and cookie inside it remain owner-only.",
                      __func__, dir.string());
        }
#endif
    }
}

namespace {

class ProcessImpl : public Process
{
public:
    int connect(const fs::path& ipc_dir, std::string& address) override
    {
        EnsureWinsock();
        struct sockaddr_un addr;
        [[maybe_unused]] const fs::path path = ParseAddress(address, ipc_dir, addr);
#ifdef WIN32
        // Held until this returns, after the connect below.
        const win::EndpointHold hold = CheckEndpointBeforeConnect(path);
#endif
        int connect_errno = 0;
        int fd = TryConnect(addr, connect_errno);
        if (fd == -1) {
            throw std::system_error(SocketErrorCode(connect_errno));
        }
        return fd;
    }

    int bind(const fs::path& ipc_dir, std::string& address) override
    {
        EnsureWinsock();
        struct sockaddr_un addr;
        const fs::path path = ParseAddress(address, ipc_dir, addr);

        // The socket's directory is prepared (created owner-only, or checked and
        // left as configured) before the bind. See PrepareIpcDirectory.
        if (path.has_parent_path()) PrepareIpcDirectory(path.parent_path());

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
            // The daemon sets an owner-only process umask at startup, so bind()
            // creates the socket node owner-only. This function does not touch
            // the process umask: it is process-wide state that other threads
            // share. The mode is set explicitly below, through an fd of the path,
            // before listen() so no peer can connect before it is final.
            // (No umask on Windows; there the fail-closed ApplyEndpointAcl
            // after the bind below gives the socket its access list.)
#endif
            const int rc = ::bind(fd, (const struct sockaddr*)&addr, sizeof(addr));
            const int bind_errno = LastSocketError();
            if (rc == 0) {
#ifndef WIN32
                // Fail closed: if the socket cannot be confirmed at its final
                // access without following a symlink, refuse to serve rather than
                // leave the wallet IPC socket potentially accessible. That is
                // 0600, or 0660 in the attach group when one is configured.
                try {
                    const std::optional<gid_t> attach_gid = AttachGroupGid();
                    ApplyPathAccessNoFollow(path, PathKind::Socket, attach_gid ? 0660 : 0600, attach_gid);
                } catch (...) {
                    CloseSocket(fd);
                    throw;
                }
#else
                // Same fail-closed contract as the mode set above: give the
                // socket its access list (this account and SYSTEM, and with an
                // attach group that group's right to connect; non-inheritable,
                // protected, verified by read-back), and refuse to serve rather
                // than listen unprotected if that fails.
                try {
                    win::ApplyEndpointAcl(path, win::IpcObject::Socket);
                } catch (...) {
                    CloseSocket(fd);
                    throw;
                }
#endif
                return fd;
            }
            CloseSocket(fd);
            const std::error_code bind_ec = SocketErrorCode(bind_errno);
            if (!IsPossiblyStaleSocketBindError(bind_ec) || attempt == 1) {
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
            // POSIX we additionally confirm it is a socket file before unlinking.
#ifndef WIN32
            if (fs::symlink_status(path).type() == fs::socket_file) {
                fs::remove(path); // stale: safe to remove, then retry the bind
            }
#else
            // Windows equivalent of the POSIX socket_file guard. An AF_UNIX socket
            // there is a reparse point, which boost::filesystem reports as
            // reparse_file (or, defensively, regular_file) -- never socket_file --
            // so the type cannot be compared verbatim. Refuse to touch a directory
            // (that is a misconfigured -datadir/address, not a stale socket, and
            // removing it could destroy data), and say so; anything else at this
            // path was put there by a previous run of this daemon.
            const fs::file_type type = fs::symlink_status(path).type();
            if (type == fs::directory_file) {
                throw std::runtime_error(strprintf(
                    "The IPC socket path '%s' is a directory; move it aside or point the IPC "
                    "address at a different path.", path.string()));
            }
            if (type == fs::file_not_found) {
                // Nothing to clean up, so the bind failure was not a stale socket
                // after all -- surface the original error rather than spinning.
                throw std::system_error(bind_ec);
            }
            // boost::filesystem::remove() reports "already gone" by RETURNING FALSE,
            // and throws only on a genuine failure (access denied, sharing violation
            // because a handle is still open). The previous catch-and-ignore of
            // filesystem_error therefore swallowed exactly the failures that matter
            // and handled a case that never reached it. Use the non-throwing overload
            // and surface a real failure: a silent no-op here means every subsequent
            // start comes up with no IPC listener, forever.
            boost::system::error_code remove_ec;
            fs::remove(path, remove_ec);
            if (remove_ec) {
                throw std::runtime_error(strprintf(
                    "Failed to remove the stale IPC socket '%s': %s. Nothing is listening on it; "
                    "delete the file manually and restart.", path.string(), remove_ec.message()));
            }
#endif
        }
        throw std::runtime_error("Failed to bind the IPC socket"); // unreachable
    }
};
} // namespace

std::unique_ptr<Process> MakeProcess() { return std::make_unique<ProcessImpl>(); }

} // namespace ipc
