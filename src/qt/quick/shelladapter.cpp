// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/quick/shelladapter.h>

#include <qt/guifrontend.h>
#include <qt/quick/qmlprovider.h>

#include <QJSEngine>
#include <QSystemTrayIcon>
#include <QtGlobal>

#include <utility>

ShellAdapter::ShellAdapter(GuiFrontEnd& frontend, int marker)
    : QObject(nullptr), m_frontend(&frontend), m_marker(marker)
{
}

ShellAdapter* ShellAdapter::create(QQmlEngine*, QJSEngine*)
{
    QmlProvider* provider = QmlProvider::installed();
    if (!provider) {
        // A programming error that would hand QML a null singleton; qFatal
        // halts in every build.
        qFatal("ShellAdapter::create: no QmlProvider is installed");
    }
    QJSEngine::setObjectOwnership(&provider->shell(), QJSEngine::CppOwnership);
    return &provider->shell();
}

void ShellAdapter::requestQuit()
{
    m_frontend->requestQuit();
}

bool ShellAdapter::requestClose()
{
    if (m_quit_requested) return true;
    if (m_minimize_on_close && m_tray_available()) {
        emit hideToTrayRequested();
        return false;
    }
    m_frontend->requestQuit();
    return true;
}

void ShellAdapter::setShown(bool shown)
{
    if (m_shown == shown) return;
    m_shown = shown;
    emit shownChanged();
}

void ShellAdapter::setStartMinimized(bool minimized)
{
    if (m_start_minimized == minimized) return;
    m_start_minimized = minimized;
    emit startMinimizedChanged();
}

void ShellAdapter::setQuitRequested()
{
    if (m_quit_requested) return;
    m_quit_requested = true;
    emit quitRequestedChanged();
}

void ShellAdapter::setMinimizeOnClose(bool minimize_on_close)
{
    m_minimize_on_close = minimize_on_close;
}

void ShellAdapter::setTrayAvailableProbe(std::function<bool()> probe)
{
    if (probe) {
        m_tray_available = std::move(probe);
    } else {
        m_tray_available = &QSystemTrayIcon::isSystemTrayAvailable;
    }
}

void ShellAdapter::setIpcInfo(const GuiIpcInfo& info)
{
    m_ipc = info;
    emit ipcInfoChanged();
}

void ShellAdapter::notifyCoreMessage(const QString& caption, const QString& message, bool modal)
{
    emit coreMessage(caption, message, modal);
}

void ShellAdapter::notifyUpdateAvailable(const QString& caption, const QString& version, int update_version,
                                         const QString& message)
{
    emit updateAvailable(caption, version, update_version, message);
}

void ShellAdapter::notifyBuildMismatch(const QString& gui_commit, const QString& node_commit)
{
    emit buildMismatchWarning(gui_commit, node_commit);
}

void ShellAdapter::notifyShowMain(bool minimized)
{
    emit showMainRequested(minimized);
}

void ShellAdapter::notifyHideMain()
{
    emit hideMainRequested();
}
