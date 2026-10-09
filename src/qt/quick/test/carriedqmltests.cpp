// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "carriedqmltests.h"

#include "carriedqmlexpected.h"
#include "qmltestutil.h"

#include <qt/quick/qmlengine.h>

#include <QByteArray>
#include <QColor>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QLibraryInfo>
#include <QList>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlError>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStringList>
#include <QTest>
#include <QUrl>
#include <QVersionNumber>
#include <QtGlobal>

#include <chrono>
#include <memory>
#include <utility>

namespace {

//! The engine every probe here uses: the module import path, no output to
//! stderr, and the engine's warnings collected for the failure message.
void ConfigureEngine(QQmlEngine& engine, QStringList& warnings)
{
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
    engine.setOutputWarningsToStandardError(false);
    QObject::connect(&engine, &QQmlEngine::warnings, &engine, [&warnings](const QList<QQmlError>& list) {
        for (const QQmlError& error : list) warnings << error.toString();
    });
}

const CarriedQmlExpectedFailure* FindExpectedFailure(const QString& file)
{
    for (const CarriedQmlExpectedFailure& entry : CARRIED_QML_EXPECTED_FAILURES) {
        if (file == QLatin1String(entry.file)) return &entry;
    }
    return nullptr;
}

//! Creates an inline object that reads a singleton on a fresh engine; a
//! singleton that does not resolve appends a line to problems.
void ProbeSingleton(const QString& name, const QByteArray& source, QStringList& problems)
{
    QStringList warnings;
    QQmlEngine engine;
    ConfigureEngine(engine, warnings);
    QQmlComponent component(&engine);
    component.setData(source, QUrl(QStringLiteral("qrc:/qmltest/singleton-%1.qml").arg(name)));
    std::unique_ptr<QObject> object(component.create());
    if (!object || !object->property("resolved").toBool()) {
        problems << QStringLiteral("singleton not resolved: %1: %2 %3")
                        .arg(name, component.errorString().trimmed(), warnings.join(QLatin1Char(' ')));
    }
}

//! The messages the resource system printed, for iconResourcesDoNotCollide.
QStringList g_resource_messages;
QtMessageHandler g_previous_handler = nullptr;

void CollectMessage(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    g_resource_messages << message;
    if (g_previous_handler) g_previous_handler(type, context, message);
}

//! The Image inside a ColorizableImage.
QQuickItem* InnerImage(QQuickItem* colorizable)
{
    for (QQuickItem* child : colorizable->childItems()) {
        if (child->inherits("QQuickImage")) return child;
    }
    return nullptr;
}

//! The tint effects of a ColorizableImage. A layer's effect item is a child of
//! the layered item's parent, here the ColorizableImage itself.
QList<QQuickItem*> TintOverlays(QQuickItem* colorizable)
{
    QList<QQuickItem*> overlays;
    for (QQuickItem* child : colorizable->childItems()) {
        if (child->objectName() == QLatin1String("tintOverlay")) overlays << child;
    }
    return overlays;
}

} // namespace

