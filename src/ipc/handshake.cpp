// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "ipc/handshake.h"

#include "clientversion.h"
#include "crypto/sha256.h"
#include "random.h"
#include "tinyformat.h"
#include "util/strencodings.h"
#ifdef WIN32
#include "ipc/process.h" // ApplyOwnerOnlyDacl / MakeOwnerOnlyDacl
#endif

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <fcntl.h>
#include <iterator>
#include <sys/stat.h>
#include <system_error>
#ifndef WIN32
#include <unistd.h>
#else
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h> // CreateFileW / WriteFile / MoveFileExW / security descriptors
#endif

namespace ipc {
namespace {
const char* const COOKIE_FILE = "ipc.cookie";

//! Read an entire file into a string; nullopt if it cannot be opened.
std::optional<std::string> ReadFile(const fs::path& path)
{
    // fsbridge::ifstream, not std::ifstream(path.string()): on Windows the GNU
    // runtime has no wide-char stream constructor, so a narrow open goes through
    // the system code page (fs.h, "GNU libstdc++ specific workaround") and cannot
    // open a path that is not representable there -- while WriteFileAtomic below
    // writes through the wide API. A datadir under a non-ASCII profile path
    // (C:\Users\<non-codepage name>\...) would therefore be written successfully
    // and then read back as "no cookie", leaving the GUI stuck reporting that the
    // daemon is not running while it is running perfectly well. fsbridge::ifstream
    // opens via _wfopen and reads exactly the file the writer wrote.
    fsbridge::ifstream in(path, std::ios::binary);
    if (!in.good()) return std::nullopt;
    std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return contents;
}

//! Deletes the atomic-write temp file unless explicitly released. The temp file
//! holds the IPC authentication cookie: on any error path between creating it and
//! renaming it into place it must not be left behind, or a partial-write failure
//! leaves a copy of the secret on disk indefinitely (nothing ever cleans up
//! ipc.cookie.tmp -- the next start writes a *new* temp file and, on success,
//! renames it away, so the stale one simply persists). Never throws: it runs
//! during exception unwinding.
class TempFileGuard
{
public:
    explicit TempFileGuard(const fs::path& path) : m_path(path) {}
    TempFileGuard(const TempFileGuard&) = delete;
    TempFileGuard& operator=(const TempFileGuard&) = delete;
    ~TempFileGuard()
    {
        if (!m_armed) return;
        boost::system::error_code ec;
        fs::remove(m_path, ec); // best effort: we are already failing
    }
    void release() { m_armed = false; }

private:
    fs::path m_path;
    bool m_armed = true;
};

//! Write \p contents to \p path atomically (temp file + rename). The temp file is
//! created with mode 0600 from the start (no umask window that could expose the
//! secret cookie), the write is verified, and any failure throws so the caller
//! knows the file does not hold what it expects.
#ifdef WIN32
//! Windows port of the atomic cookie writer. Same guarantees as the POSIX path,
//! mapped to the Win32 API: an owner-only protected DACL supplied to CreateFileW
//! (the analogue of POSIX's mode 0600 at open(): the secret is never on disk under
//! a permissive ACL, not even briefly), no sharing for an exclusive open,
//! bInheritHandle=FALSE for close-on-exec, FlushFileBuffers() for fsync, and
//! MoveFileExW(REPLACE_EXISTING) for the atomic replace (boost::filesystem::
//! rename's overwrite behaviour on Windows is version-dependent, so do not rely
//! on it). O_NOFOLLOW has no direct analogue; CREATE_NEW over a pre-planted name
//! fails rather than following it, which covers the same threat.
//!
//! This used to open with _wsopen_s(..., _S_IREAD | _S_IWRITE) and rely on the
//! file inheriting an owner-only datadir ACL. Neither protected anything: the CRT
//! pmode is the DOS read-only *attribute*, not an access control entry (it does
//! not restrict any user), and nothing in the tree ever set or verified the
//! datadir ACL it was relying on. On-device (W4) the inherited ACL did happen to
//! be owner + SYSTEM + Administrators for a default %APPDATA% install, but that
//! was an observation about one machine, not a property this code enforced.
void WriteFileAtomic(const fs::path& path, const std::string& contents)
{
    fs::path tmp = path;
    tmp += ".tmp";
    const std::string tmp_str = tmp.string();

    // SECURITY_ATTRIBUTES are honoured only when CreateFileW actually *creates*
    // the file, so a leftover temp file must go first and the open must use
    // CREATE_NEW: CREATE_ALWAYS would truncate an existing file and silently keep
    // whatever ACL that file already had.
    boost::system::error_code remove_ec;
    fs::remove(tmp, remove_ec);
    if (remove_ec) {
        throw std::runtime_error("Failed to remove a leftover IPC cookie temp file " + tmp_str +
                                 ": " + remove_ec.message());
    }

    PACL dacl = static_cast<PACL>(MakeOwnerOnlyDacl(/*inheritable=*/false));
    SECURITY_DESCRIPTOR sd;
    HANDLE handle = INVALID_HANDLE_VALUE;
    DWORD create_err = 0;
    // SE_DACL_PROTECTED: without it the parent directory's inheritable ACEs are
    // merged into the new file's DACL, which would defeat the point of naming an
    // explicit trustee set here.
    if (!::InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION) ||
        !::SetSecurityDescriptorDacl(&sd, TRUE, dacl, FALSE) ||
        !::SetSecurityDescriptorControl(&sd, SE_DACL_PROTECTED, SE_DACL_PROTECTED)) {
        create_err = ::GetLastError();
    } else {
        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = &sd;
        sa.bInheritHandle = FALSE; // don't leak the handle into a child process
        handle = ::CreateFileW(tmp.wstring().c_str(), GENERIC_WRITE, /*dwShareMode=*/0, &sa,
                               CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) create_err = ::GetLastError();
    }
    ::LocalFree(dacl); // the created file holds its own copy of the ACL
    if (handle == INVALID_HANDLE_VALUE) {
        throw std::system_error(static_cast<int>(create_err), std::system_category(),
                                "create " + tmp_str);
    }
    // From here on the temp file exists and holds (or will hold) the secret: every
    // exit that is not the successful rename must delete it.
    TempFileGuard guard(tmp);

