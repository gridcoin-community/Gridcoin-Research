#define BOOST_TEST_MODULE Gridcoin Test Suite
#include <boost/test/unit_test.hpp>

#include <leveldb/env.h>
#include <leveldb/helpers/memenv/memenv.h>

#include "banman.h"
#include "chainparams.h"
#include "wallet/db.h"
#include "main.h"
#include "txdb-leveldb.h"
#include "wallet/wallet.h"

extern CWallet* pwalletMain;
extern leveldb::DB *txdb;
extern CClientUIInterface uiInterface;

extern bool fPrintToConsole;
extern void noui_connect();
extern leveldb::Options GetOptions();

struct TestingSetup {
    TestingSetup() {
        fPrintToDebugger = true; // don't want to write to debug.log file
        fUseFastIndex = true; // Don't verify block hashes when loading
        SelectParams(CBaseChainParams::MAIN);
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
        g_banman = MakeUnique<BanMan>(GetDataDir() / "banlist.dat", &uiInterface, GetArg("-bantime", DEFAULT_MISBEHAVING_BANTIME));
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
