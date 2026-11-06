// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "crypto/sha256.h"
#include "fs.h"
#include "gridcoin/backup.h"
#include "gridcoin/accrual/snapshot.h"
#include "gridcoin/cpid.h"
#include "interfaces/connection_scope.h"
#include "interfaces/file_transfer.h"
#include "key.h"
#include "key_io.h"
#include "rpc/protocol.h"
#include "support/allocators/secure.h"
#include "test/lone_best_block.h"
#include "test/staking_test_peers.h"
#include "test/state_guard.h"
#include "test/wallet_file_placeholder.h"
#include "uint256.h"
#include "util/system.h"
#include "util/time.h"
#include "wallet/capability.h"
#include "wallet/transfer_sources.h"
#include "wallet/wallet.h"
#include "wallet/walletdump.h"

#include <boost/test/unit_test.hpp>

#include <chrono>
#include <cstdint>
#include <fstream>
#include <future>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifndef WIN32
#include "test/umask_guard.h"

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

using grc_test::EncryptedWallet;

//! A wallet dump with one line, for \p key.
std::string DumpOf(const CKey& key)
{
    return "# A wallet dump with one key in it.\n" + EncodeSecret(key) + " 2026-01-01T00:00:00Z label=transfer\n";
}

//! A FileTransfer capability begun into \p wallet's IPC slot, as a client
//! begins one.
CapabilityId BeginFileTransfer(CWallet& wallet)
{
    BOOST_REQUIRE(wallet.BeginIpcElevation(EncryptedWallet::PASSPHRASE, CapabilityKind::FileTransfer, {}));
    const std::optional<CapabilityId> id = wallet.IpcCapability({});
    BOOST_REQUIRE(id.has_value());
    return *id;
}

//! The end function the wallet interface gives a source or sink: \p id is
//! ended, and taken out of the slot when the slot still holds it.
wallet::EndCapabilityFn EndThroughSlot(CWallet& wallet, CapabilityId id)
{
    return [&wallet, id] { wallet.EndIpcElevationIf(id, {}); };
}

//! Whether capability \p id still lets a FileTransfer scope be opened.
bool CapabilityLive(CWallet& wallet, CapabilityId id)
{
    const CapabilityScope scope(wallet, id, CapabilityKind::FileTransfer);
    return scope.Active();
}

//! The dump a generator made now writes under capability \p id.
std::string GeneratedDump(CWallet& wallet, CapabilityId id)
{
    const CapabilityScope scope(wallet, id, CapabilityKind::FileTransfer);
    BOOST_REQUIRE(scope.Active());

    WalletDumpGenerator generator(wallet);
    std::string all;
    while (generator.Next(all, 4096)) {
    }
    return all;
}

//! An in-memory wallet keeps key pool entries with no database to read them
//! from, so a dump of it (GetAllReserveKeys()) would fail; a dump case on one
//! drops the pool first. A node's wallet is file-backed.
void DropKeyPool(CWallet& wallet)
{
    LOCK(wallet.cs_wallet);
    wallet.setKeyPool.clear();
}

//! The importwallet upload cap, as the console's command table gives it.
uint64_t ImportCap()
{
    const interfaces::ConsoleFileCommand* command = interfaces::FindConsoleFileCommand("importwallet");
    BOOST_REQUIRE(command);
    return command->max_upload_bytes;
}

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

#ifndef WIN32
    // 0600 whatever the umask: the copy sets it through its own descriptor when it creates the
    // file, and nothing changes the mode by the snapshot's name afterwards.
    {
        const grc_test::UmaskGuard narrow(0277);
        const std::optional<fs::path> narrow_snapshot = wallet::SnapshotWallet(*pwalletMain, error);
        BOOST_REQUIRE_MESSAGE(narrow_snapshot, error);
        BOOST_CHECK_EQUAL(ModeOf(*narrow_snapshot), 0600u);
        fs::remove(*narrow_snapshot);
    }
#endif

    // A wallet with no file has nothing to copy.
    CWallet in_memory;
    BOOST_CHECK(!wallet::SnapshotWallet(in_memory, error));
    BOOST_CHECK(error.find("file-backed") != std::string::npos);
    BOOST_CHECK_EQUAL(CountEntries(wallet::GetTransferDir()), 0u);
}