    size_t written = 0;
    while (written < contents.size()) {
        const DWORD chunk = static_cast<DWORD>(
            std::min<size_t>(contents.size() - written, static_cast<size_t>(1) << 20));
        DWORD n = 0;
        if (!::WriteFile(handle, contents.data() + written, chunk, &n, nullptr) || n == 0) {
            const DWORD e = ::GetLastError();
            ::CloseHandle(handle);
            throw std::system_error(static_cast<int>(e), std::system_category(), "write " + tmp_str);
        }
        written += static_cast<size_t>(n);
    }
    if (!::FlushFileBuffers(handle)) { // flush to disk (fsync equivalent)
        const DWORD e = ::GetLastError();
        ::CloseHandle(handle);
        throw std::system_error(static_cast<int>(e), std::system_category(), "flush " + tmp_str);
    }
    if (!::CloseHandle(handle)) {
        throw std::system_error(static_cast<int>(::GetLastError()), std::system_category(),
                                "close " + tmp_str);
    }

    // Re-state and verify the DACL on the finished file before it becomes the
    // cookie: a volume without ACL support accepts the SECURITY_ATTRIBUTES above
    // and stores nothing, and we would rather refuse to serve IPC than hand out a
    // world-readable authentication secret. Doing this before the rename means a
    // volume that cannot protect the cookie never gets one -- the guard deletes
    // the temp copy on the way out.
    ApplyOwnerOnlyDacl(tmp, /*inheritable=*/false);

