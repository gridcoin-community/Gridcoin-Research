#include <QTest>
#include <QObject>

#include "bitcoinunitstests.h"
#include "uritests.h"
#include "wallet_event_queue_tests.h"
#include "wallettxstore_tests.h"

#if defined(QT_STATICPLUGIN)
#include <QtPlugin>
#if defined(QT_QPA_PLATFORM_MINIMAL)
Q_IMPORT_PLUGIN(QMinimalIntegrationPlugin);
#endif
#if defined(QT_QPA_PLATFORM_XCB)
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin);
#elif defined(QT_QPA_PLATFORM_WINDOWS)
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
#elif defined(QT_QPA_PLATFORM_COCOA)
Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin);
#endif
#endif


// This is all you need to run all the tests
int main(int argc, char *argv[])
{
    bool fInvalid = false;

    BitcoinUnitsTests test1;
    if (QTest::qExec(&test1) != 0)
        fInvalid = true;

    URITests test2;
    if (QTest::qExec(&test2) != 0)
        fInvalid = true;

    WalletEventQueueTests test3;
    if (QTest::qExec(&test3) != 0)
        fInvalid = true;

    WalletTxStoreTests test4;
    if (QTest::qExec(&test4) != 0)
        fInvalid = true;

    return fInvalid;
}
