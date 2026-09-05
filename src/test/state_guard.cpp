// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "test/state_guard.h"

#include "chainparams.h"
#include "gridcoin/beacon.h"
#include "gridcoin/contract/registry.h"
#include "gridcoin/protocol.h"
#include "gridcoin/scraper/scraper_registry.h"
#include "gridcoin/staking/kernel.h"
#include "gridcoin/staking/status.h"
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

        // Only the registries with a cheap accessor. The whitelist, sidestake
        // and pool registries expose no size without a filtered copy; they are
        // covered by RegistryReset where a suite touches them.
        s.has_registry_sizes = true;
        s.beacon_count = GRC::GetBeaconRegistry().Beacons().size();
        s.protocol_entry_count = GRC::GetProtocolRegistry().ProtocolEntries().size();
        s.scraper_count = GRC::GetScraperRegistry().Scrapers().size();
    }

    return s;
}

std::vector<StateDiff> DiffState(const StateSnapshot& before, const StateSnapshot& after)
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

    if (before.has_registry_sizes && after.has_registry_sizes) {
        Monotone(out, "GetBeaconRegistry().Beacons().size()", before.beacon_count, after.beacon_count);
        Monotone(out, "GetProtocolRegistry().ProtocolEntries().size()", before.protocol_entry_count, after.protocol_entry_count);
        Monotone(out, "GetScraperRegistry().Scrapers().size()", before.scraper_count, after.scraper_count);
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
                } else {
                    it = mapBlockIndex.erase(it);
                }
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
