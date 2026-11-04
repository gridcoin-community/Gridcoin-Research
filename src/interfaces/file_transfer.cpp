// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/file_transfer.h"

#include "crypto/sha256.h"
#include "tinyformat.h"
#include "util/transfer_file.h"

#include <algorithm>
#include <exception>
#include <memory>
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
    std::vector<FileEntryInfo> infos;

    try {
        infos = src.entries();
    } catch (const std::exception& e) {
        return Fail(TransferStatus::SourceError, 0, e.what());
    }

    if (entry >= infos.size()) {
        return Fail(TransferStatus::SourceError, 0, "the source has no such file");
    }

    const int64_t total = infos[entry].size;
    const uint64_t need = total >= 0 ? static_cast<uint64_t>(total) : infos[entry].size_hint;

    fs::path dir = dest.parent_path();

    if (dir.empty()) {
        dir = ".";
    }

    std::string error;

    if (!util::HasFreeSpace(dir, need, error)) {
        return Fail(TransferStatus::NoSpace, 0, error);
    }

    std::optional<util::TransferTempFile> tmp = util::TransferTempFile::Create(dest, error);

    if (!tmp) {
        return Fail(TransferStatus::LocalError, 0, error);
    }

    uint64_t done = 0;

    // Every return below goes through tmp's destructor or Commit(), so the temporary
    // file is gone on every path other than Ok.
    for (;;) {
        if (cancel.load()) {
            CancelQuietly(src);
            return Fail(TransferStatus::Cancelled, done, "cancelled");
        }

        FileChunk chunk;

        try {
            chunk = src.read(entry, chunk_bytes);
        } catch (const std::exception& e) {
            return Fail(TransferStatus::SourceError, done, e.what());
        }

        if (!chunk.error.empty()) {
            return Fail(TransferStatus::SourceError, done, chunk.error);
        }

        if (chunk.data.size() > chunk_bytes) {
            return Fail(TransferStatus::SourceError, done, "the source sent more than the requested chunk size");
        }

        // A source that returns nothing and never reaches eof would otherwise spin.
        if (chunk.data.empty() && !chunk.eof) {
            return Fail(TransferStatus::SourceError, done, "the source returned no data");
        }

        if (!chunk.data.empty() && !tmp->Write(chunk.data.data(), chunk.data.size(), error)) {
            return Fail(TransferStatus::LocalError, done, error);
        }

        done += chunk.data.size();

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
        return Fail(TransferStatus::SourceError, done, e.what());
    }

    if (!trailer.ok) {
        return Fail(TransferStatus::SourceError, done, trailer.error.empty() ? "the source gave no trailer" : trailer.error);
    }

    if (trailer.size != tmp->Size() || trailer.sha256 != tmp->Sha256() || (total >= 0 && static_cast<uint64_t>(total) != done)) {
        return Fail(TransferStatus::VerifyFailed, done, "the received data does not match the size and SHA-256 the source sent");
    }

    const uint256 sha = tmp->Sha256();

    if (!tmp->Commit(trailer.size, trailer.sha256, error)) {
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
    FileSinkResult failed;
    failed.ok = false;

    auto cancel_sink = [&sink]() {
        try {
            sink.cancel();
        } catch (const std::exception&) {
        }
    };

    boost::system::error_code ec;
    const uintmax_t file_size = fs::file_size(local, ec);

    if (ec) {
        failed.error = strprintf("cannot read %s: %s", local.string(), ec.message());
        cancel_sink();
        return failed;
    }

    fsbridge::ifstream in(local, std::ios::binary);

    if (!in.is_open()) {
        failed.error = strprintf("cannot open %s", local.string());
        cancel_sink();
        return failed;
    }

    CSHA256 hasher;
    uint64_t sent = 0;
    std::vector<unsigned char> buf(chunk_bytes);

    for (;;) {
        if (cancel.load()) {
            cancel_sink();
            failed.error = "cancelled";
            return failed;
        }

        in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
        const std::streamsize got = in.gcount();

        if (got > 0) {
            buf.resize(static_cast<size_t>(got));

            hasher.Write(buf.data(), buf.size());

            try {
                if (!sink.write(buf)) {
                    failed.error = "the receiver refused the data";
                    return failed;
                }
            } catch (const std::exception& e) {
                failed.error = e.what();
                return failed;
            }

            sent += static_cast<uint64_t>(got);
            buf.resize(chunk_bytes);

            if (progress) {
                progress(sent, static_cast<int64_t>(file_size));
            }
        }

        if (in.bad()) {
            failed.error = strprintf("read error on %s", local.string());
            cancel_sink();
            return failed;
        }

        if (got <= 0 || in.eof()) {
            break;
        }
    }

    uint256 sha;
    hasher.Finalize(sha.begin());

    try {
        return sink.finish(sent, sha);
    } catch (const std::exception& e) {
        failed.error = e.what();
        return failed;
    }
}

} // namespace interfaces
