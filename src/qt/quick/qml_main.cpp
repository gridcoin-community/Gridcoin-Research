// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/guifrontend.h>
#include <qt/quick/qmlfonts.h>
#include <qt/quick/qmlfrontend.h>

#include <QLibraryInfo>
#include <QString>
#include <QUrl>
#include <QVersionNumber>
#include <QtGlobal>

#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace {

//! The carried root, served from the Gridcoin.App module's resources.
const QString QML_ROOT_URL = QStringLiteral("qrc:/qt/qml/Gridcoin/App/WindowManager.qml");

//! GuiMain calls this after the application object exists and its resources
//! are registered, and before the front end loads its root. It registers the
//! QML front end's fonts first: QFontDatabase needs the application object,
//! which does not exist yet when main() starts.
std::unique_ptr<GuiFrontEnd> MakeQmlFrontEnd()
{
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
    return GuiMain(argc, argv, MakeQmlFrontEnd);
}