#ifndef WIN32
// A wallet snapshot for a transfer stays strictly owner-only: where the copy cannot keep mode
// 0600 it is refused and removed, rather than kept as a backup in -backupdir would be, and the
// backup directory's once-per-process log line is not spent on it. On a filesystem with POSIX
// modes the snapshot is made 0600. The refusal runs where the transfer directory's files do
// not keep their mode.
BOOST_FIXTURE_TEST_CASE(a_transfer_snapshot_is_owner_only_or_refused, TransferCase)
{
    const grc_test::WalletFilePlaceholder wallet_file;
    grc_test::RequireWalletFileNotInUse();
    GRC::ResetUnrestrictedBackupLogForTesting();

    std::string error;
    const std::optional<fs::path> snapshot = wallet::SnapshotWallet(*pwalletMain, error);
    if (snapshot) {
        BOOST_CHECK_EQUAL(ModeOf(*snapshot), 0600u);
        fs::remove(*snapshot);
    } else {
        BOOST_TEST_MESSAGE("refused: " << error);
        BOOST_CHECK(!error.empty());
        BOOST_CHECK_EQUAL(CountEntries(wallet::GetTransferDir()), 0u);
    }

    // The backup directory's log line is still to come.
    BOOST_CHECK(GRC::LogUnrestrictedBackupOnce("/media/stick/backups", "a check that the line is unspent"));
    GRC::ResetUnrestrictedBackupLogForTesting();
}
#endif

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

// dumpwallet's stream: the text the generator writes, made under the
// capability as it is read, held for the run and ended at the dump's end.
// Nothing is written to the transfer directory.
BOOST_FIXTURE_TEST_CASE(dumpwallet_source_streams_and_ends_capability, TransferCase)
{
    const grc_test::LoneBestBlock tip;
    SetMockTime(1700000000);

    EncryptedWallet w;
    w.UnlockFor(UnlockScope::StakingOnly);
    DropKeyPool(w.wallet);

    const CapabilityId id = BeginFileTransfer(w.wallet);
    const std::string expected = GeneratedDump(w.wallet, id);
    BOOST_CHECK(expected.find(EncodeSecret(w.key) + " ") != std::string::npos);

    int ended = 0;
    const wallet::EndCapabilityFn end = [&] {
        ++ended;
        w.wallet.EndIpcElevationIf(id, {});
    };

    std::unique_ptr<interfaces::FileSource> source =
        wallet::MakeDumpWalletSource(w.wallet, id, end, interfaces::MakeConnectionScope());
    BOOST_REQUIRE(source);

    const std::vector<interfaces::FileEntryInfo> infos = source->entries();
    BOOST_REQUIRE_EQUAL(infos.size(), 1u);
    BOOST_CHECK_EQUAL(infos[0].name, "wallet-dump.txt");
    BOOST_CHECK_EQUAL(infos[0].size, -1);
    BOOST_CHECK_GT(infos[0].size_hint, 0u);
    BOOST_CHECK(infos[0].note.empty());

    // Reads of 100 bytes, less than a key's line: the source keeps the rest of
    // a line for the next read, and no chunk is larger than asked for.
    size_t chunks = 0;
    const Bytes got = ReadToEnd(*source, 0, 100, &chunks);
    BOOST_CHECK(got == BytesOf(expected));
    BOOST_CHECK_GT(chunks, 1u);

    const interfaces::FileTrailer trailer = source->trailer(0);
    BOOST_CHECK(trailer.ok);
    BOOST_CHECK_EQUAL(trailer.size, got.size());
    BOOST_CHECK(trailer.sha256 == Sha256Of(got));

    // Ended once, at the dump's end, and taken out of the slot.
    BOOST_CHECK_EQUAL(ended, 1);
    BOOST_CHECK(!CapabilityLive(w.wallet, id));
    BOOST_CHECK(!w.wallet.IpcCapability({}).has_value());
    BOOST_CHECK(!source->read(0, 100).error.empty());
    BOOST_CHECK_EQUAL(CountEntries(wallet::GetTransferDir()), 0u);

    source.reset();
    BOOST_CHECK_EQUAL(ended, 1);
    BOOST_CHECK(w.wallet.IsUnlockedForStakingOnly());
}

