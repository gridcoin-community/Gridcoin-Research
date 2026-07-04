// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_NODE_MEMPOOL_PERSIST_H
#define GRIDCOIN_NODE_MEMPOOL_PERSIST_H

#include "fs.h"
#include "primitives/transaction.h"

#include <cstdint>
#include <utility>
#include <vector>

class CTxMemPool;

namespace node {

//! On-disk format version for the unbroadcast persistence file.
static constexpr uint64_t UNBROADCAST_DUMP_VERSION = 1;

//! (transaction, original pool-entry time) pairs as written to / read from disk.
using MempoolPersistEntries = std::vector<std::pair<CTransaction, int64_t>>;

//! Atomically write \p entries to \p path (temp file + rename), so an interrupted
//! dump cannot leave a partially written file behind.
bool WriteMempoolEntries(const fs::path& path, const MempoolPersistEntries& entries);

//! Read entries previously written by WriteMempoolEntries().
bool ReadMempoolEntries(const fs::path& path, MempoolPersistEntries& entries);

//! Persist ONLY the unbroadcast set -- the node's own locally-originated
//! transactions that have not yet been seen propagating -- so they survive a
//! restart and can be rebroadcast. The whole pool is intentionally not persisted:
//! every other transaction is redelivered by block/inv gossip anyway.
bool DumpUnbroadcast(const CTxMemPool& pool, const fs::path& dump_path);

//! Reload persisted unbroadcast transactions: re-accept each through
//! AcceptToMemoryPool (which drops anything already confirmed or now invalid) and
//! re-arm the survivors for rebroadcast. Returns false only on a read error.
bool LoadUnbroadcast(CTxMemPool& pool, const fs::path& load_path);

} // namespace node

#endif // GRIDCOIN_NODE_MEMPOOL_PERSIST_H
