// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/quick/qmlfonts.h>

#include <qt/guilog.h>

#include <QDir>
#include <QDirIterator>
#include <QFont>
#include <QFontDatabase>
#include <QtGlobal>

namespace {
#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
constexpr bool APPLE_PLATFORM = true;
#else
constexpr bool APPLE_PLATFORM = false;
#endif
} // namespace

QString ChooseUiFontFamily(bool apple_platform, const QString& system_family)
{
    return apple_platform ? system_family : QStringLiteral("Inter");
}

int LoadQmlFonts()
{
    int registered = 0;
    QDirIterator it(QStringLiteral(":/fonts"), QDir::Files);
    while (it.hasNext()) {
        const QString path = it.next();
        if (QFontDatabase::addApplicationFont(path) < 0) {
            GUILogPrintf("QML: font not registered: %s", path.toStdString());
        } else {
            ++registered;
        }
    }
    return registered;
}

FontsAdapter::FontsAdapter(QObject* parent)
    : QObject(parent),
      m_ui_family(ChooseUiFontFamily(APPLE_PLATFORM,
                                     QFontDatabase::systemFont(QFontDatabase::GeneralFont).family()))
{
}

QString FontsAdapter::uiFamily() const { return m_ui_family; }
