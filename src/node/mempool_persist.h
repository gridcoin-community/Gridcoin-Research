// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_NODE_MEMPOOL_PERSIST_H
#define GRIDCOIN_NODE_MEMPOOL_PERSIST_H

#include "fs.h"
#include "primitives/transaction.h"
#include "sync.h"
#include "uint256.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class CTxMemPool;

namespace node {

//! On-disk format version for the unbroadcast persistence file.
static constexpr uint64_t UNBROADCAST_DUMP_VERSION = 1;

//! (transaction, original pool-entry time) pairs as written to / read from disk.
using MempoolPersistEntries = std::vector<std::pair<CTransaction, int64_t>>;

//! Atomically write \p entries to \p path (temp file + fsync + rename), so an
//! interrupted dump cannot leave a partially written file behind. A failure is
//! logged, unless \p error_out is given, in which case it receives the message.
//!
//! Writes to one path must be serialized by the caller: the temporary file is
//! always \p path plus ".new", so that one left behind by a crash is replaced by
//! the next write, and two concurrent writes to one path would share it.
//! UnbroadcastPersister serializes its writes.
bool WriteMempoolEntries(const fs::path& path, const MempoolPersistEntries& entries,
                         std::string* error_out = nullptr);

//! Read entries previously written by WriteMempoolEntries().
bool ReadMempoolEntries(const fs::path& path, MempoolPersistEntries& entries);

//! How often the node checks whether unbroadcast.dat still matches the
//! unbroadcast set, and rewrites it if not. A process crash loses at most the
//! changes since the last pass: about this interval, longer if another job on
//! the scheduler delays the pass.
static constexpr std::chrono::seconds UNBROADCAST_PERSIST_INTERVAL{5};

//! (hash, pool entry time) of each NeverSent entry, in hash order: what
//! unbroadcast.dat records, without the transactions themselves.
using UnbroadcastKeys = std::vector<std::pair<uint256, int64_t>>;

//!
//! \brief Keeps unbroadcast.dat in step with the pool's unbroadcast set.
//!
//! The file is the set's NeverSent entries as of its last write. Written only at
//! shutdown, as it was before this class, it would fall behind every change
//! since the last clean one, and a crash would restore a stale set in both
//! directions: a send since then would have lost its marker, so nothing would
//! re-announce it if its first broadcast was lost; and a transaction this node
//! had taken out since then (cancelunbroadcasttransaction) would come back,
//! while one a peer had fetched would be marked never sent again, which reopens
//! the cancel gate for it.
//!
//! WriteIfChanged() compares the set's NeverSent entries with what the file last
//! received and rewrites the file when they differ; the node calls it every
//! UNBROADCAST_PERSIST_INTERVAL. Comparing the whole set, rather than being told
//! of each change, means no path that changes it can be missed. Write() rewrites
//! the file regardless, at shutdown. Each write replaces the file through a
//! temporary file, an fsync and a rename, so a crash mid-write leaves the
//! previous file. A write that fails is logged once, and retried on every pass
//! until the file matches the set again.
//!
//! Nothing is written until Enable(), which the node calls once the persisted
//! set has been reloaded, or moved aside if it could not be read: before that the
//! pool does not hold it yet, and a write would replace the file with an
//! incomplete one. Enable() is never called with -persistmempool=0, or when an
//! unreadable file could not be moved aside.
//!
class UnbroadcastPersister
{
public:
    //! Allow writes, to \p path. The node calls it once, at startup, before any
    //! write, so the first WriteIfChanged() writes whatever the set holds.
    void Enable(fs::path path);

    //! Write the set if it differs from what the file last received.
    //! \return Whether it wrote. False before Enable(), when nothing changed, and
    //! on a failed write.
    bool WriteIfChanged(const CTxMemPool& pool);

    //! Write the set whether or not it differs.
    //! \return Whether it wrote. False before Enable() and on a failed write.
    bool Write(const CTxMemPool& pool);

private:
    bool WriteLocked(const CTxMemPool& pool, bool only_if_changed) EXCLUSIVE_LOCKS_REQUIRED(m_mutex);

    //! Serializes whole writes: the scheduler's and the shutdown's.
    Mutex m_mutex;
    fs::path m_path GUARDED_BY(m_mutex);
    bool m_enabled GUARDED_BY(m_mutex){false};
    //! What the file holds, as of the last successful write, if any.
    std::optional<UnbroadcastKeys> m_written GUARDED_BY(m_mutex);
    //! A write failed and its error was logged, and the file has not matched
    //! the set since.
    bool m_failing GUARDED_BY(m_mutex){false};
};

//! Reload persisted unbroadcast transactions: re-accept each through
//! AcceptToMemoryPool (which drops anything already confirmed or now invalid) and
//! re-arm the survivors for rebroadcast. A persisted transaction that is already in
//! the pool, as the wallet's startup re-accept leaves its own ones, is re-armed too,
//! unless AcceptToMemoryPool now refuses it as invalid.
//! Returns false only on a read error.
bool LoadUnbroadcast(CTxMemPool& pool, const fs::path& load_path);

} // namespace node

#endif // GRIDCOIN_NODE_MEMPOOL_PERSIST_H
