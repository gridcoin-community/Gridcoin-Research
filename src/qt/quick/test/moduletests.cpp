// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "moduletests.h"

#include "interfacefakes_qml.h"

#include <qt/quick/qmlengine.h>
#include <qt/quick/qmlprovider.h>
#include <qt/quick/shelladapter.h>
#include <qt/quick/splashadapter.h>

#include <QColor>
#include <QJSEngine>
#include <QPointer>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlError>
#include <QStringList>
#include <QUrl>
#include <QtQml>

#include <memory>

//!
//! \file moduletests.cpp
//! \brief The Gridcoin.Qml module as QML sees it: its import resolves, its
//! singletons are the provider's own instances, and they outlive the engine.
//!

void QmlModuleTests::moduleResolves()
{
    qml_test::QueuingFrontEnd fake;
    auto provider = std::make_unique<QmlProvider>(fake);
    QPointer<ShellAdapter> shell_ptr(&provider->shell());
    const int provider_marker = provider->marker();

    bool cpp_owned = false;
    bool ready = false;
    bool object_made = false;
    bool root_made = false;
    QString component_errors;
    QStringList warnings;
    int marker = 0;
    QString color_name;
    {
        auto engine = std::make_unique<QQmlEngine>();
        engine->setOutputWarningsToStandardError(false);
        QObject::connect(engine.get(), &QQmlEngine::warnings, engine.get(), [&warnings](const QList<QQmlError>& list) {
            for (const QQmlError& error : list) warnings << error.toString();
        });

        auto root = SetupQmlEngine(*engine, *provider, qml_test::FIXTURE_ROOT_URL);

        QQmlComponent component(engine.get());
        component.setData(
            "import QtQuick; import Gridcoin.Qml; import FixtureTheme 1.0; "
            "QtObject { property int m: Shell.marker; property color c: FixtureTheme.c }",
            QUrl(QStringLiteral("qrc:/qmltest/inline.qml")));
        std::unique_ptr<QObject> object(component.create());

        ready = component.status() == QQmlComponent::Ready;
        component_errors = component.errorString();
        object_made = object != nullptr;
        root_made = root != nullptr;
        if (object) {
            marker = object->property("m").toInt();
            color_name = object->property("c").value<QColor>().name();
        }
        cpp_owned = QJSEngine::objectOwnership(&provider->shell()) == QJSEngine::CppOwnership;
        // Leaving the scope destroys the object, the component, the root and
        // then the engine: all before the provider.
    }
    // Without C++ ownership the engine has deleted the Shell; the provider's
    // own delete would then be a second one.
    if (!cpp_owned || !shell_ptr) (void)provider.release();

    QVERIFY2(ready, qPrintable(component_errors));
    QVERIFY(object_made);
    QVERIFY(root_made);
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join(QLatin1Char(' '))));
    QCOMPARE(marker, provider_marker);
    QCOMPARE(color_name, QStringLiteral("#123456"));
}

void QmlModuleTests::singletonsOutliveEngine()
{
    qml_test::QueuingFrontEnd fake;
    auto provider = std::make_unique<QmlProvider>(fake);
    auto engine = std::make_unique<QQmlEngine>();
    auto root = SetupQmlEngine(*engine, *provider, qml_test::FIXTURE_ROOT_URL);

    const int shell_id = qmlTypeId("Gridcoin.Qml", 1, 0, "Shell");
    const int splash_id = qmlTypeId("Gridcoin.Qml", 1, 0, "Splash");
    ShellAdapter* shell = shell_id >= 0 ? engine->singletonInstance<ShellAdapter*>(shell_id) : nullptr;
    SplashAdapter* splash = splash_id >= 0 ? engine->singletonInstance<SplashAdapter*>(splash_id) : nullptr;
    QPointer<ShellAdapter> shell_ptr(shell);
    QPointer<SplashAdapter> splash_ptr(splash);

    root.reset();
    engine.reset();

    const bool shell_alive = !shell_ptr.isNull();
    const bool splash_alive = !splash_ptr.isNull();
    // An engine that deleted a singleton the provider owns would make the
    // provider's own delete a second one.
    if (!shell_alive || !splash_alive) (void)provider.release();

    QVERIFY(shell_id >= 0);
    QVERIFY(splash_id >= 0);
    QVERIFY2(shell_alive, "the engine deleted the Shell singleton");
    QVERIFY2(splash_alive, "the engine deleted the Splash singleton");
}

void QmlModuleTests::singletonIsProviderInstance()
{
    qml_test::QueuingFrontEnd fake;
    auto provider = std::make_unique<QmlProvider>(fake);
    QPointer<ShellAdapter> shell_ptr(&provider->shell());
    const int provider_marker = provider->marker();

    bool cpp_owned = false;
    bool ready = false;
    bool object_made = false;
    QString component_errors;
    int marker = 0;
    bool same_instance = false;
    {
        auto engine = std::make_unique<QQmlEngine>();
        auto root = SetupQmlEngine(*engine, *provider, qml_test::FIXTURE_ROOT_URL);

        QQmlComponent component(engine.get());
        component.setData("import QtQuick; import Gridcoin.Qml; QtObject { property int m: Shell.marker }",
                          QUrl(QStringLiteral("qrc:/qmltest/inline.qml")));
        std::unique_ptr<QObject> object(component.create());

        ready = component.status() == QQmlComponent::Ready;
        component_errors = component.errorString();
        object_made = object != nullptr;
        if (object) marker = object->property("m").toInt();
        const int shell_id = qmlTypeId("Gridcoin.Qml", 1, 0, "Shell");
        same_instance = shell_id >= 0 && engine->singletonInstance<ShellAdapter*>(shell_id) == &provider->shell();
        cpp_owned = QJSEngine::objectOwnership(&provider->shell()) == QJSEngine::CppOwnership;
    }
    if (!cpp_owned || !shell_ptr) (void)provider.release();

    QVERIFY2(ready, qPrintable(component_errors));
    QVERIFY(object_made);
    QCOMPARE(marker, provider_marker);
    QVERIFY(same_instance);
}
