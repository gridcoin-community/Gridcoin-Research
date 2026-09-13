// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_TEST_STATE_GUARD_H
#define GRIDCOIN_TEST_STATE_GUARD_H

#include "chain.h"
#include "gridcoin/contract/payload.h"
#include "uint256.h"
#include "util/settings.h"

#include <univalue.h>

#include <cstdint>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

//!
//! Process-state isolation helpers for the unit-test binary.
//!
//! test_gridcoin runs every suite in one process on top of a single global
//! TestingSetup (test_gridcoin.cpp). Anything a suite leaves behind in a
//! process global is therefore visible to every suite that runs after it, and
//! WHICH suites run after it is link order -- so a leak that is harmless on one
//! CI leg is a failure on another. Three helpers address that:
//!
//!  - StateSnapshot / CaptureState / DiffState: the invariant set the leak
//!    detector (state_leak_detector.cpp) checks at every suite boundary.
//!  - StateGuard: an RAII restorer for the same globals, usable as a suite
//!    decorator, a per-case fixture base, or a local.
//!  - RegistryReset and V15HeightGuard: the two mutations the guard deliberately
//!    does not cover, as opt-ins.
//!
//! WHAT IS DELIBERATELY NOT RESTORED. The LevelDB memenv behind txdb is opened
//! once with error_if_exists and every CTxDB adopts the handle, so it must never
//! be closed; pwalletMain's Berkeley DB environment is bound to the one
//! -datadir; bitdb, g_banman, g_connman and g_peerman are created exactly once
//! (the fixture asserts they are null first). None of those can be rebuilt per
//! suite without moving them out of process globals, which is a separate
//! effort. The registries are excluded because Reset() is destructive and some
//! suites need it on entry only (see RegistryReset). ArgsManager::ClearArgs()
//! wipes only the registered-argument table, which is outside the snapshot.
//! The temporary datadir is left on disk: it is under the system temp
//! directory with a random name, and creating or removing directories through
//! GetDataDir() is not portable to the Windows cross-compile leg (Wine).
//!

namespace grc_test {

//!
//! \brief The process globals the leak detector compares across a suite.
//!
//! Case scope captures only the cheap scalars; suite scope also records the
//! mapBlockIndex key set and the registry sizes, whose accessors copy.
//!
struct StateSnapshot
{
    const CBlockIndex* pindex_best{nullptr};
    const CBlockIndex* pindex_genesis{nullptr};
    uint256 hash_best_chain;
    int n_best_height{0};
    size_t map_block_index_size{0};
    bool has_block_index_keys{false};
    std::unordered_set<uint256, BlockHasher> map_block_index_keys;

    std::string network_id;
    int block_v15_height{0};
    unsigned int stake_min_age{0};
    int coinbase_maturity{0};
    int grandfather{0};
    int max_outbound{0};

    int64_t previous_block_time{0};
    int64_t mock_time{0};
    bool mock_deterministic{false};

    unsigned long mempool_size{0};
    bool staking_active{false};
    bool use_fast_index{false};

    //! "<map>:<name>" -> serialized value, over forced_settings,
    //! command_line_options and rw_settings. Presence-sensitive: a key present
    //! at its default value and an absent key are different states.
    std::map<std::string, std::string> settings;

    bool has_registry_sizes{false};
    size_t beacon_count{0};
    size_t protocol_entry_count{0};
    size_t scraper_count{0};
};

enum class CaptureScope { CASE, SUITE };

StateSnapshot CaptureState(CaptureScope scope);

//!
//! \brief One invariant that differs between two snapshots.
//!
//! \p monotone marks the size-like fields (mempool, mapBlockIndex, registries,
//! StakingActive) for which only growth is a leak: a suite that CLEARS an
//! earlier suite's residue is a cleaner, not a leaker, and several fixtures
//! (RegtestChainSetup, CleanStateGuard, RegistryReset) clear on exit by design.
//!
struct StateDiff
{
    std::string invariant;
    std::string before;
    std::string after;
    bool monotone{false};
    bool growth{false};