    if (!::MoveFileExW(tmp.wstring().c_str(), path.wstring().c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        throw std::system_error(static_cast<int>(::GetLastError()), std::system_category(),
                                "MoveFileEx " + tmp_str + " -> " + path.string());
    }
    guard.release(); // renamed into place: nothing left to clean up
}
#else
void WriteFileAtomic(const fs::path& path, const std::string& contents)
{
    fs::path tmp = path;
    tmp += ".tmp";
    const std::string tmp_str = tmp.string();

    // Clear any leftover temp file first, then create exclusively. O_EXCL is what
    // makes the mode argument mean something: without it, O_CREAT|O_TRUNC happily
    // opens a file that already exists and writes the cookie into it, keeping
    // whatever owner and permissions that file had. O_NOFOLLOW rules out a
    // pre-planted symlink but says nothing about a pre-planted HARD LINK or a
    // plain attacker-owned regular file -- either would have captured the session
    // cookie on a datadir the user has widened. (WriteCookie also runs before the
    // datadir is hardened, and the datadir is only hardened at creation, so this
    // cannot lean on directory permissions.)
    boost::system::error_code remove_ec;
    fs::remove(tmp, remove_ec);
    if (remove_ec) {
        throw std::runtime_error("Failed to remove a leftover IPC cookie temp file " + tmp_str +
                                 ": " + remove_ec.message());
    }

    // O_CLOEXEC: don't leak this fd into any exec'd child.
    int fd = ::open(tmp_str.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd == -1) {
        const int e = errno;
        if (e == EEXIST) {
            // Lost a race against something re-creating the path between the
            // remove above and this open. Refuse: retrying would be racing an
            // adversary for the right to write a secret.
            throw std::runtime_error("Refusing to write the IPC cookie: " + tmp_str +
                                     " reappeared between removal and exclusive creation. Another "
                                     "process is writing to the data directory.");
        }
        throw std::system_error(e, std::system_category(), "open " + tmp_str);
    }
    // The temp file now holds (or will hold) the cookie: every exit that is not
    // the successful rename must delete it rather than leave the secret on disk.
    TempFileGuard guard(tmp);
    size_t written = 0;
    while (written < contents.size()) {
        ssize_t n = ::write(fd, contents.data() + written, contents.size() - written);
        if (n < 0) {
            int e = errno;
            ::close(fd);
            throw std::system_error(e, std::system_category(), "write " + tmp_str);
        }
        written += static_cast<size_t>(n);
    }
    if (::fsync(fd) != 0) {
        int e = errno;
        ::close(fd);
        throw std::system_error(e, std::system_category(), "fsync " + tmp_str);
    }
    if (::close(fd) != 0) {
        throw std::system_error(errno, std::system_category(), "close " + tmp_str);
    }
    fs::rename(tmp, path); // throws (boost::filesystem) on failure
    guard.release();       // renamed into place: nothing left to clean up
}
#endif
} // namespace

interfaces::BuildInfo GetLocalBuildInfo()
{
    interfaces::BuildInfo info;
    info.git_commit = FormatFullVersion();
    info.built_at = __DATE__ " " __TIME__;
    info.schema_major = IPC_SCHEMA_MAJOR;
    info.schema_minor = IPC_SCHEMA_MINOR;
    info.protocol_version = IPC_PROTOCOL_VERSION;
    return info;
}

bool ConstantTimeEqual(const std::string& a, const std::string& b)
{
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    }
    return diff == 0;
}

std::string WriteCookie(const fs::path& dir)
{
    std::array<unsigned char, 32> buf{};
    GetStrongRandBytes(buf);
    const std::string cookie = HexStr(buf);
    WriteFileAtomic(dir / COOKIE_FILE, cookie);
    return cookie;
}

std::optional<std::string> ReadCookie(const fs::path& dir)
{
    auto contents = ReadFile(dir / COOKIE_FILE);
    if (!contents) return std::nullopt;
    // Trim trailing whitespace/newline defensively.
    while (!contents->empty() && (contents->back() == '\n' || contents->back() == '\r' || contents->back() == ' ')) {
        contents->pop_back();
    }
    return contents;
}