// Without a capability (an unencrypted or fully unlocked wallet needs none) the
// source and the sink rely on the wallet's own CanUseKeys(): an unencrypted
// wallet streams and imports, and one unlocked for staking only is refused.
// Neither has a capability to end, so the end function is never called.
BOOST_FIXTURE_TEST_CASE(transfers_without_a_capability_rely_on_the_wallet, TransferCase)
{
    const grc_test::LoneBestBlock tip;

    int ended = 0;
    const wallet::EndCapabilityFn end = [&ended] { ++ended; };

    CWallet plain;
    CKey own;
    own.MakeNewKey(true);
    {
        LOCK(plain.cs_wallet);
        BOOST_REQUIRE(plain.AddKey(own));
    }

    std::unique_ptr<interfaces::FileSource> source = wallet::MakeDumpWalletSource(plain, std::nullopt, end, nullptr);
    BOOST_REQUIRE(source);
    const Bytes dump = ReadToEnd(*source, 0, 100);
    const std::string text(dump.begin(), dump.end());
    BOOST_CHECK(text.find(EncodeSecret(own) + " ") != std::string::npos);
    BOOST_CHECK(source->trailer(0).ok);
    source.reset();

    CKey fresh;
    fresh.MakeNewKey(true);
    const Bytes data = BytesOf(DumpOf(fresh));

    std::unique_ptr<interfaces::FileSink> sink =
        wallet::MakeImportWalletSink(plain, std::nullopt, end, data.size(), nullptr);
    BOOST_REQUIRE(sink);
    BOOST_REQUIRE(sink->write(data));
    const interfaces::FileSinkResult imported = sink->finish(data.size(), Sha256Of(data));
    BOOST_CHECK_MESSAGE(imported.ok, imported.error);
    BOOST_CHECK(plain.HaveKey(fresh.GetPubKey().GetID()));
    sink.reset();

    EncryptedWallet w;
    w.UnlockFor(UnlockScope::StakingOnly);
    DropKeyPool(w.wallet);

    std::unique_ptr<interfaces::FileSource> refused_source =
        wallet::MakeDumpWalletSource(w.wallet, std::nullopt, end, nullptr);
    BOOST_REQUIRE(refused_source);
    const interfaces::FileChunk chunk = refused_source->read(0, 100);
    BOOST_CHECK_MESSAGE(chunk.error.find("keys are not available") != std::string::npos, chunk.error);
    BOOST_CHECK(chunk.data.empty());
    refused_source.reset();

    std::unique_ptr<interfaces::FileSink> refused_sink =
        wallet::MakeImportWalletSink(w.wallet, std::nullopt, end, data.size(), nullptr);
    BOOST_REQUIRE(refused_sink);
    BOOST_REQUIRE(refused_sink->write(data));
    const interfaces::FileSinkResult refused = refused_sink->finish(data.size(), Sha256Of(data));
    BOOST_CHECK(!refused.ok);
    BOOST_CHECK_MESSAGE(refused.error.find("staking only") != std::string::npos, refused.error);
    BOOST_CHECK(!w.wallet.HaveKey(fresh.GetPubKey().GetID()));
    refused_sink.reset();

    BOOST_CHECK_EQUAL(ended, 0);
}

// The capability ends during a dump (its lifetime, or the client's
// UnlockContext ending it early): the next read is an error with no data, and
// the trailer is refused, so nothing is saved under the client's name.
BOOST_FIXTURE_TEST_CASE(dumpwallet_source_stops_when_capability_ends, TransferCase)
{
    const grc_test::LoneBestBlock tip;

    EncryptedWallet w;
    w.UnlockFor(UnlockScope::StakingOnly);
    DropKeyPool(w.wallet);

    const CapabilityId id = BeginFileTransfer(w.wallet);
    std::unique_ptr<interfaces::FileSource> source =
        wallet::MakeDumpWalletSource(w.wallet, id, EndThroughSlot(w.wallet, id), interfaces::MakeConnectionScope());
    BOOST_REQUIRE(source);

    const interfaces::FileChunk first = source->read(0, 100);
    BOOST_REQUIRE_MESSAGE(first.error.empty(), first.error);
    BOOST_CHECK(!first.data.empty());
    BOOST_CHECK(!first.eof);

    // The client ends its capability early, as its UnlockContext going away
    // does, which also empties the slot.
    w.wallet.EndIpcElevation({});

    const interfaces::FileChunk after = source->read(0, 100);
    BOOST_CHECK(!after.error.empty());
    BOOST_CHECK(after.data.empty());
    BOOST_CHECK(!after.eof);
    BOOST_CHECK(!source->trailer(0).ok);

    // A capability begun since does not bring the stream back.
    const CapabilityId again = BeginFileTransfer(w.wallet);
    BOOST_CHECK(!source->read(0, 100).error.empty());

    w.wallet.EndIpcElevation({});
    BOOST_CHECK(!CapabilityLive(w.wallet, again));
    BOOST_CHECK(w.wallet.IsUnlockedForStakingOnly());

    // An unencrypted wallet can use its keys with or without a capability, so
    // there what stops the stream is the source's own check of it, with lines
    // already generated waiting for the next read.
    CWallet plain;
    {
        CKey key;
        key.MakeNewKey(true);
        LOCK(plain.cs_wallet);
        BOOST_REQUIRE(plain.AddKey(key));
    }
    BOOST_REQUIRE(plain.BeginIpcElevation(SecureString{}, CapabilityKind::FileTransfer, {}));
    const std::optional<CapabilityId> plain_id = plain.IpcCapability({});
    BOOST_REQUIRE(plain_id.has_value());

    std::unique_ptr<interfaces::FileSource> plain_source =
        wallet::MakeDumpWalletSource(plain, *plain_id, EndThroughSlot(plain, *plain_id), nullptr);
    BOOST_REQUIRE(plain_source);
    const interfaces::FileChunk plain_first = plain_source->read(0, 10);
    BOOST_REQUIRE_MESSAGE(plain_first.error.empty(), plain_first.error);
    BOOST_REQUIRE_EQUAL(plain_first.data.size(), 10u);

    plain.EndIpcElevation({});

    const interfaces::FileChunk plain_after = plain_source->read(0, 100);
    BOOST_CHECK(!plain_after.error.empty());
    BOOST_CHECK(plain_after.data.empty());
    plain.EndIpcElevation({});
}

