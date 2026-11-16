// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_QMLENGINE_H
#define BITCOIN_QT_QUICK_QMLENGINE_H

#include <QObject>
#include <QStringList>
#include <QUrl>

#include <memory>

class QByteArray;
class QQmlEngine;
class QmlProvider;

//! Adds the module's import paths to `engine`: qrc:/qt/qml, where the QML-file
//! modules live and which Qt below 6.5 does not search. Every engine the module
//! builds is set up through it.
void AddQmlImportPaths(QQmlEngine& engine);

//! Sets `engine` up for the module and loads the root: adds qrc:/qt/qml to the
//! import path (Qt below 6.5 does not search it, and the QML-file modules live
//! there), installs `provider` so the singletons' create() functions find it,
//! and creates the component at `root`. Returns the root object, which the
//! caller owns; null when the root did not load, after writing every error the
//! component reported to the GUI log and to stderr.
[[nodiscard]] std::unique_ptr<QObject> SetupQmlEngine(QQmlEngine& engine, QmlProvider& provider, const QUrl& root);

//! Compiles, and never creates, the component at `root` on a fresh engine with
//! the module's import paths and no provider. Returns one line per error when
//! the component is not ready, and an empty list when it compiled. It covers
//! every file in the root's static type tree; it does not cover a file reached
//! only through Qt.createComponent or a Loader, nor an error raised only when
//! the root is created.
[[nodiscard]] QStringList CompileQmlRoot(const QUrl& root);

//! Compiles `source` on a fresh engine with the module's import paths and, when
//! it compiled, creates the object and destroys it again. Returns one line per
//! error when either step failed, and an empty list when the object was
//! created.
[[nodiscard]] QStringList ProbeQmlComponent(const QByteArray& source);

//! ProbeQmlComponent of a source that creates the two Qt5Compat effects the
//! carried QML uses, ColorOverlay and DropShadow. An empty list means both can
//! be created.
[[nodiscard]] QStringList ProbeQmlEffects();

#endif // BITCOIN_QT_QUICK_QMLENGINE_H
