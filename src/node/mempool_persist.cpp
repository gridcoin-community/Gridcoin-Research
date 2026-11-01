// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "node/mempool_persist.h"

#include "clientversion.h"
#include "streams.h"
#include "sync.h"
#include "txmempool.h"
#include "util.h"
#include "consensus/validation.h"
#include "validation.h"

#include <tinyformat.h>

#include <algorithm>
#include <cerrno>
#include <cstring>

namespace node {

bool WriteMempoolEntries(const fs::path& path, const MempoolPersistEntries& entries, std::string* error_out)
{
    const auto fail = [&](const std::string& message) {
        if (error_out) {
            *error_out = message;
            return false;
        }
        return error("%s", message);
    };

    // Write to a temporary file and rename into place so an interrupted dump can
    // never leave a partially written file. The temporary name is fixed: writes
    // to one path are serialized (UnbroadcastPersister), so a file left behind
    // by a crash is simply replaced by the next write.
    fs::path tmp = path;
    tmp += ".new";

    // Removal of the temporary after a failure must not throw: the persister
    // writes from the scheduler thread, where an exception ends the process.
    const auto remove_tmp = [&] {
        boost::system::error_code ec;
        fs::remove(tmp, ec);
    };

    FILE* file = fsbridge::fopen(tmp, "wb");
    CAutoFile fileout(file, SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull()) {
        fileout.fclose();
        return fail(strprintf("%s: failed to open %s", __func__, tmp.string()));
    }

    try {
        fileout << UNBROADCAST_DUMP_VERSION;
        fileout << entries;
    } catch (const std::exception& e) {
        fileout.fclose();
        remove_tmp();
        return fail(strprintf("%s: serialize error - %s", __func__, e.what()));
    }

    if (!FileCommit(fileout.Get())) {
        fileout.fclose();
        remove_tmp();
        return fail(strprintf("%s: FileCommit failed for %s", __func__, tmp.string()));
    }
    fileout.fclose();

    if (!RenameOver(tmp, path)) {
        remove_tmp();
        return fail(strprintf("%s: rename-into-place failed for %s", __func__, path.string()));
    }

    return true;
}

bool ReadMempoolEntries(const fs::path& path, MempoolPersistEntries& entries)
{
    entries.clear();

    FILE* file = fsbridge::fopen(path, "rb");
    if (file == nullptr) {
        // A missing file is normal (first run, or nothing was in flight) and is
        // reported as success with an empty set; any other open failure is a real
        // error the caller should hear about.
        if (errno == ENOENT) {
            return true;
        }
        return error("%s: failed to open %s: %s", __func__, path.string(), std::strerror(errno));
    }
    CAutoFile filein(file, SER_DISK, CLIENT_VERSION);

    try {
        uint64_t version;
        filein >> version;
        if (version != UNBROADCAST_DUMP_VERSION) {
            filein.fclose();
            return error("%s: unsupported unbroadcast.dat version %d (expected %d)",
                         __func__, version, UNBROADCAST_DUMP_VERSION);
        }
        filein >> entries;
    } catch (const std::exception& e) {
        entries.clear();
        filein.fclose();
        return error("%s: deserialize or I/O error - %s", __func__, e.what());
    }

    filein.fclose();
    return true;
}

void UnbroadcastPersister::Enable(fs::path path)
{
    LOCK(m_mutex);
    m_path = std::move(path);
    m_enabled = true;
}

bool UnbroadcastPersister::Write(const CTxMemPool& pool)
{
    LOCK(m_mutex);
    return WriteLocked(pool, /*only_if_changed=*/false);
}

bool UnbroadcastPersister::WriteIfChanged(const CTxMemPool& pool)
{
    LOCK(m_mutex);
    return WriteLocked(pool, /*only_if_changed=*/true);
}

bool UnbroadcastPersister::WriteLocked(const CTxMemPool& pool, const bool only_if_changed)
{
    AssertLockHeld(m_mutex);

    if (!m_enabled) return false;

    UnbroadcastKeys keys;
    MempoolPersistEntries entries;
    bool unchanged = false;

    {
        LOCK(pool.cs);

        for (const auto& [hash, reason] : pool.m_unbroadcast) {
            // NeverSent only. A Reannounce entry says peers have already seen
            // the transaction and a reorg took it out of their pools, which is
            // a statement about this session's chain and does not survive a
            // restart. Worse, LoadUnbroadcast re-adds with the default reason,
            // so persisting one would quietly launder it into NeverSent and
            // re-open the cancel gate this distinction exists to keep shut.
            if (reason != UnbroadcastReason::NeverSent) continue;

            const auto it = pool.mapTx.find(hash);
            if (it != pool.mapTx.end()) {
                keys.emplace_back(hash, it->second.GetTime());
            }
        }

        // The set is compared as a whole with what was last written, rather than
        // told about each change, so that no path that changes it can be missed.
        // m_unbroadcast is ordered, so equal sets give equal key lists.
        unchanged = only_if_changed && m_written == keys;

        if (!unchanged) {
            entries.reserve(keys.size());
            for (const auto& [hash, time] : keys) {
                entries.emplace_back(pool.mapTx.find(hash)->second.GetTx(), time);
            }
        }
    }

    if (unchanged) {
        // A failed write leaves the file as it was, so a set that has come back
        // to what was last written is in step with the file again.
        if (m_failing) {
            LogPrintf("INFO: %s: %s matches the unbroadcast set again.", __func__, m_path.string());
            m_failing = false;
        }
        return false;
    }

    // Written outside the pool lock, to avoid holding cs across disk I/O. The
    // whole write is serialized by m_mutex, so a later one never replaces the
    // file with an earlier state.
    std::string error_message;
    if (!WriteMempoolEntries(m_path, entries, &error_message)) {
        // Logged once, not on every retry. m_written is left as it was, so the
        // next call tries again even if nothing changes in between.
        if (!m_failing) {
            LogPrintf("ERROR: %s: %s. Retrying on every pass until the file matches the set again.",
                      __func__, error_message);
            m_failing = true;
        }
        return false;
    }

    if (m_failing) {
        LogPrintf("INFO: %s: %s written again.", __func__, m_path.string());
        m_failing = false;
    }

    m_written = std::move(keys);
    return true;
}

bool LoadUnbroadcast(CTxMemPool& pool, const fs::path& load_path)
{
    MempoolPersistEntries entries;
    if (!ReadMempoolEntries(load_path, entries)) {
        return false;
    }

    // Re-accept oldest-first so the original relative ordering is preserved.
    std::sort(entries.begin(), entries.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });

