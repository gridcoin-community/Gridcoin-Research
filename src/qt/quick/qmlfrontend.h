// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_QMLFRONTEND_H
#define BITCOIN_QT_QUICK_QMLFRONTEND_H

#include <qt/guifrontend.h>
#include <qt/quick/qmlcallrunner.h>
#include <qt/quick/qmlprovider.h>

#include <QObject>
#include <QString>
#include <QUrl>

#include <functional>
#include <memory>
#include <string>

class QQmlEngine;
class ShellAdapter;

//! The target of bitcoin.cpp's string-invoked core bridges. Its slots only emit
//! signals and return, so the core thread is never held by QML.
class QmlCoreBridge : public QObject
{
    Q_OBJECT

public:
    explicit QmlCoreBridge(ShellAdapter& shell);

    //! The URI is kept until the send screen's adapter claims it.
    QString pendingUri() const;
    //! Returns the pending URI and clears it.
    QString takePendingUri();

public Q_SLOTS:
    //! A core message: emitted on Shell.coreMessage.
    void error(const QString& caption, const QString& message, bool modal);
    void update(const QString& caption, const QString& version, int update_version, const QString& message);
    //! Stores the URI as the pending one and signals it.
    void handleURI(const QString& uri);

Q_SIGNALS:
    void pendingUriChanged();

private:
    ShellAdapter& m_shell;
    QString m_pending_uri;
};

//! The QML front end: a GuiFrontEnd that loads a QML root given by URL and
//! delivers every core message queued, so no core thread waits for QML. It
//! owns its call runner and the provider of the module's singletons.
class QmlFrontEnd : public GuiFrontEnd
{
public:
    //! What the composition root wires in: how a call runner recognises and
    //! reports a lost daemon connection (IsDaemonDisconnectMessage,
    //! QuitOnDaemonConnectionLost). Injected, so this module does not link the
    //! composition root. Quit needs no callback: RequestGuiQuit calls
    //! requestQuit() on the registered front end, and the module's own quit
    //! paths call it directly.
    struct Callbacks {
        std::function<bool(const std::string&)> is_disconnect;
        std::function<void(const std::string&)> on_disconnect;
    };

    //! Throws std::invalid_argument naming the empty member if either callback
    //! is empty.
    QmlFrontEnd(QUrl root_url, Callbacks callbacks);
    ~QmlFrontEnd() override;

    //! Creates the engine, loads the root and returns the core-message bridge.
    QObject* construct() override;
    //! Destroys the bridge, the root and the engine, in that order; idempotent.
    void destroyMain() override;
    //! Marks the splash active and returns its adapter.
    QObject* createSplash() override;
    void finishSplash() override;
    void destroySplash() override;
    void showBuildMismatchWarning(const QString& gui_commit, const QString& node_commit) override;
    void setIpcConnectionInfo(const GuiIpcInfo& info) override;
    //! Stores the model pointers and tells the shell whether closing the window
    //! minimizes it.
    void attachModels(const ModelBundle& models) override;
    void showMain(bool minimized) override;
    //! No native main window is owned in C++: the root creates its windows.
    WId nativeWindowId() override;
    //! Records the quit on the shell and quits the application. Quit does
    //! nothing outside the event loop; the flag lets the root's close handler
    //! accept the closes quit sends.
    void requestQuit() override;
    bool queuesModalCoreMessages() const override;

    QmlCallRunner& callRunner();
    QmlProvider& provider();
    QObject* rootObject() const;

protected:
    //! Cancels the runner's queued calls and waits for its in-flight serial
    //! call; pooled calls are reaped by the composition root's global-pool
    //! waits. Then tells QML to hide the main window.
    void hideMain() override;
    void detachClient() override;
    void detachWallet() override;
    void detachMRC() override;
    void detachResearcher() override;
    void detachVoting() override;
    void detachPSGT() override;
    //! Closes the call runner, so no call starts during the detach.
    void onDetachStarting() noexcept override;

private:
    static Callbacks ValidateCallbacks(Callbacks callbacks);
    ShellAdapter& shell();

    QUrl m_root_url;
    Callbacks m_callbacks;
    std::unique_ptr<QmlProvider> m_provider;   // destroyed after the runner, the engine, the root and the bridge
    std::unique_ptr<QmlCallRunner> m_runner;    // destroyed before the provider
    std::unique_ptr<QQmlEngine> m_engine;
    std::unique_ptr<QObject> m_root;
    std::unique_ptr<QmlCoreBridge> m_bridge;
    ClientModel* m_client{nullptr};
    WalletModel* m_wallet{nullptr};
    MRCModel* m_mrc{nullptr};
    ResearcherModel* m_researcher{nullptr};
    VotingModel* m_voting{nullptr};
    interfaces::PSGTPoolContext* m_psgt{nullptr};
};

#endif // BITCOIN_QT_QUICK_QMLFRONTEND_H
