// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_INTERFACES_FILE_TRANSFER_H
#define GRIDCOIN_INTERFACES_FILE_TRANSFER_H

#include "fs.h"
#include "uint256.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace interfaces {

//! The size of one transfer chunk.
static constexpr uint32_t FILE_TRANSFER_CHUNK_BYTES = 4 * 1024 * 1024;

//! Free space that must remain on the destination filesystem beyond the transfer.
static constexpr uint64_t FILE_TRANSFER_FREE_SPACE_MARGIN = 64 * 1024 * 1024;

//! One file a source offers. A size of -1 means the content is generated and its
//! size is unknown in advance; size_hint is then an estimate for the space check.
struct FileEntryInfo
{
    std::string name;
    int64_t size{-1};
    uint64_t size_hint{0};
    std::string note;
};

//! One chunk of an entry. A non-empty error ends the transfer.
struct FileChunk
{
    std::vector<unsigned char> data;
    bool eof{false};
    std::string error;
};

//! The size and SHA-256 the source computed over an entry it has sent in full.
struct FileTrailer
{
    bool ok{false};
    uint64_t size{0};
    uint256 sha256;
    std::string error;
};

struct FileSinkResult
{
    bool ok{false};
    std::string output;
    std::string error;
};

//! Where a download reads from (the core side of the transfer).
class FileSource
{
public:
    virtual ~FileSource() = default;

    virtual std::vector<FileEntryInfo> entries() = 0;

    //! Sequential. The server caps max_bytes at FILE_TRANSFER_CHUNK_BYTES.
    virtual FileChunk read(uint32_t entry, uint32_t max_bytes) = 0;

    //! Valid once that entry reached eof.
    virtual FileTrailer trailer(uint32_t entry) = 0;

    virtual void cancel() = 0;
};

//! Where an upload writes to (the core side of the transfer).
class FileSink
{
public:
    virtual ~FileSink() = default;

    //! False if the sink refused the data (the transfer is over).
    virtual bool write(const std::vector<unsigned char>& data) = 0;

    virtual FileSinkResult finish(uint64_t size, const uint256& sha256) = 0;

    virtual void cancel() = 0;
};

enum class BackupItem : uint8_t { Wallet = 1, Config = 2, Settings = 4 };

enum class TransferDirection { Download, Upload };

//! A console command whose argument names a file on the machine running the GUI.
//! path_index is the position of that argument; max_upload_bytes is the upload cap
//! (0 for downloads).
struct ConsoleFileCommand
{
    const char* method;
    size_t path_index;
    TransferDirection direction;
    uint64_t max_upload_bytes;
};

//! The path-taking console commands, in table order.
const std::vector<ConsoleFileCommand>& ConsoleFileCommands();

//! The row for method, or nullptr if the command takes no file path.
const ConsoleFileCommand* FindConsoleFileCommand(const std::string& method);

enum class TransferStatus { Ok, Cancelled, SourceError, LocalError, VerifyFailed, NoSpace };

struct TransferOutcome
{
    TransferStatus status{TransferStatus::LocalError};
    uint64_t bytes{0};
    uint256 sha256;
    std::string message;
};

//! Called after every chunk. total is the entry size, or -1 if it is not known.
using ProgressFn = std::function<void(uint64_t done, int64_t total)>;

//! Free-space check (size, else size_hint), then a temporary file, then reads until
//! eof (cancel is checked between chunks and calls src.cancel()), then the trailer,
//! then the verified rename. Any exception from src, such as a lost connection, is a
//! SourceError. The temporary file is gone on every path other than Ok.
TransferOutcome DownloadEntry(FileSource& src, uint32_t entry, const fs::path& dest, const ProgressFn& progress,
                              const std::atomic<bool>& cancel, uint32_t chunk_bytes = FILE_TRANSFER_CHUNK_BYTES);

//! Reads local in chunk_bytes pieces, hashing as it goes, then finish(size, sha).
//! Setting cancel calls sink.cancel().
FileSinkResult UploadFile(FileSink& sink, const fs::path& local, const ProgressFn& progress,
                          const std::atomic<bool>& cancel, uint32_t chunk_bytes = FILE_TRANSFER_CHUNK_BYTES);

} // namespace interfaces

#endif // GRIDCOIN_INTERFACES_FILE_TRANSFER_H