    int accepted = 0;
    int already_pooled = 0;
    {
        LOCK(cs_main);
        for (auto& [tx, entry_time] : entries) {
            CValidationState state;
            CTransaction mutable_tx = tx; // AcceptToMemoryPool takes a non-const ref.
            // Preserve the original entry time; ATMP drops anything already
            // confirmed or now invalid.
            if (AcceptToMemoryPool(pool, mutable_tx, state, nullptr, entry_time)) {
                pool.AddUnbroadcast(mutable_tx.GetHash());
                ++accepted;
            } else if (!state.IsInvalid() && pool.exists(mutable_tx.GetHash())) {
                // Already pooled, which ATMP refuses without saying so. The state
                // check matters: ATMP runs validity checks before the duplicate check
                // (among them the V15 claim rule and the contract validation), so a
                // pooled transaction can also be refused as invalid, and that one must
                // not be re-armed. The already-pooled refusal leaves the state valid.
                //
                // Already pooled is the ordinary case for the wallet's own transactions:
                // before this load runs, the wallet's startup re-accept puts each
                // restarted own unconfirmed transaction that AcceptToMemoryPool still
                // accepts back in the pool, so without this branch the marker persisted
                // for exactly those transactions would be dropped, and with it the
                // rebroadcast and the cancel gate.
                //
                // Restored as NeverSent, which is what was persisted: no peer asked
                // this node for it last session. The network starts before this load,
                // so in between a peer that sends "mempool" could fetch it without the
                // fetch being recorded, or a peer that already has it could relay it
                // in; either way it is still restored as NeverSent. No Gridcoin node
                // sends "mempool", and the gate is already documented as a strong
                // signal rather than a proof.
                pool.AddUnbroadcast(mutable_tx.GetHash());
                ++already_pooled;
            }
        }
    }

    LogPrintf("Reloaded unbroadcast transactions: %d re-accepted, %d already in the pool, of %d persisted",
              accepted, already_pooled, (int)entries.size());
    return true;
}

} // namespace node
