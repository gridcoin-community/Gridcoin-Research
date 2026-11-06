// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_UTIL_TRANSFER_FILE_H
#define GRIDCOIN_UTIL_TRANSFER_FILE_H

#include "crypto/sha256.h"
#include "fs.h"
#include "uint256.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace util {

//! A destination file written through an exclusively created, owner-only temporary
//! file next to it. The bytes are hashed as they are written, and the temporary file
//! replaces the destination only when the size and SHA-256 given at the end match
//! what was written. On every other path the temporary file is removed and the
//! destination is left as it was. Nothing is held in memory beyond the caller's
//! current buffer.
class TransferTempFile
{
public:
    //! Creates "<dest>.<16 hex random>.part" in dest's directory. POSIX opens it with
    //! O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC and mode 0600, then fchmod(fd, 0600)
    //! so the mode does not depend on the umask. Windows uses CreateFileW(CREATE_NEW)
    //! with the owner-only DACL, so the file has it from creation, and reads it back
    //! through the handle.
    static std::optional<TransferTempFile> Create(const fs::path& dest, std::string& error);

    //! The same with the temporary name given. Create() calls this with a random name;
    //! it exists so tests can plant the name first.
    static std::optional<TransferTempFile> CreateAt(const fs::path& dest, const fs::path& temp, std::string& error);

    TransferTempFile(const TransferTempFile&) = delete;
    TransferTempFile& operator=(const TransferTempFile&) = delete;
    TransferTempFile(TransferTempFile&& other) noexcept;
    TransferTempFile& operator=(TransferTempFile&& other) noexcept;
    ~TransferTempFile();

    //! Appends to the temporary file, updating the running SHA-256 and byte count.
    //! A short write that cannot be completed is an error.
    bool Write(const unsigned char* data, size_t len, std::string& error);

    //! Bytes written so far.
    uint64_t Size() const { return m_size; }

    //! SHA-256 of the bytes written so far.
    uint256 Sha256() const;

    //! fsync and close, then refuse (removing the temporary file) unless Size() equals
    //! expected_size and Sha256() equals expected_sha256, then rename over the
    //! destination. A failed rename removes the temporary file.
    bool Commit(uint64_t expected_size, const uint256& expected_sha256, std::string& error);

    //! Close and remove the temporary file. Idempotent.
    void Abort();

    //! True if Abort() (or a failed Commit()) could not remove the temporary file.
    bool RemoveFailed() const { return m_remove_failed; }

    //! The note to append to an error message when RemoveFailed(), else empty.
    std::string RemoveFailedNote() const;

private:
    TransferTempFile() = default;

    void CloseHandle();
    void AppendRemoveNote(std::string& error) const;
    void MoveFrom(TransferTempFile& other) noexcept;

    fs::path m_dest;
    fs::path m_temp;
#ifdef WIN32
    void* m_handle{nullptr};
#else
    int m_fd{-1};
#endif
    bool m_active{false};
    bool m_remove_failed{false};
    CSHA256 m_hasher;
    uint64_t m_size{0};
};

enum class SpaceCheck { Enough, Short, QueryFailed };

//! Like HasFreeSpace, but tells a shortfall from a failed query of the filesystem.
SpaceCheck CheckFreeSpace(const fs::path& dir, uint64_t need, std::string& error);

//! True if the filesystem holding dir has at least need plus the transfer free-space
//! margin available. On false, error names the directory and the shortfall (or why
//! the free space could not be determined).
bool HasFreeSpace(const fs::path& dir, uint64_t need, std::string& error);

} // namespace util

#endif // GRIDCOIN_UTIL_TRANSFER_FILE_H
