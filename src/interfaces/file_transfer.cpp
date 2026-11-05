// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/file_transfer.h"

#include "crypto/sha256.h"
#include "tinyformat.h"
#include "util/transfer_file.h"

#include <exception>
#include <optional>

namespace interfaces {
namespace {

TransferOutcome Fail(TransferStatus status, uint64_t bytes, std::string message)
{
    TransferOutcome out;
    out.status = status;
    out.bytes = bytes;
    out.message = std::move(message);
    return out;
}

// Tell the source to stop. A source that is already gone (a lost connection) throws,
// and that must not hide the outcome being reported.
void CancelQuietly(FileSource& src)
{
    try {
        src.cancel();
    } catch (const std::exception&) {
    }
}

// Why the sink refused a write. A sink that refuses a write has stopped, and a stopped
// sink's finish() reports why. offered counts the refused bytes, which the sink did not
// take, so the finish of a sink that had not stopped does not match what it received and
// acts on nothing.
std::string RefusalReason(FileSink& sink, uint64_t offered)
{
    try {
        const FileSinkResult result = sink.finish(offered, uint256{});
        if (!result.ok && !result.error.empty()) return result.error;
    } catch (const std::exception&) {
    }

    return "the receiver refused the data";
}

// The path without the "." a trailing separator leaves as its last element.
fs::path WithoutTrailingDot(fs::path p)
{
    while (p.filename() == "." && p.has_parent_path()) {
        p = p.parent_path();
    }

    return p;
}

// The path made absolute and normal as it is written, with no link resolved.
fs::path AsWritten(const fs::path& p)
{
    fs::path abs = p;

    if (!abs.is_absolute()) {
        boost::system::error_code ec;
        const fs::path cwd = fs::current_path(ec);
        if (!ec) abs = cwd / abs;
    }

    return WithoutTrailingDot(abs.lexically_normal());
}

// The path with the links in the part of it that exists resolved. Where this account
// cannot resolve them (a directory it may not search), the path as written.
fs::path Resolved(const fs::path& p)
{
    const fs::path written = AsWritten(p);
    boost::system::error_code ec;
    const fs::path canonical = fs::weakly_canonical(written, ec);

    return ec ? written : WithoutTrailingDot(canonical.lexically_normal());
}

// p is dir, or a path below it, element by element.
bool IsWithin(const fs::path& p, const fs::path& dir)
{
    if (dir.empty()) return false;

    auto e = p.begin();

    for (auto d = dir.begin(); d != dir.end(); ++d, ++e) {
        if (e == p.end() || *e != *d) return false;
    }

    return true;
}

// Whether a and b are one file. An equivalence that cannot be decided, for a file that
// does not exist or one this account may not examine, is not one.
bool SameFile(const fs::path& a, const fs::path& b)
{
    boost::system::error_code ec;
    const bool same = fs::equivalent(a, b, ec);

    return same && !ec;
}

} // namespace

const std::vector<ConsoleFileCommand>& ConsoleFileCommands()
{
    // The file argument of dumpcontracts is its second (contract_type comes first),
    // and it takes three more after it (txids_only, low_height, high_height). It
    // exports public chain data, so it needs no passphrase.
    static const std::vector<ConsoleFileCommand> table{
        {"dumpwallet", 0, TransferDirection::Download, 0, 1, true},
        {"importwallet", 0, TransferDirection::Upload, 64ULL * 1024 * 1024, 1, true},
        {"dumpcontracts", 1, TransferDirection::Download, 0, 5, false},
        {"parseaccrualsnapshotfile", 0, TransferDirection::Upload, 256ULL * 1024 * 1024, 1, true},
    };

    return table;
}

const ConsoleFileCommand* FindConsoleFileCommand(const std::string& method)
{
    for (const ConsoleFileCommand& c : ConsoleFileCommands()) {
        if (method == c.method) {
            return &c;
        }
    }

    return nullptr;
}

TransferOutcome DownloadEntry(FileSource& src, uint32_t entry, const fs::path& dest, const ProgressFn& progress,
                              const std::atomic<bool>& cancel, uint32_t chunk_bytes)
{
    if (chunk_bytes == 0) {
        return Fail(TransferStatus::LocalError, 0, "the chunk size must not be zero");
    }

    std::vector<FileEntryInfo> infos;

    try {
        infos = src.entries();
    } catch (const std::exception& e) {
        return Fail(TransferStatus::SourceError, 0, e.what());
    }

    // From here on every path other than Ok stops the source, so a core-side source
    // that holds a capability while streaming lets go at once.
    if (entry >= infos.size()) {
        CancelQuietly(src);
        return Fail(TransferStatus::SourceError, 0, "the source has no such file");
    }

    const int64_t total = infos[entry].size;
    const uint64_t need = total >= 0 ? static_cast<uint64_t>(total) : infos[entry].size_hint;

    fs::path dir = dest.parent_path();

    if (dir.empty()) {
        dir = ".";
    }

    std::string error;

    switch (util::CheckFreeSpace(dir, need, error)) {
    case util::SpaceCheck::Enough:
        break;
    case util::SpaceCheck::Short:
        CancelQuietly(src);
        return Fail(TransferStatus::NoSpace, 0, error);
    case util::SpaceCheck::QueryFailed:
        CancelQuietly(src);
        return Fail(TransferStatus::LocalError, 0, error);
    }

    std::optional<util::TransferTempFile> tmp = util::TransferTempFile::Create(dest, error);

    if (!tmp) {
        CancelQuietly(src);
        return Fail(TransferStatus::LocalError, 0, error);
    }

    uint64_t done = 0;

    // Stops the source, removes the temporary file and reports why. If the file
    // cannot be removed the message says so.
    auto bail = [&](TransferStatus status, std::string message) {
        CancelQuietly(src);
        tmp->Abort();

        if (tmp->RemoveFailed()) {
            message += "; " + tmp->RemoveFailedNote();
        }

        return Fail(status, done, std::move(message));
    };

    for (;;) {
        if (cancel.load()) {
            return bail(TransferStatus::Cancelled, "cancelled");
        }

        FileChunk chunk;

        try {
            chunk = src.read(entry, chunk_bytes);
        } catch (const std::exception& e) {
            return bail(TransferStatus::SourceError, e.what());
        }

        if (!chunk.error.empty()) {
            return bail(TransferStatus::SourceError, chunk.error);
        }

        if (chunk.data.size() > chunk_bytes) {
            return bail(TransferStatus::SourceError, "the source sent more than the requested chunk size");
        }

        // A source that returns nothing and never reaches eof would otherwise spin.
        if (chunk.data.empty() && !chunk.eof) {
            return bail(TransferStatus::SourceError, "the source returned no data");
        }

        if (!chunk.data.empty() && !tmp->Write(chunk.data.data(), chunk.data.size(), error)) {
            return bail(TransferStatus::LocalError, error);
        }

        done += chunk.data.size();

        // A known-size entry that delivers more than it declared is refused at once.
        if (total >= 0 && done > static_cast<uint64_t>(total)) {
            return bail(TransferStatus::SourceError, "the source sent more data than the declared size");
        }

        if (progress) {
            progress(done, total);
        }

        if (chunk.eof) {
            break;
        }
    }

    FileTrailer trailer;

    try {
        trailer = src.trailer(entry);
    } catch (const std::exception& e) {
        return bail(TransferStatus::SourceError, e.what());
    }

    if (!trailer.ok) {
        return bail(TransferStatus::SourceError, trailer.error.empty() ? "the source gave no trailer" : trailer.error);
    }

    if (trailer.size != tmp->Size() || trailer.sha256 != tmp->Sha256() || (total >= 0 && static_cast<uint64_t>(total) != done)) {
        return bail(TransferStatus::VerifyFailed, "the received data does not match the size and SHA-256 the source sent");
    }

    // A cancel that arrived during the last chunk still stops the transfer.
    if (cancel.load()) {
        return bail(TransferStatus::Cancelled, "cancelled");
    }

    const uint256 sha = tmp->Sha256();

    if (!tmp->Commit(trailer.size, trailer.sha256, error)) {
        CancelQuietly(src);
        return Fail(TransferStatus::LocalError, done, error);
    }

    TransferOutcome out;
    out.status = TransferStatus::Ok;
    out.bytes = done;
    out.sha256 = sha;
    return out;
}

FileSinkResult UploadFile(FileSink& sink, const fs::path& local, const ProgressFn& progress,
                          const std::atomic<bool>& cancel, uint32_t chunk_bytes)
{
    // Every failure path tells the sink to stop, so a receiver that holds state for
    // the upload lets go at once. A sink that is already gone throws, which is ignored.
    auto fail = [&sink](std::string message) {
        try {
            sink.cancel();
        } catch (const std::exception&) {
        }

        FileSinkResult failed;
        failed.ok = false;
        failed.error = std::move(message);
        return failed;
    };

    if (chunk_bytes == 0) {
        return fail("the chunk size must not be zero");
    }

    boost::system::error_code ec;
    const uintmax_t file_size = fs::file_size(local, ec);

    if (ec) {
        return fail(strprintf("cannot read %s: %s", local.string(), ec.message()));
    }

    fsbridge::ifstream in(local, std::ios::binary);

    if (!in.is_open()) {
        return fail(strprintf("cannot open %s", local.string()));
    }

    CSHA256 hasher;
    uint64_t sent = 0;
    std::vector<unsigned char> buf(chunk_bytes);

    for (;;) {
        if (cancel.load()) {
            return fail("cancelled");
        }

        in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
        const std::streamsize got = in.gcount();

        if (got > 0) {
            buf.resize(static_cast<size_t>(got));

            hasher.Write(buf.data(), buf.size());

            try {
                if (!sink.write(buf)) {
                    return fail(RefusalReason(sink, sent + static_cast<uint64_t>(got)));
                }
            } catch (const std::exception& e) {
                return fail(e.what());
            }

            sent += static_cast<uint64_t>(got);
            buf.resize(chunk_bytes);

            if (progress) {
                progress(sent, static_cast<int64_t>(file_size));
            }
        }

        if (in.bad()) {
            return fail(strprintf("read error on %s", local.string()));
        }

        if (got <= 0 || in.eof()) {
            break;
        }
    }

    // A cancel that arrived during the last chunk must stop the upload before finish(),
    // which can act on the data (for example by importing it).
    if (cancel.load()) {
        return fail("cancelled");
    }

    uint256 sha;
    hasher.Finalize(sha.begin());

    try {
        return sink.finish(sent, sha);
    } catch (const std::exception& e) {
        return fail(e.what());
    }
}

bool CheckDestination(const fs::path& dest, const std::vector<fs::path>& core_files, const fs::path& core_datadir,
                      std::string& error)
{
    error.clear();

    if (dest.empty()) {
        error = "no destination was chosen";
        return false;
    }

    // Follows a link, so a link to a directory is one too. A path this account cannot
    // examine is left to the write, which reports why it fails.
    boost::system::error_code ec;
    if (fs::is_directory(dest, ec)) {
        error = strprintf("%s is a directory", dest.string());
        return false;
    }

    const fs::path dest_written = AsWritten(dest);
    const fs::path dest_resolved = Resolved(dest);

    // As written, the core's paths are refused even where this account cannot look
    // at them (the core runs as another account). Equivalence also finds a hard link,
    // and a spelling the comparisons miss.
    for (const fs::path& file : core_files) {
        if (file.empty()) continue;

        if (dest_written == AsWritten(file) || dest_resolved == Resolved(file) || SameFile(dest, file)) {
            error = strprintf("%s is one of the core's own files (%s)", dest.string(), file.string());
            return false;
        }
    }

    if (core_datadir.empty()) return true;

    for (const char* name : {"database", "transfers"}) {
        const fs::path dir = core_datadir / name;
        bool inside = IsWithin(dest_written, AsWritten(dir)) || IsWithin(dest_resolved, Resolved(dir));

        // A directory above dest that is dir under another name.
        for (fs::path above = dest_written.parent_path(); !inside && !above.empty() && above != above.parent_path();
             above = above.parent_path()) {
            inside = SameFile(above, dir);
        }

        if (inside) {
            error = strprintf("%s is inside the core's %s directory", dest.string(), dir.string());
            return false;
        }
    }

    return true;
}

bool EntryIsSkipped(const FileEntryInfo& info)
{
    return info.size == 0 && !info.note.empty();
}

std::vector<BackupEntryResult> DownloadBackupEntries(FileSource& src, const std::vector<fs::path>& dests,
                                                     const BackupDownloadFn& download)
{
    std::vector<BackupEntryResult> results;

    const auto source_failed = [&](std::string message) {
        CancelQuietly(src);

        BackupEntryResult failed;
        failed.outcome = Fail(TransferStatus::SourceError, 0, std::move(message));
        results.push_back(std::move(failed));
        return results;
    };

    std::vector<FileEntryInfo> infos;

    try {
        infos = src.entries();
    } catch (const std::exception& e) {
        return source_failed(e.what());
    }

    if (infos.size() != dests.size()) {
        return source_failed(strprintf("the core listed %u files for the %u requested", infos.size(), dests.size()));
    }

    for (size_t i = 0; i < infos.size(); ++i) {
        BackupEntryResult result;
        result.name = infos[i].name;
        result.dest = dests[i];

        if (EntryIsSkipped(infos[i])) {
            result.skipped = true;
            result.note = infos[i].note;
            results.push_back(std::move(result));
            continue;
        }

        result.outcome = download(src, static_cast<uint32_t>(i), infos[i], dests[i]);
        const bool arrived = result.outcome.status == TransferStatus::Ok;
        results.push_back(std::move(result));

        // DownloadEntry has stopped the source already, but a download can fail
        // before it reaches the source.
        if (!arrived) {
            CancelQuietly(src);
            break;
        }
    }

    return results;
}

} // namespace interfaces
