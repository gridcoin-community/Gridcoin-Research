// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_GUIFRONTEND_H
#define BITCOIN_QT_GUIFRONTEND_H

#include <qt/guiipcinfo.h>

#include <QObject>
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

//! What DeliverCoreMessage did with one core message.
enum class CoreMessageDelivery {
    Queued,          //!< Not modal: delivered queued, as before.
    Blocking,        //!< Modal, front end does not queue modal messages: blocking connection, as before.
    QueuedAndLogged, //!< Modal, queuing front end: delivered queued and written to the GUI log and stderr.
};

class GuiFrontEnd;

//! Delivers one core message to `target`'s error(QString,QString,bool) slot.
//! With no front end, or one that does not queue modal messages, it uses the
//! blocking connection for a modal message and a queued one otherwise, as
//! ThreadSafeMessageBox always did. For a front end that queues modal messages
//! it posts every message queued, from any thread, and returns at once; a modal
//! one is also written to the GUI log and to stderr. The result says which of
//! these happened.
CoreMessageDelivery DeliverCoreMessage(QObject* target, GuiFrontEnd* frontend, const std::string& caption, const std::string& message, bool modal);

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

    //! True for a front end whose core-message target cannot block inside its
    //! error slot; DeliverCoreMessage then never makes the raising thread wait.
    //! Default false (the Widgets front end keeps it).
    virtual bool queuesModalCoreMessages() const;

    //! Detach every model from the front end, in the order StartGridcoinQt's
    //! teardown has always used: hide the main window, then client, wallet, MRC,
    //! researcher, voting and PSGT.
    //! Before the first hook starts, it calls onDetachStarting(); that step runs
    //! once, on the call that starts the first hook.
    //! Each call runs, in that order, the hooks not yet started. The progress
    //! index moves past a hook before the hook runs, so a hook that throws is
    //! never re-entered and its exception propagates to the caller. After such a
    //! throw, a later call resumes at the next hook; once every hook has started,
    //! a call runs none. So the normal-path call and the exception-path guard can
    //! both run it, and each hook runs at most once.
    void detachModels();

    //! True once every detach hook has been started. FrontEndDetachGuard loops
    //! on it.
    bool detached() const;

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

    //! Called by detachModels() before its first hook, once. A front end closes
    //! its asynchronous work here so nothing new starts during the detach; must
    //! not throw. Default does nothing.
    virtual void onDetachStarting() noexcept;

private:
    //! The next detach hook to start, as an index into detachModels()'s
    //! sequence. It equals the number of hooks once every hook has started.
    int m_next_hook{0};
};

//! Runs GuiFrontEnd::detachModels() until GuiFrontEnd::detached() when it goes
//! out of scope, on every exit path. It logs and swallows each exception a hook
//! throws and calls detachModels() again, so every hook after a throwing one
//! still runs, each at most once. Declare it after the models and their sources,
//! so it runs before their destructors: otherwise an exception unwinds the
//! models and sources first, and the front end's view models later unregister
//! from a freed source (UAF). Declare it before GuiFrontEnd::attachModels(), so
//! it also covers a throw part-way through the attach: the detach hooks must
//! therefore tolerate models that were never or only partly attached. On the
//! normal path the explicit detachModels() call runs first and leaves this
//! nothing to run.
//! After the detach it waits for every job on QThreadPool::globalInstance(),
//! including one a detach hook started, so a pooled job finishes while the
//! models and sources declared before the guard are alive. Destroy it while
//! the QCoreApplication is alive, as GlobalPoolReapGuard requires.
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
