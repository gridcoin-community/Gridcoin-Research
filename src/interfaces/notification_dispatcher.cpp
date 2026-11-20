// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/notification_dispatcher.h"

#include "logging.h"
#include "sync.h"
#include "util.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <thread>

namespace interfaces {

namespace {

std::atomic<NotificationDelivery> g_delivery{NotificationDelivery::Inline};
std::atomic<uint64_t> g_dropped{0};

//! One queue and one worker thread. m_mutex is a leaf. It is never held while a
//! notification runs, while logging, while a queued closure is destroyed, or while
//! taking another lock. m_lifecycle only serializes Start and Stop against each
//! other. No notification path takes it, so holding it across the join in Stop does
//! not block an emitter.
class Dispatcher
{
public:
    Dispatcher() = default;
    Dispatcher(const Dispatcher&) = delete;
    Dispatcher& operator=(const Dispatcher&) = delete;

    // GetDispatcher() never destroys the instance. At process exit a thread still
    // delivering to a stalled GUI must not be joined, since that could block the
    // exit, and a joinable std::thread would abort it, so the thread and its
    // members outlive the process's static destructors.
    ~Dispatcher() = delete;

    void Start()
    {
        LOCK(m_lifecycle);
        if (m_thread.joinable()) return;

        {
            LOCK(m_mutex);
            m_stop = false;
        }

        // The thread is created before the running flag is set, so a failure to
        // create it leaves the dispatcher stopped and drops posts.
        m_thread = std::thread([this] { TraceThread("grc-ipc-notify", [this] { Run(); }); });

        LOCK(m_mutex);
        m_running = true;
    }

    void Stop()
    {
        LOCK(m_lifecycle);
        if (!m_thread.joinable()) return;

        // The discarded closures are destroyed after m_mutex is released. A closure
        // can hold the last reference to a client proxy whose destructor makes a
        // remote call, logs and takes other locks.
        std::deque<std::function<void()>> discarded;
        {
            LOCK(m_mutex);
            m_running = false;
            m_stop = true;
            discarded.swap(m_queue);
        }
        m_cv.notify_all();
        m_thread.join();

        g_dropped += discarded.size();
        discarded.clear();

        LOCK(m_mutex);
        m_dropping = false;
        m_episode_dropped = 0;
    }

    void Post(std::function<void()> call)
    {
        bool log_first_drop = false;
        {
            LOCK(m_mutex);
            if (m_running && m_queue.size() < NOTIFICATION_QUEUE_MAX) {
                m_queue.push_back(std::move(call));
                m_cv.notify_one();
                return;
            }

            ++g_dropped;
            if (m_running) {
                ++m_episode_dropped;
                log_first_drop = !m_dropping;
                m_dropping = true;
            }
        }

        // The rejected closure, still held by the parameter, is destroyed on return,
        // after m_mutex is released.
        if (log_first_drop) {
            LogPrintf("IPC: the GUI is not taking notifications; dropping until it catches up");
        }
    }

private:
    void Run()
    {
        while (true) {
            std::function<void()> call;
            {
                WAIT_LOCK(m_mutex, lock);
                m_cv.wait(lock, [this]() EXCLUSIVE_LOCKS_REQUIRED(m_mutex) { return m_stop || !m_queue.empty(); });
                if (m_stop) return;

                call = std::move(m_queue.front());
                m_queue.pop_front();
            }

            // The call carries its own GuardNotify catch.
            call();
            call = nullptr;

            // The episode ends when the dispatcher has caught up.
            uint64_t episode_dropped = 0;
            {
                LOCK(m_mutex);
                if (m_dropping && m_queue.empty()) {
                    episode_dropped = m_episode_dropped;
                    m_dropping = false;
                    m_episode_dropped = 0;
                }
            }

            if (episode_dropped > 0) {
                LogPrintf("IPC: the GUI is taking notifications again; dropped %u while it was behind",
                          episode_dropped);
            }
        }
    }

    Mutex m_lifecycle;
    Mutex m_mutex;
    std::condition_variable m_cv;
    std::thread m_thread GUARDED_BY(m_lifecycle);
    std::deque<std::function<void()>> m_queue GUARDED_BY(m_mutex);
    bool m_running GUARDED_BY(m_mutex){false};
    bool m_stop GUARDED_BY(m_mutex){false};
    bool m_dropping GUARDED_BY(m_mutex){false};
    uint64_t m_episode_dropped GUARDED_BY(m_mutex){0};
};

Dispatcher& GetDispatcher()
{
    // Intentionally never destroyed, see ~Dispatcher.
    static Dispatcher* const dispatcher = new Dispatcher();
    return *dispatcher;
}

} // namespace

void SetNotificationDelivery(NotificationDelivery mode)
{
    g_delivery = mode;
}

NotificationDelivery GetNotificationDelivery()
{
    return g_delivery;
}

void StartNotificationDispatcher()
{
    GetDispatcher().Start();
}

void StopNotificationDispatcher()
{
    GetDispatcher().Stop();
}

uint64_t DroppedNotificationCount()
{
    return g_dropped;
}

namespace detail {

void PostNotification(std::function<void()> call)
{
    GetDispatcher().Post(std::move(call));
}

void NotificationPostFailed(const char* what)
{
    ++g_dropped;
    LogDroppedNotification(what);
}

} // namespace detail

} // namespace interfaces
