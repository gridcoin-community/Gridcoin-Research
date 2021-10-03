#define BOOST_TEST_MODULE Gridcoin Test Suite
#include <boost/test/unit_test.hpp>

#include <leveldb/env.h>
#include <leveldb/helpers/memenv/memenv.h>

#include "banman.h"
#include "chainparams.h"
#include "dbwrapper.h"
#include "wallet/db.h"
#include "main.h"
#include "wallet/wallet.h"

extern CWallet* pwalletMain;
extern leveldb::DB *txdb;
extern CClientUIInterface uiInterface;

extern void noui_connect();
extern leveldb::Options GetOptions();
extern void InitLogging();


struct TestingSetup {
    TestingSetup() {
        fs::path m_path_root = fs::temp_directory_path() / "test_common_" PACKAGE_NAME / GetRandHash().ToString();
        fUseFastIndex = true; // Don't verify block hashes when loading
        gArgs.ForceSetArg("-datadir", m_path_root.string());
        gArgs.ClearPathCache();
        SelectParams(CBaseChainParams::MAIN);

        // Forces logger to log to the console, and also not log to the debug.log file.
        gArgs.ForceSetArg("-debuglogfile", "none");
        gArgs.SoftSetBoolArg("-printtoconsole", true);

        InitLogging();

        // TODO: Refactor CTxDB to something like bitcoin's current CDBWrapper and remove this workaround.
        leveldb::Options db_options;
        db_options.env = leveldb::NewMemEnv(leveldb::Env::Default()); // Use a memory environment to avoid polluting the production leveldb.
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
    }
    ~TestingSetup()
    {
        delete pwalletMain;
        pwalletMain = nullptr;
        bitdb.Flush(true);
        g_banman.reset();
        delete txdb;
        txdb = nullptr;
    }
};

BOOST_GLOBAL_FIXTURE(TestingSetup);
