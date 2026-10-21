// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/voting/legacy_store.h"
#include "chain.h"
#include "chainparams.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/voting/payloads.h"
#include "gridcoin/voting/poll.h"
#include "gridcoin/voting/vote.h"
#include "logging.h"
#include "node/blockstorage.h"
#include "primitives/block.h"
#include "txdb.h"
#include "validation.h"

using namespace GRC;

namespace {
//! The key type of the store's entries in the transaction LevelDB; its version is at (KEY_TYPE + "_db", "version").
const std::string KEY_TYPE = "legacy_poll";

//!
//! \brief Extract a poll title from a legacy vote contract.
//!
//! \param key The value of the legacy contract key.
//!
//! \return Normalized title of the poll to associate the vote with.
//!
std::string ParseLegacyVoteTitle(const std::string& key)
{
    return PollTitleKey(key.substr(0, key.find(';')));
}
} // Anonymous namespace

void LegacyPollStore::Initialize(const int floor, const int ceiling)
{
    m_floor = floor;
    m_ceiling = ceiling;

    if (m_loaded) {
        return;
    }

    CTxDB txdb("rw");
    std::pair<std::string, std::string> version_key = std::make_pair(KEY_TYPE + "_db", "version");
    uint32_t version = 0;

    if (!txdb.ReadGenericSerializable(version_key, version) || version != VERSION) {
        LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: legacy poll store version %u, code version %u: clearing the store.",
                 __func__, version, VERSION);

        std::string key_type = KEY_TYPE;
        uint256 start_key_hint;

        if (!txdb.EraseGenericSerializablesByKeyType(key_type, start_key_hint)
            || !txdb.WriteGenericSerializable(version_key, VERSION)) {
            LogPrintf("WARN: %s: failed to clear the legacy poll store.", __func__);
        }
    }

    std::string key_type = KEY_TYPE;
    uint256 start_key_hint;

    if (!txdb.ReadGenericSerializablesToMap(key_type, m_entries, start_key_hint)) {
        LogPrintf("WARN: %s: failed to read every entry of the legacy poll store.", __func__);
    }

    m_loaded = true;
}

LegacyPollStore::Lookup LegacyPollStore::Fetch(const uint256& txid, const LegacyPollEntry*& entry)
{
    entry = nullptr;

    // A stored entry is served without a chain read: its block and votes are far below the checkpoints. The
    // floor applies here too, so raising it hides the entries below it.
    if (const auto iter = m_entries.find(txid); iter != m_entries.end()) {
        if (iter->second.m_height < m_floor) {
            return Lookup::BELOW_FLOOR;
        }

        entry = &iter->second;

        return Lookup::FOUND;
    }

    LegacyPollEntry computed;
    const Lookup lookup = Compute(txid, computed);

    if (lookup != Lookup::FOUND) {
        return lookup;
    }

    CTxDB txdb("rw");
    std::pair<std::string, uint256> key = std::make_pair(KEY_TYPE, txid);

    if (!txdb.WriteGenericSerializable(key, computed)) {
        LogPrintf("WARN: %s: failed to store legacy poll %s; it is served from memory.", __func__, txid.ToString());
    }

    entry = &m_entries.emplace(txid, std::move(computed)).first->second;

    return Lookup::FOUND;
}

std::vector<const LegacyPollEntry*> LegacyPollStore::Entries() const
{
    std::vector<const LegacyPollEntry*> entries;

    for (const auto& [txid, entry] : m_entries) {
        if (entry.m_height >= m_floor) {
            entries.push_back(&entry);
        }
    }

    return entries;
}

void LegacyPollStore::Clear()
{
    CTxDB txdb("rw");
    std::string key_type = KEY_TYPE;
    uint256 start_key_hint;

    if (!txdb.EraseGenericSerializablesByKeyType(key_type, start_key_hint)) {
        LogPrintf("WARN: %s: failed to clear the legacy poll store.", __func__);
    }

    m_entries.clear();
}

