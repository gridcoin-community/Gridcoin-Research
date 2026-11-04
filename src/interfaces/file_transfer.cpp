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

} // namespace

const std::vector<ConsoleFileCommand>& ConsoleFileCommands()
{
    // The file argument of dumpcontracts is its second (contract_type comes first).
    static const std::vector<ConsoleFileCommand> table{
        {"dumpwallet", 0, TransferDirection::Download, 0},
        {"importwallet", 0, TransferDirection::Upload, 64ULL * 1024 * 1024},
        {"dumpcontracts", 1, TransferDirection::Download, 0},
        {"parseaccrualsnapshotfile", 0, TransferDirection::Upload, 256ULL * 1024 * 1024},
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
                    return fail("the receiver refused the data");
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

} // namespace interfaces
