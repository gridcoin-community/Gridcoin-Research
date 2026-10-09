// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/gridcoin-config.h>
#endif

#include <qt/guifrontend.h>
#include <qt/quick/qmlengine.h>
#include <qt/quick/qmlfonts.h>
#include <qt/quick/qmlfrontend.h>
#include <util/system.h>

#include <QLibraryInfo>
#include <QMessageBox>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVersionNumber>
#include <QtGlobal>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace {

//! The carried root, served from the Gridcoin.App module's resources.
const QString QML_ROOT_URL = QStringLiteral("qrc:/qt/qml/Gridcoin/App/WindowManager.qml");

//! Thrown by MakeQmlFrontEnd once it has reported that the QML root cannot be
//! used; main() catches it and returns EXIT_FAILURE. A tag type, not derived
//! from std::exception, so no handler for std::exception on the way to main()
//! takes it.
struct QmlRootUnusable {};

//! GuiMain calls this after the application object exists and its resources
//! are registered, and before the front end loads its root. It registers the
//! QML front end's fonts first: QFontDatabase needs the application object,
//! which does not exist yet when main() starts.
std::unique_ptr<GuiFrontEnd> MakeQmlFrontEnd()
{
    // GuiMain calls the factory before it opens the GUI log, connects to a
    // daemon or starts the core, so a root that does not compile (a QML module
    // missing anywhere in its static type tree, the Qt5Compat effects module
    // among them, or a broken file there) stops the process here, before any
    // of that. The GUI log gets nothing: it is not open yet, and a line queued
    // for it would be dropped when the process ends, so stderr and the message
    // box carry the error. -help and -version skip the check: both return
    // before the root loads.
    if (!HelpRequested(gArgs) && !gArgs.IsArgSet("-version")) {
        const QStringList errors = CompileQmlRoot(QUrl(QML_ROOT_URL));
        if (!errors.isEmpty()) {
            // The effects probe only chooses the first sentence.
            const QString sentence = ProbeQmlEffects().isEmpty()
                ? QStringLiteral("gridcoinresearch-qml: the QML front end's root "
                                 "(qrc:/qt/qml/Gridcoin/App/WindowManager.qml) did not compile; the errors below "
                                 "name the file or import that failed.")
                : QStringLiteral("gridcoinresearch-qml: the QML front end could not create the "
                                 "Qt5Compat.GraphicalEffects types it needs (ColorOverlay, DropShadow); the errors "
                                 "below name what is missing. On Debian and Ubuntu the module is "
                                 "qml6-module-qt5compat-graphicaleffects (it needs libqt6shadertools6).");
            std::cerr << sentence.toStdString() << std::endl;
            for (const QString& error : errors) {
                std::cerr << error.toStdString() << std::endl;
            }
            QMessageBox::critical(nullptr, PACKAGE_NAME,
                                  sentence + QStringLiteral("\n\n") + errors.join(QLatin1Char('\n')));
            throw QmlRootUnusable{};
        }
    }
    // The root's theme names its font family by Fonts.uiFamily; the fonts it
    // can name are registered here, before the root loads.
    LoadQmlFonts();
    QmlFrontEnd::Callbacks callbacks{
        IsDaemonDisconnectMessage,
        [](const std::string&) {
            QuitOnDaemonConnectionLost("a QML front-end call was interrupted by the daemon disconnecting");
        }};
    return std::make_unique<QmlFrontEnd>(QUrl(QML_ROOT_URL), std::move(callbacks));
}

} // namespace

int main(int argc, char* argv[])
{
    // The root, WindowManager.qml, reaches RectangularShadow (see the test's
    // expected-failure list); without a root the GUI would run with nothing on screen.
    if (QLibraryInfo::version() < QVersionNumber(6, 9)) {
        std::cerr << "gridcoinresearch-qml: the carried QML needs Qt 6.9 or later; this is Qt " << qVersion() << ".\n";
        return 1;
    }
    try {
        return GuiMain(argc, argv, MakeQmlFrontEnd);
    } catch (const QmlRootUnusable&) {
        // MakeQmlFrontEnd has reported why, on stderr and in the message box.
        return EXIT_FAILURE;
    }
}
