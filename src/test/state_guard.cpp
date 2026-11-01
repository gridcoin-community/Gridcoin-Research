// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "test/state_guard.h"

#include "chainparams.h"
#include "gridcoin/beacon.h"
#include "gridcoin/contract/registry.h"
#include "gridcoin/pool.h"
#include "gridcoin/protocol.h"
#include "gridcoin/scraper/scraper_registry.h"
#include "gridcoin/staking/kernel.h"
#include "gridcoin/staking/status.h"
#include "hash.h"
#include "net.h"
#include "primitives/block.h"
#include "sync.h"
#include "tinyformat.h"
#include "txmempool.h"
#include "util/system.h"
#include "util/time.h"
#include "validation.h"

#include <boost/test/unit_test.hpp>

#include <limits>
#include <map>
#include <string>
#include <vector>

extern bool g_mock_deterministic_tests;

namespace grc_test {

namespace {

//! The forced-setting key ForceSetArg("-blockv15height", ...) writes: the
//! ArgsManager strips the leading dash before storing.
const std::string V15_SETTING_KEY = "blockv15height";

std::string Ptr(const void* p)
{
    return p ? strprintf("%p", p) : "null";
}

void SerializeSettings(const util::Settings& s, std::map<std::string, std::string>& out)
{
    for (const auto& [name, value] : s.forced_settings) {
        out["forced:" + name] = value.write();
    }
    for (const auto& [name, values] : s.command_line_options) {
        std::string joined;
        for (const auto& v : values) joined += v.write() + ";";
        out["cmdline:" + name] = joined;
    }
    for (const auto& [name, value] : s.rw_settings) {
        out["rw:" + name] = value.write();
    }
}

template <typename T>
void Plain(std::vector<StateDiff>& out, const char* name, const T& before, const T& after)
{
    if (before == after) return;
    out.push_back({name, strprintf("%s", before), strprintf("%s", after), /*monotone=*/false, /*growth=*/false});
}

void Monotone(std::vector<StateDiff>& out, const char* name, size_t before, size_t after)
{
    if (before == after) return;
    out.push_back({name, strprintf("%u", before), strprintf("%u", after), /*monotone=*/true, after > before});
}

//! A registry leak report names at most this many keys; the rest are counted.
constexpr size_t MAX_NAMED_KEYS = 5;

//! Registry key (as text) -> a hash of the entry's serialization. \p key_text
//! turns a map key into text; \p project maps an entry to a type whose
//! serialization covers all of its fields.
template <typename Map, typename KeyText, typename Project>
RegistryDigest DigestMap(const Map& map, KeyText key_text, Project project)
{
    RegistryDigest digest;
    for (const auto& [key, entry] : map) {
        digest.entries[key_text(key)] = SerializeHash(project(*entry));
    }
    return digest;
}

//! "N entries, digest <16 hex>": one hash over every key and entry hash, so
//! two reports of the same registry can be compared at a glance.
std::string Describe(const RegistryDigest& d)
{
    CHashWriter writer(SER_GETHASH, PROTOCOL_VERSION);
    for (const auto& [key, hash] : d.entries) writer << key << hash;

    return strprintf("%u entries, digest %s", d.count(), writer.GetHash().ToString().substr(0, 16));
}

//! The keys whose entry differs between \p before and \p after, each tagged
//! "edited", "removed" or "added": edits and removals first, then additions,
//! each in key order.
std::vector<std::string> ChangedKeys(const RegistryDigest& before, const RegistryDigest& after)
{
    std::vector<std::string> changed;

    for (const auto& [key, hash] : before.entries) {
        const auto it = after.entries.find(key);
        if (it == after.entries.end()) {
            changed.push_back("removed " + key);
        } else if (it->second != hash) {
            changed.push_back("edited " + key);
        }
    }
    for (const auto& entry : after.entries) {
        if (!before.entries.count(entry.first)) changed.push_back("added " + entry.first);
    }

    return changed;
}

//! "a, b, c, d, e ... and N more": the first MAX_NAMED_KEYS of \p changed.
std::string NameKeys(const std::vector<std::string>& changed)
{
    std::string text;
    for (size_t i = 0; i < changed.size() && i < MAX_NAMED_KEYS; ++i) {
        // sign-compare checked: i is size_t; 0U is a non-negative literal, so both sides are unsigned.
        if (i > 0U) text += ", ";
        text += changed[i];
    }
    if (changed.size() > MAX_NAMED_KEYS) {
        text += strprintf(" ... and %u more", changed.size() - MAX_NAMED_KEYS);
    }
    return text;
}

//! True when \p after is \p before with entries removed and nothing else: no
//! key added and every surviving entry unchanged. With \p baseline, every
//! removed key must also be absent from it, so removing an entry the first
//! suite found is never a clean-up.
bool OnlyRemovedResidue(const RegistryDigest& before, const RegistryDigest& after,
                        const RegistryDigest* baseline)
{
    for (const auto& [key, hash] : after.entries) {
        const auto it = before.entries.find(key);
        if (it == before.entries.end() || it->second != hash) return false;
    }

    if (baseline == nullptr) return true;

    for (const auto& entry : before.entries) {
        if (!after.entries.count(entry.first) && baseline->entries.count(entry.first)) return false;
    }

    return true;
}

//! The classification is exact, key by key. A registry change is a cleaner
//! when the registry ends equal to \p baseline (the state the first suite
//! found), or when the suite only removed entries: no key added, every
//! surviving entry unchanged, and no removed key present in \p baseline.
//! Anything else (an edited entry, an added key, or a removed baseline key) is
//! a leak. With no baseline, only the removal rule applies.
void Registry(std::vector<StateDiff>& out, const char* name, const RegistryDigest& before,
              const RegistryDigest& after, const RegistryDigest* baseline)
{
    if (before == after) return;

    const bool cleaner = (baseline != nullptr && after == *baseline) || OnlyRemovedResidue(before, after, baseline);

    // A cleaner is reported as a monotone field that did not grow; anything
    // else as a plain field, which IsLeak() always fails. Either way the after
    // text names the keys that changed.
    out.push_back({name, Describe(before), strprintf("%s [%s]", Describe(after), NameKeys(ChangedKeys(before, after))),
                   /*monotone=*/cleaner, /*growth=*/false});
}

} // anonymous namespace

StateSnapshot CaptureState(CaptureScope scope)
{
    StateSnapshot s;

    gArgs.LockSettings([&](util::Settings& settings) { SerializeSettings(settings, s.settings); });

    s.network_id = Params().NetworkIDString();
    s.block_v15_height = GetBlockV15Height();
    s.stake_min_age = nStakeMinAge;
    s.coinbase_maturity = nCoinbaseMaturity;
    s.grandfather = nGrandfather;
    s.max_outbound = MAX_OUTBOUND_CONNECTIONS;

    s.previous_block_time = g_previous_block_time.load();
    s.mock_time = GetMockTime().count();
    s.mock_deterministic = g_mock_deterministic_tests;

    s.mempool_size = mempool.size();
    s.staking_active = g_miner_status.StakingActive();
    s.use_fast_index = fUseFastIndex;

    LOCK(cs_main);

    s.pindex_best = pindexBest;
    s.pindex_genesis = pindexGenesisBlock;
    s.hash_best_chain = hashBestChain;
    s.n_best_height = nBestHeight;
    s.map_block_index_size = mapBlockIndex.size();

    if (scope == CaptureScope::SUITE) {
        s.has_block_index_keys = true;
        s.map_block_index_keys.reserve(mapBlockIndex.size());
        for (const auto& entry : mapBlockIndex) s.map_block_index_keys.insert(entry.first);

        // Each registry's current entries, key by key, not its count: a case
        // can edit an entry in place and leave the count alone, as the miner
        // suite's POOL cases do with a builtin seed. Pending beacons, the
        // historical stores, and the whitelist, sidestake and poll registries
        // are not captured; they are covered by RegistryReset where a suite
        // touches them.
        const auto whole = [](const auto& entry) -> const auto& { return entry; };

        s.has_registries = true;
        s.beacons = DigestMap(
            GRC::GetBeaconRegistry().Beacons(),
            [](const GRC::Cpid& cpid) { return cpid.ToString(); },
            [](const GRC::Beacon& beacon) {
                // Beacon's own serialization writes only the public key.
                return GRC::StorageBeacon(beacon);
            });
        s.protocol_entries = DigestMap(
            GRC::GetProtocolRegistry().ProtocolEntries(),
            [](const std::string& key) { return key; },
            whole);
        s.scrapers = DigestMap(
            GRC::GetScraperRegistry().Scrapers(),
            [](const CKeyID& key_id) { return key_id.ToString(); },
            whole);

        // Entries() is a vector; each Pool carries its registry key, m_cpid.
        for (const GRC::Pool& pool : GRC::GetPoolRegistry().Entries()) {
            s.pools.entries[pool.m_cpid.ToString()] = SerializeHash(pool);
        }
    }

    return s;
}

std::vector<StateDiff> DiffState(const StateSnapshot& before, const StateSnapshot& after,
                                 const StateSnapshot* baseline)
{
    std::vector<StateDiff> out;

    Plain(out, "pindexBest", Ptr(before.pindex_best), Ptr(after.pindex_best));
    Plain(out, "pindexGenesisBlock", Ptr(before.pindex_genesis), Ptr(after.pindex_genesis));
    Plain(out, "hashBestChain", before.hash_best_chain.ToString(), after.hash_best_chain.ToString());
    Plain(out, "nBestHeight", before.n_best_height, after.n_best_height);

    if (before.has_block_index_keys && after.has_block_index_keys) {
        size_t added = 0;
        for (const auto& key : after.map_block_index_keys) {
            if (!before.map_block_index_keys.count(key)) ++added;
        }
        size_t removed = 0;
        for (const auto& key : before.map_block_index_keys) {
            if (!after.map_block_index_keys.count(key)) ++removed;
        }
        if (added || removed) {
            out.push_back({"mapBlockIndex",
                           strprintf("%u entries", before.map_block_index_keys.size()),
                           strprintf("%u entries (+%u -%u)", after.map_block_index_keys.size(), added, removed),
                           /*monotone=*/true, added > 0});
        }
    } else {
        Monotone(out, "mapBlockIndex", before.map_block_index_size, after.map_block_index_size);
    }

    Plain(out, "Params().NetworkIDString()", before.network_id, after.network_id);
    Plain(out, "GetBlockV15Height()", before.block_v15_height, after.block_v15_height);
    Plain(out, "nStakeMinAge", before.stake_min_age, after.stake_min_age);
    Plain(out, "nCoinbaseMaturity", before.coinbase_maturity, after.coinbase_maturity);
    Plain(out, "nGrandfather", before.grandfather, after.grandfather);
    Plain(out, "MAX_OUTBOUND_CONNECTIONS", before.max_outbound, after.max_outbound);

    Plain(out, "g_previous_block_time", before.previous_block_time, after.previous_block_time);
    Plain(out, "GetMockTime()", before.mock_time, after.mock_time);
    Plain(out, "g_mock_deterministic_tests", before.mock_deterministic, after.mock_deterministic);

    Monotone(out, "mempool.size()", before.mempool_size, after.mempool_size);
    Monotone(out, "g_miner_status.StakingActive()", before.staking_active ? 1 : 0, after.staking_active ? 1 : 0);
    Plain(out, "fUseFastIndex", before.use_fast_index, after.use_fast_index);

    // Settings: one diff per key, so the report names the argument.
    for (const auto& [key, value] : after.settings) {
        auto it = before.settings.find(key);
        if (it == before.settings.end()) {
            out.push_back({"settings[" + key + "]", "(absent)", value, false, false});
        } else if (it->second != value) {
            out.push_back({"settings[" + key + "]", it->second, value, false, false});
        }
    }
    for (const auto& [key, value] : before.settings) {
        if (!after.settings.count(key)) {
            out.push_back({"settings[" + key + "]", value, "(absent)", false, false});
        }
    }

    if (before.has_registries && after.has_registries) {
        const StateSnapshot* base = baseline != nullptr && baseline->has_registries ? baseline : nullptr;

        Registry(out, "GetBeaconRegistry().Beacons()", before.beacons, after.beacons,
                 base ? &base->beacons : nullptr);
        Registry(out, "GetProtocolRegistry().ProtocolEntries()", before.protocol_entries, after.protocol_entries,
                 base ? &base->protocol_entries : nullptr);
        Registry(out, "GetScraperRegistry().Scrapers()", before.scrapers, after.scrapers,
                 base ? &base->scrapers : nullptr);
        Registry(out, "GetPoolRegistry().Entries()", before.pools, after.pools,
                 base ? &base->pools : nullptr);
    }

    return out;
}

StateGuard::StateGuard(unsigned reset)
    : m_reset(reset)
    , m_network_id(Params().NetworkIDString())
    , m_stake_min_age(nStakeMinAge)
    , m_coinbase_maturity(nCoinbaseMaturity)
    , m_grandfather(nGrandfather)
    , m_max_outbound(MAX_OUTBOUND_CONNECTIONS)
    , m_mock_time(GetMockTime().count())
    , m_mock_deterministic(g_mock_deterministic_tests)
    , m_use_fast_index(fUseFastIndex)
{
    gArgs.LockSettings([&](util::Settings& settings) { m_settings = settings; });

    {
        LOCK(cs_main);

        m_pindex_best = pindexBest;
        m_pindex_genesis = pindexGenesisBlock;
        m_hash_best_chain = hashBestChain;
        m_n_best_height = nBestHeight;
        m_previous_block_time = g_previous_block_time.load();

        if (m_reset & CHAIN) {
            // Moved, not copied: each CBlockIndex::phashBlock aliases its map key.
            m_moved_index = std::move(mapBlockIndex);
            mapBlockIndex.clear();

            pindexBest = nullptr;
            pindexGenesisBlock = nullptr;
            hashBestChain = uint256();
            nBestHeight = -1;
        } else {
            m_preexisting_keys.reserve(mapBlockIndex.size());
            for (const auto& entry : mapBlockIndex) m_preexisting_keys.insert(entry.first);
        }
    }

    if (m_reset & MEMPOOL) mempool.clear();
}

StateGuard::~StateGuard()
{
    if (m_reset & MEMPOOL) mempool.clear();

    {
        LOCK(cs_main);

        if (m_reset & CHAIN) {
            mapBlockIndex.clear();
            mapBlockIndex = std::move(m_moved_index);
        } else {
            for (auto it = mapBlockIndex.begin(); it != mapBlockIndex.end();) {
                if (m_preexisting_keys.count(it->first)) {
                    ++it;
                    continue;
                }

                // Unlink the index from the chain BEFORE its key is freed.
                //
                // phashBlock aliases the map key (validation.cpp), and the index
                // objects are pool-allocated and deliberately never deleted, so
                // erasing the node leaves any surviving pointer to that index
                // holding a dangling hash. Restoring pindexBest and
                // pindexGenesisBlock below is not enough: nothing clears the
                // previous tip's pnext, so a FORWARD walk still reaches an index
                // a suite mined. CWallet::ScanForWalletTransactions does exactly
                // that walk, which is how this surfaced -- one case mined a block
                // and the next one to scan read the freed key.
                //
                // Harmless for the InsertBlockIndex fixtures this guard was
                // written for, whose entries have no pprev in the active chain;
                // load-bearing for anything that mines.
                if (CBlockIndex* pindex = it->second) {
                    if (pindex->pprev != nullptr && pindex->pprev->pnext == pindex) {
                        pindex->pprev->pnext = nullptr;
                    }
                }

                it = mapBlockIndex.erase(it);
            }
        }

        pindexBest = m_pindex_best;
        pindexGenesisBlock = m_pindex_genesis;
        hashBestChain = m_hash_best_chain;
        nBestHeight = m_n_best_height;
        g_previous_block_time.store(m_previous_block_time);
    }

    fUseFastIndex = m_use_fast_index;
    g_mock_deterministic_tests = m_mock_deterministic;
    SetMockTime(m_mock_time);

    nStakeMinAge = m_stake_min_age;
    nCoinbaseMaturity = m_coinbase_maturity;
    nGrandfather = m_grandfather;
    MAX_OUTBOUND_CONNECTIONS = m_max_outbound;

    // Settings before params: the datadir is network-specific and the path
    // cache must be dropped after both change.
    gArgs.LockSettings([&](util::Settings& settings) { settings = m_settings; });
    gArgs.ClearPathCache();
    SelectParams(m_network_id);
    gArgs.ClearPathCache();
}

RegistryReset::RegistryReset(std::initializer_list<GRC::ContractType> types)
    : m_types(types)
{
    Apply();
}

RegistryReset::~RegistryReset()
{
    Apply();
}

void RegistryReset::Apply() const
{
    LOCK(cs_main);

    for (const auto type : m_types) {
        GRC::RegistryBookmarks::GetRegistryWithDB(type).Reset();
    }
}

ForcedArgGuard::ForcedArgGuard(std::string key, const std::string& value)
    : m_key(std::move(key))
{
    gArgs.LockSettings([&](util::Settings& settings) {
        auto it = settings.forced_settings.find(m_key);
        if (it != settings.forced_settings.end()) m_saved = it->second;
    });

    gArgs.ForceSetArg("-" + m_key, value);
}

ForcedArgGuard::~ForcedArgGuard()
{
    gArgs.LockSettings([&](util::Settings& settings) {
        if (m_saved) {
            settings.forced_settings[m_key] = *m_saved;
        } else {
            settings.forced_settings.erase(m_key);
        }
    });
}

V15HeightGuard::V15HeightGuard(int height)
{
    gArgs.LockSettings([&](util::Settings& settings) {
        auto it = settings.forced_settings.find(V15_SETTING_KEY);
        if (it != settings.forced_settings.end()) m_saved = it->second;
    });

    gArgs.ForceSetArg("-blockv15height", strprintf("%d", height));
}

V15HeightGuard::~V15HeightGuard()
{
    gArgs.LockSettings([&](util::Settings& settings) {
        if (m_saved) {
            settings.forced_settings[V15_SETTING_KEY] = *m_saved;
        } else {
            settings.forced_settings.erase(V15_SETTING_KEY);
        }
    });
}

void RequireV15Unscheduled()
{
    const int v15 = GetBlockV15Height();

    BOOST_REQUIRE_MESSAGE(v15 == std::numeric_limits<int>::max(),
                          strprintf("V15 rules must be inert for this test, but GetBlockV15Height() is %d "
                                    "(-blockv15height %s)",
                                    v15,
                                    gArgs.IsArgSet("-blockv15height")
                                        ? "= " + gArgs.GetArg("-blockv15height", "")
                                        : "unset; chainparams pinned it"));
}

} // namespace grc_test
