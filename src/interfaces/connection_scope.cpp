// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/connection_scope.h"

#include "logging.h"
#include "sync.h"

#include <atomic>
#include <condition_variable>
#include <exception>
#include <list>
#include <thread>
#include <utility>

namespace interfaces {

ConnectionScope::~ConnectionScope() = default;

namespace {

class ConnectionScopeImpl final : public ConnectionScope
{
public:
    HookId addCloseHook(std::function<void()> fn) override
    {
        {
            LOCK(m_mutex);
            if (!m_closed.load()) {
                const HookId id = m_next_id++;
                m_hooks.emplace_back(id, std::move(fn));
                return id;
            }
        }

        RunHook(fn);
        return 0;
    }

    void removeCloseHook(HookId id) override
    {
        // Declared before the lock, so it is destroyed after the lock is released
        // and whatever the hook captured is never destroyed under the mutex.
        std::function<void()> removed;

        WAIT_LOCK(m_mutex, lock);
        // From inside a hook (the thread running close()), waiting for close()
        // to finish would wait for itself. A hook still pending is erased there
        // all the same, so it does not run.
        if (!m_running || m_closing_thread != std::this_thread::get_id()) {
            m_cv.wait(lock, [this]() EXCLUSIVE_LOCKS_REQUIRED(m_mutex) { return !m_running; });
        }

        for (auto it = m_hooks.begin(); it != m_hooks.end(); ++it) {
            if (it->first == id) {
                removed = std::move(it->second);
                m_hooks.erase(it);
                break;
            }
        }
    }

    void close() override
    {
        {
            WAIT_LOCK(m_mutex, lock);
            if (m_closed.load()) {
                // Return only once the hooks have run, so a caller that releases
                // the attach slot after close() cannot release it early. From
                // inside a hook that would wait for itself.
                if (m_running && m_closing_thread == std::this_thread::get_id()) return;
                m_cv.wait(lock, [this]() EXCLUSIVE_LOCKS_REQUIRED(m_mutex) { return !m_running; });
                return;
            }
            // Set before any hook runs, so a lock-free closed() reads true from
            // here on. No hook can be added after this; addCloseHook() runs it.
            m_closed.store(true);
            m_running = true;
            m_closing_thread = std::this_thread::get_id();
        }

        // One at a time, in order, each run outside the mutex. A hook that
        // removes a later one, directly or by destroying its owner, therefore
        // erases it before it is taken, and it does not run.
        while (true) {
            // Destroyed at the end of each pass, outside the mutex and before the
            // next hook is taken, so a destructor that removes a later hook stops
            // it too, and nothing of a hook is left when waiters are released.
            std::function<void()> hook;
            {
                LOCK(m_mutex);
                if (m_hooks.empty()) break;
                hook = std::move(m_hooks.front().second);
                m_hooks.pop_front();
            }
            RunHook(hook);
        }

        {
            LOCK(m_mutex);
            m_running = false;
            m_closing_thread = std::thread::id{};
        }
        m_cv.notify_all();
    }

    bool closed() const override { return m_closed.load(); }

private:
    //! Run one hook, containing what it throws. close() runs on the IPC
    //! event-loop thread, where an exception would unwind through
    //! libmultiprocess, and one failed hook must not keep the rest from running.
    static void RunHook(const std::function<void()>& fn)
    {
        if (!fn) return;
        try {
            fn();
        } catch (const std::exception& e) {
            LogPrintf("ERROR: %s: an IPC connection teardown hook failed: %s", __func__, e.what());
        } catch (...) {
            LogPrintf("ERROR: %s: an IPC connection teardown hook failed with an unknown exception", __func__);
        }
    }

    //! A leaf: no other lock is taken under it, and no hook runs under it.
    mutable Mutex m_mutex;
    std::condition_variable m_cv;
    std::list<std::pair<HookId, std::function<void()>>> m_hooks GUARDED_BY(m_mutex){};
    HookId m_next_id GUARDED_BY(m_mutex){1};
    //! Written only under m_mutex (in close()); read without it by closed(), so
    //! that callers holding other locks can ask, and under it by addCloseHook().
    //! ServeInit's re-check in ClaimAttachSlot pairs this store with its load of
    //! the held flag (and that flag's store with the hook's load), which needs
    //! both to stay sequentially consistent; do not relax to acquire/release.
    std::atomic<bool> m_closed{false};
    //! Whether close() is running the hooks, and on which thread.
    bool m_running GUARDED_BY(m_mutex){false};
    std::thread::id m_closing_thread GUARDED_BY(m_mutex){};
};

} // namespace

std::shared_ptr<ConnectionScope> MakeConnectionScope()
{
    return std::make_shared<ConnectionScopeImpl>();
}

std::function<bool()> ConnectionClosedCheck(std::shared_ptr<ConnectionScope> scope)
{
    if (!scope) return {};
    return [scope = std::move(scope)] { return scope->closed(); };
}

} // namespace interfaces
