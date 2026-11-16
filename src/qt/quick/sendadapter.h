// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_SENDADAPTER_H
#define BITCOIN_QT_QUICK_SENDADAPTER_H

#include <qt/quick/sendrecipientmodel.h>
#include <qt/sendfeenegotiator.h>

#include <QtQml/qqmlregistration.h>

#include <QAbstractItemModel>
#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QtGlobal>

#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <vector>

class ClientModel;
class QmlCallRunner;
class QQmlEngine;
class QJSEngine;
class WalletModel;
class WalletOpGate;

//! The QML singleton `SendCoins`: the send page. It holds the rows, asks for
//! the send confirmation and any fee above the one accepted so far through
//! signals, and reports each send's outcome with the Widgets send dialog's
//! texts. It is owned by QmlProvider, which hands QML this instance through
//! create(). Amounts are halfords with a text in the display unit.
//!
//! A send asks WalletOps for the wallet, which may ask for the passphrase;
//! each attempt runs on the serial lane through the gate, and the wallet stays
//! unlocked while a fee question is open. The adapter makes no post of its
//! own. A URI waits in the core bridge while a send is in progress and is
//! claimed when it finishes.
class SendAdapter : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SendCoins)
    QML_SINGLETON
    Q_PROPERTY(int marker READ marker CONSTANT)
    Q_PROPERTY(QAbstractItemModel* recipients READ recipients CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool messageAllowed READ messageAllowed NOTIFY messageAllowedChanged)
    Q_PROPERTY(QString unitName READ unitName NOTIFY unitNameChanged)

public:
    enum Outcome { Sent, Aborted, Failed, Busy };
    Q_ENUM(Outcome)

    //! `marker` identifies the provider that owns this instance; every wallet
    //! call goes through `gate`, which must outlive this object.
    SendAdapter(int marker, WalletOpGate& gate);
    ~SendAdapter() override;

    //! The singleton factory: hands QML the installed provider's instance with
    //! C++ ownership. Halts the process when no provider is installed.
    static SendAdapter* create(QQmlEngine* qml_engine, QJSEngine* js_engine);

    //! Follows the wallet model's options (the display unit) and, given a
    //! client model, its block height. The models must outlive the matching
    //! detach(). The adapter posts nothing itself: `runner` is the one the gate
    //! was attached with.
    void attach(WalletModel& wallet, ClientModel* client, QmlCallRunner& runner);
    //! Marks the adapter unattached, disconnects from the models, drops the
    //! open request without a signal (the rows are kept), clears busy and
    //! drops the URI source.
    void detach();
    //! How the adapter reaches the core bridge's pending URI: `take` returns
    //! and clears it, `put` stores a URI there as a received one.
    void setUriSource(std::function<QString()> take, std::function<void(const QString&)> put);

    //! Row edits; false when refused (unattached, a send in progress, an
    //! invalid row, or the last row), changing nothing then.
    Q_INVOKABLE bool addRecipient();
    Q_INVOKABLE bool removeRecipient(int row);
    Q_INVOKABLE bool clearRecipients();
    //! Sets any of `recipient`, `label`, `amount` (text in the display unit),
    //! `message` and `subtractFee`; an edited field is valid again until the
    //! next send().
    Q_INVOKABLE bool updateRecipient(int row, const QVariantMap& fields);
    //! Validates every row, marks the invalid ones and returns 0 if any is
    //! invalid (asking nothing); else copies the rows into a new request, asks
    //! for the confirmation and returns the request's id. 0 while unattached
    //! or busy.
    Q_INVOKABLE quint64 send();
    //! Answers the confirmation of request `id`. False, and no call, unless
    //! the request is waiting for that answer.
    Q_INVOKABLE bool confirmSend(quint64 id, bool yes);
    //! Answers the fee question of request `id`. False, and no call, unless
    //! the request is waiting for that answer.
    Q_INVOKABLE bool confirmFee(quint64 id, bool yes);
    //! Hands a URL to the core bridge, as a received URI; false without a URI
    //! source.
    Q_INVOKABLE bool acceptUri(const QString& url);
    //! Takes the bridge's pending URI unless a send is in progress: a valid
    //! one fills the page (uriAccepted), any other is rejected (uriRejected).
    Q_INVOKABLE void claimPendingUri();
    //! The Widgets warning shown before the send page while the wallet is not
    //! in sync.
    Q_INVOKABLE QString outOfSyncTitle() const;
    Q_INVOKABLE QString outOfSyncText() const;

    int marker() const { return m_marker; }
    QAbstractItemModel* recipients() const;
    bool busy() const { return m_busy; }
    bool messageAllowed() const { return m_message_allowed; }
    QString unitName() const;

public Q_SLOTS:
    //! Whether a message can be sent at the block after `height`; unattached,
    //! nothing.
    void setChainHeight(int height);

signals:
    void busyChanged();
    void messageAllowedChanged();
    void unitNameChanged();
    void sendConfirmationRequested(quint64 id, const QString& title, const QString& text);
    void feeConfirmationRequested(quint64 id, qint64 fee, const QString& title, const QString& text);
    void finished(quint64 id, SendAdapter::Outcome outcome, const QString& title, const QString& message,
                  const QString& txid);
    void uriAccepted();
    void uriRejected(const QString& title, const QString& text);

private:
    struct AttemptResult;

    enum class State { AwaitingConfirm, AwaitingGate, AttemptInFlight, AwaitingFee };

    //! The one open request: the rows as confirmed, and the fee loop's state.
    struct Request {
        quint64 id{0};
        State state{State::AwaitingConfirm};
        quint64 gate_id{0};
        bool granted{false};
        QList<SendCoinsRecipient> recipients;
        std::unique_ptr<SendFeeNegotiator> negotiator;
    };

    bool attached() const { return m_wallet != nullptr; }
    bool validRow(int row) const;
    //! The open request if it is `id` and in `state`, else null.
    Request* requestIn(quint64 id, State state);

    void onGranted(quint64 id, quint64 gate_id);
    void onRefused(quint64 id);
    void postAttempt();
    void deliverAttempt(quint64 id, const AttemptResult& result);
    //! Every exit of a request: ends the gate's request, clears the rows on
    //! Sent, drops the request, clears busy, emits finished and claims a URI
    //! that waited.
    void finish(SendAdapter::Outcome outcome, const QString& title, const QString& message, const QString& txid);
    void setBusy(bool busy);
    void onDisplayUnitChanged(int unit);
    //! Fills the rows from a parsed URI, as SendCoinsDialog::pasteEntry does.
    void paste(const SendCoinsRecipient& recipient);

    int m_marker;
    WalletOpGate& m_gate;
    SendRecipientModel m_recipients;
    WalletModel* m_wallet{nullptr};
    std::vector<QMetaObject::Connection> m_connections;
    std::function<QString()> m_take_uri;
    std::function<void(const QString&)> m_put_uri;
    int m_unit;
    bool m_busy{false};
    bool m_message_allowed{true};
    quint64 m_next_id{0};
    std::optional<Request> m_request;
};

static_assert(!std::is_default_constructible_v<SendAdapter>,
              "Qt 6.4 prefers new T over create() for a default-constructible singleton");

#endif // BITCOIN_QT_QUICK_SENDADAPTER_H
