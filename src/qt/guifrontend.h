// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_GUIFRONTEND_H
#define BITCOIN_QT_GUIFRONTEND_H

#include <qt/guiipcinfo.h>

#include <QString>
#include <QWidget>

#include <memory>
#include <string>

class ClientModel;
class WalletModel;
class MRCModel;
class ResearcherModel;
class VotingModel;
class QObject;

namespace interfaces {
class PSGTPoolContext;
} // namespace interfaces

//! The models StartGridcoinQt builds for one GUI session, handed to the front
//! end in a single attach. Every model is owned by StartGridcoinQt and outlives
//! the front end's use of it; the front end only borrows them.
struct ModelBundle
{
    ClientModel& client;
    WalletModel& wallet;
    MRCModel& mrc;
    ResearcherModel& researcher;
    VotingModel& voting;
    interfaces::PSGTPoolContext* psgt;
};

//! The seam between the GUI composition root (GuiMain / StartGridcoinQt in
//! bitcoin.cpp: Init selection, IPC connect, OptionsModel, readiness, the model
//! bundle and teardown) and the widgets that present it. The composition root
//! drives a front end through this interface only, so a second front end can
//! reuse all of that wiring.
class GuiFrontEnd
{
public:
    GuiFrontEnd() = default;
    virtual ~GuiFrontEnd();

    GuiFrontEnd(const GuiFrontEnd&) = delete;
    GuiFrontEnd& operator=(const GuiFrontEnd&) = delete;

    //! Create the main window. The returned object is the target of the
    //! string-invoked core bridges and must provide the slots
    //! error(QString,QString,bool), update(QString,QString,int,QString) and
    //! handleURI(QString).
    virtual QObject* construct() = 0;
    //! Destroy the main window; idempotent.
    virtual void destroyMain() = 0;
    //! Create and show the splash. The returned object is the target of the
    //! string-invoked InitMessage bridge and must provide the slot
    //! showMessage(QString,int).
    virtual QObject* createSplash() = 0;
    //! Close the splash once the core is ready.
    virtual void finishSplash() = 0;
    //! Destroy the splash; idempotent.
    virtual void destroySplash() = 0;
    //! Show the multiprocess mixed-build banner (GUI and daemon commits differ).
    virtual void showBuildMismatchWarning(const QString& gui_commit, const QString& node_commit) = 0;
    //! Hand the IPC connection facts to the About dialog's multiprocess section.
    virtual void setIpcConnectionInfo(const GuiIpcInfo& info) = 0;
    //! Attach the session's models. The PSGT context attaches before the wallet,
    //! because the wallet attach builds the PSGT page's interface-backed table
    //! model.
    virtual void attachModels(const ModelBundle& models) = 0;
    //! Show the main window, minimized for -min.
    virtual void showMain(bool minimized) = 0;
    //! The native handle of the main window (the Windows shutdown-block reason).
    virtual WId nativeWindowId() = 0;
    //! Begin an explicit application quit. It must not be vetoed by
    //! minimize-on-close (#2995).
    virtual void requestQuit() = 0;

    //! Detach every model from the front end, in the order StartGridcoinQt's
    //! teardown has always used: hide the main window, then client, wallet, MRC,
    //! researcher, voting and PSGT.
    //! Idempotent: only the first call reaches the hooks, so the exception-path
    //! guard and the normal-path call can both run it.
    void detachModels();

protected:
    //! Hide the main window before its models go away.
    virtual void hideMain() = 0;
    virtual void detachClient() = 0;
    //! Destroys the tx-table view models while the tx source they unregister
    //! from is still alive.
    virtual void detachWallet() = 0;
    //! Clears the raw MRC model copies the front end keeps, before the model
    //! (a stack object in StartGridcoinQt) is destroyed.
    virtual void detachMRC() = 0;
    virtual void detachResearcher() = 0;
    //! Must drain PollTableModel's in-flight QtConcurrent refresh worker, which
    //! still dereferences the voting model, before that model is destroyed.
    virtual void detachVoting() = 0;
    //! Tears down the PSGT page's table model, which holds a reference to the
    //! PSGT pool context, before the context is destroyed.
    virtual void detachPSGT() = 0;

private:
    bool m_detached{false};
};

//! Runs GuiFrontEnd::detachModels() when it goes out of scope, on every exit
//! path. Declare it after the models and their sources, so it runs before their
//! destructors: otherwise an exception unwinds the models and sources first, and
//! the front end's view models later unregister from a freed source (UAF). On
//! the normal path the explicit detachModels() call runs first and this is a
//! no-op.
struct FrontEndDetachGuard
{
    GuiFrontEnd& frontend;
    ~FrontEndDetachGuard();
};

using GuiFrontEndFactory = std::unique_ptr<GuiFrontEnd> (*)();

//! The GUI process entry point, defined in bitcoin.cpp (the composition root).
//! make_frontend builds the front end the session presents through.
int GuiMain(int argc, char* argv[], GuiFrontEndFactory make_frontend);

//! Routes an explicit quit to the live front end's requestQuit().
void RequestGuiQuit();

//! True when an exception's text is libmultiprocess's "this connection is gone"
//! raise.
bool IsDaemonDisconnectMessage(const std::string& msg);

//! Post a graceful, popup-free GUI quit when the node connection is lost.
void QuitOnDaemonConnectionLost(const char* reason);

#endif // BITCOIN_QT_GUIFRONTEND_H
