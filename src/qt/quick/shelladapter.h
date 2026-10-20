// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_SHELLADAPTER_H
#define BITCOIN_QT_QUICK_SHELLADAPTER_H

#include <QtQml/qqmlregistration.h>

#include <QObject>
#include <QString>
#include <QSystemTrayIcon>

#include <qt/guiipcinfo.h>

#include <functional>
#include <type_traits>

class GuiFrontEnd;
class QQmlEngine;
class QJSEngine;

//! The QML singleton `Shell`: the application shell's state and requests. It is
//! owned by QmlProvider, which hands QML this instance through create(), so it
//! outlives every engine. Properties are read-only for QML; the C++-side
//! setters and notify functions are called by QmlFrontEnd.
class ShellAdapter : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Shell)
    QML_SINGLETON
    Q_PROPERTY(int marker READ marker CONSTANT)
    Q_PROPERTY(bool shown READ shown NOTIFY shownChanged)
    Q_PROPERTY(bool startMinimized READ startMinimized NOTIFY startMinimizedChanged)
    Q_PROPERTY(bool quitRequested READ quitRequested NOTIFY quitRequestedChanged)
    Q_PROPERTY(bool ipcActive READ ipcActive NOTIFY ipcInfoChanged)
    Q_PROPERTY(bool ipcGitCommitMismatch READ ipcGitCommitMismatch NOTIFY ipcInfoChanged)
    Q_PROPERTY(QString ipcGuiVersion READ ipcGuiVersion NOTIFY ipcInfoChanged)
    Q_PROPERTY(QString ipcNodeVersion READ ipcNodeVersion NOTIFY ipcInfoChanged)
    Q_PROPERTY(QString ipcNodeBuiltAt READ ipcNodeBuiltAt NOTIFY ipcInfoChanged)
    Q_PROPERTY(QString ipcSchema READ ipcSchema NOTIFY ipcInfoChanged)
    Q_PROPERTY(QString ipcProtocol READ ipcProtocol NOTIFY ipcInfoChanged)
    Q_PROPERTY(QString ipcSocketPath READ ipcSocketPath NOTIFY ipcInfoChanged)
    Q_PROPERTY(QString ipcNodeIdentity READ ipcNodeIdentity NOTIFY ipcInfoChanged)
    Q_PROPERTY(QString ipcNetwork READ ipcNetwork NOTIFY ipcInfoChanged)

public:
    //! `marker` identifies the provider that owns this instance (the identity
    //! tests compare it).
    ShellAdapter(GuiFrontEnd& frontend, int marker);

    //! The singleton factory: hands QML the installed provider's Shell with C++
    //! ownership. Halts the process when no provider is installed.
    static ShellAdapter* create(QQmlEngine* qml_engine, QJSEngine* js_engine);

    //! Begins an application quit, through the front end's requestQuit().
    Q_INVOKABLE void requestQuit();
    //! QML calls it from the main window's close handler: true means the window
    //! may close and the application quits, false that it was hidden to the tray
    //! instead; a hidden window with no tray could not be restored, so there is
    //! no hide without a tray.
    Q_INVOKABLE bool requestClose();

    int marker() const { return m_marker; }
    bool shown() const { return m_shown; }
    bool startMinimized() const { return m_start_minimized; }
    bool quitRequested() const { return m_quit_requested; }
    bool ipcActive() const { return m_ipc.active; }
    bool ipcGitCommitMismatch() const { return m_ipc.git_commit_mismatch; }
    QString ipcGuiVersion() const { return m_ipc.gui_version; }
    QString ipcNodeVersion() const { return m_ipc.node_version; }
    QString ipcNodeBuiltAt() const { return m_ipc.node_built_at; }
    QString ipcSchema() const { return m_ipc.ipc_schema; }
    QString ipcProtocol() const { return m_ipc.ipc_protocol; }
    QString ipcSocketPath() const { return m_ipc.socket_path; }
    QString ipcNodeIdentity() const { return m_ipc.node_identity; }
    QString ipcNetwork() const { return m_ipc.network; }

    //! Each setter assigns and emits its NOTIFY signal only when the value
    //! changes.
    void setShown(bool shown);
    void setStartMinimized(bool minimized);
    //! Sets quitRequested to true.
    void setQuitRequested();
    //! Whether closing the main window hides it to the tray instead of quitting.
    void setMinimizeOnClose(bool minimize_on_close);
    //! Replaces the live system-tray check that requestClose() makes (the
    //! default); an empty function restores the default.
    void setTrayAvailableProbe(std::function<bool()> probe);
    //! Emits ipcInfoChanged() once.
    void setIpcInfo(const GuiIpcInfo& info);

    //! Each notify function emits the matching signal and returns.
    void notifyCoreMessage(const QString& caption, const QString& message, bool modal);
    void notifyUpdateAvailable(const QString& caption, const QString& version, int update_version, const QString& message);
    void notifyBuildMismatch(const QString& gui_commit, const QString& node_commit);
    void notifyShowMain(bool minimized);
    void notifyHideMain();

signals:
    void shownChanged();
    void startMinimizedChanged();
    void quitRequestedChanged();
    void ipcInfoChanged();
    void coreMessage(const QString& caption, const QString& message, bool modal);
    void updateAvailable(const QString& caption, const QString& version, int updateVersion, const QString& message);
    void buildMismatchWarning(const QString& guiCommit, const QString& nodeCommit);
    void showMainRequested(bool minimized);
    void hideMainRequested();
    void hideToTrayRequested();

private:
    GuiFrontEnd* m_frontend; //!< Never null: set from the constructor's reference.
    int m_marker;
    bool m_shown{false};
    bool m_start_minimized{false};
    bool m_quit_requested{false};
    bool m_minimize_on_close{false};
    //! Whether a system tray is available; see setTrayAvailableProbe().
    std::function<bool()> m_tray_available{&QSystemTrayIcon::isSystemTrayAvailable};
    GuiIpcInfo m_ipc;
};

static_assert(!std::is_default_constructible_v<ShellAdapter>,
              "Qt 6.4 prefers new T over create() for a default-constructible singleton");

#endif // BITCOIN_QT_QUICK_SHELLADAPTER_H
