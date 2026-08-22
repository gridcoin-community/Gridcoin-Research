// Copyright (c) 2014-2025 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#define BOOST_TEST_MODULE Gridcoin Test Suite
#include <boost/test/unit_test.hpp>

#include <test/test_gridcoin.h>

#include <leveldb/env.h>
#include <leveldb/helpers/memenv/memenv.h>

#include "banman.h"
#include "net_processing.h"
#include "chainparams.h"
#include "dbwrapper.h"
#include "wallet/db.h"
#include "random.h"
#include "wallet/wallet.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <system_error>
#include <typeinfo>

leveldb::Env* txdb_env;

extern CWallet* pwalletMain;
extern leveldb::DB *txdb;
extern CClientUIInterface uiInterface;
/**
 * Flag to make GetRand in random.h return the same number
 */
extern bool g_mock_deterministic_tests;

FastRandomContext g_insecure_rand_ctx;

extern void SetupEnvironment();
extern void noui_connect();
extern leveldb::Options GetOptions();
extern void InitLogging();

//! Names the setup step in progress.
//!
//! Everything up to InitLogging() runs before the logger exists, so a throw in
//! that window produces a process with NO OUTPUT AT ALL -- ctest reports only
//! Boost's "Test setup error: ..." line and nothing else. That is precisely
//! where the intermittent Alpine CI failure lands ("system_error ... Out of
//! memory", ~0.01s, zero captured output), which is why it has resisted
//! diagnosis: the one code path with no logging is the one that fails.
//!
//! These write to stderr directly, for the same reason: no logger yet.
static const char* g_setup_step = "(not started)";

//! Report what threw and, where the type carries one, the underlying error
//! code -- the errno is the datum that would actually identify the failure,
//! and it is the one Boost's summary discards.
static void ReportSetupThrow()
{
    // Capture errno FIRST. Everything below allocates and formats, any of which
    // may set errno itself -- reporting it at the end would describe this
    // function rather than the failure it is meant to describe.
    const int saved_errno = errno;

    try {
        std::string detail;
        try {
            throw;
        } catch (const fs::filesystem_error& e) {
            // fs:: is boost::filesystem, whose errors derive from
            // std::runtime_error rather than std::system_error, so they need
            // their own arm to keep the error code.
            detail = strprintf("fs::filesystem_error  code=%d (%s)  path=%s  what=%s",
                               e.code().value(), e.code().category().name(),
                               e.path1().string(), e.what());
        } catch (const std::system_error& e) {
            detail = strprintf("std::system_error  code=%d (%s)  what=%s",
                               e.code().value(), e.code().category().name(), e.what());
        } catch (const std::exception& e) {
            detail = strprintf("%s  what=%s", typeid(e).name(), e.what());
        } catch (...) {
            detail = "(not derived from std::exception)";
        }

        const std::string message = strprintf(
            "TestingSetup threw during step: %s\n  %s\n  errno at throw: %d (%s)",
            g_setup_step, detail, saved_errno, std::strerror(saved_errno));

        // PrintException()'s shape (src/util/system.cpp): LogPrintf for the log,
        // which buffers until the logger opens, plus a direct write to stderr
        // because a buffered message is lost if the process dies first.
        LogPrintf("\n\n************************\n%s", message);
        tfm::format(std::cerr, "\n\n************************\n%s\n", message.c_str());
    } catch (...) {
        // The diagnostic itself failed -- std::bad_alloc being the obvious way,
        // and memory exhaustion is one of the conditions under investigation.
        // Fall back to something that neither allocates nor formats, so the
        // failure being diagnosed is still named instead of being masked by a
        // second exception escaping a catch handler.
        std::fputs("\n\n************************\nTestingSetup threw during step: ", stderr);
        std::fputs(g_setup_step, stderr);
        std::fputs("\n(diagnostic formatting failed; original exception follows)\n", stderr);
        std::fflush(stderr);
    }
}

struct TestingSetup {
    TestingSetup() {
        try {
            g_setup_step = "SetupEnvironment()";
            SetupEnvironment();

            g_setup_step = "fs::temp_directory_path()";
            fs::path m_path_root = fs::temp_directory_path() / "test_common_" PACKAGE_NAME / InsecureRand256().ToString();
            fUseFastIndex = true; // Don't verify block hashes when loading
            g_setup_step = "gArgs -datadir / SelectParams";
            gArgs.ForceSetArg("-datadir", m_path_root.string());
            gArgs.ClearPathCache();
            SelectParams(CBaseChainParams::MAIN);

            // Forces logger to log to the console, and also not log to the debug.log file.
            gArgs.ForceSetArg("-debuglogfile", "none");
            gArgs.SoftSetBoolArg("-printtoconsole", true);

            g_setup_step = "InitLogging()";
            InitLogging();
            g_setup_step = "ECC_Start()";
            ECC_Start();
            g_setup_step = "(past the pre-logging window)";
        } catch (...) {
            ReportSetupThrow();
            throw;
        }

        // TODO: Refactor CTxDB to something like bitcoin's current CDBWrapper and remove this workaround.
        leveldb::Options db_options;
        db_options.env = txdb_env = leveldb::NewMemEnv(leveldb::Env::Default()); // Use a memory environment to avoid polluting the production leveldb.
        db_options.create_if_missing = true;
        db_options.error_if_exists = true;
        assert(leveldb::DB::Open(db_options, "", &txdb).ok());
        noui_connect();
        bitdb.MakeMock();
        bool fFirstRun;
        pwalletMain = new CWallet("wallet.dat");
        pwalletMain->LoadWallet(fFirstRun);
        // Ban manager instance should not already be instantiated
        assert(!g_banman);
        // Create ban manager instance.
        g_banman = std::make_unique<BanMan>(GetDataDir() / "banlist.dat", &uiInterface, gArgs.GetArg("-bantime", DEFAULT_MISBEHAVING_BANTIME));
        // Mirror AppInit2 (issue #2558 PR 8b): peer-misbehavior scores live in
        // PeerManagerImpl now, so the fixture must build a (quiescent) g_connman
        // + g_peerman and register the same clear callback -- otherwise
        // CNode::Misbehaving (exercised by DoS_tests) would no-op and
        // ClearBanned()/Unban() would not reset scores, leaking state across
        // cases.
        assert(!g_connman);
        g_connman = std::make_unique<CConnman>(0, 0);
        g_connman->Init(CConnman::Options{});
        assert(!g_peerman);
        g_peerman = PeerManager::make(*g_connman, g_banman.get());
        g_banman->SetMisbehaviorClearCallback([](const CSubNet& sub_net) -> unsigned int {
            return g_peerman ? g_peerman->ClearMisbehaviorForSubnet(sub_net) : 0u;
        });
        g_mock_deterministic_tests = true;
    }
    ~TestingSetup()
    {
        delete pwalletMain;
        pwalletMain = nullptr;
        bitdb.Flush(true);
        // Tear down in reverse construction order: g_peerman holds a raw
        // BanMan* and forwards to g_connman-associated state (issue #2558 PR 8b).
        g_peerman.reset();
        g_connman.reset();
        g_banman.reset();
        delete txdb;
        delete txdb_env;
        txdb = nullptr;
        txdb_env = nullptr;
        g_mock_deterministic_tests = false;
        ECC_Stop();
    }
};

BOOST_GLOBAL_FIXTURE(TestingSetup);
