// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "crypto/sha256.h"
#include "fs.h"
#include "interfaces/file_transfer.h"
#include "test/test_gridcoin.h"
#include "uint256.h"
#include "util/system.h"

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef WIN32
#include <signal.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

#ifndef WIN32
// Restores the file-size limit and the SIGXFSZ disposition on every exit path.
class FileSizeLimitGuard
{
public:
    explicit FileSizeLimitGuard(rlim_t limit)
    {
        struct sigaction ignore;
        ignore.sa_handler = SIG_IGN;
        sigemptyset(&ignore.sa_mask);
        ignore.sa_flags = 0;

        m_signal_saved = ::sigaction(SIGXFSZ, &ignore, &m_old_action) == 0;
        m_limit_saved = ::getrlimit(RLIMIT_FSIZE, &m_old_limit) == 0;

        if (m_limit_saved) {
            struct rlimit lim = m_old_limit;
            lim.rlim_cur = limit;
            ::setrlimit(RLIMIT_FSIZE, &lim);
        }
    }

    ~FileSizeLimitGuard() { Restore(); }

    void Restore()
    {
        if (m_limit_saved) {
            ::setrlimit(RLIMIT_FSIZE, &m_old_limit);
            m_limit_saved = false;
        }

        if (m_signal_saved) {
            ::sigaction(SIGXFSZ, &m_old_action, nullptr);
            m_signal_saved = false;
        }
    }

private:
    struct rlimit m_old_limit{};
    struct sigaction m_old_action{};
    bool m_limit_saved{false};
    bool m_signal_saved{false};
};
#endif // !WIN32

std::vector<unsigned char> Bytes(size_t n)
{
    std::vector<unsigned char> v(n);

    for (size_t i = 0; i < n; ++i) {
        v[i] = static_cast<unsigned char>((i * 131 + (i >> 8)) & 0xff);
    }

    return v;
}

uint256 Sha256Of(const std::vector<unsigned char>& v)
{
    uint256 out;
    CSHA256().Write(v.data(), v.size()).Finalize(out.begin());
    return out;
}