LegacyPollStore::Lookup LegacyPollStore::Compute(const uint256& txid, LegacyPollEntry& entry) const
{
    CTransaction tx;
    uint256 block_hash;

    if (!GetTransaction(txid, tx, block_hash) || block_hash.IsNull()) {
        return Lookup::NOT_LEGACY;
    }

    const auto iter = mapBlockIndex.find(block_hash);

    if (iter == mapBlockIndex.end() || iter->second == nullptr || !iter->second->IsInMainChain()) {
        return Lookup::NOT_LEGACY;
    }

    const CBlockIndex* const pindex_poll = iter->second;

    if (pindex_poll->nHeight >= m_ceiling) {
        return Lookup::NOT_LEGACY;
    }

    bool found = false;

    for (const auto& contract : tx.GetContracts()) {
        if (contract.m_type != ContractType::POLL || contract.m_action != ContractAction::ADD) {
            continue;
        }

        const auto payload = contract.SharePayloadAs<PollPayload>();

        entry.m_payload_version = payload->m_version;
        entry.m_type = static_cast<uint8_t>(payload->m_poll.m_type.Value());
        entry.m_weight_type = static_cast<uint8_t>(payload->m_poll.m_weight_type.Value());
        entry.m_title = payload->m_poll.m_title;
        entry.m_duration_days = payload->m_poll.m_duration_days;
        found = true;

        break;
    }

    if (!found) {
        return Lookup::NOT_LEGACY;
    }

    if (pindex_poll->nHeight < m_floor) {
        return Lookup::BELOW_FLOOR;
    }

    entry.m_txid = txid;
    entry.m_timestamp = tx.nTime;
    entry.m_block_hash = block_hash;
    entry.m_height = pindex_poll->nHeight;

    // The #3470 end of a poll below the v15 height: the transaction's time plus the duration.
    const int64_t end = entry.m_timestamp + static_cast<int64_t>(entry.m_duration_days) * 86400;
    const std::string title_key = PollTitleKey(entry.m_title);
    CBlock block;

    // Through the closing block: the walk stops at the first block after the poll's whose time exceeds the end. Only
    // blocks flagged as holding a contract are read. An entry is kept for good, so it is computed only
    // from a closed window read in full: a chain that ends before the first block past the end, or a flagged block
    // that cannot be read, fills nothing.
    bool closed = false;

    for (const CBlockIndex* pindex = pindex_poll; pindex; pindex = pindex->pnext) {
        if (pindex != pindex_poll && pindex->nTime > end) {
            closed = true;
            break;
        }

        if (!pindex->IsContract()) {
            continue;
        }

        if (!ReadBlockFromDisk(block, pindex, Params().GetConsensus())) {
            LogPrintf("WARN: %s: block %d in the window of legacy poll %s could not be read; the poll is not stored.",
                      __func__, pindex->nHeight, txid.ToString());

            return Lookup::INCOMPLETE;
        }

        // In the poll's own block, the votes ahead of the poll are not its.
        bool after_poll = pindex != pindex_poll;

        // Skip coinbase and coinstake transactions.
        for (unsigned int i = 2; i < block.vtx.size(); ++i) {
            if (!after_poll) {
                after_poll = block.vtx[i].GetHash() == txid;
                continue;
            }

            for (const auto& contract : block.vtx[i].GetContracts()) {
                // Only a contract that adds a vote, as when a block connects (a REMOVE links no vote).
                if (contract.m_type != ContractType::VOTE || contract.m_action != ContractAction::ADD) {
                    continue;
                }

                // A version 2 vote names its poll by txid; a legacy vote by its poll's title key, each poll with that
                // title key whose window holds it, and only a well-formed one, as when a block connects.
                const bool linked = contract.m_version >= 2
                    ? contract.SharePayloadAs<Vote>()->m_poll_txid == txid
                    : contract.WellFormed()
                        && ParseLegacyVoteTitle(contract.m_body.AssumeLegacy()->LegacyKeyString()) == title_key;

                if (linked) {
                    entry.m_votes.emplace_back(block.vtx[i].GetHash(), pindex->nHeight);
                    break;
                }
            }
        }
    }

    return closed ? Lookup::FOUND : Lookup::INCOMPLETE;
}
