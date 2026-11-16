// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/quick/walletopgate.h>

#include <interfaces/wallet.h>
#include <qt/guiconstants.h>
#include <qt/quick/qmlcallrunner.h>
#include <qt/quick/qmlprovider.h>
#include <qt/unlockscope.h>
#include <support/allocators/secure.h>
#include <support/cleanse.h>

#include <QByteArray>
#include <QCoreApplication>
#include <QJSEngine>

#include <algorithm>
#include <string>
#include <utility>

namespace {

QString UnlockTitle()
{
    return QCoreApplication::translate("AskPassphraseDialog", "Unlock wallet");
}

QString UnlockText()
{
    return QCoreApplication::translate("AskPassphraseDialog", "This operation needs your wallet passphrase. The wallet is unlocked for the operation and returns to its previous state afterwards.");
}

//! AskPassphraseDialog::accept's failure texts, with their context.
QString FailureText(WalletOpGate::Failure reason)
{
    switch (reason) {
    case WalletOpGate::WrongPassphrase:
        return QCoreApplication::translate("AskPassphraseDialog",
                                           "The passphrase entered for the wallet decryption was incorrect.");
    case WalletOpGate::WrongPassphraseNul:
        return QCoreApplication::translate("AskPassphraseDialog",
                                           "The passphrase entered for the wallet decryption is incorrect. "
                                           "It contains a null character (ie - a zero byte). "
                                           "If the passphrase was set with a version of this software prior to 5.4.6, "
                                           "please try again with only the characters up to — but not including — "
                                           "the first null character. If this is successful, please set a new "
                                           "passphrase to avoid this issue in the future.");
    case WalletOpGate::ExpiredDuringPrompt:
        return QCoreApplication::translate("AskPassphraseDialog",
                                           "The wallet locked itself while you were entering your "
                                           "passphrase, because the unlock you had asked for ran out. "
                                           "Unlock the wallet and try again.");
    }
    return QString();
}

} // namespace

//! What an operation's unlock owes the wallet: relocking it, or narrowing it
//! back to staking only, once the unlock was armed. The release runs exactly
//! once, on whichever thread gets there first.
struct WalletOpGate::Lease {
    Lease(interfaces::Wallet& wallet_in, bool relock_in, bool restore_staking_only_in)
        : wallet(wallet_in), relock(relock_in), restore_staking_only(restore_staking_only_in)
    {
    }

    interfaces::Wallet& wallet;
    bool relock;
    bool restore_staking_only;
    //! The unlock this lease restores has happened.
    std::atomic<bool> armed{false};
    std::atomic<bool> released{false};

    //! True when the lease has a scope to give back.
    bool restores() const { return relock || restore_staking_only; }

    void release()
    {
        if (armed && !released.exchange(true)) {
            RestoreWalletScope(wallet, relock, restore_staking_only, "WalletOpGate");
        }
    }
};

//! What the lock-state read reads. A plain value.
struct WalletOpGate::ReadResult {
    interfaces::WalletLockState lock_state;
};

//! What an answer's unlock call found. A plain value.
struct WalletOpGate::AnswerResult {
    bool unlocked{false};
    Failure failure{WrongPassphrase};
};

WalletOpGate::WalletOpGate(int marker, WalletPromptLatch& latch)
    : QObject(nullptr), m_marker(marker), m_latch(latch)
{
}

WalletOpGate::~WalletOpGate() = default;

WalletOpGate* WalletOpGate::create(QQmlEngine*, QJSEngine*)
{
    QmlProvider* provider = QmlProvider::installed();
    if (!provider) {
        // A programming error that would hand QML a null singleton; qFatal
        // halts in every build.
        qFatal("WalletOpGate::create: no QmlProvider is installed");
    }
    QJSEngine::setObjectOwnership(&provider->walletOps(), QJSEngine::CppOwnership);
    return &provider->walletOps();
}

bool WalletOpGate::busy() const
{
    return m_latch.heldBy(WalletPromptLatch::Owner::WalletOps);
}

void WalletOpGate::attach(interfaces::Wallet& wallet, QmlCallRunner& runner)
{
    m_wallet = &wallet;
    m_runner = &runner;
}