// The connection's close, cancel() and destruction each end the capability,
// and a source made on a connection that has already closed ends it at once.
BOOST_FIXTURE_TEST_CASE(dumpwallet_source_close_ends_capability, TransferCase)
{
    const grc_test::LoneBestBlock tip;

    EncryptedWallet w;
    w.UnlockFor(UnlockScope::StakingOnly);
    DropKeyPool(w.wallet);

    const auto scope = interfaces::MakeConnectionScope();

    const CapabilityId id = BeginFileTransfer(w.wallet);
    std::unique_ptr<interfaces::FileSource> source =
        wallet::MakeDumpWalletSource(w.wallet, id, EndThroughSlot(w.wallet, id), scope);
    BOOST_REQUIRE(source);
    BOOST_REQUIRE(source->read(0, 100).error.empty());

    scope->close();
    BOOST_CHECK(!CapabilityLive(w.wallet, id));
    BOOST_CHECK(!source->read(0, 100).error.empty());

    const CapabilityId late_id = BeginFileTransfer(w.wallet);
    std::unique_ptr<interfaces::FileSource> late =
        wallet::MakeDumpWalletSource(w.wallet, late_id, EndThroughSlot(w.wallet, late_id), scope);
    BOOST_REQUIRE(late);
    BOOST_CHECK(!CapabilityLive(w.wallet, late_id));
    BOOST_CHECK(!late->read(0, 100).error.empty());

    const CapabilityId cancelled_id = BeginFileTransfer(w.wallet);
    std::unique_ptr<interfaces::FileSource> cancelled =
        wallet::MakeDumpWalletSource(w.wallet, cancelled_id, EndThroughSlot(w.wallet, cancelled_id), nullptr);
    BOOST_REQUIRE(cancelled);
    cancelled->cancel();
    BOOST_CHECK(!CapabilityLive(w.wallet, cancelled_id));
    BOOST_CHECK(!cancelled->read(0, 100).error.empty());

    const CapabilityId dropped_id = BeginFileTransfer(w.wallet);
    {
        std::unique_ptr<interfaces::FileSource> dropped =
            wallet::MakeDumpWalletSource(w.wallet, dropped_id, EndThroughSlot(w.wallet, dropped_id), nullptr);
        BOOST_REQUIRE(dropped);
        BOOST_REQUIRE(dropped->read(0, 100).error.empty());
        BOOST_CHECK(CapabilityLive(w.wallet, dropped_id));
    }
    BOOST_CHECK(!CapabilityLive(w.wallet, dropped_id));

    w.wallet.EndIpcElevation({});
    BOOST_CHECK(w.wallet.IsUnlockedForStakingOnly());
}

// Without a capability the source relies on the wallet's own CanUseKeys(), and
// asks it on every read, so lines it generated while the keys were usable are
// not handed out once they no longer are: a fully unlocked wallet is locked
// between two short reads.
BOOST_FIXTURE_TEST_CASE(dumpwallet_source_without_a_capability_stops_when_the_keys_go, TransferCase)
{
    const grc_test::LoneBestBlock tip;

    EncryptedWallet w;
    w.UnlockFor(UnlockScope::Full);
    DropKeyPool(w.wallet);

    std::unique_ptr<interfaces::FileSource> source = wallet::MakeDumpWalletSource(w.wallet, std::nullopt, {}, nullptr);
    BOOST_REQUIRE(source);

    // Less than the first line, so the rest of it waits for the next read.
    const interfaces::FileChunk first = source->read(0, 10);
    BOOST_REQUIRE_MESSAGE(first.error.empty(), first.error);
    BOOST_REQUIRE_EQUAL(first.data.size(), 10u);

    w.wallet.Lock();

    const interfaces::FileChunk after = source->read(0, 10);
    BOOST_CHECK_MESSAGE(after.error.find("keys are not available") != std::string::npos, after.error);
    BOOST_CHECK(after.data.empty());
    BOOST_CHECK(!source->trailer(0).ok);
}