std::string ComputeIdentityToken(const std::vector<unsigned char>& wallet_uuid)
{
    if (wallet_uuid.empty()) return std::string();

    CSHA256 hasher;
    // Domain tag (exclude the trailing NUL of the string literal).
    hasher.Write(reinterpret_cast<const unsigned char*>(IDENTITY_TOKEN_DOMAIN),
                 sizeof(IDENTITY_TOKEN_DOMAIN) - 1);
    // Length-prefix the UUID (LE32) so the tag/UUID boundary is unambiguous.
    const uint32_t len = static_cast<uint32_t>(wallet_uuid.size());
    const unsigned char len_le[4] = {
        static_cast<unsigned char>(len & 0xff),
        static_cast<unsigned char>((len >> 8) & 0xff),
        static_cast<unsigned char>((len >> 16) & 0xff),
        static_cast<unsigned char>((len >> 24) & 0xff),
    };
    hasher.Write(len_le, sizeof(len_le));
    hasher.Write(wallet_uuid.data(), wallet_uuid.size());

    std::array<unsigned char, CSHA256::OUTPUT_SIZE> out{};
    hasher.Finalize(out.data());
    return HexStr(out);
}

HandshakeResult ClientHandshake(interfaces::Init& init, const std::string& cookie,
                                const std::string& local_network,
                                const interfaces::BuildInfo& local)
{
    HandshakeResult result;
    try {
        if (!init.authenticate(cookie)) {
            result.error = "The node rejected the authentication cookie. It may have restarted "
                           "since the cookie was written; try reconnecting.";
            return result;
        }

        // Fetch build + identity once, up front: a second getIdentity() call would
        // be a redundant round-trip and could, in principle, race a divergent value.
        const interfaces::BuildInfo remote = init.getBuildInfo();
        const interfaces::NodeIdentity ident = init.getIdentity();

        if (remote.schema_major != local.schema_major) {
            result.error = strprintf("Incompatible IPC schema: this GUI speaks schema major %u but "
                                     "the node speaks %u. Use matching builds of gridcoinresearch "
                                     "and gridcoinresearchd.",
                                     local.schema_major, remote.schema_major);
            return result;
        }
        if (remote.protocol_version != local.protocol_version) {
            result.error = strprintf("Incompatible IPC protocol version: GUI %u, node %u.",
                                     local.protocol_version, remote.protocol_version);
            return result;
        }
        if (local.schema_minor > remote.schema_minor) {
            result.error = strprintf("This GUI is newer than the node (IPC schema minor GUI %u > "
                                     "node %u). Update the node.",
                                     local.schema_minor, remote.schema_minor);
            return result;
        }
        if (ident.network != local_network) {
            result.error = strprintf("This GUI is configured for the '%s' network but the daemon is "
                                     "running '%s'.",
                                     local_network, ident.network);
            return result;
        }

        // Soft findings (non-fatal). GUI schema_minor < node is forward-compatible
        // (log-only); a git_commit mismatch is the mixed-build banner.
        if (local.schema_minor < remote.schema_minor) {
            result.soft.push_back(SoftWarn::GuiOlderMinor);
        }
        if (remote.git_commit != local.git_commit) {
            result.soft.push_back(SoftWarn::GitCommitMismatch);
        }

        result.remote_build = remote;
        result.remote_ident = ident;
        result.ok = true;
        return result;
    } catch (const std::exception& e) {
        // An IPC call threw (daemon vanished mid-handshake). Distinct from an empty
        // identity token: this is a hard failure, not a graceful degrade.
        result.ok = false;
        result.soft.clear();
        result.error = strprintf("Lost connection to the daemon during the handshake: %s", e.what());
        return result;
    }
}

BindOutcome CheckIdentityBinding(const std::string& reported_token, const std::string& stored_token)
{
    if (reported_token.empty()) {
        // Empty reported token = identity unavailable. If we already have a stored
        // token this is a downgrade signal -- treat it as a mismatch, never a
        // silent skip (an attacker could force the node to report empty).
        return stored_token.empty() ? BindOutcome::UnavailableFresh : BindOutcome::UnavailableStored;
    }
    if (stored_token.empty()) return BindOutcome::FirstSeen;
    return reported_token == stored_token ? BindOutcome::Match : BindOutcome::Mismatch;
}

} // namespace ipc
