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

//! One queue and one worker thread. The mutex is a leaf. It is never held while a
//! notification runs, while logging, or while taking another lock.
class Dispatcher
{
public:
    void Start()
    {
        LOCK(m_mutex);
        if (m_running) return;

        m_running = true;
        m_stop = false;
        m_thread = std::thread([this] { TraceThread("grc-ipc-notify", [this] { Run(); }); });
    }

    void Stop()
    {
        std::thread thread;
        uint64_t discarded = 0;
        {
            LOCK(m_mutex);
            if (!m_running || m_stop) return;

            m_stop = true;
            discarded = m_queue.size();
            m_queue.clear();
            thread = std::move(m_thread);
        }
        m_cv.notify_all();
        thread.join();

        {
            LOCK(m_mutex);
            m_running = false;
            m_dropping = false;
            m_episode_dropped = 0;
        }

        g_dropped += discarded;
    }

    void Post(std::function<void()> call)
    {
        bool log_first_drop = false;
        {
            LOCK(m_mutex);
            if (m_running && !m_stop && m_queue.size() < NOTIFICATION_QUEUE_MAX) {
                m_queue.push_back(std::move(call));
                m_cv.notify_one();
                return;
            }

            ++g_dropped;
            if (m_running && !m_stop) {
                ++m_episode_dropped;
                log_first_drop = !m_dropping;
                m_dropping = true;
            }
        }

        if (log_first_drop) {
            LogPrintf("IPC: the GUI is not taking notifications; dropping until it catches up\n");
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

            uint64_t episode_dropped = 0;
            {
                LOCK(m_mutex);
                if (m_dropping) {
                    episode_dropped = m_episode_dropped;
                    m_dropping = false;
                    m_episode_dropped = 0;
                }
            }

            if (episode_dropped > 0) {
                LogPrintf("IPC: the GUI is taking notifications again; dropped %u while it was behind\n",
                          episode_dropped);
            }
        }
    }

    Mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<std::function<void()>> m_queue GUARDED_BY(m_mutex);
    std::thread m_thread GUARDED_BY(m_mutex);
    bool m_running GUARDED_BY(m_mutex){false};
    bool m_stop GUARDED_BY(m_mutex){false};
    bool m_dropping GUARDED_BY(m_mutex){false};
    uint64_t m_episode_dropped GUARDED_BY(m_mutex){0};
};

Dispatcher& GetDispatcher()
{
    static Dispatcher dispatcher;
    return dispatcher;
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

} // namespace detail

} // namespace interfaces
