#include "uritests.h"
#include "../guiutil.h"
#include "../walletmodel.h"
#include "../qtipcserver.h"

#include <QUrl>

#include <chrono>
#include <string>
#include <thread>

#ifndef WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

void URITests::uriTests()
{
    SendCoinsRecipient rv;
    QUrl uri;
    uri.setUrl(QString("gridcoin:S9FYyGQK14zzXFnSqPLuXSGREfDjDr2G9s?req-dontexist="));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("gridcoin:S9FYyGQK14zzXFnSqPLuXSGREfDjDr2G9s?dontexist="));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("S9FYyGQK14zzXFnSqPLuXSGREfDjDr2G9s"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 0);

    uri.setUrl(QString("gridcoin:S9FYyGQK14zzXFnSqPLuXSGREfDjDr2G9s?label=Wikipedia Example Address"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("S9FYyGQK14zzXFnSqPLuXSGREfDjDr2G9s"));
    QVERIFY(rv.label == QString("Wikipedia Example Address"));
    QVERIFY(rv.amount == 0);

    uri.setUrl(QString("gridcoin:S9FYyGQK14zzXFnSqPLuXSGREfDjDr2G9s?amount=0.001"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("S9FYyGQK14zzXFnSqPLuXSGREfDjDr2G9s"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 100000);

    uri.setUrl(QString("gridcoin:S9FYyGQK14zzXFnSqPLuXSGREfDjDr2G9s?amount=1.001"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("S9FYyGQK14zzXFnSqPLuXSGREfDjDr2G9s"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 100100000);

    uri.setUrl(QString("gridcoin:S9FYyGQK14zzXFnSqPLuXSGREfDjDr2G9s?amount=100&label=Wikipedia Example"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("S9FYyGQK14zzXFnSqPLuXSGREfDjDr2G9s"));
    QVERIFY(rv.amount == 10000000000LL);
    QVERIFY(rv.label == QString("Wikipedia Example"));

    uri.setUrl(QString("gridcoin:S9FYyGQK14zzXFnSqPLuXSGREfDjDr2G9s?message=Wikipedia Example Address"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("S9FYyGQK14zzXFnSqPLuXSGREfDjDr2G9s"));
    QVERIFY(rv.label == QString());

    QVERIFY(GUIUtil::parseBitcoinURI("gridcoin://S9FYyGQK14zzXFnSqPLuXSGREfDjDr2G9s?message=Wikipedia Example Address", &rv));
    QVERIFY(rv.address == QString("S9FYyGQK14zzXFnSqPLuXSGREfDjDr2G9s"));
    QVERIFY(rv.label == QString());

    // We currently don't implement the message parameter (ok, yea, we break spec...)
    uri.setUrl(QString("gridcoin:S9FYyGQK14zzXFnSqPLuXSGREfDjDr2G9s?req-message=Wikipedia Example Address"));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("gridcoin:S9FYyGQK14zzXFnSqPLuXSGREfDjDr2G9s?amount=1,000&label=Wikipedia Example"));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("gridcoin:S9FYyGQK14zzXFnSqPLuXSGREfDjDr2G9s?amount=1,000.0&label=Wikipedia Example"));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));
}

void URITests::uriQueueNameIsPerUser()
{
#ifdef WIN32
    // The name carries the first 16 hex characters of the SHA256 of the user name
    const std::string name = GetUriQueueName();
    QCOMPARE(QString::fromStdString(name).left(12), QString("GridcoinURI-"));
    QCOMPARE(static_cast<int>(name.size()), 12 + 16);
#else
    QCOMPARE(QString::fromStdString(GetUriQueueName()), QString("GridcoinURI-%1").arg(geteuid()));
#ifdef __linux__
    const std::string path = "/dev/shm/" + GetUriQueueName();
    struct stat st;

    // Never disturb a queue that a running GUI of this user already holds
    if (stat(path.c_str(), &st) == 0) {
        QSKIP("the per-user URI queue already exists (a GUI may be running)");
    }

    char arg0[] = "test";
    char* argv[] = {arg0, nullptr};
    // The mode assertion below assumes a umask that leaves the owner bits alone
    const mode_t old_umask = umask(022);
    ipcInit(1, argv, [](const std::string&) {});
    umask(old_umask);

    QVERIFY(stat(path.c_str(), &st) == 0);
    QCOMPARE(st.st_uid, geteuid());
    QCOMPARE(static_cast<unsigned int>(st.st_mode & 07777), 0600u);

    // The listener thread removes the queue after it sees the stop flag
    ipcShutdown();
    bool removed = false;
    for (int i = 0; i < 100 && !removed; ++i) {
        removed = stat(path.c_str(), &st) != 0;
        if (!removed) std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }
    QVERIFY(removed);
#endif
#endif
}
