#define BOOST_TEST_MODULE Gridcoin Test Suite
#include <boost/test/unit_test.hpp>

#include "wallet/db.h"
#include "main.h"
#include "wallet/wallet.h"
#include "banman.h"

extern CWallet* pwalletMain;
extern CClientUIInterface uiInterface;

extern bool fPrintToConsole;
extern void noui_connect();

struct TestingSetup {
    TestingSetup() {
        fPrintToDebugger = true; // don't want to write to debug.log file
        fUseFastIndex = true; // Don't verify block hashes when loading
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
        pwalletMain = NULL;
        bitdb.Flush(true);
        g_banman.reset();
    }
};

BOOST_GLOBAL_FIXTURE(TestingSetup);
