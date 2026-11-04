// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/connection_scope.h"

#include "logging.h"
#include "sync.h"

#include <condition_variable>
#include <exception>
#include <thread>
#include <utility>
#include <vector>

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
            if (!m_closed) {
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
        if (m_running && m_closing_thread == std::this_thread::get_id()) return;
        m_cv.wait(lock, [this]() EXCLUSIVE_LOCKS_REQUIRED(m_mutex) { return !m_running; });

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
        std::vector<std::pair<HookId, std::function<void()>>> hooks;
        {
            WAIT_LOCK(m_mutex, lock);
            if (m_closed) {
                // Return only once the hooks have run, so a caller that releases
                // the attach slot after close() cannot release it early. From
                // inside a hook that would wait for itself.
                if (m_running && m_closing_thread == std::this_thread::get_id()) return;
                m_cv.wait(lock, [this]() EXCLUSIVE_LOCKS_REQUIRED(m_mutex) { return !m_running; });
                return;
            }
            m_closed = true;
            m_running = true;
            m_closing_thread = std::this_thread::get_id();
            hooks.swap(m_hooks);
        }

        for (const auto& hook : hooks) {
            RunHook(hook.second);
        }

        // The hooks are destroyed before anyone waiting is released, so when
        // removeCloseHook() or a second close() returns, nothing of a hook is
        // left here. A destructor that removes its own hook comes back through
        // removeCloseHook() on this thread, which returns at once.
        hooks.clear();

        {
            LOCK(m_mutex);
            m_running = false;
            m_closing_thread = std::thread::id{};
        }
        m_cv.notify_all();
    }

    bool closed() const override
    {
        LOCK(m_mutex);
        return m_closed;
    }

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
    std::vector<std::pair<HookId, std::function<void()>>> m_hooks GUARDED_BY(m_mutex){};
    HookId m_next_id GUARDED_BY(m_mutex){1};
    bool m_closed GUARDED_BY(m_mutex){false};
    //! Whether close() is running the hooks, and on which thread.
    bool m_running GUARDED_BY(m_mutex){false};
    std::thread::id m_closing_thread GUARDED_BY(m_mutex){};
};

} // namespace

std::shared_ptr<ConnectionScope> MakeConnectionScope()
{
    return std::make_shared<ConnectionScopeImpl>();
}

} // namespace interfaces
