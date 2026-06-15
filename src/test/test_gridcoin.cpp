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
#include "main.h"
#include "random.h"
#include "wallet/wallet.h"

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

struct TestingSetup {
    TestingSetup() {
        SetupEnvironment();

        fs::path m_path_root = fs::temp_directory_path() / "test_common_" PACKAGE_NAME / InsecureRand256().ToString();
        fUseFastIndex = true; // Don't verify block hashes when loading
        gArgs.ForceSetArg("-datadir", m_path_root.string());
        gArgs.ClearPathCache();
        SelectParams(CBaseChainParams::MAIN);

        // Forces logger to log to the console, and also not log to the debug.log file.
        gArgs.ForceSetArg("-debuglogfile", "none");
        gArgs.SoftSetBoolArg("-printtoconsole", true);

        InitLogging();
        ECC_Start();

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
        RegisterWallet(pwalletMain);
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
