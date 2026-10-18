// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/quick/votingsummary.h>

#include <qt/quick/qmlcallrunner.h>
#include <qt/quick/qmlprovider.h>
#include <qt/voting/votingmodel.h>

#include <QJSEngine>

#include <memory>

//! What a title read reads. A plain value.
struct VotingSummary::TitleResult {
    QString title;
};

VotingSummary::VotingSummary(int marker, std::chrono::milliseconds refresh_interval)
    : QObject(nullptr), m_marker(marker)
{
    m_refresh_timer.setInterval(refresh_interval);
    connect(&m_refresh_timer, &QTimer::timeout, this, &VotingSummary::postRead);
}

VotingSummary* VotingSummary::create(QQmlEngine*, QJSEngine*)
{
    QmlProvider* provider = QmlProvider::installed();
    if (!provider) {
        // A programming error that would hand QML a null singleton; qFatal
        // halts in every build.
        qFatal("VotingSummary::create: no QmlProvider is installed");
    }
    QJSEngine::setObjectOwnership(&provider->votingSummary(), QJSEngine::CppOwnership);
    return &provider->votingSummary();
}

void VotingSummary::attach(VotingModel& voting, QmlCallRunner& runner)
{
    m_voting = &voting;
    m_runner = &runner;
    // A read posted by an earlier attach never delivers.
    m_read_in_flight = false;
    m_read_dirty = false;

    m_connections.push_back(connect(&voting, &VotingModel::newPollReceived, this, &VotingSummary::postRead));
    m_refresh_timer.start();
    postRead();
}

void VotingSummary::detach()
{
    for (const QMetaObject::Connection& connection : m_connections) disconnect(connection);
    m_connections.clear();
    m_refresh_timer.stop();
    ++m_epoch;
    m_voting = nullptr;
    m_runner = nullptr;
}

void VotingSummary::postRead()
{
    if (!m_voting || !m_runner) return;
    if (m_read_in_flight) {
        m_read_dirty = true;
        return;
    }

    // getCurrentPollTitle passes VotingManager::currentPollTitle() through,
    // cut to 80 characters with '_' shown as ' '.
    VotingModel& voting = *m_voting;
    auto result = std::make_shared<TitleResult>();
    const uint64_t epoch = m_epoch;
    if (m_runner->postSerial([&voting, result] { result->title = voting.getCurrentPollTitle(); },
                             [this, result, epoch] { deliverRead(epoch, *result); })) {
        m_read_in_flight = true;
        m_read_dirty = false;
    }
}

void VotingSummary::deliverRead(uint64_t epoch, const TitleResult& result)
{
    if (epoch != m_epoch) return;
    m_read_in_flight = false;
    if (result.title != m_current_poll_title) {
        m_current_poll_title = result.title;
        emit currentPollTitleChanged();
    }
    if (m_read_dirty) postRead();
}
