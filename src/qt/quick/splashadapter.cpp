// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/quick/splashadapter.h>

#include <qt/quick/qmlprovider.h>

#include <QJSEngine>
#include <QRegularExpression>
#include <QtGlobal>

namespace {
//! "<n>/<m> " at the start of a start-up message.
const QRegularExpression PROGRESS_PATTERN(QStringLiteral("^(\\d+)/(\\d+) "));
} // namespace

SplashAdapter::SplashAdapter(int marker)
    : QObject(nullptr), m_marker(marker)
{
}

SplashAdapter* SplashAdapter::create(QQmlEngine*, QJSEngine*)
{
    QmlProvider* provider = QmlProvider::installed();
    if (!provider) {
        // A programming error that would hand QML a null singleton; qFatal
        // halts in every build.
        qFatal("SplashAdapter::create: no QmlProvider is installed");
    }
    QJSEngine::setObjectOwnership(&provider->splash(), QJSEngine::CppOwnership);
    return &provider->splash();
}

void SplashAdapter::setActive(bool active)
{
    if (m_active == active) return;
    m_active = active;
    emit activeChanged();
}

void SplashAdapter::setFinished(bool finished)
{
    if (m_finished == finished) return;
    m_finished = finished;
    emit finishedChanged();
}

void SplashAdapter::showMessage(const QString& message, int alignment)
{
    Q_UNUSED(alignment);

    int loaded = 0;
    int total = 0;
    const QRegularExpressionMatch match = PROGRESS_PATTERN.match(message);
    if (match.hasMatch()) {
        loaded = match.captured(1).toInt();
        total = match.captured(2).toInt();
    }

    const bool progress_changed = loaded != m_loaded || total != m_total;
    const bool message_changed = message != m_message;
    m_loaded = loaded;
    m_total = total;
    m_message = message;
    if (progress_changed) emit progressChanged();
    if (message_changed) emit messageChanged();
}
