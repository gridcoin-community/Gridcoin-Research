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

//! Refuses (error set) when dest is a directory, or equals or fs::equivalent()s any of the core's files
//! (core_files: wallet file, conf, settings file, debug log), or lies inside core_datadir's "database" or "transfers" directories.
//!
//! Paths are compared as written (made absolute), and again with the links in the part that exists
//! resolved. A check that cannot run because this account may not examine the core's files (the
//! core runs as another account) refuses nothing by itself, so a destination outside the core's
//! data directory is never refused for that reason. An empty entry in core_files, or an empty
//! core_datadir, is skipped.
bool CheckDestination(const fs::path& dest, const std::vector<fs::path>& core_files, const fs::path& core_datadir,
                      std::string& error);

//! An entry a source lists with a note has nothing to send: a configuration or settings file that
//! does not exist. Reading it is a source error, and DownloadEntry stops the whole source on one.
bool EntryIsSkipped(const FileEntryInfo& info);

//! What became of one entry of a backup.
struct BackupEntryResult
{
    std::string name{};
    fs::path dest{};
    //! Listed with a note (EntryIsSkipped): never read, and nothing was written.
    bool skipped{false};
    std::string note{};
    //! The download's outcome; meaningless for a skipped entry.
    TransferOutcome outcome{};
};

//! Downloads one entry of a source to dest: DownloadEntry, or the GUI's wrapper of it.
using BackupDownloadFn = std::function<TransferOutcome(FileSource& src, uint32_t entry, const fs::path& dest)>;

//! Download each entry of src, in order, to the destination at the same index of dests, through
//! download. A skipped entry (EntryIsSkipped) is reported with its note and never passed to
//! download, so the entries after it still arrive. Stops after the first entry whose outcome is not
//! Ok, and stops the source. A source that cannot list its entries, or lists a different number
//! than dests, gives one result with a SourceError, and the source is stopped.
std::vector<BackupEntryResult> DownloadBackupEntries(FileSource& src, const std::vector<fs::path>& dests,
                                                     const BackupDownloadFn& download);

} // namespace interfaces

#endif // GRIDCOIN_INTERFACES_FILE_TRANSFER_H
