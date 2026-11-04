// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/guiutiltests.h"

#include "qt/guiutil.h"
#include "util.h"

#include <QFile>
#include <QMessageBox>
#include <QSettings>
#include <QString>
#include <QTemporaryDir>

#include <string>

void GUIUtilTests::extractFirstSuffixFromFilter_data()
{
    QTest::addColumn<QString>("filter");
    QTest::addColumn<QString>("expected");

    QTest::newRow("csv") << QString("Comma separated file (*.csv)") << QString("csv");
    QTest::newRow("png") << QString("PNG Image (*.png)") << QString("png");
    QTest::newRow("psbt") << QString("Partially Signed Transaction (Binary) (*.psbt)") << QString("psbt");
    QTest::newRow("multi") << QString("Images (*.png *.jpg)") << QString("png");
    QTest::newRow("all") << QString("All files (*)") << QString();
    QTest::newRow("empty") << QString() << QString();
}

void GUIUtilTests::extractFirstSuffixFromFilter()
{
    QFETCH(QString, filter);
    QFETCH(QString, expected);

    QCOMPARE(GUIUtil::ExtractFirstSuffixFromFilter(filter), expected);
}

void GUIUtilTests::secureStringFromQStringMatchesToStdString_data()
{
    QTest::addColumn<QString>("text");

    QTest::newRow("ascii") << QStringLiteral("hunter2");
    QTest::newRow("non-ascii") << QString::fromUtf8("p\xc3\xa4 ss w\xc3\xb6rd \xe2\x82\xac");
    QTest::newRow("empty") << QString();
    QTest::newRow("spaces") << QStringLiteral(" lead and trail ");
}

// The passphrase dialog used to convert with toStdString(), so the bytes must be the same ones, or
// a passphrase set before would no longer unlock the wallet.
void GUIUtilTests::secureStringFromQStringMatchesToStdString()
{
    QFETCH(QString, text);

    const SecureString converted = GUIUtil::SecureStringFromQString(text);

    QCOMPARE(std::string(converted.begin(), converted.end()), text.toStdString());
}

// A short text must still be in the secure buffer: in the string's inline storage it would be copied
// by every move, and nothing cleanses those copies.
void GUIUtilTests::secureStringFromQStringUsesSecureBuffer()
{
    const size_t inline_capacity = SecureString().capacity();

    QVERIFY(GUIUtil::SecureStringFromQString(QStringLiteral("hunter2")).capacity() > inline_capacity);
    QVERIFY(GUIUtil::SecureStringFromQString(QString()).capacity() > inline_capacity);
}

