// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "node/mempool_persist.h"

#include "clientversion.h"
#include "main.h"
#include "random.h"
#include "streams.h"
#include "sync.h"
#include "txmempool.h"
#include "util.h"

#include <tinyformat.h>

#include <algorithm>
#include <cerrno>
#include <cstring>

namespace node {

bool WriteMempoolEntries(const fs::path& path, const MempoolPersistEntries& entries)
{
    // Write to a temporary file and rename into place so an interrupted dump can
    // never leave a partially written file.
    const uint16_t randv{GetRand<uint16_t>()};
    fs::path tmp = path;
    tmp += strprintf(".%04x", randv);

    FILE* file = fsbridge::fopen(tmp, "wb");
    CAutoFile fileout(file, SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull()) {
        fileout.fclose();
        return error("%s: failed to open %s", __func__, tmp.string());
    }

    try {
        fileout << UNBROADCAST_DUMP_VERSION;
        fileout << entries;
    } catch (const std::exception& e) {
        fileout.fclose();
        fs::remove(tmp);
        return error("%s: serialize error - %s", __func__, e.what());
    }

    if (!FileCommit(fileout.Get())) {
        fileout.fclose();
        fs::remove(tmp);
        return error("%s: FileCommit failed for %s", __func__, tmp.string());
    }
    fileout.fclose();

    if (!RenameOver(tmp, path)) {
        fs::remove(tmp);
        return error("%s: rename-into-place failed for %s", __func__, path.string());
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

bool DumpUnbroadcast(const CTxMemPool& pool, const fs::path& dump_path)
{
    MempoolPersistEntries entries;
    {
        LOCK(pool.cs);
        entries.reserve(pool.m_unbroadcast.size());
        for (const uint256& hash : pool.m_unbroadcast) {
            auto it = pool.mapTx.find(hash);
            if (it != pool.mapTx.end()) {
                entries.emplace_back(it->second.GetTx(), it->second.GetTime());
            }
        }
    }

    // Write outside the pool lock to avoid holding cs across disk I/O.
    return WriteMempoolEntries(dump_path, entries);
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
            }
        }
    }

    LogPrintf("Reloaded unbroadcast transactions: %d re-accepted of %d persisted\n",
              accepted, (int)entries.size());
    return true;
}

} // namespace node
