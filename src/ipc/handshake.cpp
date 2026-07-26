// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "ipc/handshake.h"

#include "clientversion.h"
#include "crypto/sha256.h"
#include "random.h"
#include "tinyformat.h"
#include "util/strencodings.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <fcntl.h>
#include <fstream>
#include <sys/stat.h>
#include <system_error>
#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>     // _wsopen_s / _write / _commit / _close
#include <share.h>  // _SH_DENYRW
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h> // MoveFileExW
#endif

namespace ipc {
namespace {
const char* const COOKIE_FILE = "ipc.cookie";

//! Read an entire file into a string; nullopt if it cannot be opened.
std::optional<std::string> ReadFile(const fs::path& path)
{
    std::ifstream in(path.string(), std::ios::binary);
    if (!in.good()) return std::nullopt;
    std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return contents;
}

//! Write \p contents to \p path atomically (temp file + rename). The temp file is
//! created with mode 0600 from the start (no umask window that could expose the
//! secret cookie), the write is verified, and any failure throws so the caller
//! knows the file does not hold what it expects.
#ifdef WIN32
//! Windows port of the atomic cookie writer. Same guarantees as the POSIX path,
//! mapped to winsock/CRT: _O_NOINHERIT for close-on-exec, _SH_DENYRW for an
//! exclusive open, _commit() for fsync, and MoveFileExW(REPLACE_EXISTING) for the
//! atomic replace (boost::filesystem::rename's overwrite behaviour on Windows is
//! version-dependent, so do not rely on it). There is no chmod: the file inherits
//! the per-user, owner-only datadir ACL. O_NOFOLLOW has no direct analogue, but
//! the secret only needs protecting from *other* users, which that ACL provides.
//! On-device (W4): validated -- the cookie handshake round-trips (daemon writes it
//! fresh each start, GUI reads and authenticates) and the file inherits the
//! owner-only datadir NTFS ACL (owner + SYSTEM + Administrators, per icacls).
void WriteFileAtomic(const fs::path& path, const std::string& contents)
{
    fs::path tmp = path;
    tmp += ".tmp";
    const std::string tmp_str = tmp.string();

    int fd = -1;
    const errno_t oerr = ::_wsopen_s(&fd, tmp.wstring().c_str(),
                                     _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY | _O_NOINHERIT,
                                     _SH_DENYRW, _S_IREAD | _S_IWRITE);
    if (oerr != 0 || fd == -1) {
        throw std::system_error(oerr ? oerr : errno, std::generic_category(), "open " + tmp_str);
    }
    size_t written = 0;
    while (written < contents.size()) {
        const int n = ::_write(fd, contents.data() + written,
                               static_cast<unsigned int>(contents.size() - written));
        if (n < 0) {
            const int e = errno;
            ::_close(fd);
            throw std::system_error(e, std::generic_category(), "write " + tmp_str);
        }
        written += static_cast<size_t>(n);
    }
    if (::_commit(fd) != 0) { // flush to disk (fsync equivalent)
        const int e = errno;
        ::_close(fd);
        throw std::system_error(e, std::generic_category(), "commit " + tmp_str);
    }
    if (::_close(fd) != 0) {
        throw std::system_error(errno, std::generic_category(), "close " + tmp_str);
    }
    if (!::MoveFileExW(tmp.wstring().c_str(), path.wstring().c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        throw std::system_error(static_cast<int>(::GetLastError()), std::system_category(),
                                "MoveFileEx " + tmp_str + " -> " + path.string());
    }
}
#else
void WriteFileAtomic(const fs::path& path, const std::string& contents)
{
    fs::path tmp = path;
    tmp += ".tmp";
    const std::string tmp_str = tmp.string();

    // O_CLOEXEC: don't leak this fd into any exec'd child. O_NOFOLLOW: the temp
    // path must not be a pre-planted symlink (this holds secrets: the cookie).
    int fd = ::open(tmp_str.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd == -1) {
        throw std::system_error(errno, std::system_category(), "open " + tmp_str);
    }
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
        result.error = strprintf("lost connection to the daemon during the handshake: %s", e.what());
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