void GUIUtilTests::persistedLaunchModeRoundTrips()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QSettings settings(dir.filePath("launch.ini"), QSettings::IniFormat);

    // A command line that sets the mode stores it.
    {
        ArgsManager args;
        args.ForceSetArg("-multiprocess", "1");
        args.ForceSetArg("-ipcdir", "/x");
        GUIUtil::ApplyPersistedLaunchMode(settings, args);
        QVERIFY(settings.value("launchMultiprocess").toBool());
        QCOMPARE(settings.value("launchIpcDir").toString(), QString("/x"));
    }

    // A fresh command line without them takes the stored values.
    {
        ArgsManager args;
        GUIUtil::ApplyPersistedLaunchMode(settings, args);
        QVERIFY(args.GetBoolArg("-multiprocess", false));
        QCOMPARE(QString::fromStdString(args.GetArg("-ipcdir", "")), QString("/x"));
        QVERIFY(!args.IsArgSet("-ipcserviceuser"));
    }

    // -nomultiprocess overrides and stores false.
    {
        ArgsManager args;
        args.ForceSetArg("-multiprocess", "0");
        GUIUtil::ApplyPersistedLaunchMode(settings, args);
        QVERIFY(!settings.value("launchMultiprocess").toBool());
        QVERIFY(!args.GetBoolArg("-multiprocess", true));
    }
    {
        ArgsManager args;
        GUIUtil::ApplyPersistedLaunchMode(settings, args);
        QVERIFY(!args.GetBoolArg("-multiprocess", true));
    }

    // -noipcdir and -noipcserviceuser mean the defaults. They clear what was remembered, so the
    // next launch takes the defaults too, not the value "0" (which as -ipcdir names <datadir>/0).
    {
        ArgsManager args;
        args.ForceSetArg("-ipcserviceuser", "gridcoin");
        GUIUtil::ApplyPersistedLaunchMode(settings, args);
        QCOMPARE(settings.value("launchIpcDir").toString(), QString("/x"));
        QCOMPARE(settings.value("launchIpcServiceUser").toString(), QString("gridcoin"));
    }
    {
        ArgsManager args;
        args.LockSettings([](util::Settings& stored) {
            util::SettingsValue negated;
            negated.setBool(false);
            stored.command_line_options["ipcdir"] = {negated};
            stored.command_line_options["ipcserviceuser"] = {negated};
        });
        QVERIFY(args.IsArgNegated("-ipcdir"));
        QVERIFY(args.IsArgNegated("-ipcserviceuser"));
        GUIUtil::ApplyPersistedLaunchMode(settings, args);
        QVERIFY(!settings.contains("launchIpcDir"));
        QVERIFY(!settings.contains("launchIpcServiceUser"));
        QVERIFY(args.IsArgNegated("-ipcdir"));
    }
    {
        ArgsManager args;
        GUIUtil::ApplyPersistedLaunchMode(settings, args);
        QVERIFY(!args.IsArgSet("-ipcdir"));
        QVERIFY(!args.IsArgSet("-ipcserviceuser"));
    }

    // An empty value means the default as well, and is not remembered.
    {
        ArgsManager args;
        args.ForceSetArg("-ipcdir", "/y");
        GUIUtil::ApplyPersistedLaunchMode(settings, args);
        QCOMPARE(settings.value("launchIpcDir").toString(), QString("/y"));
    }
    {
        ArgsManager args;
        args.ForceSetArg("-ipcdir", "");
        GUIUtil::ApplyPersistedLaunchMode(settings, args);
        QVERIFY(!settings.contains("launchIpcDir"));
    }
    {
        ArgsManager args;
        GUIUtil::ApplyPersistedLaunchMode(settings, args);
        QVERIFY(!args.IsArgSet("-ipcdir"));
    }
}

void GUIUtilTests::coreFileOpenDecision()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const fs::path existing = fs::path(dir.path().toStdString()) / "present.log";
    {
        QFile file(QString::fromStdString(existing.string()));
        QVERIFY(file.open(QIODevice::WriteOnly));
    }
    const fs::path missing = fs::path(dir.path().toStdString()) / "absent.log";

    // A core under another account is never opened from here, whether or not
    // this account can see the file.
    QVERIFY(GUIUtil::DecideCoreFileOpen(true, existing) == GUIUtil::OpenAction::ShowAdminMessage);
    QVERIFY(GUIUtil::DecideCoreFileOpen(true, missing) == GUIUtil::OpenAction::ShowAdminMessage);
    QVERIFY(GUIUtil::DecideCoreFileOpen(false, existing) == GUIUtil::OpenAction::OpenFile);
    QVERIFY(GUIUtil::DecideCoreFileOpen(false, missing) == GUIUtil::OpenAction::Missing);

    // The message names the path where it is and in the command to read it.
    const QString message = GUIUtil::AdminReadMessage(QStringLiteral("debug log"),
                                                      QStringLiteral("/var/lib/x/debug.log"));
    QCOMPARE(message.count(QStringLiteral("/var/lib/x/debug.log")), 2);
    QVERIFY(message.contains(QStringLiteral("debug log")));

    // A path that looks like markup is shown as text.
    const auto box = GUIUtil::MakeCoreFileBox(QMessageBox::Information, nullptr, QStringLiteral("t"),
                                              QStringLiteral("<b>/x</b>"));
    QVERIFY(box->textFormat() == Qt::PlainText);
}
