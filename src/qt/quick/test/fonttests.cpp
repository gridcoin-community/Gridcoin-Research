// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "fonttests.h"

#include <qt/quick/qmlfonts.h>

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QFontDatabase>
#include <QLatin1String>
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
    // Regular, Medium and Bold come from the Widgets resource, whose font
    // files under :/fonts have no extension.
    for (const char* style : {"Regular", "Medium", "Bold"}) {
        QVERIFY2(styles.contains(QLatin1String(style)), qPrintable(styles.join(QStringLiteral(", "))));
    }
#endif
}

void QmlFontTests::loadsFontsNotLicenceTexts()
{
    Q_INIT_RESOURCE(bitcoin);

    const QStringList paths = QmlFontPaths();
    const QStringList expected{
        QStringLiteral(":/fonts/inter-regular"),
        QStringLiteral(":/fonts/inter-medium"),
        QStringLiteral(":/fonts/inter-bold"),
        QStringLiteral(":/fonts/inconsolata-regular"),
        QStringLiteral(":/fonts/Inter-Light.otf"),
        QStringLiteral(":/fonts/Inter-SemiBold.otf"),
        QStringLiteral(":/fonts/Montserrat-Black.ttf"),
        QStringLiteral(":/fonts/Montserrat-BlackItalic.ttf"),
        QStringLiteral(":/fonts/Montserrat-Bold.ttf"),
        QStringLiteral(":/fonts/Montserrat-BoldItalic.ttf"),
        QStringLiteral(":/fonts/Montserrat-ExtraBold.ttf"),
        QStringLiteral(":/fonts/Montserrat-ExtraBoldItalic.ttf"),
        QStringLiteral(":/fonts/Montserrat-ExtraLight.ttf"),
        QStringLiteral(":/fonts/Montserrat-ExtraLightItalic.ttf"),
        QStringLiteral(":/fonts/Montserrat-Italic.ttf"),
        QStringLiteral(":/fonts/Montserrat-Light.ttf"),
        QStringLiteral(":/fonts/Montserrat-LightItalic.ttf"),
        QStringLiteral(":/fonts/Montserrat-Medium.ttf"),
        QStringLiteral(":/fonts/Montserrat-MediumItalic.ttf"),
        QStringLiteral(":/fonts/Montserrat-Regular.ttf"),
        QStringLiteral(":/fonts/Montserrat-SemiBold.ttf"),
        QStringLiteral(":/fonts/Montserrat-SemiBoldItalic.ttf"),
        QStringLiteral(":/fonts/Montserrat-Thin.ttf"),
        QStringLiteral(":/fonts/Montserrat-ThinItalic.ttf"),
    };
    for (const QString& font : expected) {
        QVERIFY2(paths.contains(font), qPrintable(QStringLiteral("not loaded: %1").arg(font)));
    }
    for (const QString& path : paths) {
        QVERIFY2(!QFileInfo(path).fileName().startsWith(QStringLiteral("ofl-"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("licence text offered as a font: %1").arg(path)));
    }

    QStringList all;
    QDirIterator it(QStringLiteral(":/fonts"), QDir::Files);
    while (it.hasNext()) all.append(it.next());
    QVERIFY2(all.contains(QStringLiteral(":/fonts/OFL-Inter.txt")), "OFL-Inter.txt is not a resource");
    QVERIFY2(all.contains(QStringLiteral(":/fonts/OFL-Montserrat.txt")), "OFL-Montserrat.txt is not a resource");
}