void QmlCarriedQmlTests::carriedFilesLoad()
{
    QStringList problems;

    const QString prefix = QStringLiteral(":/qt/qml/");
    QStringList files;
    for (const QString& root : {QStringLiteral(":/qt/qml/Gridcoin/App"), QStringLiteral(":/qt/qml/MMPTheme"),
                                QStringLiteral(":/qt/qml/CurrentTime")}) {
        QDirIterator it(root, {QStringLiteral("*.qml")}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) files << it.next().mid(prefix.size());
    }
    files.sort();
    QCOMPARE(files.size(), qsizetype(48));

    ProbeSingleton(QStringLiteral("MMPTheme"),
                   "import QtQuick\nimport MMPTheme 1.0\n"
                   "QtObject { property bool resolved: MMPTheme.baseFont !== undefined }",
                   problems);
    ProbeSingleton(QStringLiteral("CurrentTime"),
                   "import QtQuick\nimport CurrentTime 1.0\n"
                   "QtObject { property bool resolved: CurrentTime.currentTime !== undefined }",
                   problems);

    int expected_failures = 0;
    QStringList warnings;
    QQmlEngine engine;
    ConfigureEngine(engine, warnings);
    for (const QString& rel : std::as_const(files)) {
        if (rel.endsWith(QLatin1String("/MMPTheme.qml")) || rel.endsWith(QLatin1String("/CurrentTime.qml"))) {
            continue; // the singletons, probed above
        }
        QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qt/qml/") + rel), QQmlComponent::PreferSynchronous);
        if (component.status() == QQmlComponent::Loading) {
            qml_test::PumpUntil([&component] { return component.status() != QQmlComponent::Loading; },
                                std::chrono::seconds(10));
        }
        const bool loaded = component.status() == QQmlComponent::Ready;
        const CarriedQmlExpectedFailure* entry = FindExpectedFailure(rel);
        const bool expected = entry && QLibraryInfo::version() < QVersionNumber(entry->major, entry->minor);
        if (!loaded && !expected) {
            problems << QStringLiteral("unexpected failure: %1: %2").arg(rel, component.errorString().trimmed());
        } else if (loaded && expected) {
            problems << QStringLiteral("listed file passes: %1").arg(rel);
        }
        if (expected) {
            ++expected_failures;
            qInfo().noquote() << QStringLiteral("expected below %1.%2: %3 (%4)")
                                     .arg(entry->major).arg(entry->minor).arg(rel, QLatin1String(entry->closure));
        }
    }

    // A relative Qt.createComponent() URL resolves against the file that makes
    // the call; the carried files use it from handlers and menu items.
    {
        QQmlComponent component(&engine);
        component.setData("import QtQuick\nQtObject { property int s: Qt.createComponent(\"Circle.qml\").status }",
                          QUrl(QStringLiteral("qrc:/qt/qml/Gridcoin/App/relativeprobe.qml")));
        std::unique_ptr<QObject> object(component.create());
        if (!object || object->property("s").toInt() != 1) {
            problems << QStringLiteral("relative createComponent does not resolve: %1").arg(component.errorString().trimmed());
        }
    }

    for (const CarriedQmlExpectedFailure& entry : CARRIED_QML_EXPECTED_FAILURES) {
        if (!files.contains(QLatin1String(entry.file))) {
            problems << QStringLiteral("list names a missing file: %1").arg(QLatin1String(entry.file));
        }
    }

    qInfo().noquote() << QStringLiteral("carried QML: Qt %1, %2 files, %3 expected failures")
                             .arg(QLibraryInfo::version().toString()).arg(files.size()).arg(expected_failures);
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join(QLatin1Char('\n'))));
}

void QmlCarriedQmlTests::rootCompileMatchesCarriedFilesLoad()
{
    const QUrl root(QStringLiteral("qrc:/qt/qml/Gridcoin/App/WindowManager.qml"));

    // The oracle is carriedFilesLoad's verdict for the root on this Qt.
    bool oracle_ready = false;
    QString oracle_errors;
    {
        QStringList warnings;
        QQmlEngine engine;
        ConfigureEngine(engine, warnings);
        QQmlComponent component(&engine, root, QQmlComponent::PreferSynchronous);
        if (component.status() == QQmlComponent::Loading) {
            qml_test::PumpUntil([&component] { return component.status() != QQmlComponent::Loading; },
                                std::chrono::seconds(10));
        }
        oracle_ready = component.status() == QQmlComponent::Ready;
        oracle_errors = component.errorString().trimmed();
    }

    const QStringList errors = CompileQmlRoot(root);
    QVERIFY2(errors.isEmpty() == oracle_ready,
             qPrintable(QStringLiteral("the start-up root compile and carriedFilesLoad disagree about "
                                       "WindowManager.qml: carriedFilesLoad %1, root compile %2: %3; "
                                       "carriedFilesLoad errors: %4")
                            .arg(oracle_ready ? QStringLiteral("ready") : QStringLiteral("not ready"),
                                 errors.isEmpty() ? QStringLiteral("ready") : QStringLiteral("not ready"),
                                 errors.join(QLatin1Char(' ')), oracle_errors)));
    if (!oracle_ready) QVERIFY(!errors.isEmpty());
}

void QmlCarriedQmlTests::themeUsesUiFamily()
{
    QStringList warnings;
    QQmlEngine engine;
    ConfigureEngine(engine, warnings);
    QQmlComponent component(&engine);
    component.setData("import QtQuick\nimport MMPTheme 1.0\nimport Gridcoin.Qml\n"
                      "QtObject { property string themeFamily: MMPTheme.baseFont.family;"
                      " property string uiFamily: Fonts.uiFamily }",
                      QUrl(QStringLiteral("qrc:/qmltest/themefamily.qml")));
    std::unique_ptr<QObject> object(component.create());
    QVERIFY2(object, qPrintable(component.errorString() + warnings.join(QLatin1Char(' '))));
    const QString theme_family = object->property("themeFamily").toString();
    const QString ui_family = object->property("uiFamily").toString();
    QVERIFY2(!ui_family.isEmpty(), "Fonts.uiFamily is empty");
    QCOMPARE(theme_family, ui_family);
}

