// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "test/state_leak_detector.h"

#include "test/state_guard.h"
#include "tinyformat.h"

#include <boost/test/tree/traverse.hpp>
#include <boost/test/tree/visitor.hpp>
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace grc_test {

namespace {

using boost::unit_test::framework::master_test_suite;
using boost::unit_test::test_suite;
using boost::unit_test::test_unit;
using boost::unit_test::test_unit_id;
using boost::unit_test::TUT_CASE;
using boost::unit_test::TUT_SUITE;

//!
//! \brief Leaks that are known, tracked, and tolerated for now.
//!
//! Matching diffs are printed rather than failed. Every entry names the issue
//! or commit that will remove it. Whether an entry fires in a given run depends
//! on which suite ran first (two suites that exit with the same value are
//! mutually exclusive) and on --run_test, so an entry that did not fire is
//! reported at the end of the run but is never itself an error. Suite names
//! that do not resolve to a top-level suite are rejected at install time.
//!
struct KnownLeak
{
    const char* suite;
    const char* invariant; //!< Matched as a prefix, so "settings[" covers every argument.
    const char* note;
};

// The union of what the detector reported over Linux link order, per-suite
// isolation, and --random seeds 3 and 7 on the commit that introduced it.
// Each entry is removed by the commit that fixes the leak at source.
const std::vector<KnownLeak> kKnownLeaks = {
    // Chain-tip globals left pointing at freed or stack memory.
    {"block_finder_tests", "pindexBest", "BlockChain<N> points the tip at a stack array and has no destructor"},
    {"block_finder_tests", "pindexGenesisBlock", "BlockChain<N> points the tip at a stack array and has no destructor"},
    {"block_finder_tests", "nBestHeight", "BlockChain<N> points the tip at a stack array and has no destructor"},
    {"Superblock", "pindexBest", "two cases new/delete CBlockIndex into the tip without nulling"},
    {"Superblock", "pindexGenesisBlock", "two cases new/delete CBlockIndex into the tip without nulling"},
    {"Superblock", "nBestHeight", "two cases new/delete CBlockIndex into the tip without nulling"},
    {"coinstake_construction_tests", "pindexBest", "fixture restores neither tip pointer after freeing the index"},
    {"coinstake_construction_tests", "pindexGenesisBlock", "fixture restores neither tip pointer after freeing the index"},
    {"mrc_tests", "pindexGenesisBlock", "fixture frees the genesis index and leaves the pointer"},

    // Determinism and mock time.
    {"random_tests", "g_mock_deterministic_tests", "fastrandom_tests turns determinism off and never back on"},
    {"DoS_tests", "GetMockTime()", "DoS_bantime and DoS_misbehavior_decay never reset mock time"},
    {"mrc_tests", "GetMockTime()", "fixture sets mock time and resets it only on the happy path"},

    // -blockv15height: both suites exit with the INT_MAX sentinel PRESENT
    // where it was absent, so whichever runs first is the one that fires.
    {"pool_tests", "settings[forced:blockv15height]", "PoolLifecycleFixture writes the sentinel instead of erasing the key"},
    {"connectinputs_tests", "settings[forced:blockv15height]", "local V15HeightGuard writes the sentinel instead of erasing the key"},

    // Settings restored to a value where the key used to be absent.
    {"coinstake_construction_tests", "settings[", "fixture restores forcecpid/email/stake args to empty strings"},
    {"mrc_tests", "settings[", "fixture restores forcecpid/email to empty strings"},
    {"Researcher", "settings[", "cases restore email/forcecpid/noncruncher/pooloperator by hand"},
    {"MiningProject", "settings[forced:email]", "cases restore email by hand"},
    {"MiningProjectMap", "settings[forced:email]", "cases restore email by hand"},
    {"DoS_tests", "settings[forced:banscore]", "DoS_banscore restores -banscore to 100 rather than absent"},
    {"script_tests", "settings[forced:maxsigcachesize]", "restores -maxsigcachesize to the default rather than absent"},
    {"Whitelist", "settings[forced:autogreylistdeepcopyheight]", "DeepCopyHeightGuard writes the chainparams value on exit"},
    {"addressbook_tests", "settings[forced:rescan]", "a case forces -rescan and never clears it"},
    {"sidestake_tests", "settings[rw:", "sidestake editors write rw settings that are never removed"},
    {"util_tests", "settings[", "util_GetArg / util_ParseParameters leave test arguments behind"},
    {"getarg_tests", "settings[cmdline:", "ResetArgs re-parses test arguments and never clears them"},

    // Registries reset on entry only.
    {"gridcoin_cbr_tests", "GetProtocolRegistry().ProtocolEntries().size()", "AddProtocolEntry resets on entry only"},
    {"protocol_tests", "GetProtocolRegistry().ProtocolEntries().size()", "cases reset the registry on entry only"},
    {"Researcher", "GetProtocolRegistry().ProtocolEntries().size()", "cases reset the registry on entry only"},
    {"scraper_registry_tests", "GetScraperRegistry().Scrapers().size()", "cases reset the registry on entry only"},
    {"BeaconPayload", "GetBeaconRegistry().Beacons().size()", "BeaconRegistryTest resets on entry only"},

    // Mock chains never torn down.
    {"BeaconPayload", "mapBlockIndex", "BeaconRegistryTest inserts every block of the fixture file and erases none"},
    {"BeaconPayload", "hashBestChain", "BeaconRegistryTest advances hashBestChain per block and never restores it"},

    // Mempool.
    {"psgt_pool_tests", "mempool.size()", "a case leaves its funding transaction in the pool"},
};

bool Matches(const KnownLeak& known, const std::string& suite, const std::string& invariant)
{
    return suite == known.suite && invariant.rfind(known.invariant, 0) == 0;
}

const KnownLeak* FindKnown(const std::string& suite, const std::string& invariant)
{
    for (const auto& known : kKnownLeaks) {
        if (Matches(known, suite, invariant)) return &known;
    }
    return nullptr;
}

//! The top-level suite a test unit belongs to (its own id for a top-level suite).
const test_unit& TopLevelSuiteOf(const test_unit& tu)
{
    const test_unit_id master = master_test_suite().p_id;
    const test_unit* cur = &tu;

    while (cur->p_parent_id != master) {
        if (cur->p_parent_id == boost::unit_test::INV_TEST_UNIT_ID) break;
        cur = &boost::unit_test::framework::get(cur->p_parent_id, TUT_SUITE);
    }

    return *cur;
}

struct Blame
{
    std::string test_case;
    StateDiff diff;
};

//!
//! Per-case capture for attribution only. Nothing here may assert: an
//! assertion from an observer hook does not reach the exit code.
//!
class StateLeakObserver : public boost::unit_test::test_observer
{
public:
    int priority() override { return 5; }

    void test_unit_start(const test_unit& tu) override
    {
        if (tu.p_type != TUT_CASE) return;
        m_case_before = CaptureState(CaptureScope::CASE);
    }

    void test_unit_finish(const test_unit& tu, unsigned long) override
    {
        if (tu.p_type != TUT_CASE) return;

        const StateSnapshot after = CaptureState(CaptureScope::CASE);
        const std::string suite = TopLevelSuiteOf(tu).p_name.get();

        for (const auto& diff : DiffState(m_case_before, after)) {
            m_blame[suite].push_back({tu.p_name.get(), diff});
        }
    }

    void test_finish() override
    {
        std::ostream& out = std::cerr;

        if (!m_reported.empty()) {
            out << "\nCross-suite state leaks reported this run:\n";
            for (const auto& line : m_reported) out << "  " << line << "\n";
        }

        std::vector<std::string> unfired;
        for (const auto& known : kKnownLeaks) {
            if (!m_fired.count(std::string(known.suite) + "\n" + known.invariant)) {
                unfired.push_back(strprintf("%s / %s (%s)", known.suite, known.invariant, known.note));
            }
        }
        if (!unfired.empty()) {
            out << "\nkKnownLeaks entries that did not fire in this run (order- or filter-dependent; "
                   "remove an entry only once it is fixed at source):\n";
            for (const auto& line : unfired) out << "  " << line << "\n";
        }
        if (!m_reported.empty() || !unfired.empty()) out << std::endl;
    }

    //! The first case that changed \p invariant in \p suite, or empty.
    std::string FirstBlame(const std::string& suite, const std::string& invariant) const
    {
        auto it = m_blame.find(suite);
        if (it == m_blame.end()) return {};

        for (const auto& blame : it->second) {
            if (blame.diff.invariant == invariant) return blame.test_case;
        }
        return {};
    }

    void Record(const std::string& line) { m_reported.push_back(line); }

    void MarkFired(const KnownLeak& known)
    {
        m_fired.insert(std::string(known.suite) + "\n" + known.invariant);
    }

    void ForgetSuite(const std::string& suite) { m_blame.erase(suite); }

private:
    StateSnapshot m_case_before;
    std::map<std::string, std::vector<Blame>> m_blame;
    std::vector<std::string> m_reported;
    std::set<std::string> m_fired;
};

StateLeakObserver& Observer()
{
    static StateLeakObserver observer;
    return observer;
}

//!
//! Attached at the FRONT of every top-level suite's fixture list, so its
//! setup() runs before, and its teardown() after, any fixture the suite
//! declared for itself. A suite decorator such as RegtestChainSetup is
//! therefore checked, not bypassed.
//!
class SuiteLeakFixture : public boost::unit_test::test_unit_fixture
{
public:
    explicit SuiteLeakFixture(std::string suite) : m_suite(std::move(suite)) {}

    void setup() override
    {
        m_before = CaptureState(CaptureScope::SUITE);
    }

    void teardown() override
    {
        const StateSnapshot after = CaptureState(CaptureScope::SUITE);

        for (const auto& diff : DiffState(m_before, after)) {
            const std::string blame = Observer().FirstBlame(m_suite, diff.invariant);
            const std::string where = blame.empty() ? "no single case changed it" : "first changed by " + blame;

            if (!diff.IsLeak()) {
                Observer().Record(strprintf("note: %s shrank %s: %s -> %s (a cleaner, not a leak; %s)",
                                            m_suite, diff.invariant, diff.before, diff.after, where));
                continue;
            }

            if (const KnownLeak* known = FindKnown(m_suite, diff.invariant)) {
                Observer().MarkFired(*known);
                Observer().Record(strprintf("known: %s leaked %s: %s -> %s (%s; %s)",
                                            m_suite, diff.invariant, diff.before, diff.after, known->note, where));
                continue;
            }

            const std::string message = strprintf(
                "suite %s leaked process state: %s was %s at suite entry and %s at suite exit (%s)",
                m_suite, diff.invariant, diff.before, diff.after, where);
            Observer().Record("FAIL: " + message);
            BOOST_ERROR(message);
        }

        Observer().ForgetSuite(m_suite);
    }

private:
    std::string m_suite;
    StateSnapshot m_before;
};

//! Collects the ids of the master suite's direct children that are suites.
struct TopLevelSuites : boost::unit_test::test_tree_visitor
{
    test_unit_id master{boost::unit_test::INV_TEST_UNIT_ID};
    std::vector<test_unit_id> ids;

    bool test_suite_start(const test_suite& ts) override
    {
        if (ts.p_id == master) return true;
        if (ts.p_parent_id == master) ids.push_back(ts.p_id);
        return false;
    }
};

} // anonymous namespace

void InstallStateLeakDetector()
{
    static bool installed = false;
    if (installed) return;
    installed = true;

    boost::unit_test::framework::register_observer(Observer());

    TopLevelSuites visitor;
    visitor.master = master_test_suite().p_id;
    boost::unit_test::traverse_test_tree(master_test_suite(), visitor, /*ignore_status=*/true);

    if (visitor.ids.empty()) {
        throw std::runtime_error("state leak detector: the master test suite has no child suites to attach to");
    }

    std::set<std::string> names;
    for (const test_unit_id id : visitor.ids) {
        test_unit& tu = boost::unit_test::framework::get(id, TUT_SUITE);
        names.insert(tu.p_name.get());
        tu.p_fixtures->insert(tu.p_fixtures->begin(),
                              boost::shared_ptr<SuiteLeakFixture>(new SuiteLeakFixture(tu.p_name.get())));
    }

    for (const auto& known : kKnownLeaks) {
        if (!names.count(known.suite)) {
            throw std::runtime_error(strprintf(
                "state leak detector: kKnownLeaks names suite '%s', which is not a top-level suite in this binary",
                known.suite));
        }
    }
}

} // namespace grc_test
