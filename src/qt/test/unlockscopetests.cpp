// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/unlockscopetests.h"

#include "chainparams.h"
#include "interfaces/wallet_coin_source.h"
#include "qt/optionsmodel.h"
#include "qt/test/interfacefakes.h"
#include "qt/test/modelfakes.h"
#include "qt/unlockscope.h"
#include "qt/walletmodel.h"
#include "support/allocators/secure.h"
#include "util.h"

#include <QCoreApplication>
#include <QTest>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

const std::string PASSPHRASE = "correct horse";

//! The models a test drives, in the order GuiMain builds them: the scripted
//! wallet behind a real WalletModel.
struct Models {
    qt_test::ScriptedWallet wallet;
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager side_stakes;
    OptionsModel options{node, side_stakes};
    qt_test::FakeWalletTxSource tx_source;
    std::shared_ptr<interfaces::WalletCoinSource> coin_source{interfaces::MakeSyntheticCoinSource(0, 1)};
    WalletModel model{wallet, tx_source, *coin_source, &options};
};

//! Plays the passphrase dialog on requireUnlock: records each question and,
//! when `answer` is set, unlocks the wallet (or widens a staking-only unlock)
//! with the scripted passphrase, as the dialog's OK does.
void PlayDialog(Models& m, bool answer, std::vector<bool>& asked)
{
    qt_test::ScriptedWallet* wallet = &m.wallet;
    QObject::connect(&m.model, &WalletModel::requireUnlock, &m.model, [wallet, answer, &asked](bool elevate) {
        asked.push_back(elevate);
        if (!answer) return;
        SecureString pass;
        pass.assign(PASSPHRASE.begin(), PASSPHRASE.end());
        if (elevate) {
            wallet->elevateWallet(pass);
        } else {
            wallet->unlockWallet(pass, false);
        }
    });
}

} // namespace

void UnlockScopeTests::initTestCase()
{
    // OptionsModel opens QSettings; keep its writes out of the user's store,
    // as OptionsModelTests does.
    m_prev_organization = QCoreApplication::organizationName();
    m_prev_application = QCoreApplication::applicationName();
    m_prev_format = QSettings::defaultFormat();

    m_settings_root = std::make_unique<QTemporaryDir>();
    QVERIFY(m_settings_root->isValid());

    QCoreApplication::setOrganizationName("GridcoinTest");
    QCoreApplication::setApplicationName("unlockscopetests");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings_root->path());

    // WalletModel validates addresses against the selected chain.
    SelectParams(CBaseChainParams::REGTEST);
}

void UnlockScopeTests::cleanupTestCase()
{
    QSettings::setDefaultFormat(m_prev_format);
    QCoreApplication::setOrganizationName(m_prev_organization);
    QCoreApplication::setApplicationName(m_prev_application);
    m_settings_root.reset();
}

void UnlockScopeTests::init()
{
    // OptionsModel::Init reaches GetDataDir(false), which creates a datadir:
    // point -datadir at a temporary directory.
    m_prev_datadir_set = gArgs.IsArgSet("-datadir");
    m_prev_datadir = gArgs.GetArg("-datadir", "");
    m_data_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_data_dir->isValid());
    gArgs.ForceSetArg("-datadir", m_data_dir->path().toStdString());
    gArgs.ClearPathCache();
}

void UnlockScopeTests::cleanup()
{
    QSettings().clear();
    if (m_prev_datadir_set) {
        gArgs.ForceSetArg("-datadir", m_prev_datadir);
    } else {
        gArgs.ClearForcedArg("-datadir");
    }
    gArgs.ClearPathCache();
    m_data_dir.reset();
}

