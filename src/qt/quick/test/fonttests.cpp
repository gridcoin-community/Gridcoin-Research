// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "fonttests.h"

#include <qt/quick/qmlfonts.h>

#include <QFontDatabase>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QStringList>
#include <QTest>
#include <QUrl>
#include <QtGlobal>

#include <memory>

void QmlFontTests::uiFamilyChoice()
{
    QCOMPARE(ChooseUiFontFamily(true, QStringLiteral("X")), QStringLiteral("X"));
    QCOMPARE(ChooseUiFontFamily(false, QStringLiteral("X")), QStringLiteral("Inter"));
}

void QmlFontTests::uiFamilyOnThisPlatform()
{
    [[maybe_unused]] const int registered = LoadQmlFonts();

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
    engine.setOutputWarningsToStandardError(false);
    QQmlComponent component(&engine);
    component.setData("import QtQuick\nimport Gridcoin.Qml\nQtObject { property string family: Fonts.uiFamily }",
                      QUrl(QStringLiteral("qrc:/qmltest/fonts.qml")));
    std::unique_ptr<QObject> object(component.create());
    QVERIFY2(object, qPrintable(component.errorString()));
    const QString family = object->property("family").toString();

    qInfo() << "Inter families:" << QFontDatabase::families().filter(QRegularExpression(QStringLiteral("^Inter")));

#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
    QCOMPARE(family, QFontDatabase::systemFont(QFontDatabase::GeneralFont).family());
#else
    QCOMPARE(family, QStringLiteral("Inter"));
    QVERIFY2(registered >= 20, qPrintable(QStringLiteral("fonts registered: %1").arg(registered)));
    const QStringList families = QFontDatabase::families();
    QVERIFY2(families.contains(QStringLiteral("Inter")), "Inter is not registered");
    QVERIFY2(families.contains(QStringLiteral("Montserrat")), "Montserrat is not registered");
    const QStringList styles = QFontDatabase::styles(QStringLiteral("Inter"));
    qInfo() << "Inter styles:" << styles;
    const bool has_light = styles.contains(QStringLiteral("Light"));
    const bool has_semibold = !styles.filter(QRegularExpression(QStringLiteral("^Semi ?Bold$"))).isEmpty();
    QVERIFY2(has_semibold && has_light, qPrintable(styles.join(QStringLiteral(", "))));
#endif
}
