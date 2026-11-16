// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/guifrontend.h>

#include <qt/guilog.h>
#include <qt/guiutil.h>

#include <QMetaObject>
#include <QString>
#include <QThreadPool>

#include <exception>
#include <iostream>
#include <iterator>

namespace {
//! The number of hooks GuiFrontEnd::detachModels() runs.
constexpr int DETACH_HOOK_COUNT = 7;

void PostCoreMessage(QObject* target, const std::string& caption, const std::string& message, bool modal)
{
    QMetaObject::invokeMethod(target, "error", Qt::QueuedConnection,
                              Q_ARG(QString, QString::fromStdString(caption)),
                              Q_ARG(QString, QString::fromStdString(message)),
                              Q_ARG(bool, modal));
}
} // namespace

GuiFrontEnd::~GuiFrontEnd() = default;

bool GuiFrontEnd::queuesModalCoreMessages() const
{
    return false;
}

void GuiFrontEnd::onDetachStarting() noexcept {}

void GuiFrontEnd::detachModels()
{
    // The established teardown order. Each entry is a virtual hook, so the call
    // below reaches the front end's override.
    using Hook = void (GuiFrontEnd::*)();
    static constexpr Hook hooks[] = {
        &GuiFrontEnd::hideMain,
        &GuiFrontEnd::detachClient,
        &GuiFrontEnd::detachWallet,
        &GuiFrontEnd::detachMRC,
        &GuiFrontEnd::detachResearcher,
        &GuiFrontEnd::detachVoting,
        &GuiFrontEnd::detachPSGT,
    };
    static_assert(std::size(hooks) == DETACH_HOOK_COUNT);

    if (m_next_hook == 0) {
        // Before the first hook: the front end stops its asynchronous work
        // before anything detaches.
        onDetachStarting();
    }

    while (m_next_hook < DETACH_HOOK_COUNT) {
        // Move past the hook BEFORE running it: a hook that throws is then never
        // re-entered, and a later call resumes at the next one.
        const Hook hook = hooks[m_next_hook];
        ++m_next_hook;
        (this->*hook)();
    }
}

bool GuiFrontEnd::detached() const
{
    return m_next_hook >= DETACH_HOOK_COUNT;
}

CoreMessageDelivery DeliverCoreMessage(QObject* target, GuiFrontEnd* frontend, const std::string& caption,
                                       const std::string& message, bool modal)
{
    if (frontend == nullptr || !frontend->queuesModalCoreMessages()) {
        QMetaObject::invokeMethod(target, "error",
                                  modal ? GUIUtil::blockingGUIThreadConnection() : Qt::QueuedConnection,
                                  Q_ARG(QString, QString::fromStdString(caption)),
                                  Q_ARG(QString, QString::fromStdString(message)),
                                  Q_ARG(bool, modal));
        return modal ? CoreMessageDelivery::Blocking : CoreMessageDelivery::Queued;
    }

    PostCoreMessage(target, caption, message, modal);
    if (!modal) {
        return CoreMessageDelivery::Queued;
    }
    GUILogPrintf("%s: %s", caption, message);
    std::cerr << caption << ": " << message << std::endl;
    return CoreMessageDelivery::QueuedAndLogged;
}

FrontEndDetachGuard::~FrontEndDetachGuard()
{
    // A destructor must not throw: this one can run during stack unwinding.
    // Every pass starts at least one hook, because detachModels() moves past a
    // hook before running it, so the loop ends after at most one pass per hook.
    while (!frontend.detached()) {
        try {
            frontend.detachModels();
        } catch (const std::exception& e) {
            GUILogPrintf("WARNING: front-end detach failed: %s", e.what());
        } catch (...) {
            GUILogPrintf("WARNING: front-end detach failed");
        }
    }

    // Then wait for every job on the global pool, including one a detach hook
    // started, while the models and sources declared before this guard are
    // still alive.
    QThreadPool::globalInstance()->waitForDone();
}
