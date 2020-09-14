#include "uritests.h"
#include "../guiutil.h"
#include "../walletmodel.h"

#include <QUrl>

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
