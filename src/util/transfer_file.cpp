// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "util/transfer_file.h"

#include "interfaces/file_transfer.h"
#include "random.h"
#include "tinyformat.h"
#include "util/dir_permissions.h"
#include "util/strencodings.h"
#include "util/syserror.h"

#include <algorithm>
#include <cerrno>
#include <exception>
#include <limits>
#include <utility>

#ifdef WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace util {
namespace {

void RemoveQuietly(const fs::path& p)
{
    boost::system::error_code ec;
    fs::remove(p, ec);
}

#ifndef WIN32
// Make the rename durable. This is best effort: by now the destination has already
// been replaced, so a filesystem that cannot sync a directory must not turn a
// completed transfer into a reported failure.
void SyncDirectory(const fs::path& dir)
{
    const fs::path d = dir.empty() ? fs::path(".") : dir;
    const int fd = ::open(d.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);

    if (fd < 0) {
        return;
    }

    (void)::fsync(fd);
    ::close(fd);
}
#endif

} // namespace

std::optional<TransferTempFile> TransferTempFile::Create(const fs::path& dest, std::string& error)
{
    unsigned char rnd[8];
    GetRandBytes(Span<unsigned char>(rnd, sizeof(rnd)));

    fs::path temp = dest;
    temp += "." + HexStr(Span<const unsigned char>(rnd, sizeof(rnd))) + ".part";

    return CreateAt(dest, temp, error);
}

std::optional<TransferTempFile> TransferTempFile::CreateAt(const fs::path& dest, const fs::path& temp, std::string& error)
{
    TransferTempFile f;
    f.m_dest = dest;
    f.m_temp = temp;

#ifdef WIN32
    HANDLE h = ::CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                             FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);

    if (h == INVALID_HANDLE_VALUE) {
        error = strprintf("cannot create temporary file %s (error %u)", temp.string(), static_cast<unsigned>(::GetLastError()));
        return std::nullopt;
    }

    f.m_handle = h;
    f.m_active = true;

    try {
        ApplyOwnerOnlyDacl(temp, false);
    } catch (const std::exception& e) {
        error = strprintf("cannot restrict temporary file %s: %s", temp.string(), e.what());
        return std::nullopt;
    }
#else
    // O_EXCL refuses any existing name and O_NOFOLLOW refuses a symlink in the last
    // component, so a name planted ahead of time is never written through.
    const int fd = ::open(temp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);

    if (fd < 0) {
        error = strprintf("cannot create temporary file %s: %s", temp.string(), SysErrorString(errno));
        return std::nullopt;
    }

    f.m_fd = fd;
    f.m_active = true;

    // The mode given to open() is masked by the umask, so set it explicitly.
    if (::fchmod(fd, 0600) != 0) {
        error = strprintf("cannot restrict temporary file %s: %s", temp.string(), SysErrorString(errno));
        return std::nullopt;
    }
#endif

    return std::optional<TransferTempFile>(std::move(f));
}

TransferTempFile::TransferTempFile(TransferTempFile&& other) noexcept
{
    MoveFrom(other);
}

TransferTempFile& TransferTempFile::operator=(TransferTempFile&& other) noexcept
{
    if (this != &other) {
        Abort();
        MoveFrom(other);
    }

    return *this;
}

TransferTempFile::~TransferTempFile()
{
    Abort();
}

void TransferTempFile::MoveFrom(TransferTempFile& other) noexcept
{
    m_dest = std::move(other.m_dest);
    m_temp = std::move(other.m_temp);
    m_active = other.m_active;
    m_hasher = other.m_hasher;
    m_size = other.m_size;
#ifdef WIN32
    m_handle = other.m_handle;
    other.m_handle = nullptr;
#else
    m_fd = other.m_fd;
    other.m_fd = -1;
#endif
    other.m_active = false;
}

void TransferTempFile::CloseHandle()
{
#ifdef WIN32
    if (m_handle != nullptr) {
        ::CloseHandle(static_cast<HANDLE>(m_handle));
        m_handle = nullptr;
    }
#else
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
#endif
}