    //! True when this diff should fail a suite: any change for a plain field,
    //! growth only for a monotone one.
    bool IsLeak() const { return !monotone || growth; }
};

std::vector<StateDiff> DiffState(const StateSnapshot& before, const StateSnapshot& after);

//!
//! \brief Restore the process globals a suite mutates.
//!
//! Captures on construction and restores on destruction. Three ways to use it:
//!
//!     BOOST_AUTO_TEST_SUITE(x_tests, *boost::unit_test::fixture<grc_test::StateGuard>())
//!     BOOST_FIXTURE_TEST_SUITE(x_tests, grc_test::StateGuard)
//!     { grc_test::StateGuard guard; ... }
//!
//! Default mode records the mapBlockIndex key set and erases the entries a
//! suite added. Entries built by MockBlockIndex::InsertBlockIndex are
//! pool-allocated and their phashBlock aliases the map key, so erasing is the
//! whole cleanup. A fixture that points phashBlock at heap memory it owns must
//! still delete that memory itself.
//!
//! The Reset flags additionally CLEAR state on entry: MEMPOOL empties the
//! pool on entry and exit; CHAIN moves mapBlockIndex aside and nulls the tip
//! globals, which is what a suite needs before it can call LoadBlockIndex()
//! (it bails with "hashBestChain not found" whenever pindexGenesisBlock is
//! non-null). CleanStateGuard and MempoolStateGuard are the two named shapes.
//!
class StateGuard
{
public:
    //! What to clear on entry (and again on exit), beyond restoring.
    enum Reset : unsigned {
        NONE = 0,
        MEMPOOL = 1, //!< mempool.clear() on entry and exit.
        CHAIN = 2,   //!< move mapBlockIndex aside and null the tip globals on entry.
        ALL = MEMPOOL | CHAIN,
    };

    explicit StateGuard(unsigned reset = NONE);
    ~StateGuard();

    StateGuard(const StateGuard&) = delete;
    StateGuard& operator=(const StateGuard&) = delete;

private:
    unsigned m_reset;

    util::Settings m_settings;
    std::string m_network_id;
    unsigned int m_stake_min_age;
    int m_coinbase_maturity;
    int m_grandfather;
    int m_max_outbound;
    int64_t m_mock_time;
    bool m_mock_deterministic;
    bool m_use_fast_index;

    CBlockIndex* m_pindex_best;
    CBlockIndex* m_pindex_genesis;
    uint256 m_hash_best_chain;
    int m_n_best_height;
    int64_t m_previous_block_time;
    BlockMap m_moved_index;
    std::unordered_set<uint256, BlockHasher> m_preexisting_keys;
};

//! Chain and mempool cleared on entry: what a suite needs before LoadBlockIndex().
struct CleanStateGuard : StateGuard
{
    CleanStateGuard() : StateGuard(ALL) {}
};

//! Mempool cleared on entry and exit; everything else restored as found.
struct MempoolStateGuard : StateGuard
{
    MempoolStateGuard() : StateGuard(MEMPOOL) {}
};

//!
//! \brief Reset the named registries on construction AND destruction.
//!
//! Every registry reset in the tree used to be entry-only, which is how an
//! ACTIVE protocol entry from one suite reached a later suite that mined.
//! Symmetry is the point of this type.
//!
class RegistryReset
{
public:
    explicit RegistryReset(std::initializer_list<GRC::ContractType> types);
    ~RegistryReset();

    RegistryReset(const RegistryReset&) = delete;
    RegistryReset& operator=(const RegistryReset&) = delete;

private:
    std::vector<GRC::ContractType> m_types;

    void Apply() const;
};

//!
//! \brief RegistryReset with the types fixed at compile time, so it can be a
//! suite decorator: *boost::unit_test::fixture<RegistryResetFor<ContractType::PROTOCOL>>().
//!
template <GRC::ContractType... Types>
struct RegistryResetFor : RegistryReset
{
    RegistryResetFor() : RegistryReset({Types...}) {}
};

//!
//! \brief Pin -blockv15height for a scope and put it back EXACTLY.
//!
//! "Back" means presence as well as value: if the forced setting was absent
//! the key is erased again, not written as the chainparams sentinel. The leak
//! detector's settings snapshot is presence-sensitive, and an empty string is
//! not a usable "unset" either -- it would activate the gate from genesis.
//!
class V15HeightGuard
{
public:
    explicit V15HeightGuard(int height);
    ~V15HeightGuard();

    V15HeightGuard(const V15HeightGuard&) = delete;
    V15HeightGuard& operator=(const V15HeightGuard&) = delete;

private:
    std::optional<util::SettingsValue> m_saved;
};

//!
//! \brief Precondition for tests that rely on V15 rules being inert.
//!
//! Fails the case (BOOST_REQUIRE) if GetBlockV15Height() is anything other
//! than the unscheduled sentinel, naming the override that set it.
//!
void RequireV15Unscheduled();

} // namespace grc_test

#endif // GRIDCOIN_TEST_STATE_GUARD_H