// importwallet's upload: held in memory and imported at finish under the
// capability, which ends with it. A wrong SHA-256, more data than announced,
// or an announced size above the cap imports nothing.
BOOST_FIXTURE_TEST_CASE(import_sink_rejects_oversize_and_bad_hash, TransferCase)
{
    const grc_test::LoneBestBlock tip;

    EncryptedWallet w;
    w.UnlockFor(UnlockScope::StakingOnly);

    CKey fresh;
    fresh.MakeNewKey(true);
    const CKeyID fresh_id = fresh.GetPubKey().GetID();
    const Bytes data = BytesOf(DumpOf(fresh));

    // A wrong SHA-256 at finish.
    {
        const CapabilityId id = BeginFileTransfer(w.wallet);
        std::unique_ptr<interfaces::FileSink> sink =
            wallet::MakeImportWalletSink(w.wallet, id, EndThroughSlot(w.wallet, id),
                                         data.size(), interfaces::MakeConnectionScope());
        BOOST_REQUIRE(sink);
        BOOST_REQUIRE(sink->write(data));

        uint256 wrong = Sha256Of(data);
        *wrong.begin() ^= 0x01;
        const interfaces::FileSinkResult result = sink->finish(data.size(), wrong);
        BOOST_CHECK(!result.ok);
        BOOST_CHECK(!result.error.empty());
        BOOST_CHECK(!w.wallet.HaveKey(fresh_id));
        BOOST_CHECK(!CapabilityLive(w.wallet, id));
    }

    // More data than announced: the write is refused, and so is the finish.
    {
        const CapabilityId id = BeginFileTransfer(w.wallet);
        std::unique_ptr<interfaces::FileSink> sink =
            wallet::MakeImportWalletSink(w.wallet, id, EndThroughSlot(w.wallet, id), data.size() - 1, nullptr);
        BOOST_REQUIRE(sink);
        BOOST_CHECK(!sink->write(data));
        BOOST_CHECK(!sink->finish(data.size(), Sha256Of(data)).ok);
        BOOST_CHECK(!w.wallet.HaveKey(fresh_id));
        BOOST_CHECK(!CapabilityLive(w.wallet, id));
    }

    // An announced size above the cap: refused before any byte is taken.
    {
        const CapabilityId id = BeginFileTransfer(w.wallet);
        std::unique_ptr<interfaces::FileSink> sink =
            wallet::MakeImportWalletSink(w.wallet, id, EndThroughSlot(w.wallet, id), ImportCap() + 1, nullptr);
        BOOST_REQUIRE(sink);
        BOOST_CHECK(!sink->write(data));
        const interfaces::FileSinkResult result = sink->finish(data.size(), Sha256Of(data));
        BOOST_CHECK(!result.ok);
        BOOST_CHECK_MESSAGE(result.error.find("too large") != std::string::npos, result.error);
        BOOST_CHECK(!CapabilityLive(w.wallet, id));
    }

    // Cancelled part way.
    {
        const CapabilityId id = BeginFileTransfer(w.wallet);
        std::unique_ptr<interfaces::FileSink> sink =
            wallet::MakeImportWalletSink(w.wallet, id, EndThroughSlot(w.wallet, id), data.size(), nullptr);
        BOOST_REQUIRE(sink);
        BOOST_REQUIRE(sink->write(Bytes(data.begin(), data.begin() + 10)));
        sink->cancel();
        BOOST_CHECK(!sink->write(Bytes(data.begin() + 10, data.end())));
        BOOST_CHECK(!sink->finish(data.size(), Sha256Of(data)).ok);
        BOOST_CHECK(!CapabilityLive(w.wallet, id));
    }

    // An unencrypted wallet can use its keys with or without a capability, so
    // there what refuses an import whose capability has ended is the sink's
    // own check of it.
    {
        CWallet plain;
        BOOST_REQUIRE(plain.BeginIpcElevation(SecureString{}, CapabilityKind::FileTransfer, {}));
        const std::optional<CapabilityId> plain_id = plain.IpcCapability({});
        BOOST_REQUIRE(plain_id.has_value());

        std::unique_ptr<interfaces::FileSink> sink =
            wallet::MakeImportWalletSink(plain, *plain_id, EndThroughSlot(plain, *plain_id), data.size(), nullptr);
        BOOST_REQUIRE(sink);
        BOOST_REQUIRE(sink->write(data));

        plain.EndIpcElevation({});

        const interfaces::FileSinkResult result = sink->finish(data.size(), Sha256Of(data));
        BOOST_CHECK(!result.ok);
        BOOST_CHECK_MESSAGE(result.error.find("capability") != std::string::npos, result.error);
        BOOST_CHECK(!plain.HaveKey(fresh_id));
        plain.EndIpcElevation({});
    }

    // In two pieces with the right SHA-256: imported, and the capability ended.
    {
        const CapabilityId id = BeginFileTransfer(w.wallet);
        std::unique_ptr<interfaces::FileSink> sink =
            wallet::MakeImportWalletSink(w.wallet, id, EndThroughSlot(w.wallet, id),
                                         data.size(), interfaces::MakeConnectionScope());
        BOOST_REQUIRE(sink);
        BOOST_REQUIRE(sink->write(Bytes(data.begin(), data.begin() + 10)));
        BOOST_REQUIRE(sink->write(Bytes(data.begin() + 10, data.end())));

        const interfaces::FileSinkResult result = sink->finish(data.size(), Sha256Of(data));
        BOOST_CHECK_MESSAGE(result.ok, result.error);
        BOOST_CHECK(w.wallet.HaveKey(fresh_id));
        BOOST_CHECK(!CapabilityLive(w.wallet, id));

        // The sink is done.
        BOOST_CHECK(!sink->write(data));
    }

    // The connection closes during an upload: the capability ends, and the
    // finish imports nothing.
    {
        CKey other;
        other.MakeNewKey(true);
        const Bytes other_data = BytesOf(DumpOf(other));

        const auto scope = interfaces::MakeConnectionScope();
        const CapabilityId id = BeginFileTransfer(w.wallet);
        std::unique_ptr<interfaces::FileSink> sink =
            wallet::MakeImportWalletSink(w.wallet, id, EndThroughSlot(w.wallet, id), other_data.size(), scope);
        BOOST_REQUIRE(sink);
        BOOST_REQUIRE(sink->write(other_data));

        scope->close();
        BOOST_CHECK(!CapabilityLive(w.wallet, id));
        BOOST_CHECK(!sink->finish(other_data.size(), Sha256Of(other_data)).ok);
        BOOST_CHECK(!w.wallet.HaveKey(other.GetPubKey().GetID()));
    }

    w.wallet.EndIpcElevation({});
    BOOST_CHECK(w.wallet.IsUnlockedForStakingOnly());
}

