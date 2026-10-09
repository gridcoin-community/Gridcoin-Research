// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_QMLENGINE_H
#define BITCOIN_QT_QUICK_QMLENGINE_H

#include <QObject>
#include <QUrl>

#include <memory>

class QQmlEngine;
class QmlProvider;

//! Sets `engine` up for the module and loads the root: adds qrc:/qt/qml to the
//! import path (Qt below 6.5 does not search it, and the QML-file modules live
//! there), installs `provider` so the singletons' create() functions find it,
//! and creates the component at `root`. Returns the root object, which the
//! caller owns; null when the root did not load, after logging every error the
//! component reported.
[[nodiscard]] std::unique_ptr<QObject> SetupQmlEngine(QQmlEngine& engine, QmlProvider& provider, const QUrl& root);

#endif // BITCOIN_QT_QUICK_QMLENGINE_H
