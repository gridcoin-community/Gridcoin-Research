// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/quick/qmlfrontend.h>

#include <qt/clientmodel.h>
#include <qt/guilog.h>
#include <qt/optionsmodel.h>
#include <qt/quick/qmlengine.h>
#include <qt/quick/shelladapter.h>
#include <qt/quick/splashadapter.h>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QWindow>

#include <stdexcept>
#include <utility>

QmlCoreBridge::QmlCoreBridge(ShellAdapter& shell)
    : QObject(nullptr), m_shell(shell)
{
}

QString QmlCoreBridge::pendingUri() const
{
    return m_pending_uri;
}

QString QmlCoreBridge::takePendingUri()
{
    QString uri;
    uri.swap(m_pending_uri);
    return uri;
}

void QmlCoreBridge::error(const QString& caption, const QString& message, bool modal)
{
    m_shell.notifyCoreMessage(caption, message, modal);
}

void QmlCoreBridge::update(const QString& caption, const QString& version, int update_version, const QString& message)
{
    m_shell.notifyUpdateAvailable(caption, version, update_version, message);
}

void QmlCoreBridge::handleURI(const QString& uri)
{
    m_pending_uri = uri;
    emit pendingUriChanged();
}

QmlFrontEnd::Callbacks QmlFrontEnd::ValidateCallbacks(Callbacks callbacks)
{
    if (!callbacks.is_disconnect) {
        throw std::invalid_argument("QmlFrontEnd: callbacks.is_disconnect is empty");
    }
    if (!callbacks.on_disconnect) {
        throw std::invalid_argument("QmlFrontEnd: callbacks.on_disconnect is empty");
    }
    return callbacks;
}

QmlFrontEnd::QmlFrontEnd(QUrl root_url, Callbacks callbacks)
    : m_root_url(std::move(root_url)),
      m_callbacks(ValidateCallbacks(std::move(callbacks))),
      m_provider(std::make_unique<QmlProvider>(*this)),
      m_runner(std::make_unique<QmlCallRunner>(
          QmlCallRunner::DisconnectPolicy{m_callbacks.is_disconnect, m_callbacks.on_disconnect}))
{
}

QmlFrontEnd::~QmlFrontEnd()
{
    destroyMain();
}

ShellAdapter& QmlFrontEnd::shell()
{
    return m_provider->shell();
}

QObject* QmlFrontEnd::construct()
{
    if (m_constructed) {
        throw std::logic_error("QmlFrontEnd::construct() called twice");
    }
    m_constructed = true;
    // First, so a native window exists whatever the root does; create() makes
    // it now, not on the first winId() call.
    m_anchor = std::make_unique<QWindow>();
    m_anchor->create();
    m_engine = std::make_unique<QQmlEngine>();
    // Quit goes through the front end, like every other quit path.
    QObject::connect(m_engine.get(), &QQmlEngine::quit, m_engine.get(), [this] { requestQuit(); });
    // Qt.exit(code) too; the code is dropped: StartGridcoinQt sets the process status.
    QObject::connect(m_engine.get(), &QQmlEngine::exit, m_engine.get(), [this](int) { requestQuit(); });
    // QGuiApplication::lastWindowClosed is deliberately not connected: Qt emits
    // it from the event loop whenever the last visible window closes, even with
    // quit-on-last-window-closed off, and a secondary window closed while the
    // main window is hidden must not quit.
    m_root = SetupQmlEngine(*m_engine, *m_provider, m_root_url);
    if (!m_root) {
        GUILogPrintf("QML: the root %s did not load", m_root_url.toString().toStdString());
        // Without a root the process would run with no window and no tray.
        // StartGridcoinQt reports this and ends the GUI with EXIT_FAILURE before
        // the core starts; under -multiprocess the daemon keeps running.
        throw GuiFrontEndUnusable("QML: the root " + m_root_url.toString().toStdString() +
                                  " did not load; the QML errors are on stderr and in the GUI log "
                                  "(debug.log, or debug_gui.log under -multiprocess)");
    }
    m_bridge = std::make_unique<QmlCoreBridge>(m_provider->shell());
    return m_bridge.get();
}

