// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/quick/qmlengine.h>

#include <qt/guilog.h>
#include <qt/quick/qmlprovider.h>

#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlError>

std::unique_ptr<QObject> SetupQmlEngine(QQmlEngine& engine, QmlProvider& provider, const QUrl& root)
{
    // Qt below 6.5 does not search qrc:/qt/qml, where the QML-file modules live.
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
    QmlProvider::install(&provider);

    QQmlComponent component(&engine, root);
    std::unique_ptr<QObject> object(component.create());
    if (!object) {
        for (const QQmlError& error : component.errors()) {
            GUILogPrintf("QML: %s", error.toString().toStdString());
        }
    }
    return object;
}
