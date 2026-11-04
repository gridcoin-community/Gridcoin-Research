// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "wallet/transfer_sources.h"

#include "crypto/sha256.h"
#include "gridcoin/backup.h"
#include "interfaces/connection_scope.h"
#include "logging.h"
#include "random.h"
#include "span.h"
#include "sync.h"
#include "tinyformat.h"
#include "uint256.h"
#include "util/dir_permissions.h"
#include "util/strencodings.h"
#include "util/syserror.h"
#include "util/system.h"
#include "util/transfer_file.h"
#include "wallet/wallet.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <utility>

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

//! Worded to be true in both builds, since the monolithic one has no IPC.
const char* const CONFIG_BACKUP_REFUSED_OWNER =
    "The configuration file belongs to another account and holds the RPC password, so it is not included in the "
    "backup. Back up the settings file instead.";

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

//! A fresh "snap-<16 hex>.tmp" in dir.
fs::path NewSnapshotPath(const fs::path& dir)
{
    unsigned char rnd[8];
    GetRandBytes(Span<unsigned char>(rnd, sizeof(rnd)));

    return dir / ("snap-" + HexStr(Span<const unsigned char>(rnd, sizeof(rnd))) + ".tmp");
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
    if (!GRC::BackupWallet(wallet, snapshot.string())) {
        RemoveFile(snapshot);
        error = "the wallet file could not be copied; try again, and see the core's debug.log if it keeps failing";
        return std::nullopt;
    }

#ifndef WIN32
    // The copy takes the wallet file's mode. On Windows it inherits the
    // directory's owner-only access instead.
    if (::chmod(snapshot.c_str(), S_IRUSR | S_IWUSR) != 0) {
        error = strprintf("the copy of the wallet file could not be made owner-only: %s", SysErrorString(errno));
        RemoveFile(snapshot);
        return std::nullopt;
    }
#endif

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

} // namespace wallet
