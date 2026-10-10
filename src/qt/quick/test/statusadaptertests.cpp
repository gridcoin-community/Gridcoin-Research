// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "statusadaptertests.h"

#include "interfacefakes_qml.h"
#include "modelharness.h"
#include "qmltestutil.h"

#include <qt/guiutil.h>
#include <qt/quick/qmlcallrunner.h>
#include <qt/quick/qmlfrontend.h>
#include <util.h>

#include <QCoreApplication>
#include <QSettings>
#include <QString>
#include <QTemporaryDir>
#include <QUrl>

#include <atomic>
#include <stdexcept>
#include <string>

namespace {

//! The root the front ends here are built with, as gridcoinresearch-qml loads
//! it.
const QUrl ROOT_URL{QStringLiteral("qrc:/qt/qml/Gridcoin/App/WindowManager.qml")};

//! A disconnect policy for a runner a test builds itself: it matches nothing,
//! so it notifies nothing.
[[maybe_unused]] QmlCallRunner::DisconnectPolicy TestPolicy()
{
    return QmlCallRunner::DisconnectPolicy{[](const std::string&) { return false; }, [](const std::string&) {}};
}

} // namespace

void QmlStatusAdapterTests::harnessBuildsModels()
{
    // (1) No harness is alive, so the throw can come only from the
    // organization check. The saved name is put back when the block ends.
    {
        const QString previous_org = QCoreApplication::organizationName();
        qml_test::ScopeExit restore_org([previous_org] { QCoreApplication::setOrganizationName(previous_org); });
        QCoreApplication::setOrganizationName(QStringLiteral("Gridcoin"));
        QVERIFY_THROWS_EXCEPTION(std::logic_error, qml_test::ModelHarness{});
    }

    // (2) A harness is built and destroyed.
    QVERIFY_THROWS_NO_EXCEPTION(qml_test::ModelHarness{});

    {
        // (3) The harness builds the five models, and their settings and
        // datadir stay under its own roots.
        qml_test::ModelHarness harness;
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        fe.attachModels(harness.bundle());

        harness.options.setMaskValues(true);
        QVERIFY(harness.options.getMaskValues());
        const QString settings_file = QSettings().fileName();
        QVERIFY2(settings_file.startsWith(harness.isolation.settingsRoot()), qPrintable(settings_file));
        const QString data_dir = GUIUtil::boostPathToQString(GetDataDir(false));
        QVERIFY2(data_dir.startsWith(harness.isolation.dataDirRoot()), qPrintable(data_dir));

        // (4) One harness at a time: a second one throws, and leaves the live
        // harness's settings path and datadir as they were.
        QVERIFY_THROWS_EXCEPTION(std::logic_error, qml_test::ModelHarness{});
        QVERIFY2(QSettings().fileName().startsWith(harness.isolation.settingsRoot()),
                 qPrintable(QSettings().fileName()));
        QVERIFY(GUIUtil::boostPathToQString(GetDataDir(false)).startsWith(harness.isolation.dataDirRoot()));

        fe.detachModels();
    }
    QVERIFY(qml_test::g_rethrown.empty());

    // (5) A harness puts back the -datadir it found.
    {
        QTemporaryDir own;
        QVERIFY(own.isValid());
        const bool was_set = gArgs.IsArgSet("-datadir");
        const std::string was = gArgs.GetArg("-datadir", "");
        qml_test::ScopeExit restore_datadir([was_set, was] {
            if (was_set) {
                gArgs.ForceSetArg("-datadir", was);
            } else {
                gArgs.ClearForcedArg("-datadir");
            }
            gArgs.ClearPathCache();
        });
        gArgs.ForceSetArg("-datadir", own.path().toStdString());
        gArgs.ClearPathCache();
        {
            qml_test::ModelHarness harness;
        }
        QCOMPARE(QString::fromStdString(gArgs.GetArg("-datadir", "")), own.path());
    }
}