std::string ReadAll(const fs::path& p)
{
    std::ifstream in(p.string(), std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

size_t CountEntries(const fs::path& dir)
{
    return static_cast<size_t>(std::distance(fs::directory_iterator(dir), fs::directory_iterator()));
}

struct FakeSource : public interfaces::FileSource
{
    explicit FakeSource(std::vector<unsigned char> d) : data(std::move(d)) {}

    std::vector<unsigned char> data;
    int64_t reported_size{-1};      // -2 means the data size
    uint64_t size_hint{0};
    int throw_on_read{0};           // 1-based read number that throws
    int error_on_read{0};           // 1-based read number that returns an error chunk
    bool wrong_sha{false};
    bool trailer_ok{true};
    int oversize_on_read{0};        // 1-based read number that returns more than max_bytes
    int empty_on_read{0};           // 1-based read number that returns no data and no eof
    size_t pos{0};
    int reads{0};
    uint32_t max_requested{0};
    bool cancelled{false};

    std::vector<interfaces::FileEntryInfo> entries() override
    {
        interfaces::FileEntryInfo e;
        e.name = "fake";
        e.size = reported_size == -2 ? static_cast<int64_t>(data.size()) : reported_size;
        e.size_hint = size_hint;
        return {e};
    }

    interfaces::FileChunk read(uint32_t, uint32_t max_bytes) override
    {
        ++reads;
        max_requested = std::max(max_requested, max_bytes);

        if (reads == throw_on_read) {
            throw std::runtime_error("connection lost");
        }

        interfaces::FileChunk c;

        if (reads == error_on_read) {
            c.error = "source failed";
            return c;
        }

        if (reads == empty_on_read) {
            return c;
        }

        if (reads == oversize_on_read) {
            c.data.assign(static_cast<size_t>(max_bytes) + 1, 0x11);
            return c;
        }

        const size_t n = std::min<size_t>(max_bytes, data.size() - pos);
        c.data.assign(data.begin() + pos, data.begin() + pos + n);
        pos += n;
        c.eof = pos == data.size();
        return c;
    }

    interfaces::FileTrailer trailer(uint32_t) override
    {
        interfaces::FileTrailer t;
        t.ok = trailer_ok;
        t.error = trailer_ok ? "" : "no trailer available";
        t.size = data.size();
        t.sha256 = wrong_sha ? Sha256Of(Bytes(7)) : Sha256Of(data);
        return t;
    }

    void cancel() override { cancelled = true; }
};

struct FakeSink : public interfaces::FileSink
{
    std::vector<unsigned char> received;
    size_t max_chunk{0};
    bool cancelled{false};
    bool finished{false};
    uint64_t finish_size{0};
    uint256 finish_sha;
    int fail_on_write{0};     // 1-based write number that returns false
    int throw_on_write{0};    // 1-based write number that throws
    int writes{0};

    bool write(const std::vector<unsigned char>& d) override
    {
        ++writes;

        if (writes == throw_on_write) {
            throw std::runtime_error("connection lost");
        }

        if (writes == fail_on_write) {
            return false;
        }

        max_chunk = std::max(max_chunk, d.size());
        received.insert(received.end(), d.begin(), d.end());
        return true;
    }

    interfaces::FileSinkResult finish(uint64_t size, const uint256& sha) override
    {
        finished = true;
        finish_size = size;
        finish_sha = sha;
        interfaces::FileSinkResult r;
        r.ok = true;
        return r;
    }

    void cancel() override { cancelled = true; }
};

// A source of several entries, as a backup's. An entry with a note has no data, and
// reading it gives an error chunk carrying the note. Once cancelled, every read is an
// error, as with the core's snapshot source.
struct MultiSource : public interfaces::FileSource
{
    struct Item
    {
        std::string name{};
        std::vector<unsigned char> data{};
        std::string note{};
        size_t pos{0};
    };

    std::vector<Item> items{};
    std::vector<int> reads{};
    bool throw_on_entries{false};
    int error_entry{-1};
    bool cancelled{false};

    explicit MultiSource(std::vector<Item> i) : items(std::move(i)), reads(items.size(), 0) {}

    std::vector<interfaces::FileEntryInfo> entries() override
    {
        if (throw_on_entries) {
            throw std::runtime_error("connection lost");
        }

        std::vector<interfaces::FileEntryInfo> infos;

        for (const Item& item : items) {
            interfaces::FileEntryInfo e;
            e.name = item.name;
            e.size = item.note.empty() ? static_cast<int64_t>(item.data.size()) : 0;
            e.size_hint = item.data.size();
            e.note = item.note;
            infos.push_back(e);
        }

        return infos;
    }

    interfaces::FileChunk read(uint32_t entry, uint32_t max_bytes) override
    {
        ++reads.at(entry);
        Item& item = items.at(entry);
        interfaces::FileChunk c;

        if (cancelled) {
            c.error = "the transfer was cancelled";
            return c;
        }

        if (!item.note.empty()) {
            c.error = item.note;
            return c;
        }

        if (static_cast<int>(entry) == error_entry) {
            c.error = "source failed";
            return c;
        }

        const size_t n = std::min<size_t>(max_bytes, item.data.size() - item.pos);
        c.data.assign(item.data.begin() + item.pos, item.data.begin() + item.pos + n);
        item.pos += n;
        c.eof = item.pos == item.data.size();
        return c;
    }

    interfaces::FileTrailer trailer(uint32_t entry) override
    {
        const Item& item = items.at(entry);
        interfaces::FileTrailer t;
        t.ok = true;
        t.size = item.data.size();
        t.sha256 = Sha256Of(item.data);
        return t;
    }

    void cancel() override { cancelled = true; }
};

// Writes a small file at p, creating its directory.
void Touch(const fs::path& p)
{
    fs::create_directories(p.parent_path());
    std::ofstream out(p.string(), std::ios::binary);
    out << "core file";
}

struct DirFixture
{
    fs::path dir;

    DirFixture() : dir(GetDataDir() / "file_transfer_tests")
    {
        fs::remove_all(dir);
        fs::create_directories(dir);
    }

    ~DirFixture()
    {
        boost::system::error_code mode_ec;
        fs::permissions(dir, fs::owner_all, mode_ec);
        boost::system::error_code ec;
        fs::remove_all(dir, ec);
    }
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(file_transfer_tests, DirFixture)

BOOST_AUTO_TEST_CASE(download_chunks_and_verifies)
{
    FakeSource src(Bytes(1'000'003));
    src.reported_size = -2;
    std::atomic<bool> cancel{false};
    std::vector<uint64_t> progress;

    auto out = interfaces::DownloadEntry(src, 0, dir / "d.bin",
                                         [&](uint64_t d, int64_t) { progress.push_back(d); }, cancel, 4096);

    BOOST_CHECK(out.status == interfaces::TransferStatus::Ok);
    BOOST_CHECK_EQUAL(out.bytes, 1'000'003u);
    BOOST_CHECK(out.sha256 == Sha256Of(src.data));
    BOOST_CHECK(src.max_requested <= 4096u);
    BOOST_CHECK(std::is_sorted(progress.begin(), progress.end()));
    BOOST_CHECK(!progress.empty() && progress.back() == 1'000'003u);
    BOOST_CHECK(ReadAll(dir / "d.bin") == std::string(src.data.begin(), src.data.end()));
    BOOST_CHECK_EQUAL(CountEntries(dir), 1u);
}

BOOST_AUTO_TEST_CASE(download_trailer_mismatch_is_verify_failed)
{
    FakeSource src(Bytes(20'000));
    src.wrong_sha = true;
    std::atomic<bool> cancel{false};

    auto out = interfaces::DownloadEntry(src, 0, dir / "d.bin", nullptr, cancel, 4096);

    BOOST_CHECK(out.status == interfaces::TransferStatus::VerifyFailed);
    BOOST_CHECK(!fs::exists(dir / "d.bin"));
    BOOST_CHECK_EQUAL(CountEntries(dir), 0u);
}

// A source that fails mid-stream leaves no temporary file and the destination unchanged
BOOST_AUTO_TEST_CASE(download_source_error_mid_stream_removes_part)
{
    {
        std::ofstream((dir / "d.bin").string(), std::ios::binary) << "old";
    }

    FakeSource src(Bytes(100'000));
    src.throw_on_read = 3;
    std::atomic<bool> cancel{false};

    auto out = interfaces::DownloadEntry(src, 0, dir / "d.bin", nullptr, cancel, 4096);

    BOOST_CHECK(out.status == interfaces::TransferStatus::SourceError);
    BOOST_CHECK(out.message.find("connection lost") != std::string::npos);
    BOOST_CHECK_EQUAL(ReadAll(dir / "d.bin"), "old");
    BOOST_CHECK_EQUAL(CountEntries(dir), 1u);

    // an error chunk ends the transfer the same way
    FakeSource src2(Bytes(100'000));
    src2.error_on_read = 2;

    out = interfaces::DownloadEntry(src2, 0, dir / "d.bin", nullptr, cancel, 4096);

    BOOST_CHECK(out.status == interfaces::TransferStatus::SourceError);
    BOOST_CHECK(out.message.find("source failed") != std::string::npos);
    BOOST_CHECK_EQUAL(ReadAll(dir / "d.bin"), "old");
    BOOST_CHECK_EQUAL(CountEntries(dir), 1u);
}

BOOST_AUTO_TEST_CASE(download_cancel_calls_source_cancel)
{
    FakeSource src(Bytes(100'000));
    std::atomic<bool> cancel{false};
    int chunks = 0;

    auto out = interfaces::DownloadEntry(src, 0, dir / "d.bin",
                                         [&](uint64_t, int64_t) { if (++chunks == 2) cancel = true; }, cancel, 4096);

    BOOST_CHECK(out.status == interfaces::TransferStatus::Cancelled);
    BOOST_CHECK(src.cancelled);
    BOOST_CHECK(!fs::exists(dir / "d.bin"));
    BOOST_CHECK_EQUAL(CountEntries(dir), 0u);
}

BOOST_AUTO_TEST_CASE(download_no_space_refused_before_create)
{
    FakeSource src(Bytes(10));
    src.reported_size = std::numeric_limits<int64_t>::max() / 2;
    std::atomic<bool> cancel{false};

    auto out = interfaces::DownloadEntry(src, 0, dir / "d.bin", nullptr, cancel, 4096);

    BOOST_CHECK(out.status == interfaces::TransferStatus::NoSpace);
    BOOST_CHECK_EQUAL(src.reads, 0);
    BOOST_CHECK_EQUAL(CountEntries(dir), 0u);

    // a generated entry is checked against its size hint
    FakeSource src2(Bytes(10));
    src2.reported_size = -1;
    src2.size_hint = std::numeric_limits<uint64_t>::max() / 2;

    out = interfaces::DownloadEntry(src2, 0, dir / "d.bin", nullptr, cancel, 4096);

    BOOST_CHECK(out.status == interfaces::TransferStatus::NoSpace);
    BOOST_CHECK_EQUAL(CountEntries(dir), 0u);
}

BOOST_AUTO_TEST_CASE(download_generated_entry_has_unknown_total)
{
    FakeSource src(Bytes(10'000));
    std::atomic<bool> cancel{false};
    int64_t last_total = 0;

    auto out = interfaces::DownloadEntry(src, 0, dir / "g.bin",
                                         [&](uint64_t, int64_t total) { last_total = total; }, cancel, 4096);

    BOOST_CHECK(out.status == interfaces::TransferStatus::Ok);
    BOOST_CHECK_EQUAL(last_total, -1);
}

BOOST_AUTO_TEST_CASE(upload_sends_size_and_hash)
{
    const std::vector<unsigned char> content = Bytes(100'003);
    {
        std::ofstream out((dir / "up.bin").string(), std::ios::binary);
        out.write(reinterpret_cast<const char*>(content.data()), static_cast<std::streamsize>(content.size()));
    }

    FakeSink sink;
    std::atomic<bool> cancel{false};
    uint64_t last = 0;

    auto r = interfaces::UploadFile(sink, dir / "up.bin", [&](uint64_t d, int64_t) { last = d; }, cancel, 4096);

    BOOST_CHECK(r.ok);
    BOOST_CHECK(sink.finished);
    BOOST_CHECK_EQUAL(sink.finish_size, content.size());
    BOOST_CHECK(sink.finish_sha == Sha256Of(content));
    BOOST_CHECK(sink.max_chunk <= 4096u);
    BOOST_CHECK(sink.received == content);
    BOOST_CHECK_EQUAL(last, content.size());
}

BOOST_AUTO_TEST_CASE(upload_cancel_and_failures)
{
    {
        std::ofstream out((dir / "up.bin").string(), std::ios::binary);
        const std::vector<unsigned char> content = Bytes(100'000);
        out.write(reinterpret_cast<const char*>(content.data()), static_cast<std::streamsize>(content.size()));
    }

    std::atomic<bool> cancel{false};

    FakeSink cancelled_sink;
    int chunks = 0;
    auto r = interfaces::UploadFile(cancelled_sink, dir / "up.bin",
                                    [&](uint64_t, int64_t) { if (++chunks == 2) cancel = true; }, cancel, 4096);
    BOOST_CHECK(!r.ok);
    BOOST_CHECK(cancelled_sink.cancelled);
    BOOST_CHECK(!cancelled_sink.finished);

    cancel = false;
    FakeSink refusing;
    refusing.fail_on_write = 3;
    r = interfaces::UploadFile(refusing, dir / "up.bin", nullptr, cancel, 4096);
    BOOST_CHECK(!r.ok);
    BOOST_CHECK(!r.error.empty());
    BOOST_CHECK(!refusing.finished);

    FakeSink throwing;
    throwing.throw_on_write = 2;
    r = interfaces::UploadFile(throwing, dir / "up.bin", nullptr, cancel, 4096);
    BOOST_CHECK(!r.ok);
    BOOST_CHECK(r.error.find("connection lost") != std::string::npos);
    BOOST_CHECK(!throwing.finished);

    FakeSink missing;
    r = interfaces::UploadFile(missing, dir / "does-not-exist", nullptr, cancel, 4096);
    BOOST_CHECK(!r.ok);
    BOOST_CHECK(!missing.finished);
}

BOOST_AUTO_TEST_CASE(download_cancel_after_final_chunk_is_honoured)
{
    // one chunk holds the whole file; cancel is set from the final progress callback
    FakeSource src(Bytes(1000));
    std::atomic<bool> cancel{false};

    auto out = interfaces::DownloadEntry(src, 0, dir / "d.bin", [&](uint64_t, int64_t) { cancel = true; }, cancel, 4096);

    BOOST_CHECK(out.status == interfaces::TransferStatus::Cancelled);
    BOOST_CHECK(src.cancelled);
    BOOST_CHECK_EQUAL(CountEntries(dir), 0u);
}

BOOST_AUTO_TEST_CASE(download_more_than_declared_size_fails_at_once)
{
    FakeSource src(Bytes(100'000));
    src.reported_size = 5000;
    std::atomic<bool> cancel{false};

    auto out = interfaces::DownloadEntry(src, 0, dir / "d.bin", nullptr, cancel, 4096);

    BOOST_CHECK(out.status == interfaces::TransferStatus::SourceError);
    // stops at the chunk that crossed the declared size, not at eof
    BOOST_CHECK_EQUAL(src.reads, 2);
    BOOST_CHECK(src.cancelled);
    BOOST_CHECK_EQUAL(CountEntries(dir), 0u);
}

BOOST_AUTO_TEST_CASE(download_declared_size_differs_from_received)
{
    // declared larger than the data the source delivers, trailer matches the data
    FakeSource src(Bytes(10'000));
    src.reported_size = 20'000;
    std::atomic<bool> cancel{false};

    auto out = interfaces::DownloadEntry(src, 0, dir / "d.bin", nullptr, cancel, 4096);

    BOOST_CHECK(out.status == interfaces::TransferStatus::VerifyFailed);
    BOOST_CHECK(src.cancelled);
    BOOST_CHECK(!fs::exists(dir / "d.bin"));
    BOOST_CHECK_EQUAL(CountEntries(dir), 0u);
}

BOOST_AUTO_TEST_CASE(download_oversized_and_empty_chunks_are_refused)
{
    std::atomic<bool> cancel{false};

    FakeSource big(Bytes(100'000));
    big.oversize_on_read = 2;
    auto out = interfaces::DownloadEntry(big, 0, dir / "d.bin", nullptr, cancel, 4096);
    BOOST_CHECK(out.status == interfaces::TransferStatus::SourceError);
    BOOST_CHECK(big.cancelled);
    BOOST_CHECK_EQUAL(CountEntries(dir), 0u);

    FakeSource empty(Bytes(100'000));
    empty.empty_on_read = 2;
    out = interfaces::DownloadEntry(empty, 0, dir / "d.bin", nullptr, cancel, 4096);
    BOOST_CHECK(out.status == interfaces::TransferStatus::SourceError);
    BOOST_CHECK(empty.cancelled);
    BOOST_CHECK_EQUAL(CountEntries(dir), 0u);
}

BOOST_AUTO_TEST_CASE(download_trailer_not_ok_and_bad_entry_index)
{
    std::atomic<bool> cancel{false};

    FakeSource src(Bytes(10'000));
    src.trailer_ok = false;
    auto out = interfaces::DownloadEntry(src, 0, dir / "d.bin", nullptr, cancel, 4096);
    BOOST_CHECK(out.status == interfaces::TransferStatus::SourceError);
    BOOST_CHECK(out.message.find("no trailer available") != std::string::npos);
    BOOST_CHECK(src.cancelled);
    BOOST_CHECK_EQUAL(CountEntries(dir), 0u);

    FakeSource other(Bytes(10));
    out = interfaces::DownloadEntry(other, 7, dir / "d.bin", nullptr, cancel, 4096);
    BOOST_CHECK(out.status == interfaces::TransferStatus::SourceError);
    BOOST_CHECK_EQUAL(other.reads, 0);
    BOOST_CHECK_EQUAL(CountEntries(dir), 0u);
}

BOOST_AUTO_TEST_CASE(download_zero_chunk_size_and_missing_directory)
{
    std::atomic<bool> cancel{false};

    FakeSource src(Bytes(10));
    auto out = interfaces::DownloadEntry(src, 0, dir / "d.bin", nullptr, cancel, 0);
    BOOST_CHECK(out.status == interfaces::TransferStatus::LocalError);
    BOOST_CHECK_EQUAL(src.reads, 0);

    // a failed free-space query is a local error, not a space shortfall
    FakeSource src2(Bytes(10));
    out = interfaces::DownloadEntry(src2, 0, dir / "no-such-dir" / "d.bin", nullptr, cancel, 4096);
    BOOST_CHECK(out.status == interfaces::TransferStatus::LocalError);
    BOOST_CHECK(!out.message.empty());
    BOOST_CHECK_EQUAL(src2.reads, 0);
}

#ifndef WIN32
BOOST_AUTO_TEST_CASE(download_write_failure_removes_part)
{
    std::ofstream((dir / "d.bin").string(), std::ios::binary) << "old";

    FakeSource src(Bytes(64 * 1024));
    std::atomic<bool> cancel{false};
    interfaces::TransferOutcome out;

    {
        FileSizeLimitGuard guard(4096);
        out = interfaces::DownloadEntry(src, 0, dir / "d.bin", nullptr, cancel, 64 * 1024);
        guard.Restore();
    }

    BOOST_CHECK(out.status == interfaces::TransferStatus::LocalError);
    BOOST_CHECK(src.cancelled);
    BOOST_CHECK_EQUAL(ReadAll(dir / "d.bin"), "old");
    BOOST_CHECK_EQUAL(CountEntries(dir), 1u);
}

BOOST_AUTO_TEST_CASE(download_reports_temp_file_that_cannot_be_removed)
{
    // an owner-only directory mode does not stop root from unlinking
    if (::geteuid() == 0) {
        BOOST_TEST_MESSAGE("skipped: running as root, so a read-only directory does not block unlink");
        return;
    }

    FakeSource src(Bytes(100'000));
    src.throw_on_read = 3;
    std::atomic<bool> cancel{false};
    int chunks = 0;

    // the directory becomes read-only after the first chunk, so the unlink fails
    auto out = interfaces::DownloadEntry(src, 0, dir / "d.bin",
                                         [&](uint64_t, int64_t) { if (++chunks == 1) ::chmod(dir.c_str(), 0500); },
                                         cancel, 4096);

    ::chmod(dir.c_str(), 0700);

    BOOST_CHECK(out.status == interfaces::TransferStatus::SourceError);
    BOOST_CHECK(out.message.find("could not be removed") != std::string::npos);
    BOOST_CHECK_EQUAL(CountEntries(dir), 1u);
}
#endif // !WIN32

BOOST_AUTO_TEST_CASE(upload_cancel_after_final_chunk_is_honoured)
{
    {
        std::ofstream out((dir / "up.bin").string(), std::ios::binary);
        out << "small file";
    }

    FakeSink sink;
    std::atomic<bool> cancel{false};

    auto r = interfaces::UploadFile(sink, dir / "up.bin", [&](uint64_t, int64_t) { cancel = true; }, cancel, 4096);

    BOOST_CHECK(!r.ok);
    BOOST_CHECK(sink.cancelled);
    BOOST_CHECK(!sink.finished);
}

BOOST_AUTO_TEST_CASE(upload_failures_cancel_the_sink)
{
    {
        std::ofstream out((dir / "up.bin").string(), std::ios::binary);
        out << "some content";
    }

    std::atomic<bool> cancel{false};

    FakeSink refusing;
    refusing.fail_on_write = 1;
    auto r = interfaces::UploadFile(refusing, dir / "up.bin", nullptr, cancel, 4096);
    BOOST_CHECK(!r.ok);
    BOOST_CHECK(refusing.cancelled);

    FakeSink throwing;
    throwing.throw_on_write = 1;
    r = interfaces::UploadFile(throwing, dir / "up.bin", nullptr, cancel, 4096);
    BOOST_CHECK(!r.ok);
    BOOST_CHECK(throwing.cancelled);

    FakeSink zero;
    r = interfaces::UploadFile(zero, dir / "up.bin", nullptr, cancel, 0);
    BOOST_CHECK(!r.ok);
    BOOST_CHECK(zero.cancelled);
    BOOST_CHECK_EQUAL(zero.writes, 0);
}

BOOST_AUTO_TEST_CASE(console_file_commands_table)
{
    const auto& t = interfaces::ConsoleFileCommands();

    BOOST_REQUIRE_EQUAL(t.size(), 4u);
    BOOST_CHECK_EQUAL(std::string(t[0].method), "dumpwallet");
    BOOST_CHECK_EQUAL(std::string(t[1].method), "importwallet");
    BOOST_CHECK_EQUAL(std::string(t[2].method), "dumpcontracts");
    BOOST_CHECK_EQUAL(std::string(t[3].method), "parseaccrualsnapshotfile");

    BOOST_CHECK_EQUAL(interfaces::FindConsoleFileCommand("dumpwallet")->path_index, 0u);
    BOOST_CHECK(interfaces::FindConsoleFileCommand("dumpwallet")->direction == interfaces::TransferDirection::Download);
    BOOST_CHECK_EQUAL(interfaces::FindConsoleFileCommand("dumpcontracts")->path_index, 1u);
    BOOST_CHECK(interfaces::FindConsoleFileCommand("dumpcontracts")->direction == interfaces::TransferDirection::Download);
    BOOST_CHECK(interfaces::FindConsoleFileCommand("importwallet")->direction == interfaces::TransferDirection::Upload);
    BOOST_CHECK_EQUAL(interfaces::FindConsoleFileCommand("importwallet")->max_upload_bytes, 64u * 1024 * 1024);
    BOOST_CHECK_EQUAL(interfaces::FindConsoleFileCommand("parseaccrualsnapshotfile")->path_index, 0u);
    BOOST_CHECK(interfaces::FindConsoleFileCommand("parseaccrualsnapshotfile")->direction == interfaces::TransferDirection::Upload);
    BOOST_CHECK_EQUAL(interfaces::FindConsoleFileCommand("parseaccrualsnapshotfile")->max_upload_bytes, 256u * 1024 * 1024);
    BOOST_CHECK(!interfaces::FindConsoleFileCommand("backupwallet"));
}

BOOST_AUTO_TEST_CASE(destination_guard_refuses_core_files)
{
    const fs::path core = dir / "core";
    const fs::path elsewhere = dir / "elsewhere";
    const std::vector<fs::path> core_files{core / "wallet.dat", core / "gridcoinresearch.conf",
                                           core / "gridcoinsettings.json", core / "debug.log"};

    for (const fs::path& f : core_files) {
        Touch(f);
    }

    fs::create_directories(core / "database");
    fs::create_directories(core / "transfers");
    fs::create_directories(elsewhere);

    std::string error;

    for (const fs::path& f : core_files) {
        BOOST_CHECK_MESSAGE(!interfaces::CheckDestination(f, core_files, core, error), f.string());
        BOOST_CHECK(!error.empty());
    }

    // The same file written another way.
    BOOST_CHECK(!interfaces::CheckDestination(elsewhere / ".." / "core" / "wallet.dat", core_files, core, error));

#ifndef WIN32
    fs::create_symlink(core / "wallet.dat", elsewhere / "link");
    BOOST_CHECK(!interfaces::CheckDestination(elsewhere / "link", core_files, core, error));
#endif

    BOOST_CHECK(!interfaces::CheckDestination(core / "database" / "x", core_files, core, error));
    BOOST_CHECK(error.find("database") != std::string::npos);
    BOOST_CHECK(!interfaces::CheckDestination(core / "transfers" / "x", core_files, core, error));
    BOOST_CHECK(error.find("transfers") != std::string::npos);
    BOOST_CHECK(!interfaces::CheckDestination(core / "database" / "deeper" / "x", core_files, core, error));
    BOOST_CHECK(!interfaces::CheckDestination(elsewhere / ".." / "core" / "database" / "x", core_files, core, error));

    BOOST_CHECK(!interfaces::CheckDestination(core, core_files, core, error));
    BOOST_CHECK(error.find("directory") != std::string::npos);
    BOOST_CHECK(!interfaces::CheckDestination(elsewhere, core_files, core, error));
    BOOST_CHECK(!interfaces::CheckDestination(fs::path(), core_files, core, error));

    // A new file elsewhere, an existing one elsewhere, and a new file beside the
    // core's (not one of its files, nor in database or transfers) are all fine.
    BOOST_CHECK(interfaces::CheckDestination(elsewhere / "backup.dat", core_files, core, error));
    BOOST_CHECK(error.empty());
    Touch(elsewhere / "old-backup.dat");
    BOOST_CHECK(interfaces::CheckDestination(elsewhere / "old-backup.dat", core_files, core, error));
    BOOST_CHECK(interfaces::CheckDestination(core / "backup.dat", core_files, core, error));
    BOOST_CHECK(interfaces::CheckDestination(core / "databases" / "x", core_files, core, error));

    // Empty entries are skipped, and so is the directory check without a datadir.
    BOOST_CHECK(interfaces::CheckDestination(elsewhere / "backup.dat", {fs::path()}, fs::path(), error));
    BOOST_CHECK(interfaces::CheckDestination(core / "database" / "x", {}, fs::path(), error));
}

#ifndef WIN32
BOOST_AUTO_TEST_CASE(destination_guard_sees_through_links)
{
    const fs::path core = dir / "core";
    const fs::path elsewhere = dir / "elsewhere";
    const std::vector<fs::path> core_files{core / "wallet.dat", core / "debug.log"};

    for (const fs::path& f : core_files) {
        Touch(f);
    }

    fs::create_directories(core / "database");
    fs::create_directories(elsewhere);

    std::string error;

    // A hard link has a path of its own, so only equivalence finds it.
    fs::create_hard_link(core / "wallet.dat", elsewhere / "hard");
    BOOST_CHECK(!interfaces::CheckDestination(elsewhere / "hard", core_files, core, error));

    // Through a linked directory, to a new file inside database and to a core file.
    fs::create_directory_symlink(core / "database", elsewhere / "dblink");
    BOOST_CHECK(!interfaces::CheckDestination(elsewhere / "dblink" / "x", core_files, core, error));
    fs::create_directory_symlink(core, elsewhere / "corelink");
    BOOST_CHECK(!interfaces::CheckDestination(elsewhere / "corelink" / "wallet.dat", core_files, core, error));
    BOOST_CHECK(!interfaces::CheckDestination(elsewhere / "corelink" / "database" / "x", core_files, core, error));
    BOOST_CHECK(interfaces::CheckDestination(elsewhere / "corelink" / "backup.dat", core_files, core, error));
}

BOOST_AUTO_TEST_CASE(destination_guard_without_access_to_the_core_files)
{
    // The core runs as another account and this one may not look inside its data
    // directory. Mode 0 stands in for that; root would see through it.
    if (::geteuid() == 0) {
        BOOST_TEST_MESSAGE("skipped: running as root, so a mode 0 directory can still be searched");
        return;
    }

    const fs::path core = dir / "core";
    const fs::path elsewhere = dir / "elsewhere";
    const std::vector<fs::path> core_files{core / "wallet.dat", core / "gridcoinresearch.conf",
                                           core / "gridcoinsettings.json", core / "debug.log"};

    for (const fs::path& f : core_files) {
        Touch(f);
    }

    fs::create_directories(core / "database");
    fs::create_directories(elsewhere);
    Touch(elsewhere / "old-backup.dat");

    BOOST_REQUIRE_EQUAL(::chmod(core.c_str(), 0), 0);

    struct RestoreMode
    {
        fs::path path{};
        ~RestoreMode() { ::chmod(path.c_str(), 0700); }
    } restore{core};

    // The equivalence check cannot run, and refuses nothing by itself.
    boost::system::error_code ec;
    fs::equivalent(elsewhere / "backup.dat", core / "wallet.dat", ec);
    BOOST_CHECK(ec);

    std::string error;

    BOOST_CHECK(interfaces::CheckDestination(elsewhere / "backup.dat", core_files, core, error));
    BOOST_CHECK_MESSAGE(error.empty(), error);
    BOOST_CHECK(interfaces::CheckDestination(elsewhere / "old-backup.dat", core_files, core, error));
    BOOST_CHECK_MESSAGE(error.empty(), error);

    // The core's own paths are still refused as written.
    BOOST_CHECK(!interfaces::CheckDestination(core / "wallet.dat", core_files, core, error));
    BOOST_CHECK(!interfaces::CheckDestination(core / "database" / "x", core_files, core, error));
    BOOST_CHECK(!interfaces::CheckDestination(core / "transfers" / "x", core_files, core, error));
    BOOST_CHECK(!interfaces::CheckDestination(core, core_files, core, error));
}
#endif // !WIN32

BOOST_AUTO_TEST_CASE(entry_with_a_note_is_skipped)
{
    interfaces::FileEntryInfo e;
    e.name = "gridcoinsettings.json";
    e.size = 0;
    BOOST_CHECK(!interfaces::EntryIsSkipped(e));

    e.note = "no settings file";
    BOOST_CHECK(interfaces::EntryIsSkipped(e));
}

BOOST_AUTO_TEST_CASE(backup_skips_an_entry_with_a_note)
{
    // A missing configuration file between the wallet and the settings file: the
    // settings file must still arrive.
    MultiSource src({{"wallet.dat", Bytes(10'000), ""},
                     {"gridcoinresearch.conf", {}, "no configuration file"},
                     {"gridcoinsettings.json", Bytes(300), ""}});
    std::atomic<bool> cancel{false};
    const std::vector<fs::path> dests{dir / "w.dat", dir / "c.conf", dir / "s.json"};
    int downloads = 0;

    const std::vector<interfaces::BackupEntryResult> results = interfaces::DownloadBackupEntries(
        src, dests, [&](interfaces::FileSource& s, uint32_t entry, const fs::path& dest) {
            ++downloads;
            return interfaces::DownloadEntry(s, entry, dest, nullptr, cancel, 4096);
        });

    BOOST_REQUIRE_EQUAL(results.size(), 3u);
    BOOST_CHECK_EQUAL(downloads, 2);

    BOOST_CHECK_EQUAL(results[0].name, "wallet.dat");
    BOOST_CHECK(results[0].dest == dests[0]);
    BOOST_CHECK(!results[0].skipped);
    BOOST_CHECK(results[0].outcome.status == interfaces::TransferStatus::Ok);
    BOOST_CHECK_EQUAL(results[0].outcome.bytes, 10'000u);

    BOOST_CHECK_EQUAL(results[1].name, "gridcoinresearch.conf");
    BOOST_CHECK(results[1].dest == dests[1]);
    BOOST_CHECK(results[1].skipped);
    BOOST_CHECK_EQUAL(results[1].note, "no configuration file");

    BOOST_CHECK(!results[2].skipped);
    BOOST_CHECK(results[2].outcome.status == interfaces::TransferStatus::Ok);
    BOOST_CHECK_EQUAL(results[2].outcome.bytes, 300u);

    BOOST_CHECK_EQUAL(src.reads[1], 0);
    BOOST_CHECK(!src.cancelled);
    BOOST_CHECK(ReadAll(dests[0]) == std::string(src.items[0].data.begin(), src.items[0].data.end()));
    BOOST_CHECK(ReadAll(dests[2]) == std::string(src.items[2].data.begin(), src.items[2].data.end()));
    BOOST_CHECK(!fs::exists(dests[1]));
    BOOST_CHECK_EQUAL(CountEntries(dir), 2u);
}

BOOST_AUTO_TEST_CASE(backup_stops_at_the_first_failure)
{
    MultiSource src({{"wallet.dat", Bytes(10'000), ""}, {"gridcoinsettings.json", Bytes(300), ""}});
    src.error_entry = 0;
    std::atomic<bool> cancel{false};
    int downloads = 0;

    const std::vector<interfaces::BackupEntryResult> results = interfaces::DownloadBackupEntries(
        src, {dir / "w.dat", dir / "s.json"}, [&](interfaces::FileSource& s, uint32_t entry, const fs::path& dest) {
            ++downloads;
            return interfaces::DownloadEntry(s, entry, dest, nullptr, cancel, 4096);
        });

    BOOST_REQUIRE_EQUAL(results.size(), 1u);
    BOOST_CHECK_EQUAL(downloads, 1);
    BOOST_CHECK(results[0].outcome.status == interfaces::TransferStatus::SourceError);
    BOOST_CHECK_EQUAL(results[0].outcome.message, "source failed");
    BOOST_CHECK(src.cancelled);
    BOOST_CHECK_EQUAL(src.reads[1], 0);
    BOOST_CHECK_EQUAL(CountEntries(dir), 0u);

    // A download that fails without reaching the source (the GUI has no transfer
    // thread) still leaves the source stopped.
    MultiSource untouched({{"wallet.dat", Bytes(10), ""}, {"gridcoinsettings.json", Bytes(10), ""}});
    const std::vector<interfaces::BackupEntryResult> local = interfaces::DownloadBackupEntries(
        untouched, {dir / "w.dat", dir / "s.json"}, [](interfaces::FileSource&, uint32_t, const fs::path&) {
            interfaces::TransferOutcome out;
            out.status = interfaces::TransferStatus::LocalError;
            out.message = "no file transfer thread is running";
            return out;
        });

    BOOST_REQUIRE_EQUAL(local.size(), 1u);
    BOOST_CHECK(local[0].outcome.status == interfaces::TransferStatus::LocalError);
    BOOST_CHECK(untouched.cancelled);
    BOOST_CHECK_EQUAL(untouched.reads[0] + untouched.reads[1], 0);
}

BOOST_AUTO_TEST_CASE(backup_cancel_leaves_nothing_and_stops_the_rest)
{
    MultiSource src({{"wallet.dat", Bytes(100'000), ""}, {"gridcoinsettings.json", Bytes(300), ""}});
    std::atomic<bool> cancel{false};
    int chunks = 0;

    const std::vector<interfaces::BackupEntryResult> results = interfaces::DownloadBackupEntries(
        src, {dir / "w.dat", dir / "s.json"}, [&](interfaces::FileSource& s, uint32_t entry, const fs::path& dest) {
            return interfaces::DownloadEntry(
                s, entry, dest, [&](uint64_t, int64_t) { if (++chunks == 3) cancel = true; }, cancel, 4096);
        });

    BOOST_REQUIRE_EQUAL(results.size(), 1u);
    BOOST_CHECK(results[0].outcome.status == interfaces::TransferStatus::Cancelled);
    BOOST_CHECK(src.cancelled);
    BOOST_CHECK_EQUAL(src.reads[1], 0);
    BOOST_CHECK_EQUAL(CountEntries(dir), 0u);
}

BOOST_AUTO_TEST_CASE(backup_source_that_cannot_list_its_entries)
{
    int downloads = 0;
    const interfaces::BackupDownloadFn count = [&](interfaces::FileSource&, uint32_t, const fs::path&) {
        ++downloads;
        return interfaces::TransferOutcome{};
    };

    MultiSource lost({{"wallet.dat", Bytes(10), ""}});
    lost.throw_on_entries = true;
    std::vector<interfaces::BackupEntryResult> results = interfaces::DownloadBackupEntries(lost, {dir / "w.dat"}, count);

    BOOST_REQUIRE_EQUAL(results.size(), 1u);
    BOOST_CHECK(results[0].outcome.status == interfaces::TransferStatus::SourceError);
    BOOST_CHECK_EQUAL(results[0].outcome.message, "connection lost");

    // Two entries for one destination: nothing is downloaded, and the source stops.
    MultiSource two({{"wallet.dat", Bytes(10), ""}, {"gridcoinsettings.json", Bytes(10), ""}});
    results = interfaces::DownloadBackupEntries(two, {dir / "w.dat"}, count);

    BOOST_REQUIRE_EQUAL(results.size(), 1u);
    BOOST_CHECK(results[0].outcome.status == interfaces::TransferStatus::SourceError);
    BOOST_CHECK(!results[0].outcome.message.empty());
    BOOST_CHECK(two.cancelled);

    BOOST_CHECK_EQUAL(downloads, 0);
    BOOST_CHECK_EQUAL(CountEntries(dir), 0u);
}

BOOST_AUTO_TEST_SUITE_END()
