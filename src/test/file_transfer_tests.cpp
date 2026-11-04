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

BOOST_AUTO_TEST_SUITE_END()