void WalletOpGate::detach()
{
    m_wallet = nullptr;
    m_runner = nullptr;
    // The runner was drained, so no serial call runs and every lease's armed
    // flag is final. These are the gate's only wallet calls on the GUI thread;
    // narrowing to staking only waits there for any other holder of the
    // wallet's lock.
    for (const std::shared_ptr<Lease>& lease : m_leases) lease->release();
    m_leases.clear();
    m_request.reset();
    if (busy()) {
        m_latch.release(WalletPromptLatch::Owner::WalletOps);
        emit busyChanged();
    }
}

quint64 WalletOpGate::begin(Granted on_granted, Refused on_refused)
{
    if (!attached()) return 0;
    if (!m_latch.tryAcquire(WalletPromptLatch::Owner::WalletOps)) return 0;
    emit busyChanged();

    // A release a moved generation dropped is made up here, before this
    // request's read, so the read sees the wallet as the last operation left
    // it.
    pruneLeases();
    for (const std::shared_ptr<Lease>& lease : m_leases) {
        // A refused post means a closed runner: the read below is refused
        // too, and detach makes the release up.
        if (lease->armed && !lease->released && !postRelease(lease)) break;
    }

    const quint64 id = m_next_id + 1;
    interfaces::Wallet& wallet = *m_wallet;
    auto result = std::make_shared<ReadResult>();
    Request request;
    request.id = id;
    request.state = State::Reading;
    request.on_granted = std::move(on_granted);
    request.on_refused = std::move(on_refused);
    m_request = std::move(request);
    if (!m_runner->postSerial([&wallet, result] { result->lock_state = wallet.getLockState(); },
                              [this, id, result] { deliverRead(id, *result); })) {
        m_request.reset();
        m_latch.release(WalletPromptLatch::Owner::WalletOps);
        emit busyChanged();
        return 0;
    }
    m_next_id = id;
    return id;
}

void WalletOpGate::deliverRead(quint64 id, const ReadResult& result)
{
    if (!m_request || m_request->id != id || m_request->state != State::Reading) return;
    const interfaces::WalletLockState& state = result.lock_state;
    interfaces::Wallet& wallet = *m_wallet;

    if (!state.crypted || (!state.locked && !state.unlocked_for_staking_only)) {
        // Nothing to unlock: the lease restores nothing.
        m_request->lease = std::make_shared<Lease>(wallet, false, false);
        m_request->state = State::Granted;
        const Granted granted = m_request->on_granted;
        granted(id);
        return;
    }

    // The mode is decided by this read and kept with the request: it is never
    // read again, so an unlock for staking that expires while the prompt is
    // open is reported, not quietly turned into a fresh unlock.
    const Mode mode = state.locked ? Unlock : Elevate;
    m_request->mode = mode;
    m_request->lease = std::make_shared<Lease>(wallet, mode == Unlock, mode == Elevate);
    m_leases.push_back(m_request->lease);
    m_request->state = State::Waiting;
    emit unlockRequested(id, mode, UnlockTitle(), UnlockText());
}

bool WalletOpGate::answerUnlock(quint64 id, const QString& passphrase)
{
    if (!attached()) return false;
    if (!m_request || m_request->id != id || m_request->state != State::Waiting) return false;

    // As AskPassphraseDialog converts it: the reserve comes before the
    // assign, because a short passphrase would otherwise sit in the string's
    // inline buffer, which the secure allocator never cleanses.
    auto pass = std::make_shared<SecureString>();
    pass->reserve(MAX_PASSPHRASE_SIZE);
    QByteArray bytes = passphrase.toUtf8();
    pass->assign(bytes.constData(), bytes.size());
    memory_cleanse(bytes.data(), bytes.size());

    interfaces::Wallet& wallet = *m_wallet;
    const std::shared_ptr<Lease> lease = m_request->lease;
    auto cancelled = std::make_shared<std::atomic<bool>>(false);
    auto result = std::make_shared<AnswerResult>();
    const Mode mode = m_request->mode;
    m_request->cancelled = cancelled;
    m_request->state = State::Answering;
    if (!m_runner->postSerial(
            [&wallet, lease, cancelled, pass, result, mode] {
                const bool unlocked = mode == Unlock ? wallet.unlockWallet(*pass, false) : wallet.elevateWallet(*pass);
                if (unlocked) {
                    lease->armed = true;
                    // Cancelled while this call ran: give the scope back here,
                    // on the worker, before anything else can use it.
                    if (*cancelled) lease->release();
                    result->unlocked = true;
                    return;
                }
                const interfaces::WalletLockState state = wallet.getLockState();
                if (mode == Elevate && state.locked) {
                    result->failure = ExpiredDuringPrompt;
                } else if (pass->find('\0') != std::string::npos) {
                    result->failure = WrongPassphraseNul;
                } else {
                    result->failure = WrongPassphrase;
                }
            },
            [this, id, result] { deliverAnswer(id, *result); })) {
        m_request->cancelled.reset();
        m_request->state = State::Waiting;
        return false;
    }
    return true;
}