void QmlFrontEnd::destroyMain()
{
    // First: no call starts and no result is delivered once the root is gone,
    // also on the paths that never reach the detach (a shutdown before the core
    // is ready, an early throw).
    m_runner->close();
    m_runner->bumpGeneration();
    m_bridge.reset();
    m_root.reset();
    m_engine.reset();
    m_anchor.reset();
}

QObject* QmlFrontEnd::createSplash()
{
    m_provider->splash().setActive(true);
    return &m_provider->splash();
}

void QmlFrontEnd::finishSplash()
{
    m_provider->splash().setFinished(true);
}

void QmlFrontEnd::destroySplash()
{
    // The adapter itself lives in the provider.
    m_provider->splash().setActive(false);
}

void QmlFrontEnd::showBuildMismatchWarning(const QString& gui_commit, const QString& node_commit)
{
    shell().notifyBuildMismatch(gui_commit, node_commit);
}

void QmlFrontEnd::setIpcConnectionInfo(const GuiIpcInfo& info)
{
    shell().setIpcInfo(info);
}

void QmlFrontEnd::attachModels(const ModelBundle& models)
{
    m_client = &models.client;
    m_wallet = &models.wallet;
    m_mrc = &models.mrc;
    m_researcher = &models.researcher;
    m_voting = &models.voting;
    m_psgt = models.psgt;

    OptionsModel* options = models.client.getOptionsModel();
    shell().setMinimizeOnClose(options && options->getMinimizeOnClose());
}

void QmlFrontEnd::showMain(bool minimized)
{
    shell().setStartMinimized(minimized);
    shell().notifyShowMain(minimized);
    shell().setShown(true);
    // A quit requested before the event loop runs (a queued quit request handled
    // in StartGridcoinQt's readiness wait) did nothing then: quit acts only inside
    // the event loop. Post it again. Nothing processes events between this call
    // and the start of the event loop, so the loop delivers it once it starts.
    if (shell().quitRequested()) {
        QMetaObject::invokeMethod(qApp, &QCoreApplication::quit, Qt::QueuedConnection);
    }
}

WId QmlFrontEnd::nativeWindowId()
{
    return m_anchor ? m_anchor->winId() : 0;
}

void QmlFrontEnd::requestQuit()
{
    // Quit does nothing outside the event loop; the flag makes the root's close
    // handler accept closes once a quit was requested.
    shell().setQuitRequested();
    QCoreApplication::quit();
}

bool QmlFrontEnd::queuesModalCoreMessages() const
{
    return true;
}

QmlCallRunner& QmlFrontEnd::callRunner()
{
    return *m_runner;
}

QmlProvider& QmlFrontEnd::provider()
{
    return *m_provider;
}

QObject* QmlFrontEnd::rootObject() const
{
    return m_root.get();
}

void QmlFrontEnd::hideMain()
{
    // Hide first: the drain can wait for a long serial call, and no event loop
    // runs here. Drain cancels queued calls on both lanes and waits only for the
    // in-flight serial call; pooled calls are reaped by the composition root's
    // global-pool waits.
    shell().notifyHideMain();
    // A window a view opened with createObject(window) is out of the root's
    // reach, and Qt hides no transient child with its parent.
    for (QWindow* w : QGuiApplication::topLevelWindows()) {
        if (w->isVisible()) w->hide();
    }
    m_runner->drain();
}

void QmlFrontEnd::detachClient()
{
    m_client = nullptr;
    shell().setMinimizeOnClose(false);
}

void QmlFrontEnd::detachWallet()
{
    m_wallet = nullptr;
}

void QmlFrontEnd::detachMRC()
{
    m_mrc = nullptr;
}

void QmlFrontEnd::detachResearcher()
{
    m_researcher = nullptr;
}

void QmlFrontEnd::detachVoting()
{
    m_voting = nullptr;
}

void QmlFrontEnd::detachPSGT()
{
    m_psgt = nullptr;
}

void QmlFrontEnd::onDetachStarting() noexcept
{
    m_runner->close();
}