void UnlockScopeTests::unlockContextCharacterization()
{
    // A locked wallet: one unlock question, and the scope relocks.
    {
        Models m;
        m.wallet.setPassphrase(PASSPHRASE);
        m.wallet.setLockState(true, true, false);
        std::vector<bool> asked;
        PlayDialog(m, true, asked);
        bool valid = false;
        {
            WalletModel::UnlockContext ctx(m.model.requestUnlock());
            valid = ctx.isValid();
        }
        QCOMPARE(asked.size(), size_t{1});
        QCOMPARE(asked[0], false);
        QVERIFY(valid);
        QCOMPARE(m.wallet.lockCalls(), 1);
        QCOMPARE(m.wallet.restrictCalls(), 0);
    }

    // Unlocked for staking only: one elevation question, and the scope
    // narrows the wallet back instead of locking it.
    {
        Models m;
        m.wallet.setPassphrase(PASSPHRASE);
        m.wallet.setLockState(true, false, true);
        std::vector<bool> asked;
        PlayDialog(m, true, asked);
        bool valid = false;
        {
            WalletModel::UnlockContext ctx(m.model.requestUnlock());
            valid = ctx.isValid();
        }
        QCOMPARE(asked.size(), size_t{1});
        QCOMPARE(asked[0], true);
        QVERIFY(valid);
        QCOMPARE(m.wallet.elevateCalls(), 1);
        QCOMPARE(m.wallet.restrictCalls(), 1);
        QCOMPARE(m.wallet.lockCalls(), 0);
    }

    // The dialog does nothing: the context is invalid and restores nothing.
    {
        Models m;
        m.wallet.setPassphrase(PASSPHRASE);
        m.wallet.setLockState(true, true, false);
        std::vector<bool> asked;
        PlayDialog(m, false, asked);
        bool valid = true;
        {
            WalletModel::UnlockContext ctx(m.model.requestUnlock());
            valid = ctx.isValid();
        }
        QCOMPARE(asked.size(), size_t{1});
        QVERIFY(!valid);
        QCOMPARE(m.wallet.lockCalls(), 0);
        QCOMPARE(m.wallet.restrictCalls(), 0);
    }

    // Fully unlocked: no question, and nothing is restored.
    {
        Models m;
        m.wallet.setPassphrase(PASSPHRASE);
        m.wallet.setLockState(true, false, false);
        std::vector<bool> asked;
        PlayDialog(m, true, asked);
        bool valid = false;
        {
            WalletModel::UnlockContext ctx(m.model.requestUnlock());
            valid = ctx.isValid();
        }
        QCOMPARE(asked.size(), size_t{0});
        QVERIFY(valid);
        QCOMPARE(m.wallet.lockCalls(), 0);
        QCOMPARE(m.wallet.restrictCalls(), 0);
    }

    // The narrowing throws a std::exception: the scope still ends, and the
    // test goes on.
    {
        Models m;
        m.wallet.setPassphrase(PASSPHRASE);
        m.wallet.setLockState(true, false, true);
        m.wallet.setThrow(qt_test::ScriptedWallet::Method::RestrictToStakingOnly,
                          qt_test::ScriptedWallet::ThrowKind::Runtime, "restrict failed");
        std::vector<bool> asked;
        PlayDialog(m, true, asked);
        {
            WalletModel::UnlockContext ctx(m.model.requestUnlock());
            QVERIFY(ctx.isValid());
        }
        QCOMPARE(m.wallet.restrictCalls(), 1);
        QCOMPARE(m.wallet.lockCalls(), 0);
    }
}

void UnlockScopeTests::unlockContextNonStdThrow()
{
    // As above with an exception that is not a std::exception.
    Models m;
    m.wallet.setPassphrase(PASSPHRASE);
    m.wallet.setLockState(true, false, true);
    m.wallet.setThrow(qt_test::ScriptedWallet::Method::RestrictToStakingOnly,
                      qt_test::ScriptedWallet::ThrowKind::NonStd);
    std::vector<bool> asked;
    PlayDialog(m, true, asked);
    {
        WalletModel::UnlockContext ctx(m.model.requestUnlock());
        QVERIFY(ctx.isValid());
    }
    QCOMPARE(m.wallet.restrictCalls(), 1);
    QCOMPARE(m.wallet.lockCalls(), 0);
}

void UnlockScopeTests::unlockContextNarrowFailsWarnRead()
{
    // The narrowing is refused while the wallet is still fully unlocked: the
    // scope reads the lock state exactly once to decide whether to warn.
    Models m;
    m.wallet.setPassphrase(PASSPHRASE);
    m.wallet.setLockState(true, false, true);
    m.wallet.scriptRestrict(false);
    std::vector<bool> asked;
    PlayDialog(m, true, asked);
    int reads_before = 0;
    {
        WalletModel::UnlockContext ctx(m.model.requestUnlock());
        QVERIFY(ctx.isValid());
        const interfaces::WalletLockState state = m.wallet.lockState();
        QVERIFY(state.crypted && !state.locked && !state.unlocked_for_staking_only);
        reads_before = m.wallet.lockStateCalls();
    }
    QCOMPARE(m.wallet.lockStateCalls() - reads_before, 1);
    QCOMPARE(m.wallet.restrictCalls(), 1);
    QCOMPARE(m.wallet.lockCalls(), 0);
}