// parseaccrualsnapshotfile's upload: a core temporary file, which the RPC reads
// at finish, and which is gone on every path.
BOOST_FIXTURE_TEST_CASE(rpc_file_sink_parses_uploaded_snapshot_and_deletes_temp, TransferCase)
{
    const fs::path local = dir / "5000000";
    {
        GRC::AccrualSnapshotWriter writer(local);
        writer.WriteHeader(5000000);
        writer.WriteRecord(GRC::Cpid::Parse("f5d8234352e5a5ae3915debba7258294"), 12345);
    }
    const Bytes bytes = FileBytes(local);
    BOOST_REQUIRE(!bytes.empty());

    const uint64_t max = 256 * 1024 * 1024;

    {
        std::unique_ptr<interfaces::FileSink> sink = wallet::MakeRpcFileSink(
            "parseaccrualsnapshotfile", {}, 0, bytes.size(), max, interfaces::MakeConnectionScope());
        BOOST_REQUIRE(sink);

        BOOST_REQUIRE(sink->write(Bytes(bytes.begin(), bytes.begin() + 5)));
        BOOST_CHECK_EQUAL(CountEntries(wallet::GetTransferDir()), 1u);
        BOOST_REQUIRE(sink->write(Bytes(bytes.begin() + 5, bytes.end())));

        const interfaces::FileSinkResult result = sink->finish(bytes.size(), Sha256Of(bytes));
        BOOST_CHECK_MESSAGE(result.ok, result.error);
        BOOST_CHECK_MESSAGE(result.output.find("\"height\": 5000000") != std::string::npos, result.output);
        BOOST_CHECK_EQUAL(CountEntries(wallet::GetTransferDir()), 0u);
    }

    // Too few bytes for a snapshot's header: the RPC's error, and nothing left.
    {
        const Bytes junk = BytesOf("abc");
        std::unique_ptr<interfaces::FileSink> sink =
            wallet::MakeRpcFileSink("parseaccrualsnapshotfile", {}, 0, junk.size(), max, nullptr);
        BOOST_REQUIRE(sink);
        BOOST_REQUIRE(sink->write(junk));
        const interfaces::FileSinkResult result = sink->finish(junk.size(), Sha256Of(junk));
        BOOST_CHECK(!result.ok);
        BOOST_CHECK(!result.error.empty());
        BOOST_CHECK_EQUAL(CountEntries(wallet::GetTransferDir()), 0u);
    }

    // A command that throws something that is no exception: its error, and
    // nothing left.
    {
        const wallet::RpcRunner other = [](const std::string&, const std::vector<std::string>&) -> std::string {
            throw 42;
        };
        std::unique_ptr<interfaces::FileSink> sink =
            wallet::MakeRpcFileSink("parseaccrualsnapshotfile", {}, 0, bytes.size(), max, nullptr, other);
        BOOST_REQUIRE(sink);
        BOOST_REQUIRE(sink->write(bytes));
        const interfaces::FileSinkResult result = sink->finish(bytes.size(), Sha256Of(bytes));
        BOOST_CHECK(!result.ok);
        BOOST_CHECK(!result.error.empty());
        BOOST_CHECK_EQUAL(CountEntries(wallet::GetTransferDir()), 0u);
    }

    // More than announced, and an announced size above the cap: refused.
    {
        std::unique_ptr<interfaces::FileSink> sink =
            wallet::MakeRpcFileSink("parseaccrualsnapshotfile", {}, 0, bytes.size() - 1, max, nullptr);
        BOOST_REQUIRE(sink);
        BOOST_CHECK(!sink->write(bytes));
        BOOST_CHECK(!sink->finish(bytes.size(), Sha256Of(bytes)).ok);
        BOOST_CHECK_EQUAL(CountEntries(wallet::GetTransferDir()), 0u);

        std::unique_ptr<interfaces::FileSink> over =
            wallet::MakeRpcFileSink("parseaccrualsnapshotfile", {}, 0, 10, 9, nullptr);
        BOOST_REQUIRE(over);
        BOOST_CHECK(!over->write(BytesOf("123")));
        BOOST_CHECK(!over->finish(3, Sha256Of(BytesOf("123"))).ok);
    }

    // The connection closes, cancel(), or the sink goes away part way.
    {
        const auto scope = interfaces::MakeConnectionScope();
        std::unique_ptr<interfaces::FileSink> sink =
            wallet::MakeRpcFileSink("parseaccrualsnapshotfile", {}, 0, bytes.size(), max, scope);
        BOOST_REQUIRE(sink);
        BOOST_REQUIRE(sink->write(Bytes(bytes.begin(), bytes.begin() + 5)));
        scope->close();
        BOOST_CHECK_EQUAL(CountEntries(wallet::GetTransferDir()), 0u);
        BOOST_CHECK(!sink->write(Bytes(bytes.begin() + 5, bytes.end())));

        std::unique_ptr<interfaces::FileSink> cancelled =
            wallet::MakeRpcFileSink("parseaccrualsnapshotfile", {}, 0, bytes.size(), max, nullptr);
        BOOST_REQUIRE(cancelled);
        BOOST_REQUIRE(cancelled->write(Bytes(bytes.begin(), bytes.begin() + 5)));
        cancelled->cancel();
        BOOST_CHECK_EQUAL(CountEntries(wallet::GetTransferDir()), 0u);

        {
            std::unique_ptr<interfaces::FileSink> dropped =
                wallet::MakeRpcFileSink("parseaccrualsnapshotfile", {}, 0, bytes.size(), max, nullptr);
            BOOST_REQUIRE(dropped);
            BOOST_REQUIRE(dropped->write(Bytes(bytes.begin(), bytes.begin() + 5)));
            BOOST_CHECK_EQUAL(CountEntries(wallet::GetTransferDir()), 1u);
        }
        BOOST_CHECK_EQUAL(CountEntries(wallet::GetTransferDir()), 0u);
    }

    // The file it was sent from is untouched.
    BOOST_CHECK(FileBytes(local) == bytes);
}

