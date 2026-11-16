// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_WALLETOPGATE_H
#define BITCOIN_QT_QUICK_WALLETOPGATE_H

#include <qt/quick/walletpromptlatch.h>

#include <QtQml/qqmlregistration.h>

#include <QObject>
#include <QString>
#include <QtGlobal>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <vector>

class QmlCallRunner;
class QQmlEngine;
class QJSEngine;

namespace interfaces {
class Wallet;
} // namespace interfaces

//! The QML singleton `WalletOps`: the wallet-operation gate. One wallet
//! operation at a time across the module, through its prompt latch, which
//! the wallet's other passphrase prompts take too in a later change. It is
//! owned by QmlProvider, which hands QML this instance through create().
//!
//! An operation asks with begin(). The gate decides the mode from one
//! lock-state read on the call runner's serial lane; an unencrypted or fully
//! unlocked wallet is granted at once, and a locked one (or one unlocked for
//! staking only) asks QML for the passphrase through unlockRequested, which
//! QML answers with answerUnlock() or cancelUnlock(). The unlock is held by a
//! lease that gives the wallet back its scope (relocks it, or narrows it back
//! to staking only) exactly once: after the operation's last call, before an
//! exception the operation throws is classified, when the request is
//! cancelled while its answer runs, or at detach. No serial call waits for
//! the user: each prompt is emitted from the delivery of a call that has
//! returned, and each answer posts a new call. Every wallet call the gate
//! makes runs on the serial lane, except the restore detach() makes at quit.
class WalletOpGate : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(WalletOps)
    QML_SINGLETON
    Q_PROPERTY(int marker READ marker CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    //! What the passphrase is for: unlocking a locked wallet, or widening an
    //! unlock for staking only to a full one.
    enum Mode { Unlock, Elevate };
    Q_ENUM(Mode)
    //! Why an answer failed: a wrong passphrase, a wrong one holding a zero
    //! byte, or an elevation that found the wallet locked again.
    enum Failure { WrongPassphrase, WrongPassphraseNul, ExpiredDuringPrompt };
    Q_ENUM(Failure)
    enum Result { Completed, Cancelled };
    Q_ENUM(Result)

    //! Called on the GUI thread once the request may make its wallet calls.
    using Granted = std::function<void(quint64 id)>;
    //! Called on the GUI thread when the request ends without being granted.
    using Refused = std::function<void(quint64 id, Result result)>;

    //! `marker` identifies the provider that owns this instance; `latch` is
    //! the prompt latch the gate shares, and must outlive it.
    WalletOpGate(int marker, WalletPromptLatch& latch);
    ~WalletOpGate() override;

    //! The singleton factory: hands QML the installed provider's instance with
    //! C++ ownership. Halts the process when no provider is installed.
    static WalletOpGate* create(QQmlEngine* qml_engine, QJSEngine* js_engine);

    //! The wallet and the runner must outlive the matching detach().
    void attach(interfaces::Wallet& wallet, QmlCallRunner& runner);
    //! Marks the gate unattached, gives back the scope of every unlock a lease
    //! still holds (synchronously, on the GUI thread), drops the request
    //! without a signal or a callback, and releases the latch if the gate
    //! holds it. Called after the runner was drained, so no serial call runs.
    void detach();

    //! Starts a request and returns its id (from 1), or 0: unattached, the
    //! latch is held, or the runner refused the read. Neither callback runs for
    //! a request that returned 0.
    quint64 begin(Granted on_granted, Refused on_refused);
    //! Queues `fn` on the serial lane for the granted request `id`, and
    //! `deliver` after it on the GUI thread. An exception `fn` throws gives the
    //! wallet back its scope first, on the worker, and is then classified by
    //! the runner as any other. False when `id` is not granted, or when the
    //! runner refused the call.
    bool post(quint64 id, std::function<void(interfaces::Wallet&)> fn, std::function<void()> deliver);
    //! Ends the granted request `id`: the restore is queued after every call
    //! already posted for it, the latch is released and finished(id,
    //! Completed) is emitted. Does nothing for any other id.
    void end(quint64 id);

    //! Answers the request `id` with a passphrase. False, and no call, unless
    //! `id` is waiting for that answer. The passphrase becomes a SecureString
    //! at once, and is kept nowhere else.
    Q_INVOKABLE bool answerUnlock(quint64 id, const QString& passphrase);
    //! Cancels the request `id`, waiting or with its answer running. False
    //! unless `id` is open for an answer.
    Q_INVOKABLE bool cancelUnlock(quint64 id);

    int marker() const { return m_marker; }
    //! The gate holds the prompt latch.
    bool busy() const;

signals:
    void busyChanged();
    void unlockRequested(quint64 id, WalletOpGate::Mode mode, const QString& title, const QString& text);
    void unlockFailed(quint64 id, WalletOpGate::Failure reason, const QString& text);
    void unlockAccepted(quint64 id);
    void finished(quint64 id, WalletOpGate::Result result);

private:
    struct Lease;
    struct ReadResult;
    struct AnswerResult;

    enum class State { Reading, Waiting, Answering, Granted };

    //! The one open request.
    struct Request {
        quint64 id{0};
        State state{State::Reading};
        Mode mode{Unlock};
        Granted on_granted;
        Refused on_refused;
        std::shared_ptr<Lease> lease;
        std::shared_ptr<std::atomic<bool>> cancelled;
    };

    void deliverRead(quint64 id, const ReadResult& result);
    void deliverAnswer(quint64 id, const AnswerResult& result);
    //! Posts a lease's release on the serial lane and returns postSerial's
    //! result. A refused post is dropped: the lease is still kept, so detach
    //! makes it up.
    bool postRelease(const std::shared_ptr<Lease>& lease);
    //! Drops the request and releases the latch, emitting busyChanged.
    void closeRequest();
    //! Forgets the kept leases that were released, or whose request ended
    //! without arming them.
    void pruneLeases();
    bool attached() const { return m_wallet && m_runner; }

    int m_marker;
    WalletPromptLatch& m_latch;
    interfaces::Wallet* m_wallet{nullptr};
    QmlCallRunner* m_runner{nullptr};
    quint64 m_next_id{0};
    std::optional<Request> m_request;
    //! Every lease that restores something, from the read's delivery until
    //! its release has run, not only the open request's.
    std::vector<std::shared_ptr<Lease>> m_leases;
};

static_assert(!std::is_default_constructible_v<WalletOpGate>,
              "Qt 6.4 prefers new T over create() for a default-constructible singleton");

#endif // BITCOIN_QT_QUICK_WALLETOPGATE_H
