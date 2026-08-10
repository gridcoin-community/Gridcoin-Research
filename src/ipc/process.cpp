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
#include <climits>
#include <mutex>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <afunix.h>
#include <aclapi.h> // SetEntriesInAclW / Set|GetNamedSecurityInfoW (advapi32)
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

    // OPEN QUESTION (Windows, non-ASCII paths): sun_path is filled from
    // path.string(), which on Windows is the *system code page* narrow form, while
    // the cookie writer (handshake.cpp) uses path.wstring(). Microsoft's AF_UNIX
    // documentation does not state which narrow encoding the kernel expects in
    // sun_path (UTF-8 vs. the ANSI code page), and we have no on-device evidence
    // either way, so the behaviour is deliberately left alone rather than guessed
    // at: changing it blindly could break the ASCII path that is known to work.
    // A datadir containing characters outside the system code page may therefore
    // fail to bind/connect on Windows even once the cookie is readable; that needs
    // an on-device experiment before any encoding change here.
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

//! The two trustees an IPC object's DACL may name: this process's token user
//! ("me") and SYSTEM. SYSTEM is included because excluding it breaks backup,
//! indexing and service scenarios while adding nothing -- SYSTEM can take
//! ownership of any object regardless of its DACL, so denying it is theatre.
//! Administrators are deliberately *not* included: an administrator can likewise
//! take ownership at will, but leaving the group off keeps the ACL an accurate
//! statement of intent (owner-only) rather than an invitation.
struct OwnerOnlyTrustees
{
    std::vector<unsigned char> token_user; //!< TOKEN_USER header + the SID it points into
    unsigned char system_sid[SECURITY_MAX_SID_SIZE];

    PSID user() const
    {
        return reinterpret_cast<const TOKEN_USER*>(token_user.data())->User.Sid;
    }
    PSID system() const { return const_cast<unsigned char*>(system_sid); }
};

//! Resolve the process token user SID and the SYSTEM SID. Throws on failure --
//! without both SIDs no DACL can be built, and silently skipping the DACL is the
//! very hole this exists to close.
OwnerOnlyTrustees GetOwnerOnlyTrustees()
{
    OwnerOnlyTrustees t{};

    HANDLE token = nullptr;
    if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token)) {
        throw std::system_error(static_cast<int>(::GetLastError()), std::system_category(),
                                "OpenProcessToken");
    }
    // First call sizes the buffer (it fails with ERROR_INSUFFICIENT_BUFFER by design).
    DWORD len = 0;
    ::GetTokenInformation(token, TokenUser, nullptr, 0, &len);
    t.token_user.resize(len ? len : 1);
    if (!::GetTokenInformation(token, TokenUser, t.token_user.data(), len, &len)) {
        const DWORD err = ::GetLastError();
        ::CloseHandle(token);
        throw std::system_error(static_cast<int>(err), std::system_category(),
                                "GetTokenInformation(TokenUser)");
    }
    ::CloseHandle(token);

    DWORD system_sid_size = sizeof(t.system_sid);
    if (!::CreateWellKnownSid(WinLocalSystemSid, nullptr, t.system_sid, &system_sid_size)) {
        throw std::system_error(static_cast<int>(::GetLastError()), std::system_category(),
                                "CreateWellKnownSid(WinLocalSystemSid)");
    }
    return t;
}

//! Build the two-ACE allow-only DACL for \p trustees. The caller owns the
//! returned ACL and must LocalFree() it.
PACL BuildOwnerOnlyDacl(const OwnerOnlyTrustees& trustees, bool inheritable)
{
    EXPLICIT_ACCESS_W ea[2] = {};
    const PSID sids[2] = {trustees.user(), trustees.system()};
    // On a directory the ACEs must be inheritable, so anything created inside
    // (node.sock, ipc.cookie) is owner-only from creation rather than only after
    // an after-the-fact SetNamedSecurityInfo -- that closes the create/protect
    // window rather than merely narrowing it.
    const DWORD inheritance = inheritable ? (CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE)
                                          : NO_INHERITANCE;
    for (int i = 0; i < 2; ++i) {
        ea[i].grfAccessPermissions = GENERIC_ALL;
        ea[i].grfAccessMode = SET_ACCESS;
        ea[i].grfInheritance = inheritance;
        ea[i].Trustee.pMultipleTrustee = nullptr;
        ea[i].Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
        ea[i].Trustee.TrusteeForm = TRUSTEE_IS_SID;
        ea[i].Trustee.TrusteeType = i == 0 ? TRUSTEE_IS_USER : TRUSTEE_IS_WELL_KNOWN_GROUP;
        ea[i].Trustee.ptstrName = reinterpret_cast<LPWSTR>(sids[i]);
    }

    PACL dacl = nullptr;
    const DWORD rc = ::SetEntriesInAclW(2, ea, nullptr, &dacl);
    if (rc != ERROR_SUCCESS || dacl == nullptr) {
        throw std::system_error(static_cast<int>(rc), std::system_category(), "SetEntriesInAcl");
    }
    return dacl;
}

