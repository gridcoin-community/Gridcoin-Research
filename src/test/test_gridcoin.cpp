#define BOOST_TEST_MODULE Gridcoin Test Suite
#include <boost/test/unit_test.hpp>

#include <test/test_gridcoin.h>

#include <leveldb/env.h>
#include <leveldb/helpers/memenv/memenv.h>

#include "banman.h"
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
        g_mock_deterministic_tests = true;
    }
    ~TestingSetup()
    {
        delete pwalletMain;
        pwalletMain = nullptr;
        bitdb.Flush(true);
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
