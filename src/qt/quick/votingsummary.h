// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_VOTINGSUMMARY_H
#define BITCOIN_QT_QUICK_VOTINGSUMMARY_H

#include <QtQml/qqmlregistration.h>

#include <QMetaObject>
#include <QObject>
#include <QString>
#include <QTimer>

#include <chrono>
#include <cstdint>
#include <type_traits>
#include <vector>

class QmlCallRunner;
class QQmlEngine;
class QJSEngine;
class VotingModel;

//! The QML singleton `VotingSummary`: the overview's current poll title. It is
//! owned by QmlProvider, which hands QML this instance through create(). The
//! title is read through VotingModel::getCurrentPollTitle on the call
//! runner's serial lane: at attach, on each new poll, and on a timer, as the
//! Widgets overview refreshes it.
class VotingSummary : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(VotingSummary)
    QML_SINGLETON
    Q_PROPERTY(int marker READ marker CONSTANT)
    Q_PROPERTY(QString currentPollTitle READ currentPollTitle NOTIFY currentPollTitleChanged)

public:
    //! `marker` identifies the provider that owns this instance; the title is
    //! read again every `refresh_interval`.
    explicit VotingSummary(int marker, std::chrono::milliseconds refresh_interval = std::chrono::seconds(60));

    //! The singleton factory: hands QML the installed provider's instance with
    //! C++ ownership. Halts the process when no provider is installed.
    static VotingSummary* create(QQmlEngine* qml_engine, QJSEngine* js_engine);

    //! Reads the title and follows new polls and the refresh timer. The model
    //! and the runner must outlive the matching detach().
    void attach(VotingModel& voting, QmlCallRunner& runner);
    //! Disconnects, stops the timer; a read still in flight delivers nothing.
    //! The title is kept. A detach() before attach() does nothing.
    void detach();

    int marker() const { return m_marker; }
    QString currentPollTitle() const { return m_current_poll_title; }

signals:
    void currentPollTitleChanged();

private:
    struct TitleResult;

    void postRead();
    void deliverRead(uint64_t epoch, const TitleResult& result);

    int m_marker;
    VotingModel* m_voting{nullptr};
    QmlCallRunner* m_runner{nullptr};
    std::vector<QMetaObject::Connection> m_connections;
    QTimer m_refresh_timer;
    //! Moved by every detach(); a delivery of an earlier epoch does nothing.
    uint64_t m_epoch{0};
    QString m_current_poll_title;
    bool m_read_in_flight{false};
    bool m_read_dirty{false};
};

static_assert(!std::is_default_constructible_v<VotingSummary>,
              "Qt 6.4 prefers new T over create() for a default-constructible singleton");

#endif // BITCOIN_QT_QUICK_VOTINGSUMMARY_H
