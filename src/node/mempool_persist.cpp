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
#include "util/system.h"

#include <tinyformat.h>

#include <algorithm>

namespace node {

bool WriteMempoolEntries(const fs::path& path, const MempoolPersistEntries& entries)
{
    // Write to a temporary file and rename into place so an interrupted dump
    // can never leave a partially written mempool.dat.
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
        fileout << MEMPOOL_DUMP_VERSION;
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
    CAutoFile filein(file, SER_DISK, CLIENT_VERSION);
    if (filein.IsNull()) {
        filein.fclose();
        return error("%s: failed to open %s", __func__, path.string());
    }

    try {
        uint64_t version;
        filein >> version;
        if (version != MEMPOOL_DUMP_VERSION) {
            filein.fclose();
            return error("%s: unsupported mempool.dat version %d", __func__, version);
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

bool DumpMempool(const CTxMemPool& pool, const fs::path& dump_path)
{
    MempoolPersistEntries entries;
    {
        LOCK(pool.cs);
        entries.reserve(pool.mapTx.size());
        for (const auto& [hash, entry] : pool.mapTx) {
            entries.emplace_back(entry.GetTx(), entry.GetTime());
        }
    }

    // Write outside the pool lock to avoid holding cs across disk I/O.
    return WriteMempoolEntries(dump_path, entries);
}

bool LoadMempool(CTxMemPool& pool, const fs::path& load_path)
{
    MempoolPersistEntries entries;
    if (!ReadMempoolEntries(load_path, entries)) {
        return false;
    }

    // Re-accept oldest-first so the original queue ordering is preserved.
    std::sort(entries.begin(), entries.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });

    int accepted = 0;
    {
        LOCK(cs_main);
        for (auto& [tx, time] : entries) {
            CValidationState state;
            if (AcceptToMemoryPool(pool, tx, state, nullptr, time)) {
                ++accepted;
            }
        }
    }

    LogPrintf("Imported mempool transactions from disk: %d accepted of %" PRIszu,
              accepted, entries.size());
    return true;
}

} // namespace node
