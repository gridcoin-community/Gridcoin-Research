// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/autostarttests.h"

#include "chainparams.h"
#include "qt/guiutil.h"
#include "util.h"

#include <QDir>
#include <QFile>
#include <QStringList>
#include <QTemporaryDir>

#include <string>

//!
//! \file autostarttests.cpp
//! \brief A regtest GUI must neither create nor rewrite an OS login item.
//!
//! The login item is named and argued for mainnet or testnet only, and
//! everything that is not testnet falls through to the mainnet form. On
//! regtest, GUIUtil::SetStartOnSystemStartup/GetStartOnSystemStartup must
//! therefore do nothing at all: no entry of their own, and no change to the
//! mainnet entry that a regtest launch would otherwise rewrite at startup.
//!
//! The cases drive those public entry points end to end, so they run on Linux
//! only, where the entry is a .desktop file under $XDG_CONFIG_HOME/autostart
//! and init() points that at a temporary directory. The Windows .lnk path is
//! not exercised here.
//!
//! The Qt test main selects no chain, so each case selects its own with
//! SelectParams(). A selection cannot be undone; that is harmless because no
//! other class in this binary reads Params().
//!

#ifdef Q_OS_LINUX
namespace {
//! Point the datadir at \p datadir and select \p chain, as the GUI's own
//! start-up does before it regenerates the login item.
void UseChain(const std::string& chain, const QString& datadir)
{
    gArgs.ForceSetArg("-datadir", datadir.toStdString());
    gArgs.ClearPathCache();
    SelectParams(chain);
}

QString MakeDataDir(const QTemporaryDir& root, const QString& name)
{
    const QString path = root.filePath(name);
    QDir().mkpath(path);
    return path;
}

QStringList AutostartEntries(const QTemporaryDir& config_home)
{
    return QDir(config_home.filePath("autostart")).entryList(QDir::Files, QDir::Name);
}

QByteArray ReadEntry(const QTemporaryDir& config_home, const QString& name)
{
    QFile file(config_home.filePath("autostart/" + name));
    if (!file.open(QIODevice::ReadOnly)) return QByteArray();
    return file.readAll();
}
} // anonymous namespace
#endif // Q_OS_LINUX

AutoStartTests::AutoStartTests() = default;
AutoStartTests::~AutoStartTests() = default;

void AutoStartTests::init()
{
    m_config_home = std::make_unique<QTemporaryDir>();
    m_data_root = std::make_unique<QTemporaryDir>();
    QVERIFY(m_config_home->isValid());
    QVERIFY(m_data_root->isValid());

    m_had_config_home = qEnvironmentVariableIsSet("XDG_CONFIG_HOME");
    m_saved_config_home = qgetenv("XDG_CONFIG_HOME");
    qputenv("XDG_CONFIG_HOME", QFile::encodeName(m_config_home->path()));
}

void AutoStartTests::cleanup()
{
    gArgs.ForceSetArg("-datadir", "");
    gArgs.ClearPathCache();

    if (m_had_config_home) {
        qputenv("XDG_CONFIG_HOME", m_saved_config_home);
    } else {
        qunsetenv("XDG_CONFIG_HOME");
    }

    m_config_home.reset();
    m_data_root.reset();
}

void AutoStartTests::regtestCreatesNoLoginItem()
{
#ifndef Q_OS_LINUX
    QSKIP("The login item is checked end to end on Linux only.");
#else
    UseChain(CBaseChainParams::REGTEST, MakeDataDir(*m_data_root, "regtest"));

    // Enabling autostart on regtest must write nothing, and must not report
    // itself as enabled. The result is checked after the listing, so a failure
    // shows what was written.
    const bool set_on = GUIUtil::SetStartOnSystemStartup(true, true);

    const QStringList written = AutostartEntries(*m_config_home);
    QVERIFY2(written.isEmpty(), qPrintable("regtest wrote a login item: " + written.join(", ")));
    QVERIFY(!set_on);
    QVERIFY(!GUIUtil::GetStartOnSystemStartup());

    // Control: the same call on mainnet writes into the redirected directory,
    // so the empty listing above is not a redirect that never took effect.
    UseChain(CBaseChainParams::MAIN, MakeDataDir(*m_data_root, "main"));
    QVERIFY(GUIUtil::SetStartOnSystemStartup(true, true));
    QCOMPARE(AutostartEntries(*m_config_home), QStringList{"gridcoin-mainnet.desktop"});
    QVERIFY(GUIUtil::GetStartOnSystemStartup());
#endif
}

void AutoStartTests::regtestLeavesTheMainnetLoginItemAlone()
{
#ifndef Q_OS_LINUX
    QSKIP("The login item is checked end to end on Linux only.");
#else
    // A mainnet user with autostart on. The entry names the mainnet datadir.
    const QString main_datadir = MakeDataDir(*m_data_root, "main");
    UseChain(CBaseChainParams::MAIN, main_datadir);
    QVERIFY(GUIUtil::SetStartOnSystemStartup(true, true));

    const QByteArray mainnet_entry = ReadEntry(*m_config_home, "gridcoin-mainnet.desktop");
    QVERIFY(mainnet_entry.contains(QFile::encodeName(main_datadir)));

    // The same user starts a regtest GUI on another datadir. The autostart
    // setting is shared with mainnet, so its start-up calls this with true.
    UseChain(CBaseChainParams::REGTEST, MakeDataDir(*m_data_root, "regtest"));
    const bool set_on = GUIUtil::SetStartOnSystemStartup(true, true);
    QCOMPARE(ReadEntry(*m_config_home, "gridcoin-mainnet.desktop"), mainnet_entry);
    QVERIFY(!set_on);

    // Turning autostart off from the regtest GUI must not remove it either.
    const bool set_off = GUIUtil::SetStartOnSystemStartup(false, true);
    QCOMPARE(ReadEntry(*m_config_home, "gridcoin-mainnet.desktop"), mainnet_entry);
    QVERIFY(!set_off);

    QCOMPARE(AutostartEntries(*m_config_home), QStringList{"gridcoin-mainnet.desktop"});
    QVERIFY(!GUIUtil::GetStartOnSystemStartup());
#endif
}