//! Read \p path's DACL back and confirm it says what we just asked for: present,
//! non-NULL, protected (no inherited ACEs), and every ACE an allow-ACE naming
//! only one of \p trustees. This is the "verify" half of fail-closed: some
//! filesystems (FAT/exFAT volumes, some network redirectors) accept a
//! SetNamedSecurityInfo call and store nothing, which would otherwise leave the
//! cookie world-readable while every call reported success.
void VerifyOwnerOnlyDacl(const fs::path& path, const OwnerOnlyTrustees& trustees)
{
    std::wstring wpath = path.wstring();
    PACL dacl = nullptr;
    PSECURITY_DESCRIPTOR sd = nullptr;
    // DACL_SECURITY_INFORMATION only: PROTECTED_DACL_SECURITY_INFORMATION is a
    // set-side flag and is rejected here. Whether the DACL is protected is read
    // below from the descriptor's SE_DACL_PROTECTED control bit instead.
    const DWORD rc = ::GetNamedSecurityInfoW(wpath.data(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                                             nullptr, nullptr, &dacl, nullptr, &sd);
    if (rc != ERROR_SUCCESS) {
        throw std::system_error(static_cast<int>(rc), std::system_category(),
                                "GetNamedSecurityInfo " + path.string());
    }

    std::string failure;
    SECURITY_DESCRIPTOR_CONTROL control{};
    DWORD revision = 0;
    if (!::GetSecurityDescriptorControl(sd, &control, &revision)) {
        failure = "the security descriptor could not be read back";
    } else if ((control & SE_DACL_PRESENT) == 0 || dacl == nullptr) {
        // A NULL/absent DACL grants everyone full access -- the worst outcome.
        failure = "it has no DACL (all users would have access)";
    } else if ((control & SE_DACL_PROTECTED) == 0) {
        failure = "its DACL is not protected (it still inherits ACEs from the parent)";
    } else {
        for (WORD i = 0; i < dacl->AceCount; ++i) {
            void* ace = nullptr;
            if (!::GetAce(dacl, i, &ace)) {
                failure = "one of its ACEs could not be read back";
                break;
            }
            const ACE_HEADER* header = static_cast<const ACE_HEADER*>(ace);
            if (header->AceType != ACCESS_ALLOWED_ACE_TYPE) continue; // a deny ACE only narrows
            PSID sid = &static_cast<ACCESS_ALLOWED_ACE*>(ace)->SidStart;
            if (!::EqualSid(sid, trustees.user()) && !::EqualSid(sid, trustees.system())) {
                failure = "its DACL grants access to a trustee other than the current user and SYSTEM";
                break;
            }
        }
    }
    if (sd != nullptr) ::LocalFree(sd);
    if (!failure.empty()) {
        throw std::runtime_error(strprintf(
            "Refusing to serve IPC: '%s' could not be restricted to the current user -- %s. "
            "The data directory must live on an NTFS volume that supports access control.",
            path.string(), failure));
    }
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
//! so the IPC fd cannot leak into an exec'd child. POSIX uses SOCK_CLOEXEC
//! atomically where available (Linux, the BSDs) and falls back to
//! fcntl(FD_CLOEXEC) (notably macOS); Windows uses WSA_FLAG_NO_HANDLE_INHERIT.
//! Returns -1 with the platform socket error set on failure.
int MakeCloexecStreamSocket()
{
#ifdef WIN32
    // WSA_FLAG_OVERLAPPED is required: libmultiprocess drives these sockets with
    // kj's IOCP async backend (overlapped AcceptEx / WSARecv). Without it kj cannot
    // post an async accept, so the daemon never accepts and clients get
    // WSAECONNREFUSED even though node.sock exists and listen() succeeded.
    // WSA_FLAG_NO_HANDLE_INHERIT is the close-on-exec equivalent.
    SOCKET s = ::WSASocketW(AF_UNIX, SOCK_STREAM, 0, nullptr, 0,
                            WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT);
    if (s == INVALID_SOCKET) return -1;
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
    return static_cast<int>(s);
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
        EnsureWinsock();
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
        EnsureWinsock();
        struct sockaddr_un addr;
        const fs::path path = ParseAddress(address, data_dir, addr);

        // Socket directory access control (design section 4.3).
        //
        // READ THIS BEFORE RELYING ON THE create BRANCH BELOW: for the DEFAULT
        // address ("unix" -> <datadir>/node.sock) it does not run. GetDataDir()
        // itself calls fs::create_directories (ArgsManager::GetDataDirPath in
        // util/system.cpp), and AppInit2, WriteCookie(GetDataDir()) and this
        // function's own GetDataDir() argument have all called it long before we
        // get here -- so the directory always already exists and `created` is
        // false. The branch is live only for a custom `unix:<path>` whose parent
        // does not exist yet.
        //
        // The practical effect is that the datadir keeps whatever the OS gave it
        // (%APPDATA%'s inherited ACL on Windows, 0755 on macOS), and that is
        // deliberate as well as accidental: we do not re-tighten a directory the
        // operator may have widened on purpose. What actually protects the secret
        // is that node.sock and ipc.cookie are created BY US, owner-only, and fail
        // closed if that cannot be applied -- see below and in handshake.cpp.
        //
        // When the create branch does run, it fails closed: if we cannot restrict a
        // directory we just made, refuse to listen rather than expose the socket in
        // a world-accessible one.
        // Windows has no chmod/umask for AF_UNIX, so the equivalent is an explicit
        // owner-only *protected* DACL (ApplyOwnerOnlyDacl), applied inheritably so
        // node.sock and ipc.cookie are owner-only from creation. The port used to
        // rely instead on the per-user-profile datadir ACL being owner-only by
        // default: true for a default %APPDATA% install (icacls on-device showed
        // owner + SYSTEM + Administrators only), but nothing set or checked it, so
        // a -datadir on a shared volume, an inherited-permissive parent or a
        // non-ACL filesystem silently exposed the wallet IPC socket and the auth
        // cookie.
        //
        // Applied ON CREATION ONLY, on both platforms. See the comment below: an
        // existing datadir's permissions belong to the operator, not to us.
        if (path.has_parent_path()) {
            const fs::path parent = path.parent_path();

            // Harden the datadir ONLY when we are the ones creating it. If it already
            // exists its permissions are the operator's decision: an admin may have
            // deliberately widened them (a shared group, a second account, a backup
            // agent), and silently re-tightening on every start would undo that with
            // no warning and no way to make it stick. So we warn about a permissive
            // datadir but never change one we did not create.
            //
            // This is not a hole: the two things that actually matter -- the socket
            // and the auth cookie -- are ours, recreated on every start, and are
            // still created owner-only and fail closed below. A widened datadir makes
            // the directory listable, not the cookie readable.
            boost::system::error_code ec;
            const bool created = fs::create_directories(parent, ec);
            if (ec) {
                throw std::system_error(ec.value(), std::system_category(),
                                        "could not create the IPC socket directory " + parent.string());
            }

            if (created) {
#ifndef WIN32
                if (::chmod(parent.string().c_str(), 0700) != 0) {
                    throw std::system_error(errno, std::system_category());
                }
#else
                ApplyOwnerOnlyDacl(parent, /*inheritable=*/true);
#endif
            } else {
#ifndef WIN32
                struct stat st;
                if (::stat(parent.string().c_str(), &st) == 0 && (st.st_mode & (S_IRWXG | S_IRWXO)) != 0) {
                    LogPrintf("WARN: %s: the data directory %s is accessible to other users (mode %03o). "
                              "Leaving it as configured; the IPC socket and cookie inside it remain owner-only.",
                              __func__, parent.string(), static_cast<unsigned>(st.st_mode & 07777));
                }
#else
                try {
                    VerifyOwnerOnlyDacl(parent, GetOwnerOnlyTrustees());
                } catch (const std::exception&) {
                    LogPrintf("WARN: %s: the data directory %s is not restricted to this account. "
                              "Leaving it as configured; the IPC socket and cookie inside it remain owner-only.",
                              __func__, parent.string());
                }
#endif
            }
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
#else
                // Same fail-closed contract as the chmod above. The socket already
                // inherited the directory's owner-only ACEs at creation (applied
                // inheritably above), so this only re-states them non-inheritably
                // and protects them; a residual window exists solely if the parent
                // ACL was changed between the two calls, and any failure here
                // refuses to serve rather than listening unprotected.
                try {
                    ApplyOwnerOnlyDacl(path, /*inheritable=*/false);
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

#ifdef WIN32
void ApplyOwnerOnlyDacl(const fs::path& path, bool inheritable)
{
    const OwnerOnlyTrustees trustees = GetOwnerOnlyTrustees();
    PACL dacl = BuildOwnerOnlyDacl(trustees, inheritable);

    // SetNamedSecurityInfoW takes a mutable LPWSTR, hence the local buffer.
    std::wstring wpath = path.wstring();
    const DWORD rc = ::SetNamedSecurityInfoW(wpath.data(), SE_FILE_OBJECT,
                                             DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                                             nullptr, nullptr, dacl, nullptr);
    ::LocalFree(dacl);
    if (rc != ERROR_SUCCESS) {
        throw std::system_error(static_cast<int>(rc), std::system_category(),
                                "SetNamedSecurityInfo " + path.string());
    }
    // Setting an ACL can succeed nominally on a volume that does not store one;
    // read it back before we act as though the secret is protected.
    VerifyOwnerOnlyDacl(path, trustees);
}

void* MakeOwnerOnlyDacl(bool inheritable)
{
    return BuildOwnerOnlyDacl(GetOwnerOnlyTrustees(), inheritable);
}
#endif // WIN32
} // namespace ipc