// dumpcontracts' download: the command writes a fresh file the core names in
// its transfer directory, which is streamed with the reply as its note and
// deleted.
BOOST_FIXTURE_TEST_CASE(rpc_output_source_streams_the_produced_file, TransferCase)
{
    std::vector<std::vector<std::string>> calls;
    const wallet::RpcRunner runner = [&calls](const std::string& method, const std::vector<std::string>& args) {
        BOOST_CHECK_EQUAL(method, "dumpcontracts");
        calls.push_back(args);
        BOOST_REQUIRE_GE(args.size(), 2u);

        const fs::path path(args[1]);
        BOOST_CHECK(path.parent_path() == wallet::GetTransferDir());
        BOOST_CHECK(!Present(path));

        WriteBytes(path, BytesOf("abc\n"));
        return std::string("{\"n\":1}");
    };

    std::string error;
    std::unique_ptr<interfaces::FileSource> source = wallet::MakeRpcOutputSource(
        "dumpcontracts", {"beacon"}, 1, interfaces::MakeConnectionScope(), error, runner);
    BOOST_REQUIRE_MESSAGE(source, error);
    BOOST_CHECK(error.empty());

    BOOST_REQUIRE_EQUAL(calls.size(), 1u);
    BOOST_REQUIRE_EQUAL(calls[0].size(), 2u);
    BOOST_CHECK_EQUAL(calls[0][0], "beacon");

    const std::vector<interfaces::FileEntryInfo> infos = source->entries();
    BOOST_REQUIRE_EQUAL(infos.size(), 1u);
    BOOST_CHECK_EQUAL(infos[0].note, "{\"n\":1}");
    BOOST_CHECK_EQUAL(infos[0].size, 4);

    BOOST_CHECK(ReadToEnd(*source, 0, 100) == BytesOf("abc\n"));
    BOOST_CHECK(source->trailer(0).ok);
    BOOST_CHECK_EQUAL(CountEntries(wallet::GetTransferDir()), 0u);

    // Arguments after the file stay after it.
    source = wallet::MakeRpcOutputSource("dumpcontracts", {"beacon", "true"}, 1, nullptr, error, runner);
    BOOST_REQUIRE_MESSAGE(source, error);
    BOOST_REQUIRE_EQUAL(calls.size(), 2u);
    BOOST_REQUIRE_EQUAL(calls[1].size(), 3u);
    BOOST_CHECK_EQUAL(calls[1][0], "beacon");
    BOOST_CHECK_EQUAL(calls[1][2], "true");
    source.reset();
    BOOST_CHECK_EQUAL(CountEntries(wallet::GetTransferDir()), 0u);
}

