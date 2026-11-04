// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "crypto/sha256.h"
#include "fs.h"
#include "interfaces/connection_scope.h"
#include "interfaces/file_transfer.h"
#include "test/state_guard.h"
#include "test/wallet_file_placeholder.h"
#include "uint256.h"
#include "util/system.h"
#include "wallet/transfer_sources.h"
#include "wallet/wallet.h"

#include <boost/test/unit_test.hpp>

#include <chrono>
#include <cstdint>
#include <fstream>
#include <future>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#ifndef WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace {

using Bytes = std::vector<unsigned char>;

uint256 Sha256Of(const Bytes& bytes)
{
    uint256 out;
    CSHA256().Write(bytes.data(), bytes.size()).Finalize(out.begin());
    return out;
}

Bytes FileBytes(const fs::path& path)
{
    std::ifstream in(path.string(), std::ios::binary);
    return Bytes(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void WriteBytes(const fs::path& path, const Bytes& bytes)
{
    std::ofstream out(path.string(), std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

Bytes BytesOf(const std::string& text)
{
    return Bytes(text.begin(), text.end());
}

//! n bytes of a fixed pattern that does not repeat with any short period.
Bytes PatternBytes(size_t n)
{
    Bytes bytes(n);
    for (size_t i = 0; i < n; ++i) {
        bytes[i] = static_cast<unsigned char>((i * 131 + i / 251 + 7) & 0xff);
    }
    return bytes;
}

//! Entries directly in dir, or 0 if it does not exist.
size_t CountEntries(const fs::path& dir)
{
    boost::system::error_code ec;
    if (!fs::exists(fs::symlink_status(dir, ec))) return 0;

    size_t n = 0;
    for (fs::directory_iterator it(dir); it != fs::directory_iterator(); ++it) {
        ++n;
    }
    return n;
}

//! Whether something, a dangling link included, is at path.
bool Present(const fs::path& path)
{
    boost::system::error_code ec;
    return fs::exists(fs::symlink_status(path, ec));
}

//! "snap-" followed by 16 lowercase hex digits and ".tmp".
bool IsSnapshotName(const fs::path& path)
{
    const std::string name = path.filename().string();
    if (name.size() != 5 + 16 + 4 || name.compare(0, 5, "snap-") != 0 || name.compare(21, 4, ".tmp") != 0) {
        return false;
    }
    for (size_t i = 5; i < 21; ++i) {
        const char c = name[i];
        if ((c < '0' || c > '9') && (c < 'a' || c > 'f')) return false;
    }
    return true;
}

//! Read an entry to its end in reads of max_bytes, requiring every chunk to be
//! error-free and no larger than asked. chunks, if given, receives the count.
Bytes ReadToEnd(interfaces::FileSource& source, uint32_t entry, uint32_t max_bytes, size_t* chunks = nullptr)
{
    Bytes all;
    size_t count = 0;

    // Bounds a source that never reaches eof.
    for (int i = 0; i < 100000; ++i) {
        const interfaces::FileChunk chunk = source.read(entry, max_bytes);
        BOOST_REQUIRE_MESSAGE(chunk.error.empty(), chunk.error);
        BOOST_REQUIRE_LE(chunk.data.size(), max_bytes);
        all.insert(all.end(), chunk.data.begin(), chunk.data.end());
        ++count;
        if (chunk.eof) {
            if (chunks) *chunks = count;
            return all;
        }
    }

    BOOST_FAIL("the source never reached the end of the entry");
    return all;
}

#ifndef WIN32
unsigned ModeOf(const fs::path& path)
{
    struct stat st;
    BOOST_REQUIRE_EQUAL(::stat(path.c_str(), &st), 0);
    return static_cast<unsigned>(st.st_mode & 07777);
}
#endif

//! The case's own directory under the data directory, and the transfer
//! directory. Both are removed when the case ends, and the transfer directory
//! also when it begins, so each case finds it as a fresh start would.
struct TransferCase
{
    const fs::path dir = GetDataDir() / "transfer_sources_tests";

    TransferCase()
    {
        RemoveTransferDir();
        fs::create_directories(dir);
    }

    ~TransferCase()
    {
        boost::system::error_code ec;
        fs::remove_all(dir, ec);
        RemoveTransferDir();
    }

    static void RemoveTransferDir()
    {
        // remove_all removes a symbolic link itself, never what it points to.
        boost::system::error_code ec;
        fs::remove_all(GetDataDir() / "transfers", ec);
    }

    //! A snapshot of a file of the given bytes, made through SnapshotPlainFile.
    fs::path Snapshot(const std::string& name, const Bytes& bytes) const
    {
        const fs::path source = dir / name;
        WriteBytes(source, bytes);

        std::string error;
        const std::optional<fs::path> snapshot = wallet::SnapshotPlainFile(source, error);
        BOOST_REQUIRE_MESSAGE(snapshot, error);
        return *snapshot;
    }
};

} // namespace

BOOST_AUTO_TEST_SUITE(transfer_sources_tests)

BOOST_FIXTURE_TEST_CASE(snapshot_source_streams_file_then_deletes_it, TransferCase)
{
    const Bytes bytes = PatternBytes(2500);
    const fs::path source = dir / "source.bin";
    WriteBytes(source, bytes);

    std::string error;
    const std::optional<fs::path> snapshot = wallet::SnapshotPlainFile(source, error);
    BOOST_REQUIRE_MESSAGE(snapshot, error);
    BOOST_CHECK(snapshot->parent_path() == wallet::GetTransferDir());
    BOOST_CHECK_MESSAGE(IsSnapshotName(*snapshot), snapshot->string());
    BOOST_CHECK(FileBytes(*snapshot) == bytes);
#ifndef WIN32
    BOOST_CHECK_EQUAL(ModeOf(*snapshot), 0600u);
#endif
    // The copy is the only file it made there.
    BOOST_CHECK_EQUAL(CountEntries(wallet::GetTransferDir()), 1u);

    std::unique_ptr<interfaces::FileSource> s =
        wallet::MakeSnapshotSource({{"x", *snapshot, ""}}, interfaces::MakeConnectionScope());
    BOOST_REQUIRE(s);

    const std::vector<interfaces::FileEntryInfo> infos = s->entries();
    BOOST_REQUIRE_EQUAL(infos.size(), 1u);
    BOOST_CHECK_EQUAL(infos[0].name, "x");
    BOOST_CHECK_EQUAL(infos[0].size, 2500);
    BOOST_CHECK(infos[0].note.empty());

    size_t chunks = 0;
    BOOST_CHECK(ReadToEnd(*s, 0, 1000, &chunks) == bytes);
    BOOST_CHECK_EQUAL(chunks, 3u);

    // Deleted as soon as it was read to its end, before the trailer is asked for.
    BOOST_CHECK(!Present(*snapshot));

    const interfaces::FileTrailer trailer = s->trailer(0);
    BOOST_CHECK(trailer.ok);
    BOOST_CHECK(trailer.error.empty());
    BOOST_CHECK_EQUAL(trailer.size, bytes.size());
    BOOST_CHECK(trailer.sha256 == Sha256Of(bytes));

    BOOST_CHECK(!Present(*snapshot));
    BOOST_CHECK_EQUAL(CountEntries(wallet::GetTransferDir()), 0u);

    // The file it was copied from is untouched.
    BOOST_CHECK(FileBytes(source) == bytes);
}

BOOST_FIXTURE_TEST_CASE(snapshot_source_is_single_use, TransferCase)
{
    const Bytes bytes = PatternBytes(300);

    // Read again after its end.
    {
        const fs::path snapshot = Snapshot("a", bytes);
        std::unique_ptr<interfaces::FileSource> s =
            wallet::MakeSnapshotSource({{"a", snapshot, ""}}, interfaces::MakeConnectionScope());
        BOOST_REQUIRE(s);
        BOOST_CHECK(ReadToEnd(*s, 0, 4096) == bytes);

        const interfaces::FileChunk again = s->read(0, 4096);
        BOOST_CHECK(!again.error.empty());
        BOOST_CHECK(again.data.empty());
        BOOST_CHECK(!again.eof);

        // The entry's trailer is still there.
        BOOST_CHECK(s->trailer(0).ok);
    }

    // Cancelled part way.
    {
        const fs::path snapshot = Snapshot("b", bytes);
        std::unique_ptr<interfaces::FileSource> s =
            wallet::MakeSnapshotSource({{"b", snapshot, ""}}, interfaces::MakeConnectionScope());
        BOOST_REQUIRE(s);

        const interfaces::FileChunk first = s->read(0, 100);
        BOOST_REQUIRE_MESSAGE(first.error.empty(), first.error);
        BOOST_CHECK_EQUAL(first.data.size(), 100u);
        BOOST_CHECK(!first.eof);
        BOOST_CHECK(Present(snapshot));

        s->cancel();
        BOOST_CHECK(!Present(snapshot));

        const interfaces::FileChunk after = s->read(0, 100);
        BOOST_CHECK(!after.error.empty());
        BOOST_CHECK(after.data.empty());

        const interfaces::FileTrailer trailer = s->trailer(0);
        BOOST_CHECK(!trailer.ok);
        BOOST_CHECK(!trailer.error.empty());
    }

    // Cancelled before any read.
    {
        const fs::path snapshot = Snapshot("c", bytes);
        std::unique_ptr<interfaces::FileSource> s =
            wallet::MakeSnapshotSource({{"c", snapshot, ""}}, interfaces::MakeConnectionScope());
        BOOST_REQUIRE(s);

        s->cancel();
        BOOST_CHECK(!Present(snapshot));
        BOOST_CHECK(!s->read(0, 100).error.empty());
    }

    BOOST_CHECK_EQUAL(CountEntries(wallet::GetTransferDir()), 0u);
}

BOOST_FIXTURE_TEST_CASE(snapshot_source_reads_one_entry_at_a_time, TransferCase)
{
    const Bytes a_bytes = PatternBytes(1000);
    const Bytes b_bytes = BytesOf("the second entry");
    const fs::path a = Snapshot("a", a_bytes);
    const fs::path b = Snapshot("b", b_bytes);

    std::unique_ptr<interfaces::FileSource> s =
        wallet::MakeSnapshotSource({{"a", a, ""}, {"b", b, ""}}, interfaces::MakeConnectionScope());
    BOOST_REQUIRE(s);

    // No trailer before the end.
    BOOST_CHECK(!s->trailer(0).ok);

    const interfaces::FileChunk first = s->read(0, 10);
    BOOST_REQUIRE_MESSAGE(first.error.empty(), first.error);

    // The other entry is refused while this one is partly read.
    const interfaces::FileChunk other = s->read(1, 10);
    BOOST_CHECK(!other.error.empty());
    BOOST_CHECK(other.data.empty());

    // Neither an entry it does not have.
    BOOST_CHECK(!s->read(2, 10).error.empty());
    BOOST_CHECK(!s->trailer(2).ok);

    // The refusals left the first entry where it was.
    Bytes a_read(first.data.begin(), first.data.end());
    const Bytes rest = ReadToEnd(*s, 0, 4096);
    a_read.insert(a_read.end(), rest.begin(), rest.end());
    BOOST_CHECK(a_read == a_bytes);

    // Then the second one.
    BOOST_CHECK(ReadToEnd(*s, 1, 4096) == b_bytes);
    BOOST_CHECK(s->trailer(0).ok);
    BOOST_CHECK(s->trailer(0).sha256 == Sha256Of(a_bytes));
    BOOST_CHECK(s->trailer(1).ok);
    BOOST_CHECK(s->trailer(1).sha256 == Sha256Of(b_bytes));

    BOOST_CHECK_EQUAL(CountEntries(wallet::GetTransferDir()), 0u);
}

BOOST_FIXTURE_TEST_CASE(snapshot_source_caps_chunk_size, TransferCase)
{
    const Bytes bytes = PatternBytes(5 * 1024 * 1024);
    const fs::path snapshot = Snapshot("big", bytes);

    std::unique_ptr<interfaces::FileSource> s =
        wallet::MakeSnapshotSource({{"big", snapshot, ""}}, interfaces::MakeConnectionScope());
    BOOST_REQUIRE(s);

    // A read that asks for nothing is refused, and changes nothing.
    BOOST_CHECK(!s->read(0, 0).error.empty());

    const interfaces::FileChunk first = s->read(0, 64 * 1024 * 1024);
    BOOST_REQUIRE_MESSAGE(first.error.empty(), first.error);
    BOOST_CHECK_EQUAL(first.data.size(), interfaces::FILE_TRANSFER_CHUNK_BYTES);
    BOOST_CHECK(!first.eof);

    const interfaces::FileChunk second = s->read(0, 64 * 1024 * 1024);
    BOOST_REQUIRE_MESSAGE(second.error.empty(), second.error);
    BOOST_CHECK_EQUAL(second.data.size(), bytes.size() - interfaces::FILE_TRANSFER_CHUNK_BYTES);
    BOOST_CHECK(second.eof);

    Bytes all(first.data.begin(), first.data.end());
    all.insert(all.end(), second.data.begin(), second.data.end());
    BOOST_CHECK(all == bytes);

    const interfaces::FileTrailer trailer = s->trailer(0);
    BOOST_CHECK(trailer.ok);
    BOOST_CHECK_EQUAL(trailer.size, bytes.size());
    BOOST_CHECK(trailer.sha256 == Sha256Of(bytes));
    BOOST_CHECK(!Present(snapshot));
}

// Review Focus 2: a GUI that goes away mid-transfer leaves no snapshot behind.
BOOST_FIXTURE_TEST_CASE(connection_close_deletes_snapshot_files, TransferCase)
{
    const auto scope = interfaces::MakeConnectionScope();
    const fs::path a = Snapshot("a", PatternBytes(1000));
    const fs::path b = Snapshot("b", PatternBytes(10));

    std::unique_ptr<interfaces::FileSource> s = wallet::MakeSnapshotSource({{"a", a, ""}, {"b", b, ""}}, scope);
    BOOST_REQUIRE(s);

    const interfaces::FileChunk first = s->read(0, 10);
    BOOST_REQUIRE_MESSAGE(first.error.empty(), first.error);
    BOOST_CHECK(!first.eof);

    scope->close();

    // Both, the one part read and the one not read at all.
    BOOST_CHECK(!Present(a));
    BOOST_CHECK(!Present(b));

    const interfaces::FileChunk after = s->read(0, 10);
    BOOST_CHECK(!after.error.empty());
    BOOST_CHECK(after.data.empty());
    BOOST_CHECK(!s->read(1, 10).error.empty());
    BOOST_CHECK(!s->trailer(0).ok);

    s.reset();

    // A source made on a connection that has already closed deletes its files
    // at once.
    const fs::path c = Snapshot("c", PatternBytes(10));
    std::unique_ptr<interfaces::FileSource> late = wallet::MakeSnapshotSource({{"c", c, ""}}, scope);
    BOOST_REQUIRE(late);
    BOOST_CHECK(!Present(c));
    BOOST_CHECK(!late->read(0, 10).error.empty());

    BOOST_CHECK_EQUAL(CountEntries(wallet::GetTransferDir()), 0u);
}

#ifndef WIN32
// The close hook leaves the file a read is using to that read, which deletes it
// when it returns. Driven deterministically through a named pipe as the entry's
// file: the read blocks in it, with the mutex released, until the case writes.
// The writer's open returns only once the read has opened the pipe, which the
// read does after it has marked the entry as being read, so the close that
// follows finds it marked.
BOOST_FIXTURE_TEST_CASE(connection_close_leaves_a_file_being_read_to_that_read, TransferCase)
{
    const auto scope = interfaces::MakeConnectionScope();
    const fs::path tdir = wallet::GetTransferDir();
    const fs::path pipe = tdir / "snap-0000000000000001.tmp";
    BOOST_REQUIRE_EQUAL(::mkfifo(pipe.c_str(), 0600), 0);
    const fs::path other = Snapshot("other", PatternBytes(10));

    std::unique_ptr<interfaces::FileSource> s =
        wallet::MakeSnapshotSource({{"pipe", pipe, ""}, {"other", other, ""}}, scope);
    BOOST_REQUIRE(s);

    std::promise<void> opened;
    std::promise<void> release;
    std::shared_future<void> released = release.get_future().share();

    std::thread writer([&pipe, &opened, released] {
        // Blocks until the read opens the pipe.
        const int fd = ::open(pipe.c_str(), O_WRONLY);
        opened.set_value();
        released.wait();
        if (fd >= 0) {
            const char payload[] = "late";
            (void)!::write(fd, payload, 4);
            ::close(fd);
        }
    });

    std::future<interfaces::FileChunk> reading =
        std::async(std::launch::async, [&s] { return s->read(0, 1024); });

    const bool reached = opened.get_future().wait_for(std::chrono::seconds{20}) == std::future_status::ready;
    BOOST_CHECK_MESSAGE(reached, "the read never opened the entry's file");
    if (!reached) {
        // Lets the writer's open return, so the case can end.
        const int fd = ::open(pipe.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd >= 0) ::close(fd);
    }

    scope->close();

    // The file being read is left to the read; the other entry's is gone.
    BOOST_CHECK(Present(pipe));
    BOOST_CHECK(!Present(other));

    release.set_value();
    const interfaces::FileChunk chunk = reading.get();
    writer.join();

    // The read saw the close when it returned: its chunk goes nowhere, and it
    // deleted the file.
    BOOST_CHECK(!chunk.error.empty());
    BOOST_CHECK(chunk.data.empty());
    BOOST_CHECK(!Present(pipe));
    BOOST_CHECK_EQUAL(CountEntries(tdir), 0u);
}
#endif

BOOST_FIXTURE_TEST_CASE(snapshot_source_destruction_deletes_its_files, TransferCase)
{
    // Part read, and not read at all.
    {
        const auto scope = interfaces::MakeConnectionScope();
        const fs::path a = Snapshot("a", PatternBytes(1000));
        const fs::path b = Snapshot("b", PatternBytes(10));

        std::unique_ptr<interfaces::FileSource> s = wallet::MakeSnapshotSource({{"a", a, ""}, {"b", b, ""}}, scope);
        BOOST_REQUIRE(s);
        BOOST_REQUIRE(s->read(0, 10).error.empty());

        s.reset();
        BOOST_CHECK(!Present(a));
        BOOST_CHECK(!Present(b));

        // The destroyed source's hook was removed with it, so closing runs nothing
        // of it.
        scope->close();
    }

    // In process there is no connection scope.
    {
        const Bytes bytes = PatternBytes(10);
        const fs::path c = Snapshot("c", bytes);
        const fs::path d = Snapshot("d", bytes);

        std::unique_ptr<interfaces::FileSource> s = wallet::MakeSnapshotSource({{"c", c, ""}, {"d", d, ""}}, nullptr);
        BOOST_REQUIRE(s);
        BOOST_CHECK(ReadToEnd(*s, 0, 4096) == bytes);
        BOOST_CHECK(!Present(c));
        BOOST_CHECK(Present(d));

        s.reset();
        BOOST_CHECK(!Present(d));
    }

    BOOST_CHECK_EQUAL(CountEntries(wallet::GetTransferDir()), 0u);
}

BOOST_FIXTURE_TEST_CASE(snapshot_source_read_error_deletes_the_rest, TransferCase)
{
    const fs::path a = Snapshot("a", PatternBytes(10));
    const fs::path b = Snapshot("b", PatternBytes(10));

    std::unique_ptr<interfaces::FileSource> s =
        wallet::MakeSnapshotSource({{"a", a, ""}, {"b", b, ""}}, interfaces::MakeConnectionScope());
    BOOST_REQUIRE(s);

    // The first entry's file goes missing, so it cannot be opened.
    fs::remove(a);

    const interfaces::FileChunk chunk = s->read(0, 10);
    BOOST_CHECK(!chunk.error.empty());
    BOOST_CHECK(chunk.data.empty());

    // The transfer is over: the other entry's file is gone, and it is refused.
    BOOST_CHECK(!Present(b));
    BOOST_CHECK(!s->read(1, 10).error.empty());
    BOOST_CHECK_EQUAL(CountEntries(wallet::GetTransferDir()), 0u);
}

BOOST_FIXTURE_TEST_CASE(snapshot_source_lists_an_item_with_nothing_to_send, TransferCase)
{
    const Bytes bytes = BytesOf("wallet bytes");
    const fs::path a = Snapshot("a", bytes);

    std::unique_ptr<interfaces::FileSource> s = wallet::MakeSnapshotSource(
        {{"a", a, ""}, {"settings.json", fs::path(), "no settings file"}}, interfaces::MakeConnectionScope());
    BOOST_REQUIRE(s);

    const std::vector<interfaces::FileEntryInfo> infos = s->entries();
    BOOST_REQUIRE_EQUAL(infos.size(), 2u);
    BOOST_CHECK_EQUAL(infos[1].name, "settings.json");
    BOOST_CHECK_EQUAL(infos[1].size, 0);
    BOOST_CHECK_EQUAL(infos[1].note, "no settings file");

    const interfaces::FileChunk empty = s->read(1, 10);
    BOOST_CHECK_EQUAL(empty.error, "no settings file");
    BOOST_CHECK(empty.data.empty());
    BOOST_CHECK(!s->trailer(1).ok);

    // That does not end the transfer.
    BOOST_CHECK(ReadToEnd(*s, 0, 4096) == bytes);
    BOOST_CHECK(s->trailer(0).ok);
}

// Review Focus 2: what a core that stopped mid-transfer left behind is removed
// at the next start, and a link is removed itself, never what it points to.
BOOST_FIXTURE_TEST_CASE(transfer_dir_cleaned_at_start, TransferCase)
{
    const fs::path tdir = wallet::GetTransferDir();
    const fs::path leftover = tdir / "snap-0123456789abcdef.tmp";
    const fs::path part = tdir / "snap-0123456789abcdef.tmp.0011223344556677.part";
    WriteBytes(leftover, BytesOf("old snapshot"));
    WriteBytes(part, BytesOf("old part"));

    const fs::path target = dir / "target";
    WriteBytes(target, BytesOf("keep"));

    const fs::path sub = tdir / "sub";
    fs::create_directories(sub);
    WriteBytes(sub / "inner", BytesOf("inner"));

#ifndef WIN32
    const fs::path link = tdir / "link";
    const fs::path dir_link = tdir / "dirlink";
    fs::create_symlink(target, link);
    fs::create_directory_symlink(dir, dir_link);
#endif

    wallet::CleanTransferDir();

    BOOST_CHECK(!Present(leftover));
    BOOST_CHECK(!Present(part));
#ifndef WIN32
    BOOST_CHECK(!Present(link));
    BOOST_CHECK(!Present(dir_link));
#endif

    // Nothing the links pointed to, and no subdirectory, was touched.
    BOOST_CHECK(FileBytes(target) == BytesOf("keep"));
    BOOST_CHECK(Present(dir));
    BOOST_CHECK(FileBytes(sub / "inner") == BytesOf("inner"));
    BOOST_CHECK_EQUAL(CountEntries(tdir), 1u);

    // A transfer directory that does not exist is nothing to clean.
    TransferCase::RemoveTransferDir();
    wallet::CleanTransferDir();
    BOOST_CHECK(!Present(GetDataDir() / "transfers"));
}

#ifndef WIN32
BOOST_FIXTURE_TEST_CASE(transfer_dir_link_is_not_followed, TransferCase)
{
    const fs::path elsewhere = dir / "elsewhere";
    fs::create_directories(elsewhere);
    WriteBytes(elsewhere / "file", BytesOf("keep"));

    fs::create_directory_symlink(elsewhere, GetDataDir() / "transfers");

    wallet::CleanTransferDir();

    BOOST_CHECK(FileBytes(elsewhere / "file") == BytesOf("keep"));
}

BOOST_FIXTURE_TEST_CASE(transfer_dir_is_0700, TransferCase)
{
    // Owner-only whatever the umask.
    const mode_t old = ::umask(0);
    const fs::path tdir = wallet::GetTransferDir();
    ::umask(old);

    BOOST_CHECK(tdir == GetDataDir() / "transfers");
    BOOST_CHECK_EQUAL(ModeOf(tdir), 0700u);
}
#endif

BOOST_FIXTURE_TEST_CASE(config_backup_refused_with_attach_group, TransferCase)
{
    // This account's own configuration file.
    const fs::path conf = dir / "own.conf";
    WriteBytes(conf, BytesOf("# transfer_sources_tests\n"));
    grc_test::ForcedArgGuard conf_arg("conf", conf.string());
    BOOST_REQUIRE(GetConfigFile() == conf);

    std::string reason;
    {
        grc_test::ForcedArgGuard group("ipcattachgroup", "x");
        BOOST_CHECK(!wallet::ConfigBackupPermitted(reason));
        BOOST_CHECK(reason.find("settings file") != std::string::npos);
        BOOST_CHECK(reason.find("RPC password") != std::string::npos);
    }

    BOOST_CHECK(wallet::ConfigBackupPermitted(reason));
    BOOST_CHECK(reason.empty());

    // -noipcattachgroup counts as unset, as it does for the IPC listener.
    {
        grc_test::StateGuard settings_guard;
        gArgs.LockSettings([](util::Settings& settings) {
            util::SettingsValue negated;
            negated.setBool(false);
            settings.command_line_options["ipcattachgroup"] = {negated};
        });
        BOOST_REQUIRE(gArgs.IsArgNegated("-ipcattachgroup"));

        BOOST_CHECK(wallet::ConfigBackupPermitted(reason));
        BOOST_CHECK(reason.empty());
    }
}

#ifndef WIN32
BOOST_FIXTURE_TEST_CASE(config_backup_refused_for_another_accounts_conf, TransferCase)
{
    std::string reason;

    // A configuration file that does not exist counts as permitted.
    {
        grc_test::ForcedArgGuard conf_arg("conf", (dir / "missing.conf").string());
        BOOST_CHECK(wallet::ConfigBackupPermitted(reason));
        BOOST_CHECK(reason.empty());
    }

    // /dev/null belongs to root, so it stands for a configuration file the
    // administrator owns. As root that is this account.
    if (::geteuid() == 0) {
        BOOST_TEST_MESSAGE("skipped: running as root, so no file belongs to another account");
        return;
    }

    grc_test::ForcedArgGuard conf_arg("conf", "/dev/null");
    BOOST_CHECK(!wallet::ConfigBackupPermitted(reason));
    BOOST_CHECK(reason.find("settings file") != std::string::npos);
    BOOST_CHECK(reason.find("another account") != std::string::npos);

    // The monolithic build refuses it too, and has no IPC to speak of.
    BOOST_CHECK(reason.find("IPC") == std::string::npos);
}

// A transfer directory that is a symbolic link is refused, so every copy lands
// where CleanTransferDir reaches. Nothing is written through the link.
BOOST_FIXTURE_TEST_CASE(snapshot_refused_into_a_linked_transfer_dir, TransferCase)
{
    const fs::path elsewhere = dir / "elsewhere";
    fs::create_directories(elsewhere);
    const fs::path tdir = GetDataDir() / "transfers";
    fs::create_directory_symlink(elsewhere, tdir);

    const fs::path source = dir / "source.bin";
    WriteBytes(source, BytesOf("source"));

    std::string error;
    BOOST_CHECK(!wallet::SnapshotPlainFile(source, error));
    BOOST_CHECK_MESSAGE(error.find("symbolic link") != std::string::npos, error);

    const grc_test::WalletFilePlaceholder wallet_file;
    grc_test::RequireWalletFileNotInUse();
    error.clear();
    BOOST_CHECK(!wallet::SnapshotWallet(*pwalletMain, error));
    BOOST_CHECK_MESSAGE(error.find("symbolic link") != std::string::npos, error);

    BOOST_CHECK_EQUAL(CountEntries(elsewhere), 0u);

    // The link is left as it is, not replaced or re-tightened.
    BOOST_CHECK(fs::is_symlink(fs::symlink_status(tdir)));
}
#endif

BOOST_FIXTURE_TEST_CASE(snapshot_wallet_copies_the_wallet_file, TransferCase)
{
    const grc_test::WalletFilePlaceholder wallet_file;
    grc_test::RequireWalletFileNotInUse();

    std::string error;
    const std::optional<fs::path> snapshot = wallet::SnapshotWallet(*pwalletMain, error);
    BOOST_REQUIRE_MESSAGE(snapshot, error);
    BOOST_CHECK(snapshot->parent_path() == wallet::GetTransferDir());
    BOOST_CHECK_MESSAGE(IsSnapshotName(*snapshot), snapshot->string());
    BOOST_CHECK(FileBytes(*snapshot) == BytesOf(grc_test::WalletFilePlaceholder::CONTENT));
#ifndef WIN32
    BOOST_CHECK_EQUAL(ModeOf(*snapshot), 0600u);
#endif
    fs::remove(*snapshot);

    // A wallet with no file has nothing to copy.
    CWallet in_memory;
    BOOST_CHECK(!wallet::SnapshotWallet(in_memory, error));
    BOOST_CHECK(error.find("file-backed") != std::string::npos);
    BOOST_CHECK_EQUAL(CountEntries(wallet::GetTransferDir()), 0u);
}

BOOST_FIXTURE_TEST_CASE(snapshot_failures_leave_nothing, TransferCase)
{
    std::string error;

    BOOST_CHECK(!wallet::SnapshotPlainFile(dir / "missing", error));
    BOOST_CHECK(error.find("missing") != std::string::npos);

    error.clear();
    BOOST_CHECK(!wallet::SnapshotPlainFile(dir, error));
    BOOST_CHECK(!error.empty());

    // The mock wallet has no file in the data directory.
    BOOST_REQUIRE(!fs::exists(GetDataDir() / pwalletMain->strWalletFile));
    grc_test::RequireWalletFileNotInUse();
    error.clear();
    BOOST_CHECK(!wallet::SnapshotWallet(*pwalletMain, error));
    BOOST_CHECK(!error.empty());

    BOOST_CHECK_EQUAL(CountEntries(wallet::GetTransferDir()), 0u);
}

BOOST_AUTO_TEST_SUITE_END()
