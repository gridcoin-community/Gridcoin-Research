// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "wallet/transfer_sources.h"

#include "crypto/sha256.h"
#include "gridcoin/backup.h"
#include "interfaces/connection_scope.h"
#include "logging.h"
#include "random.h"
#include "rpc/client.h"
#include "rpc/server.h"
#include "span.h"
#include "support/allocators/secure.h"
#include "support/cleanse.h"
#include "sync.h"
#include "tinyformat.h"
#include "uint256.h"
#include "util/dir_permissions.h"
#include "util/fsmodes.h"
#include "util/strencodings.h"
#include "util/syserror.h"
#include "util/system.h"
#include "util/transfer_file.h"
#include "util/winservice.h"
#include "wallet/wallet.h"
#include "wallet/walletdump.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <istream>
#include <optional>
#include <stdexcept>
#include <streambuf>
#include <utility>

#include <univalue.h>

#ifndef WIN32
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace wallet {

namespace {

const char* const TRANSFER_DIR_NAME = "transfers";

//! With -ipcattachgroup the core serves a GUI of another account over IPC.
const char* const CONFIG_BACKUP_REFUSED_ATTACH_GROUP =
    "The configuration file belongs to the administrator in this mode and holds the RPC password, so it is not "
    "sent over IPC. Back up the settings file instead.";

#ifndef WIN32
//! Worded to be true in both builds, since the monolithic one has no IPC.
const char* const CONFIG_BACKUP_REFUSED_OWNER =
    "The configuration file belongs to another account and holds the RPC password, so it is not included in the "
    "backup. Back up the settings file instead.";
#endif

//! The transfer directory, created owner-only when missing. A symbolic link
//! there is refused, so every copy lands where CleanTransferDir() reaches;
//! nothing about it is changed.
bool PrepareTransferDir(fs::path& dir, std::string& error)
{
    dir = GetDataDir() / TRANSFER_DIR_NAME;

    boost::system::error_code ec;
    if (fs::is_symlink(fs::symlink_status(dir, ec))) {
        error = strprintf("the transfer directory %s is a symbolic link", dir.string());
        return false;
    }

    return util::CreateOwnerOnlyDirectory(dir, error);
}

//! A fresh "<prefix><16 hex>.tmp" in dir.
fs::path NewTransferPath(const fs::path& dir, const std::string& prefix)
{
    unsigned char rnd[8];
    GetRandBytes(Span<unsigned char>(rnd, sizeof(rnd)));

    return dir / (prefix + HexStr(Span<const unsigned char>(rnd, sizeof(rnd))) + ".tmp");
}

//! A fresh "snap-<16 hex>.tmp" in dir.
fs::path NewSnapshotPath(const fs::path& dir)
{
    return NewTransferPath(dir, "snap-");
}

void RemoveFile(const fs::path& path)
{
    boost::system::error_code ec;
    fs::remove(path, ec);

    if (ec) {
        LogPrintf("WARNING: %s: could not remove %s: %s", __func__, path.string(), ec.message());
    }
}

interfaces::FileChunk ErrorChunk(std::string error)
{
    interfaces::FileChunk chunk;
    chunk.error = std::move(error);
    return chunk;
}

interfaces::FileTrailer ErrorTrailer(std::string error)
{
    interfaces::FileTrailer trailer;
    trailer.error = std::move(error);
    return trailer;
}

//! Streams entry files the core made in its transfer directory.
//!
//! What m_mutex guards, and who owns each file:
//!  - An entry's file is claimed for removal at most once (Entry::removed),
//!    under the mutex, and removed by whoever claimed it, after the mutex is
//!    released. So no file I/O happens under the mutex, which the connection's
//!    close hook takes on the IPC event-loop thread.
//!  - A read marks itself (m_in_read) and then opens, reads and hashes the
//!    current entry's file with the mutex released. While it is marked it owns
//!    the open file and the running hash, and nothing else closes the file or
//!    claims that entry's file.
//!  - cancel(), the close hook, a read error and the destructor stop the source:
//!    every later call is refused, the open file is closed unless a read owns
//!    it, and every file not being read is claimed and removed at once. A read
//!    that finds the source stopped when it takes the mutex again closes the
//!    file and removes it.
//!  - A read that reaches an entry's end closes the file, finalises the
//!    trailer, and removes the file.
class SnapshotSource final : public interfaces::FileSource
{
public:
    SnapshotSource(std::vector<SnapshotEntry> entries, std::shared_ptr<interfaces::ConnectionScope> scope);
    ~SnapshotSource() override;

    SnapshotSource(const SnapshotSource&) = delete;
    SnapshotSource& operator=(const SnapshotSource&) = delete;

    std::vector<interfaces::FileEntryInfo> entries() override LOCKS_EXCLUDED(m_mutex);
    interfaces::FileChunk read(uint32_t entry, uint32_t max_bytes) override LOCKS_EXCLUDED(m_mutex);
    interfaces::FileTrailer trailer(uint32_t entry) override LOCKS_EXCLUDED(m_mutex);
    void cancel() override LOCKS_EXCLUDED(m_mutex);

private:
    struct Entry
    {
        SnapshotEntry snapshot;
        int64_t size{-1};
        //! Read to its end; trailer holds the result.
        bool done{false};
        //! The file was claimed for removal, or there never was one.
        bool removed{false};
        interfaces::FileTrailer trailer{};
    };

    //! What a caller took from under the mutex, to close and remove outside it.
    struct Release
    {
        std::FILE* file{nullptr};
        std::vector<fs::path> paths{};
    };

    //! Stop the source for \p reason (the first reason given is kept).
    void Stop(const std::string& reason) LOCKS_EXCLUDED(m_mutex);

    //! Mark the source stopped and claim the open file and every entry file,
    //! except what a read in progress owns.
    Release StopLocked(const std::string& reason) EXCLUSIVE_LOCKS_REQUIRED(m_mutex);

    //! Take the open file and claim \p index's file.
    Release ClaimLocked(uint32_t index) EXCLUSIVE_LOCKS_REQUIRED(m_mutex);

    //! Close and remove what was claimed. Called without the mutex.
    static void Finish(Release release);

    Mutex m_mutex;
    std::vector<Entry> m_entries GUARDED_BY(m_mutex){};
    //! The entry a read has begun and not finished.
    std::optional<uint32_t> m_current GUARDED_BY(m_mutex){};
    //! The current entry's file, while no read owns it.
    std::FILE* m_file GUARDED_BY(m_mutex){nullptr};
    uint64_t m_offset GUARDED_BY(m_mutex){0};
    //! The current entry's hash so far. A read copies it out and hashes its
    //! chunk with the mutex released; nothing else touches it while the read is
    //! marked, and the read stores it back.
    CSHA256 m_hasher GUARDED_BY(m_mutex){};
    bool m_in_read GUARDED_BY(m_mutex){false};
    bool m_stopped GUARDED_BY(m_mutex){false};
    std::string m_stop_reason GUARDED_BY(m_mutex){};

    //! Set in the constructor and only read after it.
    const std::shared_ptr<interfaces::ConnectionScope> m_scope;
    interfaces::ConnectionScope::HookId m_hook{0};
};

SnapshotSource::SnapshotSource(std::vector<SnapshotEntry> entries, std::shared_ptr<interfaces::ConnectionScope> scope)
    : m_scope(std::move(scope))
{
    // Measured before the mutex is taken, since no file I/O happens under it.
    std::vector<Entry> measured;
    measured.reserve(entries.size());

    for (SnapshotEntry& snapshot : entries) {
        Entry entry;

        if (snapshot.path.empty()) {
            entry.size = 0;
            entry.removed = true;
        } else {
            boost::system::error_code ec;
            const uintmax_t size = fs::file_size(snapshot.path, ec);
            // A snapshot is far below 2^63 bytes; -1 only if it cannot be measured.
            entry.size = ec ? -1 : static_cast<int64_t>(size);
        }

        entry.snapshot = std::move(snapshot);
        measured.push_back(std::move(entry));
    }

    WITH_LOCK(m_mutex, m_entries = std::move(measured));

    // Last, once every member is in place: on a scope that has already closed
    // the hook runs here, at once.
    if (m_scope) {
        m_hook = m_scope->addCloseHook([this] { Stop("the IPC connection closed"); });
    }
}

SnapshotSource::~SnapshotSource()
{
    // First, so the hook cannot run on a source that is going away. This waits
    // for a hook that is running.
    if (m_scope && m_hook != 0) {
        m_scope->removeCloseHook(m_hook);
    }

    Stop("the transfer has ended");
}

std::vector<interfaces::FileEntryInfo> SnapshotSource::entries()
{
    LOCK(m_mutex);

    std::vector<interfaces::FileEntryInfo> infos;
    infos.reserve(m_entries.size());

    for (const Entry& entry : m_entries) {
        interfaces::FileEntryInfo info;
        info.name = entry.snapshot.name;
        info.size = entry.size;
        info.size_hint = entry.size > 0 ? static_cast<uint64_t>(entry.size) : 0;
        info.note = entry.snapshot.note;
        infos.push_back(std::move(info));
    }

    return infos;
}

interfaces::FileChunk SnapshotSource::read(uint32_t entry, uint32_t max_bytes)
{
    const size_t want = std::min(max_bytes, interfaces::FILE_TRANSFER_CHUNK_BYTES);

    std::FILE* file{nullptr};
    fs::path path;
    std::string name;
    CSHA256 hasher;

    {
        LOCK(m_mutex);

        if (m_stopped) return ErrorChunk(m_stop_reason);
        if (m_in_read) return ErrorChunk("another read of this transfer is in progress");
        if (entry >= m_entries.size()) return ErrorChunk(strprintf("the transfer has no file %u", entry));

        const Entry& e = m_entries[entry];

        if (e.done) return ErrorChunk(strprintf("%s has already been sent", e.snapshot.name));
        if (m_current && *m_current != entry) {
            return ErrorChunk(strprintf("%s is still being sent", m_entries[*m_current].snapshot.name));
        }
        if (e.snapshot.path.empty()) {
            return ErrorChunk(e.snapshot.note.empty() ? strprintf("nothing to send for %s", e.snapshot.name)
                                                      : e.snapshot.note);
        }
        if (want == 0) return ErrorChunk("a read must ask for at least one byte");

        if (!m_current) {
            m_current = entry;
            m_offset = 0;
            m_hasher.Reset();
        }

        // Null on the entry's first read, which opens the file below.
        file = std::exchange(m_file, nullptr);
        hasher = m_hasher;
        path = e.snapshot.path;
        name = e.snapshot.name;
        m_in_read = true;
    }

    // Opened, read and hashed with the mutex released, so the close hook never
    // waits for file I/O or a hash. m_in_read tells it the file is in use here,
    // so it leaves the file to this call.
    std::string io_error;
    std::vector<unsigned char> data;
    bool eof = false;
    uint256 digest;

    if (!file) {
        file = fsbridge::fopen(path, "rb");
        if (!file) io_error = strprintf("the copy of %s could not be opened: %s", name, SysErrorString(errno));
    }

    if (file) {
        data.resize(want);
        const size_t got = std::fread(data.data(), 1, want, file);
        data.resize(got);

        if (got < want) {
            if (std::ferror(file)) {
                io_error = strprintf("the copy of %s could not be read", name);
            } else {
                eof = true;
            }
        }

        if (io_error.empty()) {
            hasher.Write(data.data(), data.size());
            if (eof) hasher.Finalize(digest.begin());
        }
    }

    interfaces::FileChunk chunk;
    Release release;

    {
        LOCK(m_mutex);

        m_in_read = false;
        m_file = file;

        if (m_stopped) {
            // Stopped while this read ran, which left the file to it.
            chunk.error = m_stop_reason;
            release = ClaimLocked(entry);
        } else if (!io_error.empty()) {
            chunk.error = io_error;
            release = StopLocked(io_error);
        } else {
            m_offset += data.size();

            if (eof) {
                Entry& e = m_entries[entry];
                e.done = true;
                e.trailer.ok = true;
                e.trailer.size = m_offset;
                e.trailer.sha256 = digest;
                m_current.reset();
                release = ClaimLocked(entry);
            } else {
                m_hasher = hasher;
            }

            chunk.data = std::move(data);
            chunk.eof = eof;
        }
    }

    Finish(std::move(release));

    return chunk;
}

interfaces::FileTrailer SnapshotSource::trailer(uint32_t entry)
{
    LOCK(m_mutex);

    if (m_stopped) return ErrorTrailer(m_stop_reason);
    if (entry >= m_entries.size()) return ErrorTrailer(strprintf("the transfer has no file %u", entry));

    const Entry& e = m_entries[entry];

    if (!e.done) return ErrorTrailer(strprintf("%s has not been sent to its end", e.snapshot.name));

    return e.trailer;
}

void SnapshotSource::cancel()
{
    Stop("the transfer was cancelled");
}

void SnapshotSource::Stop(const std::string& reason)
{
    Release release;

    {
        LOCK(m_mutex);
        release = StopLocked(reason);
    }

    Finish(std::move(release));
}

SnapshotSource::Release SnapshotSource::StopLocked(const std::string& reason)
{
    if (!m_stopped) {
        m_stopped = true;
        m_stop_reason = reason;
    }

    Release release;

    // A read in progress owns the open file and its entry's file.
    const std::optional<uint32_t> in_use = m_in_read ? m_current : std::nullopt;

    if (!m_in_read) {
        release.file = std::exchange(m_file, nullptr);
    }

    for (uint32_t i = 0; i < m_entries.size(); ++i) {
        Entry& e = m_entries[i];

        if (e.removed || in_use == i) continue;

        e.removed = true;
        release.paths.push_back(e.snapshot.path);
    }

    return release;
}

SnapshotSource::Release SnapshotSource::ClaimLocked(uint32_t index)
{
    Release release;
    release.file = std::exchange(m_file, nullptr);

    Entry& e = m_entries[index];

    if (!e.removed) {
        e.removed = true;
        release.paths.push_back(e.snapshot.path);
    }

    return release;
}

void SnapshotSource::Finish(Release release)
{
    // Closed first: Windows cannot remove a file that is open.
    if (release.file) std::fclose(release.file);

    for (const fs::path& path : release.paths) {
        RemoveFile(path);
    }
}

} // namespace

fs::path GetTransferDir()
{
    fs::path dir;
    std::string error;

    if (!PrepareTransferDir(dir, error)) {
        LogPrintf("ERROR: %s: %s", __func__, error);
    }

    return dir;
}

void CleanTransferDir()
{
    const fs::path dir = GetDataDir() / TRANSFER_DIR_NAME;

    boost::system::error_code ec;
    const fs::file_status status = fs::symlink_status(dir, ec);

    if (ec || !fs::exists(status)) return;

    if (fs::is_symlink(status)) {
        LogPrintf("WARNING: %s: %s is a symbolic link; it is not followed, and nothing in it is removed.",
                  __func__, dir.string());
        return;
    }

    if (!fs::is_directory(status)) {
        LogPrintf("WARNING: %s: %s is not a directory; nothing is removed.", __func__, dir.string());
        return;
    }

    // Listed first and removed after, so the directory is not changed while it
    // is being listed.
    std::vector<fs::path> leftovers;

    for (fs::directory_iterator it(dir, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
        boost::system::error_code entry_ec;
        const fs::file_status entry = it->symlink_status(entry_ec);

        // symlink_status, so a link is seen as one, and removing it removes the
        // link itself.
        if (!entry_ec && (fs::is_regular_file(entry) || fs::is_symlink(entry))) {
            leftovers.push_back(it->path());
        }
    }

    if (ec) {
        LogPrintf("WARNING: %s: could not list %s: %s", __func__, dir.string(), ec.message());
    }

    unsigned int removed = 0;
    unsigned int failed = 0;

    for (const fs::path& path : leftovers) {
        boost::system::error_code rm_ec;
        if (fs::remove(path, rm_ec) && !rm_ec) {
            ++removed;
        } else {
            ++failed;
            LogPrintf("WARNING: %s: could not remove %s: %s", __func__, path.string(), rm_ec.message());
        }
    }

    LogPrintf("%s: removed %u file(s) left in %s by an earlier run%s", __func__, removed, dir.string(),
              failed ? strprintf(", %u could not be removed", failed) : "");
}

bool ConfigBackupPermitted(std::string& reason)
{
    reason.clear();

    // The IPC listener's rule, so -noipcattachgroup counts as unset here too.
    if (!GetConfiguredOption("-ipcattachgroup").empty()) {
        reason = CONFIG_BACKUP_REFUSED_ATTACH_GROUP;
        return false;
    }

#ifndef WIN32
    // stat, not lstat: what matters is who owns the file the core reads, which
    // a link of this account's may point at. A file that cannot be examined
    // (it does not exist, most often) is no other account's to protect.
    struct stat st;
    if (::stat(GetConfigFile().c_str(), &st) == 0 && st.st_uid != ::geteuid()) {
        reason = CONFIG_BACKUP_REFUSED_OWNER;
        return false;
    }
#else
    // The owner read through a handle on the file itself (fsmodes::OwnedByUs), which does not
    // follow a link, so a configuration file that is one is refused whoever owns what it leads
    // to. A file that does not exist is no other account's to protect; one that cannot be
    // examined is refused.
    const fs::path conf = GetConfigFile();
    boost::system::error_code ec;
    if (fs::symlink_status(conf, ec).type() != fs::file_not_found && !fsmodes::OwnedByUs(conf)) {
        // Who owns it and how to make it this account's, or why that cannot be shown. Worded to be
        // true in both builds, since the monolithic one has no IPC.
        reason = "The configuration file holds the RPC password, and was left out of the backup: " +
                 fsmodes::WinNotOwnedText(conf, util::winsvc::IsRunningAsWindowsService()) +
                 ". Back up the settings file instead.";
        return false;
    }
#endif

    return true;
}

std::unique_ptr<interfaces::FileSource> MakeSnapshotSource(std::vector<SnapshotEntry> entries,
                                                           std::shared_ptr<interfaces::ConnectionScope> scope)
{
    return std::make_unique<SnapshotSource>(std::move(entries), std::move(scope));
}

std::optional<fs::path> SnapshotWallet(const CWallet& wallet, std::string& error)
{
    error.clear();

    if (!wallet.fFileBacked) {
        error = "the wallet is not file-backed";
        return std::nullopt;
    }

    fs::path dir;
    if (!PrepareTransferDir(dir, error)) return std::nullopt;

    const fs::path snapshot = NewSnapshotPath(dir);

    // BackupWallet logs a copy that failed, but not a wallet file that is in
    // use at that moment, which it does not copy.
    // A snapshot is owner-only or not made: none of the backup directory's tolerance
    // for a filesystem without modes applies here.
    if (!GRC::BackupWallet(wallet, snapshot.string(), GRC::BackupTarget::TransferSnapshot)) {
        RemoveFile(snapshot);
        error = "the wallet file could not be copied; try again, and see the core's debug.log if it keeps failing";
        return std::nullopt;
    }

    // The copy is a new file, which fsmodes::CopyFileOwnerOnly created owner-only through its own
    // descriptor: mode 0600 on POSIX, whatever the umask, or the owner-only access list on
    // Windows. A copy that did not keep that was refused and removed above (BackupTarget::
    // TransferSnapshot). Nothing changes the mode by the name afterwards, which would follow a
    // link there.
    return snapshot;
}

std::optional<fs::path> SnapshotPlainFile(const fs::path& source, std::string& error)
{
    error.clear();

    boost::system::error_code ec;
    if (!fs::is_regular_file(source, ec)) {
        error = strprintf("%s is not a file that can be copied", source.string());
        return std::nullopt;
    }

    fs::path dir;
    if (!PrepareTransferDir(dir, error)) return std::nullopt;

    std::FILE* in = fsbridge::fopen(source, "rb");
    if (!in) {
        error = strprintf("%s could not be opened: %s", source.string(), SysErrorString(errno));
        return std::nullopt;
    }

    const fs::path snapshot = NewSnapshotPath(dir);

    // Removes its temporary file on every path that does not commit.
    std::optional<util::TransferTempFile> copy = util::TransferTempFile::Create(snapshot, error);
    if (!copy) {
        std::fclose(in);
        return std::nullopt;
    }

    std::vector<unsigned char> buffer(64 * 1024);

    for (;;) {
        const size_t got = std::fread(buffer.data(), 1, buffer.size(), in);

        if (got > 0 && !copy->Write(buffer.data(), got, error)) {
            std::fclose(in);
            return std::nullopt;
        }

        if (got < buffer.size()) {
            if (std::ferror(in)) {
                error = strprintf("%s could not be read", source.string());
                std::fclose(in);
                return std::nullopt;
            }
            break;
        }
    }

    std::fclose(in);

    // The copy is checked against itself: Commit's size and hash check is for a
    // transfer, and this one has no other end. It renames the copy into place.
    if (!copy->Commit(copy->Size(), copy->Sha256(), error)) return std::nullopt;

    return snapshot;
}

namespace {

//! What a read or finish reports once the capability a source or sink holds
//! has ended.
const char* const CAPABILITY_ENDED = "the file transfer capability has ended";

//! What a read reports when, with no capability, the wallet no longer lets the
//! calling thread use its keys.
const char* const KEYS_NOT_AVAILABLE = "keys are not available";

//! Bytes in secure memory: locked while the locked pool can lock them, and
//! cleansed when freed.
using SecureBytes = std::vector<unsigned char, secure_allocator<unsigned char>>;

//! The secure allocator takes each allocation from one locked-pool arena
//! (256 KiB), so an upload is held in pieces of this size.
constexpr size_t SECURE_PIECE_BYTES = 64 * 1024;

//! An RPC error as the console shows one: "message (code n)", or its JSON when
//! it has no code and message.
std::string DescribeRpcError(const UniValue& error)
{
    try {
        const int code = find_value(error, "code").get_int();
        const std::string message = find_value(error, "message").get_str();
        return strprintf("%s (code %d)", message, code);
    } catch (const std::runtime_error&) {
        return error.write();
    }
}

//! An RPC reply as text: a string as it is, null as "", any other as JSON.
std::string ReplyText(const UniValue& reply)
{
    if (reply.isNull()) return std::string();
    if (reply.isStr()) return reply.get_str();
    return reply.write(2);
}

//! A string that is overwritten before it is freed, however its scope ends.
struct CleansedString
{
    std::string text{};

    CleansedString() = default;
    ~CleansedString()
    {
        if (!text.empty()) memory_cleanse(text.data(), text.size());
    }

    CleansedString(const CleansedString&) = delete;
    CleansedString& operator=(const CleansedString&) = delete;
};

void CleanseBytes(std::vector<unsigned char>& data)
{
    if (!data.empty()) memory_cleanse(data.data(), data.size());
}

//! Append \p len bytes to \p pieces, in pieces of SECURE_PIECE_BYTES.
void AppendToPieces(std::vector<SecureBytes>& pieces, const unsigned char* data, size_t len)
{
    while (len > 0) {
        if (pieces.empty() || pieces.back().size() >= SECURE_PIECE_BYTES) {
            pieces.emplace_back();
            pieces.back().reserve(SECURE_PIECE_BYTES);
        }

        SecureBytes& piece = pieces.back();
        const size_t take = std::min(len, SECURE_PIECE_BYTES - piece.size());
        piece.insert(piece.end(), data, data + take);
        data += take;
        len -= take;
    }
}

//! Reads the pieces an upload is held in, in order, without copying them.
class PiecesBuf final : public std::streambuf
{
public:
    explicit PiecesBuf(const std::vector<SecureBytes>& pieces) : m_pieces(pieces) {}

protected:
    int_type underflow() override
    {
        while (m_next < m_pieces.size()) {
            const SecureBytes& piece = m_pieces[m_next++];
            if (piece.empty()) continue;

            // Read only: nothing is put back or written through these.
            char* begin = reinterpret_cast<char*>(const_cast<unsigned char*>(piece.data()));
            setg(begin, begin, begin + piece.size());
            return traits_type::to_int_type(*gptr());
        }

        return traits_type::eof();
    }

private:
    const std::vector<SecureBytes>& m_pieces;
    size_t m_next{0};
};

//! Streams the text dumpwallet writes, generated as it is read, under the
//! capability the source holds when it was given one.
//!
//! What m_mutex guards, and who owns what (as in SnapshotSource):
//!  - A read marks itself (m_in_read) and takes the generator, the pending
//!    lines and the running hash out from under the mutex. It generates with
//!    the mutex released, inside a CapabilityScope it opens on its own thread,
//!    and puts them back. While it is marked nothing else touches them.
//!  - cancel(), the close hook, a read error, the dump's end and the
//!    destructor stop the source: every later call is refused, and the
//!    generator and the pending lines are freed (the pending lines are in
//!    secure memory, cleansed as they are freed), by the stopper when no read
//!    owns them, else by that read when it returns. Each of them ends the
//!    capability, once (m_cap_ended), with the mutex released.
class DumpWalletSource final : public interfaces::FileSource
{
public:
    DumpWalletSource(CWallet& wallet, std::optional<CapabilityId> cap, EndCapabilityFn end_capability,
                     std::shared_ptr<interfaces::ConnectionScope> scope);
    ~DumpWalletSource() override;

    DumpWalletSource(const DumpWalletSource&) = delete;
    DumpWalletSource& operator=(const DumpWalletSource&) = delete;

    std::vector<interfaces::FileEntryInfo> entries() override;
    interfaces::FileChunk read(uint32_t entry, uint32_t max_bytes) override LOCKS_EXCLUDED(m_mutex);
    interfaces::FileTrailer trailer(uint32_t entry) override LOCKS_EXCLUDED(m_mutex);
    void cancel() override LOCKS_EXCLUDED(m_mutex);

private:
    //! Stop the source for \p reason (the first reason given is kept) and end
    //! the capability.
    void Stop(const std::string& reason) LOCKS_EXCLUDED(m_mutex);

    //! Mark the source stopped, and take the generator and the pending lines
    //! unless a read owns them.
    void StopLocked(const std::string& reason, std::unique_ptr<WalletDumpGenerator>& generator, SecureBytes& pending)
        EXCLUSIVE_LOCKS_REQUIRED(m_mutex);

    //! End the capability, when there is one, the first time only. Called
    //! without the mutex.
    void EndCapability();

    CWallet& m_wallet;
    //! The capability the dump is made under, or none on a wallet that needs
    //! none.
    const std::optional<CapabilityId> m_cap;
    const EndCapabilityFn m_end_capability;
    std::atomic<bool> m_cap_ended{false};

    Mutex m_mutex;
    std::unique_ptr<WalletDumpGenerator> m_generator GUARDED_BY(m_mutex){};
    //! Lines generated beyond a read's bound, for the next read.
    SecureBytes m_pending GUARDED_BY(m_mutex){};
    //! The generator has written the dump's last line.
    bool m_generated_all GUARDED_BY(m_mutex){false};
    //! Sent to its end; m_trailer holds the result.
    bool m_done GUARDED_BY(m_mutex){false};
    interfaces::FileTrailer m_trailer GUARDED_BY(m_mutex){};
    uint64_t m_offset GUARDED_BY(m_mutex){0};
    //! The hash so far. A read copies it out and hashes its chunk with the
    //! mutex released; nothing else touches it while the read is marked.
    CSHA256 m_hasher GUARDED_BY(m_mutex){};
    bool m_in_read GUARDED_BY(m_mutex){false};
    bool m_stopped GUARDED_BY(m_mutex){false};
    std::string m_stop_reason GUARDED_BY(m_mutex){};

    //! Set in the constructor and only read after it.
    uint64_t m_size_hint{0};
    const std::shared_ptr<interfaces::ConnectionScope> m_scope;
    interfaces::ConnectionScope::HookId m_hook{0};
};

DumpWalletSource::DumpWalletSource(CWallet& wallet, std::optional<CapabilityId> cap,
                                   EndCapabilityFn end_capability,
                                   std::shared_ptr<interfaces::ConnectionScope> scope)
    : m_wallet(wallet), m_cap(cap), m_end_capability(std::move(end_capability)), m_scope(std::move(scope))
{
    // The generator reads the wallet under the capability, as each read does.
    // Without one it relies on CanUseKeys(), which it asks itself.
    std::unique_ptr<WalletDumpGenerator> generator;
    std::string error;

    try {
        const CapabilityScope capability(m_wallet, m_cap, CapabilityKind::FileTransfer);
        if (m_cap && !capability.Active()) {
            error = CAPABILITY_ENDED;
        } else {
            generator = std::make_unique<WalletDumpGenerator>(m_wallet);
        }
    } catch (const UniValue& e) {
        error = DescribeRpcError(e);
    } catch (const std::exception& e) {
        error = strprintf("the dump could not begin: %s", e.what());
    }

    if (generator) {
        m_size_hint = generator->SizeHint();
        WITH_LOCK(m_mutex, m_generator = std::move(generator));
    } else {
        // A source that never begins: each read reports why.
        {
            LOCK(m_mutex);
            m_stopped = true;
            m_stop_reason = error;
        }
        EndCapability();
    }

    // Last, once every member is in place: on a scope that has already closed
    // the hook runs here, at once.
    if (m_scope) {
        m_hook = m_scope->addCloseHook([this] { Stop("the IPC connection closed"); });
    }
}

DumpWalletSource::~DumpWalletSource()
{
    // First, so the hook cannot run on a source that is going away. This waits
    // for a hook that is running.
    if (m_scope && m_hook != 0) {
        m_scope->removeCloseHook(m_hook);
    }

    Stop("the transfer has ended");
}

std::vector<interfaces::FileEntryInfo> DumpWalletSource::entries()
{
    interfaces::FileEntryInfo info;
    info.name = WALLET_DUMP_ENTRY_NAME;
    info.size = -1;
    info.size_hint = m_size_hint;
    return {info};
}

interfaces::FileChunk DumpWalletSource::read(uint32_t entry, uint32_t max_bytes)
{
    const size_t want = std::min(max_bytes, interfaces::FILE_TRANSFER_CHUNK_BYTES);

    std::unique_ptr<WalletDumpGenerator> generator;
    SecureBytes pending;
    CSHA256 hasher;
    bool generated_all = false;

    {
        LOCK(m_mutex);

        if (m_stopped) return ErrorChunk(m_stop_reason);
        if (m_in_read) return ErrorChunk("another read of this transfer is in progress");
        if (entry != 0) return ErrorChunk(strprintf("the transfer has no file %u", entry));
        if (m_done) return ErrorChunk(strprintf("%s has already been sent", WALLET_DUMP_ENTRY_NAME));
        if (want == 0) return ErrorChunk("a read must ask for at least one byte");

        generator = std::move(m_generator);
        pending = std::move(m_pending);
        m_pending.clear();
        hasher = m_hasher;
        generated_all = m_generated_all;
        m_in_read = true;
    }

    // Generated with the mutex released, so the close hook never waits for the
    // wallet's locks. A capability is checked on every read, before anything is
    // sent, the lines generated by an earlier read included. Without one the
    // generator's own CanUseKeys() check is what refuses.
    std::vector<unsigned char> data;
    std::string error;

    try {
        const CapabilityScope capability(m_wallet, m_cap, CapabilityKind::FileTransfer);

        if (m_cap && !capability.Active()) {
            error = CAPABILITY_ENDED;
        } else if (!m_cap && !m_wallet.CanUseKeys()) {
            // Without a capability the wallet's own state is what lets the
            // dump through, so lines generated while it did are not handed
            // out once it no longer does.
            error = KEYS_NOT_AVAILABLE;
        } else {
            data.reserve(want);

            const size_t from_pending = std::min(want, pending.size());
            data.insert(data.end(), pending.begin(), pending.begin() + from_pending);
            pending.erase(pending.begin(), pending.begin() + from_pending);

            while (data.size() < want && !generated_all) {
                const size_t room = want - data.size();

                CleansedString lines;
                lines.text.reserve(room + 512);
                generated_all = !generator->Next(lines.text, room);

                // A line longer than the room left waits for the next read.
                const size_t now = std::min(room, lines.text.size());
                data.insert(data.end(), lines.text.begin(), lines.text.begin() + now);
                pending.insert(pending.end(), lines.text.begin() + now, lines.text.end());
            }
        }
    } catch (const UniValue& e) {
        error = DescribeRpcError(e);
    } catch (const std::exception& e) {
        error = strprintf("the dump failed: %s", e.what());
    }

    const bool eof = error.empty() && generated_all && pending.empty();
    uint256 digest;

    if (error.empty()) {
        hasher.Write(data.data(), data.size());
        if (eof) hasher.Finalize(digest.begin());
    }

    interfaces::FileChunk chunk;
    bool end_capability = false;

    {
        LOCK(m_mutex);

        m_in_read = false;

        if (m_stopped) {
            // Stopped while this read ran, which left the generator and the
            // pending lines to it; they are freed below.
            chunk.error = m_stop_reason;
            end_capability = true;
        } else if (!error.empty()) {
            m_stopped = true;
            m_stop_reason = error;
            chunk.error = error;
            end_capability = true;
        } else {
            m_offset += data.size();
            m_generated_all = generated_all;

            if (eof) {
                m_done = true;
                m_trailer.ok = true;
                m_trailer.size = m_offset;
                m_trailer.sha256 = digest;
                end_capability = true;
            } else {
                m_generator = std::move(generator);
                m_pending = std::move(pending);
                m_hasher = hasher;
            }

            chunk.data = std::move(data);
            chunk.eof = eof;
        }
    }

    // Nothing of a refused read leaves this call.
    if (!chunk.error.empty()) CleanseBytes(data);

    if (end_capability) EndCapability();

    return chunk;
}

interfaces::FileTrailer DumpWalletSource::trailer(uint32_t entry)
{
    LOCK(m_mutex);

    if (m_stopped && !m_done) return ErrorTrailer(m_stop_reason);
    if (entry != 0) return ErrorTrailer(strprintf("the transfer has no file %u", entry));
    if (!m_done) return ErrorTrailer(strprintf("%s has not been sent to its end", WALLET_DUMP_ENTRY_NAME));

    return m_trailer;
}

void DumpWalletSource::cancel()
{
    Stop("the transfer was cancelled");
}

void DumpWalletSource::Stop(const std::string& reason)
{
    std::unique_ptr<WalletDumpGenerator> generator;
    SecureBytes pending;

    {
        LOCK(m_mutex);
        StopLocked(reason, generator, pending);
    }

    // The generator and the pending lines are freed as this returns, with the
    // mutex released; the lines are cleansed as they are freed.
    EndCapability();
}

void DumpWalletSource::StopLocked(const std::string& reason, std::unique_ptr<WalletDumpGenerator>& generator,
                                  SecureBytes& pending)
{
    if (!m_stopped) {
        m_stopped = true;
        m_stop_reason = reason;
    }

    // A read in progress owns them, and frees them itself.
    if (!m_in_read) {
        generator = std::move(m_generator);
        pending = std::move(m_pending);
        m_pending.clear();
    }
}

void DumpWalletSource::EndCapability()
{
    if (!m_cap || !m_end_capability) return;
    if (!m_cap_ended.exchange(true)) m_end_capability();
}

//! Takes an upload of a wallet dump into secure memory and imports it at
//! finish(), under the capability it holds when it was given one.
//!
//! What m_mutex guards, and who owns what: a write or finish() marks itself
//! (m_busy) and takes the pieces and the running hash out from under the
//! mutex, copies or imports with it released, and puts them back or frees
//! them. cancel(), the close hook, a refused write, finish() and the
//! destructor stop the sink: later calls are refused, the pieces are freed
//! (cleansed as they are) by the stopper when no call owns them, else by that
//! call, and the capability is ended once, with the mutex released.
class ImportWalletSink final : public interfaces::FileSink
{
public:
    ImportWalletSink(CWallet& wallet, std::optional<CapabilityId> cap, EndCapabilityFn end_capability, uint64_t size,
                     std::shared_ptr<interfaces::ConnectionScope> scope);
    ~ImportWalletSink() override;

    ImportWalletSink(const ImportWalletSink&) = delete;
    ImportWalletSink& operator=(const ImportWalletSink&) = delete;

    bool write(const std::vector<unsigned char>& data) override LOCKS_EXCLUDED(m_mutex);
    interfaces::FileSinkResult finish(uint64_t size, const uint256& sha256) override LOCKS_EXCLUDED(m_mutex);
    void cancel() override LOCKS_EXCLUDED(m_mutex);

private:
    void Stop(const std::string& reason) LOCKS_EXCLUDED(m_mutex);

    //! Mark the sink stopped, and take the pieces unless a call owns them.
    void StopLocked(const std::string& reason, std::vector<SecureBytes>& pieces) EXCLUSIVE_LOCKS_REQUIRED(m_mutex);

    //! End the capability, when there is one, the first time only. Called
    //! without the mutex.
    void EndCapability();

    CWallet& m_wallet;
    //! The capability the import runs under, or none on a wallet that needs
    //! none.
    const std::optional<CapabilityId> m_cap;
    const EndCapabilityFn m_end_capability;
    const uint64_t m_size;
    std::atomic<bool> m_cap_ended{false};

    Mutex m_mutex;
    std::vector<SecureBytes> m_pieces GUARDED_BY(m_mutex){};
    uint64_t m_received GUARDED_BY(m_mutex){0};
    CSHA256 m_hasher GUARDED_BY(m_mutex){};
    bool m_busy GUARDED_BY(m_mutex){false};
    bool m_stopped GUARDED_BY(m_mutex){false};
    std::string m_stop_reason GUARDED_BY(m_mutex){};

    //! Set in the constructor and only read after it.
    const std::shared_ptr<interfaces::ConnectionScope> m_scope;
    interfaces::ConnectionScope::HookId m_hook{0};
};

ImportWalletSink::ImportWalletSink(CWallet& wallet, std::optional<CapabilityId> cap, EndCapabilityFn end_capability,
                                   uint64_t size, std::shared_ptr<interfaces::ConnectionScope> scope)
    : m_wallet(wallet), m_cap(cap), m_end_capability(std::move(end_capability)), m_size(size),
      m_scope(std::move(scope))
{
    const interfaces::ConsoleFileCommand* command = interfaces::FindConsoleFileCommand("importwallet");
    const uint64_t cap_bytes = command ? command->max_upload_bytes : 0;

    if (m_size > cap_bytes) {
        {
            LOCK(m_mutex);
            m_stopped = true;
            m_stop_reason = strprintf("the wallet dump is too large (%u bytes; at most %u)", m_size, cap_bytes);
        }
        EndCapability();
    }

    // Last: on a scope that has already closed the hook runs here, at once.
    if (m_scope) {
        m_hook = m_scope->addCloseHook([this] { Stop("the IPC connection closed"); });
    }
}

ImportWalletSink::~ImportWalletSink()
{
    if (m_scope && m_hook != 0) {
        m_scope->removeCloseHook(m_hook);
    }

    Stop("the transfer has ended");
}

bool ImportWalletSink::write(const std::vector<unsigned char>& data)
{
    std::vector<SecureBytes> pieces;
    CSHA256 hasher;
    bool owned = false;

    {
        LOCK(m_mutex);

        if (m_stopped || m_busy) return false;

        // m_received never passes m_size, so this cannot wrap.
        if (data.size() > m_size - m_received) {
            StopLocked("the upload is larger than its announced size", pieces);
        } else {
            pieces = std::move(m_pieces);
            m_pieces.clear();
            hasher = m_hasher;
            m_busy = true;
            owned = true;
        }
    }

    if (!owned) {
        // Refused: the pieces it took are freed as this returns.
        EndCapability();
        return false;
    }

    std::string error;
    try {
        AppendToPieces(pieces, data.data(), data.size());
        hasher.Write(data.data(), data.size());
    } catch (const std::exception& e) {
        error = strprintf("the upload could not be held in memory: %s", e.what());
    }

    {
        LOCK(m_mutex);

        m_busy = false;

        if (!m_stopped && error.empty()) {
            m_pieces = std::move(pieces);
            m_hasher = hasher;
            m_received += data.size();
            return true;
        }

        if (!m_stopped) {
            m_stopped = true;
            m_stop_reason = error;
        }
    }

    // Stopped meanwhile, or refused: the pieces are freed as this returns.
    EndCapability();
    return false;
}

interfaces::FileSinkResult ImportWalletSink::finish(uint64_t size, const uint256& sha256)
{
    interfaces::FileSinkResult result;
    std::vector<SecureBytes> pieces;
    uint64_t received = 0;
    CSHA256 hasher;

    {
        LOCK(m_mutex);

        if (m_stopped) {
            result.error = m_stop_reason;
            return result;
        }
        if (m_busy) {
            result.error = "another call of this transfer is in progress";
            return result;
        }

        pieces = std::move(m_pieces);
        m_pieces.clear();
        received = m_received;
        hasher = m_hasher;
        m_busy = true;
    }

    uint256 digest;
    hasher.Finalize(digest.begin());

    if (size != m_size || received != size || digest != sha256) {
        result.error = "the received data does not match the size and SHA-256 the client sent";
    } else {
        PiecesBuf buffer(pieces);
        std::istream in(&buffer);

        // Without a capability the import's own unlock check is what refuses.
        try {
            const CapabilityScope capability(m_wallet, m_cap, CapabilityKind::FileTransfer);
            if (m_cap && !capability.Active()) {
                result.error = CAPABILITY_ENDED;
            } else {
                result.output = ReplyText(ImportWalletFromStream(m_wallet, in));
                result.ok = true;
            }
        } catch (const UniValue& e) {
            result.error = DescribeRpcError(e);
        } catch (const std::exception& e) {
            result.error = strprintf("the import failed: %s", e.what());
        } catch (...) {
            result.error = "the import failed";
        }
    }

    {
        LOCK(m_mutex);
        m_busy = false;
        if (!m_stopped) {
            m_stopped = true;
            m_stop_reason = "the transfer has finished";
        }
    }

    // The pieces are freed, and cleansed, as this returns.
    EndCapability();
    return result;
}

void ImportWalletSink::cancel()
{
    Stop("the transfer was cancelled");
}

void ImportWalletSink::Stop(const std::string& reason)
{
    std::vector<SecureBytes> pieces;

    {
        LOCK(m_mutex);
        StopLocked(reason, pieces);
    }

    EndCapability();
}

void ImportWalletSink::StopLocked(const std::string& reason, std::vector<SecureBytes>& pieces)
{
    if (!m_stopped) {
        m_stopped = true;
        m_stop_reason = reason;
    }

    // A call in progress owns them, and frees them itself.
    if (!m_busy) {
        pieces = std::move(m_pieces);
        m_pieces.clear();
    }
}

void ImportWalletSink::EndCapability()
{
    if (!m_cap || !m_end_capability) return;
    if (!m_cap_ended.exchange(true)) m_end_capability();
}

//! Writes an upload to a core temporary file in the transfer directory, and at
//! finish() runs the command on it and deletes it.
//!
//! What m_mutex guards, and who owns the file: a write or finish() marks itself
//! (m_busy) and takes the temporary file out from under the mutex, writes or
//! commits and runs the command with it released, and puts it back or removes
//! it. A stopper (cancel(), the close hook, a refused write, the destructor)
//! takes the temporary file when no call owns it and removes it with the
//! mutex released; a call that finds the sink stopped when it returns
//! removes it itself. The committed file exists only inside finish(), which
//! removes it.
class RpcFileSink final : public interfaces::FileSink
{
public:
    RpcFileSink(std::string method, std::vector<std::string> args, size_t path_index, uint64_t size, uint64_t max,
                std::shared_ptr<interfaces::ConnectionScope> scope, RpcRunner run);
    ~RpcFileSink() override;

    RpcFileSink(const RpcFileSink&) = delete;
    RpcFileSink& operator=(const RpcFileSink&) = delete;

    bool write(const std::vector<unsigned char>& data) override LOCKS_EXCLUDED(m_mutex);
    interfaces::FileSinkResult finish(uint64_t size, const uint256& sha256) override LOCKS_EXCLUDED(m_mutex);
    void cancel() override LOCKS_EXCLUDED(m_mutex);

private:
    void Stop(const std::string& reason) LOCKS_EXCLUDED(m_mutex);

    //! Mark the sink stopped, and take the temporary file unless a call owns it.
    void StopLocked(const std::string& reason, std::optional<util::TransferTempFile>& temp)
        EXCLUSIVE_LOCKS_REQUIRED(m_mutex);

    const std::string m_method;
    const std::vector<std::string> m_args;
    const size_t m_path_index;
    const uint64_t m_size;
    const RpcRunner m_run;

    Mutex m_mutex;
    std::optional<util::TransferTempFile> m_temp GUARDED_BY(m_mutex){};
    uint64_t m_received GUARDED_BY(m_mutex){0};
    bool m_busy GUARDED_BY(m_mutex){false};
    bool m_stopped GUARDED_BY(m_mutex){false};
    std::string m_stop_reason GUARDED_BY(m_mutex){};

    //! Set in the constructor and only read after it: where the upload is
    //! renamed to for the command.
    fs::path m_dest{};
    const std::shared_ptr<interfaces::ConnectionScope> m_scope;
    interfaces::ConnectionScope::HookId m_hook{0};
};

RpcFileSink::RpcFileSink(std::string method, std::vector<std::string> args, size_t path_index, uint64_t size,
                         uint64_t max, std::shared_ptr<interfaces::ConnectionScope> scope, RpcRunner run)
    : m_method(std::move(method)), m_args(std::move(args)), m_path_index(path_index), m_size(size),
      m_run(std::move(run)), m_scope(std::move(scope))
{
    std::string error;
    std::optional<util::TransferTempFile> temp;

    if (m_size > max) {
        error = strprintf("%s: the file is too large (%u bytes; at most %u)", m_method, m_size, max);
    } else if (m_path_index > m_args.size()) {
        error = strprintf("%s takes %u argument(s) before its file", m_method, m_path_index);
    } else {
        fs::path dir;
        if (PrepareTransferDir(dir, error)) {
            m_dest = NewTransferPath(dir, "upload-");
            temp = util::TransferTempFile::Create(m_dest, error);
        }
    }

    {
        LOCK(m_mutex);
        if (temp) {
            m_temp = std::move(temp);
        } else {
            m_stopped = true;
            m_stop_reason = error;
        }
    }

    // Last: on a scope that has already closed the hook runs here, at once.
    if (m_scope) {
        m_hook = m_scope->addCloseHook([this] { Stop("the IPC connection closed"); });
    }
}

RpcFileSink::~RpcFileSink()
{
    if (m_scope && m_hook != 0) {
        m_scope->removeCloseHook(m_hook);
    }

    Stop("the transfer has ended");
}

bool RpcFileSink::write(const std::vector<unsigned char>& data)
{
    std::optional<util::TransferTempFile> temp;
    bool owned = false;

    {
        LOCK(m_mutex);

        if (m_stopped || m_busy) return false;

        // m_received never passes m_size, so this cannot wrap.
        if (data.size() > m_size - m_received) {
            StopLocked("the upload is larger than its announced size", temp);
        } else {
            temp = std::exchange(m_temp, std::nullopt);
            m_busy = true;
            owned = true;
        }
    }

    if (!owned) {
        if (temp) temp->Abort();
        return false;
    }

    std::string error;
    const bool written = temp->Write(data.data(), data.size(), error);

    {
        LOCK(m_mutex);

        m_busy = false;

        if (!m_stopped && written) {
            m_temp = std::move(temp);
            m_received += data.size();
            return true;
        }

        if (!m_stopped) {
            m_stopped = true;
            m_stop_reason = error;
        }
    }

    temp->Abort();
    return false;
}

interfaces::FileSinkResult RpcFileSink::finish(uint64_t size, const uint256& sha256)
{
    interfaces::FileSinkResult result;
    std::optional<util::TransferTempFile> temp;
    uint64_t received = 0;

    {
        LOCK(m_mutex);

        if (m_stopped) {
            result.error = m_stop_reason;
            return result;
        }
        if (m_busy) {
            result.error = "another call of this transfer is in progress";
            return result;
        }

        temp = std::exchange(m_temp, std::nullopt);
        received = m_received;
        m_busy = true;
    }

    std::string error;

    if (size != m_size || received != size) {
        temp->Abort();
        result.error = "the received data does not match the size and SHA-256 the client sent";
    } else if (!temp->Commit(size, sha256, error)) {
        // Commit removes the temporary file when it refuses.
        result.error = error;
    } else {
        std::vector<std::string> args = m_args;
        args.insert(args.begin() + m_path_index, m_dest.string());

        try {
            result.output = m_run(m_method, args);
            result.ok = true;
        } catch (const UniValue& e) {
            result.error = DescribeRpcError(e);
        } catch (const std::exception& e) {
            result.error = e.what();
        } catch (...) {
            // Whatever the command throws, the file goes below.
            result.error = strprintf("%s failed", m_method);
        }

        RemoveFile(m_dest);
    }

    {
        LOCK(m_mutex);
        m_busy = false;
        if (!m_stopped) {
            m_stopped = true;
            m_stop_reason = "the transfer has finished";
        }
    }

    return result;
}

void RpcFileSink::cancel()
{
    Stop("the transfer was cancelled");
}

void RpcFileSink::Stop(const std::string& reason)
{
    std::optional<util::TransferTempFile> temp;

    {
        LOCK(m_mutex);
        StopLocked(reason, temp);
    }

    // Closed and removed with the mutex released.
    if (temp) temp->Abort();
}

void RpcFileSink::StopLocked(const std::string& reason, std::optional<util::TransferTempFile>& temp)
{
    if (!m_stopped) {
        m_stopped = true;
        m_stop_reason = reason;
    }

    // A call in progress owns it, and removes it itself.
    if (!m_busy) temp = std::exchange(m_temp, std::nullopt);
}

} // namespace

const char* const WALLET_DUMP_ENTRY_NAME = "wallet-dump.txt";

std::unique_ptr<interfaces::FileSource> MakeDumpWalletSource(CWallet& wallet, std::optional<CapabilityId> cap,
                                                             EndCapabilityFn end_capability,
                                                             std::shared_ptr<interfaces::ConnectionScope> scope)
{
    return std::make_unique<DumpWalletSource>(wallet, cap, std::move(end_capability), std::move(scope));
}

std::unique_ptr<interfaces::FileSink> MakeImportWalletSink(CWallet& wallet, std::optional<CapabilityId> cap,
                                                           EndCapabilityFn end_capability, uint64_t size,
                                                           std::shared_ptr<interfaces::ConnectionScope> scope)
{
    return std::make_unique<ImportWalletSink>(wallet, cap, std::move(end_capability), size, std::move(scope));
}

RpcRunner DefaultRpcRunner()
{
    return [](const std::string& method, const std::vector<std::string>& args) {
        // As the console's dispatch: refused while the RPC server warms up.
        ThrowIfRefusedInWarmup(method);
        return ReplyText(tableRPC.execute(method, RPCConvertValues(method, args)));
    };
}

std::unique_ptr<interfaces::FileSink> MakeRpcFileSink(std::string method, std::vector<std::string> args,
                                                      size_t path_index, uint64_t size, uint64_t max,
                                                      std::shared_ptr<interfaces::ConnectionScope> scope,
                                                      RpcRunner run)
{
    return std::make_unique<RpcFileSink>(std::move(method), std::move(args), path_index, size, max,
                                         std::move(scope), std::move(run));
}

std::unique_ptr<interfaces::FileSource> MakeRpcOutputSource(const std::string& method, std::vector<std::string> args,
                                                            size_t path_index,
                                                            std::shared_ptr<interfaces::ConnectionScope> scope,
                                                            std::string& error, RpcRunner run)
{
    error.clear();

    if (path_index > args.size()) {
        error = strprintf("%s takes %u argument(s) before its file", method, path_index);
        return nullptr;
    }

    fs::path dir;
    if (!PrepareTransferDir(dir, error)) return nullptr;

    // A fresh name in a directory only this account can write, so nothing is
    // there: the command refuses to write over a file.
    const fs::path output = NewTransferPath(dir, "out-");
    args.insert(args.begin() + path_index, output.string());

    std::string reply;

    try {
        reply = run(method, args);
    } catch (const UniValue& e) {
        error = DescribeRpcError(e);
    } catch (const std::exception& e) {
        error = e.what();
    } catch (...) {
        // Whatever the command throws, what it left goes below.
        error = strprintf("%s failed", method);
    }

    boost::system::error_code ec;
    if (error.empty() && !fs::is_regular_file(fs::symlink_status(output, ec))) {
        error = strprintf("%s wrote no file", method);
    }

    if (!error.empty()) {
        // Whatever the command left there, a link included, which is removed
        // itself.
        RemoveFile(output);
        return nullptr;
    }

    return MakeSnapshotSource({{method + ".txt", output, reply}}, std::move(scope));
}

} // namespace wallet