void UnlockScopeTests::unlockContextNarrowOkNoRead()
{
    // The narrowing succeeds: no lock-state read follows it.
    Models m;
    m.wallet.setPassphrase(PASSPHRASE);
    m.wallet.setLockState(true, false, true);
    m.wallet.scriptRestrict(true);
    std::vector<bool> asked;
    PlayDialog(m, true, asked);
    int reads_before = 0;
    {
        WalletModel::UnlockContext ctx(m.model.requestUnlock());
        QVERIFY(ctx.isValid());
        reads_before = m.wallet.lockStateCalls();
    }
    QCOMPARE(m.wallet.lockStateCalls() - reads_before, 0);
    QCOMPARE(m.wallet.restrictCalls(), 1);
    QCOMPARE(m.wallet.lockCalls(), 0);
}

void UnlockScopeTests::restoreWalletScope()
{
    using Method = qt_test::ScriptedWallet::Method;

    // Relock: the wallet is locked.
    {
        qt_test::ScriptedWallet wallet;
        wallet.setLockState(true, false, false);
        QCOMPARE(RestoreWalletScope(wallet, true, false, "test"), RestoreOutcome::Relocked);
        QCOMPARE(wallet.lockCalls(), 1);
        QCOMPARE(wallet.restrictCalls(), 0);
    }

    // Restore staking only: the wallet is narrowed, not locked.
    {
        qt_test::ScriptedWallet wallet;
        wallet.setLockState(true, false, false);
        QCOMPARE(RestoreWalletScope(wallet, false, true, "test"), RestoreOutcome::Narrowed);
        QCOMPARE(wallet.restrictCalls(), 1);
        QCOMPARE(wallet.lockCalls(), 0);
    }

    // The narrowing is refused: still fully unlocked, or locked meanwhile.
    {
        qt_test::ScriptedWallet wallet;
        wallet.setLockState(true, false, false);
        wallet.scriptRestrict(false);
        QCOMPARE(RestoreWalletScope(wallet, false, true, "test"), RestoreOutcome::NarrowFailedLeftOpen);
        QCOMPARE(wallet.lockStateCalls(), 1);
        QCOMPARE(wallet.lockCalls(), 0);
    }
    {
        qt_test::ScriptedWallet wallet;
        wallet.setLockState(true, true, false);
        wallet.scriptRestrict(false);
        QCOMPARE(RestoreWalletScope(wallet, false, true, "test"), RestoreOutcome::NarrowRefusedLocked);
        QCOMPARE(wallet.lockStateCalls(), 1);
        QCOMPARE(wallet.lockCalls(), 0);
    }

    // Neither flag: no wallet call.
    {
        qt_test::ScriptedWallet wallet;
        wallet.setLockState(true, false, false);
        QCOMPARE(RestoreWalletScope(wallet, false, false, "test"), RestoreOutcome::Nothing);
        QCOMPARE(wallet.log().size(), size_t{0});
    }

    // A std::exception from the wallet: reported as Threw, nothing escapes.
    {
        qt_test::ScriptedWallet wallet;
        wallet.setLockState(true, false, false);
        wallet.setThrow(Method::LockWallet, qt_test::ScriptedWallet::ThrowKind::Runtime, "lock failed");
        bool escaped = false;
        RestoreOutcome outcome = RestoreOutcome::Nothing;
        try {
            outcome = RestoreWalletScope(wallet, true, false, "test");
        } catch (...) {
            escaped = true;
        }
        QVERIFY(!escaped);
        QCOMPARE(outcome, RestoreOutcome::Threw);
        QCOMPARE(wallet.lockCalls(), 1);
    }
}

void UnlockScopeTests::restoreWalletScopeNonStd()
{
    // An exception that is not a std::exception: reported as Threw, nothing
    // escapes.
    qt_test::ScriptedWallet wallet;
    wallet.setLockState(true, false, false);
    wallet.setThrow(qt_test::ScriptedWallet::Method::RestrictToStakingOnly,
                    qt_test::ScriptedWallet::ThrowKind::NonStd);
    bool escaped = false;
    RestoreOutcome outcome = RestoreOutcome::Nothing;
    try {
        outcome = RestoreWalletScope(wallet, false, true, "test");
    } catch (...) {
        escaped = true;
    }
    QVERIFY(!escaped);
    QCOMPARE(outcome, RestoreOutcome::Threw);
    QCOMPARE(wallet.restrictCalls(), 1);
}