// A command that throws, part way through its file or before it, or that
// writes no file: no source, the error said, nothing left behind.
BOOST_FIXTURE_TEST_CASE(rpc_output_source_runner_throw_leaves_nothing, TransferCase)
{
    std::string error;

    const wallet::RpcRunner partial_then_rpc_error = [](const std::string&,
                                                        const std::vector<std::string>& args) -> std::string {
        WriteBytes(fs::path(args[1]), BytesOf("half a fi"));
        throw JSONRPCError(RPC_MISC_ERROR, "the command failed");
    };
    BOOST_CHECK(!wallet::MakeRpcOutputSource("dumpcontracts", {"*"}, 1, nullptr, error, partial_then_rpc_error));
    BOOST_CHECK_MESSAGE(error.find("the command failed") != std::string::npos, error);
    BOOST_CHECK_EQUAL(CountEntries(wallet::GetTransferDir()), 0u);

    error.clear();
    const wallet::RpcRunner partial_then_exception = [](const std::string&,
                                                        const std::vector<std::string>& args) -> std::string {
        WriteBytes(fs::path(args[1]), BytesOf("half a fi"));
        throw std::runtime_error("the disk went away");
    };
    BOOST_CHECK(!wallet::MakeRpcOutputSource("dumpcontracts", {"*"}, 1, nullptr, error, partial_then_exception));
    BOOST_CHECK_MESSAGE(error.find("the disk went away") != std::string::npos, error);
    BOOST_CHECK_EQUAL(CountEntries(wallet::GetTransferDir()), 0u);

    // Something thrown that is no exception at all.
    error.clear();
    const wallet::RpcRunner partial_then_other = [](const std::string&,
                                                    const std::vector<std::string>& args) -> std::string {
        WriteBytes(fs::path(args[1]), BytesOf("half a fi"));
        throw 42;
    };
    BOOST_CHECK(!wallet::MakeRpcOutputSource("dumpcontracts", {"*"}, 1, nullptr, error, partial_then_other));
    BOOST_CHECK(!error.empty());
    BOOST_CHECK_EQUAL(CountEntries(wallet::GetTransferDir()), 0u);

    error.clear();
    const wallet::RpcRunner no_file = [](const std::string&, const std::vector<std::string>&) {
        return std::string("{}");
    };
    BOOST_CHECK(!wallet::MakeRpcOutputSource("dumpcontracts", {"*"}, 1, nullptr, error, no_file));
    BOOST_CHECK(!error.empty());
    BOOST_CHECK_EQUAL(CountEntries(wallet::GetTransferDir()), 0u);

    // Too few arguments for the file's place: refused before the command runs.
    error.clear();
    bool ran = false;
    const wallet::RpcRunner never = [&ran](const std::string&, const std::vector<std::string>&) {
        ran = true;
        return std::string();
    };
    BOOST_CHECK(!wallet::MakeRpcOutputSource("dumpcontracts", {}, 1, nullptr, error, never));
    BOOST_CHECK(!ran);
    BOOST_CHECK(!error.empty());
}

BOOST_AUTO_TEST_SUITE_END()