void WalletOpGate::deliverAnswer(quint64 id, const AnswerResult& result)
{
    if (!m_request || m_request->id != id || m_request->state != State::Answering) return;

    if (*m_request->cancelled) {
        // The worker armed the lease after the cancel's flag read, or read the
        // flag before the cancel: either way the scope is given back on the
        // serial lane, never here.
        // A refused post is dropped: the lease is kept, so detach makes it
        // up.
        const std::shared_ptr<Lease> lease = m_request->lease;
        if (lease->armed && !lease->released) (void)postRelease(lease);
        const Refused refused = m_request->on_refused;
        closeRequest();
        emit finished(id, Cancelled);
        refused(id, Cancelled);
        return;
    }

    if (result.unlocked) {
        m_request->state = State::Granted;
        const Granted granted = m_request->on_granted;
        emit unlockAccepted(id);
        granted(id);
        return;
    }

    m_request->state = State::Waiting;
    m_request->cancelled.reset();
    emit unlockFailed(id, result.failure, FailureText(result.failure));
}

bool WalletOpGate::cancelUnlock(quint64 id)
{
    if (!attached()) return false;
    if (!m_request || m_request->id != id) return false;

    if (m_request->state == State::Waiting) {
        const Refused refused = m_request->on_refused;
        closeRequest();
        emit finished(id, Cancelled);
        refused(id, Cancelled);
        return true;
    }
    if (m_request->state == State::Answering) {
        // The answer's delivery finishes the request.
        *m_request->cancelled = true;
        return true;
    }
    return false;
}

bool WalletOpGate::post(quint64 id, std::function<void(interfaces::Wallet&)> fn, std::function<void()> deliver)
{
    if (!attached()) return false;
    if (!m_request || m_request->id != id || m_request->state != State::Granted) return false;

    interfaces::Wallet& wallet = *m_wallet;
    const std::shared_ptr<Lease> lease = m_request->lease;
    return m_runner->postSerial(
        [&wallet, lease, fn = std::move(fn)] {
            try {
                fn(wallet);
            } catch (...) {
                // Give the scope back before the runner classifies the
                // exception: a disconnect drops the delivery, and any other
                // exception is rethrown on the GUI thread.
                lease->release();
                throw;
            }
        },
        std::move(deliver));
}

void WalletOpGate::end(quint64 id)
{
    if (!attached()) return;
    if (!m_request || m_request->id != id || m_request->state != State::Granted) return;

    // Queued after every call already posted for the request, so the restore
    // follows the operation's last call. A refused post is dropped: the lease
    // is kept, so detach makes it up.
    const std::shared_ptr<Lease> lease = m_request->lease;
    if (lease->restores()) (void)postRelease(lease);
    closeRequest();
    emit finished(id, Completed);
}

bool WalletOpGate::postRelease(const std::shared_ptr<Lease>& lease)
{
    return m_runner->postSerial([lease] { lease->release(); }, {});
}

void WalletOpGate::closeRequest()
{
    m_request.reset();
    pruneLeases();
    m_latch.release(WalletPromptLatch::Owner::WalletOps);
    emit busyChanged();
}

void WalletOpGate::pruneLeases()
{
    const Lease* open = m_request ? m_request->lease.get() : nullptr;
    m_leases.erase(std::remove_if(m_leases.begin(), m_leases.end(),
                                  [open](const std::shared_ptr<Lease>& lease) {
                                      return lease->released || (!lease->armed && lease.get() != open);
                                  }),
                   m_leases.end());
}