void TransferTempFile::Abort()
{
    CloseHandle();

    if (m_active) {
        RemoveQuietly(m_temp);
        m_active = false;
    }
}

uint256 TransferTempFile::Sha256() const
{
    CSHA256 hasher = m_hasher;
    uint256 out;
    hasher.Finalize(out.begin());
    return out;
}

bool TransferTempFile::Write(const unsigned char* data, size_t len, std::string& error)
{
    if (!m_active) {
        error = "temporary file is not open";
        return false;
    }

    size_t done = 0;

    while (done < len) {
#ifdef WIN32
        const DWORD want = static_cast<DWORD>(std::min<size_t>(len - done, 1u << 30));
        DWORD wrote = 0;

        if (!::WriteFile(static_cast<HANDLE>(m_handle), data + done, want, &wrote, nullptr)) {
            error = strprintf("write to %s failed (error %u)", m_temp.string(), static_cast<unsigned>(::GetLastError()));
            return false;
        }

        const size_t n = wrote;
#else
        const ssize_t r = ::write(m_fd, data + done, len - done);

        if (r < 0) {
            if (errno == EINTR) {
                continue;
            }

            error = strprintf("write to %s failed: %s", m_temp.string(), SysErrorString(errno));
            return false;
        }

        const size_t n = static_cast<size_t>(r);
#endif
        // A write that makes no progress is reported rather than retried forever.
        if (n == 0) {
            error = strprintf("write to %s made no progress", m_temp.string());
            return false;
        }

        m_hasher.Write(data + done, n);
        m_size += n;
        done += n;
    }

    return true;
}

bool TransferTempFile::Commit(uint64_t expected_size, const uint256& expected_sha256, std::string& error)
{
    if (!m_active) {
        error = "temporary file is not open";
        return false;
    }

#ifdef WIN32
    if (!::FlushFileBuffers(static_cast<HANDLE>(m_handle))) {
        error = strprintf("cannot flush %s (error %u)", m_temp.string(), static_cast<unsigned>(::GetLastError()));
        Abort();
        return false;
    }
#else
    if (::fsync(m_fd) != 0) {
        error = strprintf("cannot sync %s: %s", m_temp.string(), SysErrorString(errno));
        Abort();
        return false;
    }
#endif

    CloseHandle();

    if (m_size != expected_size) {
        error = strprintf("size mismatch: received %u bytes, expected %u", m_size, expected_size);
        Abort();
        return false;
    }

    if (Sha256() != expected_sha256) {
        error = "SHA-256 mismatch";
        Abort();
        return false;
    }

#ifdef WIN32
    if (!::MoveFileExW(m_temp.c_str(), m_dest.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error = strprintf("cannot rename %s over %s (error %u)", m_temp.string(), m_dest.string(),
                          static_cast<unsigned>(::GetLastError()));
        Abort();
        return false;
    }
#else
    if (::rename(m_temp.c_str(), m_dest.c_str()) != 0) {
        error = strprintf("cannot rename %s over %s: %s", m_temp.string(), m_dest.string(), SysErrorString(errno));
        Abort();
        return false;
    }

    SyncDirectory(m_dest.parent_path());
#endif

    // Committed. The temporary name no longer exists, so there is nothing to remove.
    m_active = false;
    return true;
}

bool HasFreeSpace(const fs::path& dir, uint64_t need, std::string& error)
{
    boost::system::error_code ec;
    const fs::space_info info = fs::space(dir, ec);

    if (ec) {
        error = strprintf("cannot determine the free space in %s: %s", dir.string(), ec.message());
        return false;
    }

    const uint64_t margin = interfaces::FILE_TRANSFER_FREE_SPACE_MARGIN;
    const uint64_t available = info.available;

    // need + margin without overflow
    const bool fits = need <= std::numeric_limits<uint64_t>::max() - margin && available >= need + margin;

    if (!fits) {
        const uint64_t required = need > std::numeric_limits<uint64_t>::max() - margin
                                      ? std::numeric_limits<uint64_t>::max() : need + margin;
        error = strprintf("not enough free space in %s: %u bytes available, %u bytes required (%u short)",
                          dir.string(), available, required, required - available);
        return false;
    }

    return true;
}

} // namespace util
