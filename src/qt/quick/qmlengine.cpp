// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/quick/qmlengine.h>

#include <qt/guilog.h>
#include <qt/quick/qmlprovider.h>

#include <QByteArray>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlError>
#include <QString>

#include <iostream>

namespace {

//! The effects probe's source uses both types instead of only importing the
//! module. A bare import or a qmldir check passes where neither type can be
//! used: ColorOverlay and DropShadow are QML files that import the module's
//! private part, whose plugin needs Qt Shader Tools, and that import is
//! resolved only when a component using one of the types is compiled.
const char* const EFFECTS_PROBE_SOURCE =
    "import QtQuick\n"
    "import Qt5Compat.GraphicalEffects\n"
    "Item {\n"
    "    Rectangle { id: probeSource; width: 1; height: 1 }\n"
    "    ColorOverlay { source: probeSource; color: \"red\" }\n"
    "    DropShadow { source: probeSource }\n"
    "}\n";

//! Every error `component` reported, one line each, or `fallback` when it
//! reported none.
QStringList ErrorLines(const QQmlComponent& component, const QString& fallback)
{
    QStringList lines;
    for (const QQmlError& error : component.errors()) lines << error.toString();
    if (lines.isEmpty()) lines << fallback;
    return lines;
}

} // namespace

void AddQmlImportPaths(QQmlEngine& engine)
{
    // Qt below 6.5 does not search qrc:/qt/qml, where the QML-file modules live.
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
}

std::unique_ptr<QObject> SetupQmlEngine(QQmlEngine& engine, QmlProvider& provider, const QUrl& root)
{
    AddQmlImportPaths(engine);
    QmlProvider::install(&provider);

    QQmlComponent component(&engine, root);
    std::unique_ptr<QObject> object(component.create());
    if (!object) {
        for (const QQmlError& error : component.errors()) {
            GUILogPrintf("QML: %s", error.toString().toStdString());
            std::cerr << "QML: " << error.toString().toStdString() << std::endl;
        }
    }
    return object;
}

QStringList CompileQmlRoot(const QUrl& root)
{
    // Declared in this order, the component is destroyed before its engine.
    QQmlEngine engine;
    AddQmlImportPaths(engine);
    QQmlComponent component(&engine, root, QQmlComponent::PreferSynchronous);
    if (component.status() == QQmlComponent::Ready) return {};
    if (component.status() == QQmlComponent::Loading) {
        // No event loop runs here, so a component still loading never finishes.
        return {QStringLiteral("%1: the root did not finish compiling").arg(root.toString())};
    }
    return ErrorLines(component, QStringLiteral("%1: the root did not compile").arg(root.toString()));
}

QStringList ProbeQmlComponent(const QByteArray& source)
{
    QQmlEngine engine;
    AddQmlImportPaths(engine);
    QQmlComponent component(&engine);
    component.setData(source, QUrl(QStringLiteral("qrc:/probe/EffectsProbe.qml")));
    // create() on a component that is not ready only prints that it is not.
    if (!component.isReady()) {
        return ErrorLines(component, QStringLiteral("the probe component did not compile"));
    }
    std::unique_ptr<QObject> object(component.create());
    if (!object) {
        return ErrorLines(component, QStringLiteral("the probe component was not created"));
    }
    return {};
}

QStringList ProbeQmlEffects()
{
    return ProbeQmlComponent(QByteArray(EFFECTS_PROBE_SOURCE));
}