void QmlCarriedQmlTests::iconResourcesDoNotCollide()
{
    g_resource_messages.clear();
    g_previous_handler = qInstallMessageHandler(CollectMessage);

    // The Widgets resources, as GuiMain registers them.
    Q_INIT_RESOURCE(bitcoin);

    QStringList problems;
    if (!QFileInfo(QStringLiteral(":/icons/menu")).isFile()) {
        problems << QStringLiteral(":/icons/menu is not a file");
    }
    if (QImage(QStringLiteral(":/icons/menu")).isNull()) {
        problems << QStringLiteral("the Widgets menu icon no longer loads");
    }
    int menu_icons = 0;
    QDirIterator it(QStringLiteral(":/icons/tabmenu"), QDir::Files);
    while (it.hasNext()) {
        const QString path = it.next();
        ++menu_icons;
        if (QImage(path).isNull()) problems << QStringLiteral("does not load: %1").arg(path);
    }
    if (menu_icons != 20) {
        problems << QStringLiteral("tab menu icons: %1, expected 20").arg(menu_icons);
    }

    qInstallMessageHandler(g_previous_handler);
    for (const QString& message : std::as_const(g_resource_messages)) {
        if (message.contains(QLatin1String("has both data and children"))) {
            problems << QStringLiteral("resource warning: %1").arg(message);
        }
    }
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join(QLatin1Char('\n'))));
}

void QmlCarriedQmlTests::colorizableImageDrivesColorOverlay()
{
    QStringList warnings;
    QQmlEngine engine;
    ConfigureEngine(engine, warnings);
    QQmlComponent component(&engine);
    // As MenuButton and SearchBox set them: a tinted icon, and an untinted one.
    component.setData("import QtQuick\nimport Gridcoin.App\n"
                      "Row {\n"
                      "    property alias tinted: tinted\n"
                      "    property alias plain: plain\n"
                      "    ColorizableImage { id: tinted; width: 32; height: 32; colorization: 1.0;"
                      " tintColor: \"#4a90e2\"; source: \"qrc:/icons/tabmenu/ic_menu_overview_blue.svg\" }\n"
                      "    ColorizableImage { id: plain; width: 32; height: 32; tintColor: \"#3a465e\";"
                      " source: \"qrc:/icons/tabmenu/ic_menu_overview_blue.svg\" }\n"
                      "}\n",
                      QUrl(QStringLiteral("qrc:/qmltest/colorizable.qml")));
    std::unique_ptr<QObject> object(component.create());
    QVERIFY2(object, qPrintable(component.errorString() + warnings.join(QLatin1Char(' '))));

    // Shown in a window, offscreen and under the software adaptation. Only
    // properties are checked: nothing here depends on what is drawn.
    QQuickWindow window;
    auto* row = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(row);
    row->setParentItem(window.contentItem());
    window.resize(100, 50);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto* tinted = object->property("tinted").value<QQuickItem*>();
    auto* plain = object->property("plain").value<QQuickItem*>();
    QVERIFY(tinted && plain);
    QQuickItem* tinted_image = InnerImage(tinted);
    QQuickItem* plain_image = InnerImage(plain);
    QVERIFY(tinted_image && plain_image);
    const QString seen = warnings.join(QLatin1Char(' '));

    QVERIFY2(QQmlProperty(tinted_image, QStringLiteral("layer.enabled")).read().toBool(),
             qPrintable(QStringLiteral("a tinted image has no layer. ") + seen));
    QList<QQuickItem*> overlays = TintOverlays(tinted);
    QVERIFY2(overlays.size() == 1,
             qPrintable(QStringLiteral("tint overlays: %1, expected 1. ").arg(overlays.size()) + seen));
    QVERIFY2(QByteArray(overlays.first()->metaObject()->className()).startsWith("ColorOverlay"),
             overlays.first()->metaObject()->className());
    const QColor full = overlays.first()->property("color").value<QColor>();
    QCOMPARE(full.name(), QStringLiteral("#4a90e2"));
    QVERIFY2(qAbs(full.alphaF() - 1.0) <= 1.0 / 255, "the tint at colorization 1.0 is not opaque");

    QVERIFY2(!QQmlProperty(plain_image, QStringLiteral("layer.enabled")).read().toBool(),
             "an untinted image has a layer");
    QVERIFY2(TintOverlays(plain).isEmpty(), "an untinted image has a tint overlay");

    tinted->setProperty("colorization", 0.5);
    qml_test::PumpFor(std::chrono::milliseconds(50));
    overlays = TintOverlays(tinted);
    QVERIFY(overlays.size() == 1);
    const QColor half = overlays.first()->property("color").value<QColor>();
    QVERIFY2(qAbs(half.alphaF() - 0.5) <= 1.0 / 255,
             qPrintable(QStringLiteral("the tint's alpha at colorization 0.5 is %1").arg(half.alphaF())));

    tinted->setProperty("colorization", 0.0);
    qml_test::PumpFor(std::chrono::milliseconds(50));
    QVERIFY2(!QQmlProperty(tinted_image, QStringLiteral("layer.enabled")).read().toBool(),
             "colorization 0 left the layer on");
}
