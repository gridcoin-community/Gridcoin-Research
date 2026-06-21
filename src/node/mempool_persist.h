// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_MEMPOOL_PERSIST_H
#define BITCOIN_NODE_MEMPOOL_PERSIST_H

#include "fs.h"
#include "primitives/transaction.h"

#include <cstdint>
#include <utility>
#include <vector>

class CTxMemPool;

namespace node {

//! On-disk format version for mempool.dat.
static constexpr uint64_t MEMPOOL_DUMP_VERSION = 1;

//! A persisted transaction together with its original pool-entry time.
using MempoolPersistEntries = std::vector<std::pair<CTransaction, int64_t>>;

//! Serialize the entries to \p path atomically (no validation). Exposed for tests.
bool WriteMempoolEntries(const fs::path& path, const MempoolPersistEntries& entries);

//! Deserialize entries from \p path. Returns false (and leaves \p entries empty)
//! on a missing, wrong-version, or corrupt file. Exposed for tests.
bool ReadMempoolEntries(const fs::path& path, MempoolPersistEntries& entries);

//! Snapshot the pool's transactions to \p dump_path.
bool DumpMempool(const CTxMemPool& pool, const fs::path& dump_path);

//! Reload transactions from \p load_path and re-validate each through
//! AcceptToMemoryPool (preserving the stored entry time). Stale/invalid
//! transactions are silently dropped.
bool LoadMempool(CTxMemPool& pool, const fs::path& load_path);

} // namespace node

#endif // BITCOIN_NODE_MEMPOOL_PERSIST_H
