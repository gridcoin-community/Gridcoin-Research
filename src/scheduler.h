// Copyright (c) 2015-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCHEDULER_H
#define BITCOIN_SCHEDULER_H

#include <condition_variable>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <atomic>
#include <thread>

#include <sync.h>

/**
 * Simple class for background tasks that should be run
 * periodically or once "after a while"
 *
 * Usage:
 *
 * CScheduler* s = new CScheduler();
 * s->scheduleFromNow(doSomething, std::chrono::milliseconds{11}); // Assuming a: void doSomething() { }
 * s->scheduleFromNow([=] { this->func(argument); }, std::chrono::milliseconds{3});
 * std::thread* t = new std::thread([&] { s->serviceQueue(); });
 *
 * ... then at program shutdown, make sure to call stop() to clean up the thread(s) running serviceQueue:
 * s->stop();
 * t->join();
 * delete t;
 * delete s; // Must be done after thread is interrupted/joined.
 */
class CScheduler
{
public:
    CScheduler();
    ~CScheduler();

    std::thread m_service_thread;

    typedef std::function<void()> Function;

    /** Call func at/after time t */
    void schedule(Function f, std::chrono::system_clock::time_point t);

    /** Call f once after the delta has passed */
    void scheduleFromNow(Function f, std::chrono::milliseconds delta)
    {
        schedule(std::move(f), std::chrono::system_clock::now() + delta);
    }

    /**
     * Like scheduleFromNow, but refuses once the scheduler has been told to
     * stop, and reports whether the task was accepted.
     *
     * For a caller whose correctness depends on the task actually running -- the
     * wallet's relock, which is the only thing that will end a timed unlock.
     * Asking "is it running?" and then scheduling is two steps, and a stop can
     * land between them; this decides and enqueues under one acquisition.
     *
     * It deliberately does NOT require a thread to be servicing the queue yet.
     * A task enqueued before serviceQueue() starts is not lost -- the thread
     * runs it when it comes up -- so refusing then would be a false negative,
     * and a racy one.
     */
    [[nodiscard]] bool scheduleFromNowIfRunning(Function f, std::chrono::milliseconds delta)
    {
        {
            LOCK(newTaskMutex);

            if (stopRequested) return false;

            taskQueue.insert(std::make_pair(std::chrono::system_clock::now() + delta, std::move(f)));
        }

        newTaskScheduled.notify_one();
        return true;
    }

    /**
     * Repeat f until the scheduler is stopped. First run is after delta has passed once.
     *
     * The timing is not exact: Every time f is finished, it is rescheduled to run again after delta. If you need more
     * accurate scheduling, don't use this method.
     */
    void scheduleEvery(Function f, std::chrono::milliseconds delta);

    /**
     * Mock the scheduler to fast forward in time.
     * Iterates through items on taskQueue and reschedules them
     * to be delta_seconds sooner.
     */
    void MockForward(std::chrono::seconds delta_seconds);

    /**
     * Services the queue 'forever'. Should be run in a thread.
     */
    void serviceQueue();

    /** Tell any threads running serviceQueue to stop as soon as the current task is done */
    void stop()
    {
        WITH_LOCK(newTaskMutex, stopRequested = true);
        newTaskScheduled.notify_all();
        if (m_service_thread.joinable()) m_service_thread.join();
    }
    /** Tell any threads running serviceQueue to stop when there is no work left to be done */
    void StopWhenDrained()
    {
        WITH_LOCK(newTaskMutex, stopWhenEmpty = true);
        newTaskScheduled.notify_all();
        if (m_service_thread.joinable()) m_service_thread.join();
    }

    /**
     * Returns number of tasks waiting to be serviced,
     * and first and last task times
     */
    size_t getQueueInfo(std::chrono::system_clock::time_point& first,
                        std::chrono::system_clock::time_point& last) const;

    /** Returns true if there are threads actively running in serviceQueue() */
    bool AreThreadsServicingQueue() const;

private:
    mutable Mutex newTaskMutex;
    std::condition_variable newTaskScheduled;
    std::multimap<std::chrono::system_clock::time_point, Function> taskQueue GUARDED_BY(newTaskMutex);
    int nThreadsServicingQueue GUARDED_BY(newTaskMutex){0};
    bool stopRequested GUARDED_BY(newTaskMutex){false};
    bool stopWhenEmpty GUARDED_BY(newTaskMutex){false};
    bool shouldStop() const EXCLUSIVE_LOCKS_REQUIRED(newTaskMutex) { return stopRequested || (stopWhenEmpty && taskQueue.empty()); }
};

//! The process-wide background task scheduler. Constructed in AppInit2 before
//! its serviceQueue thread is launched and destroyed at shutdown after that
//! thread is joined. Owned via unique_ptr (mirroring g_banman) so it can be
//! injected into PeerManager::StartScheduledTasks() once that lands.
extern std::unique_ptr<CScheduler> g_scheduler;

//! The scheduler, published for threads other than the one that builds it.
//!
//! g_scheduler itself is a plain unique_ptr assigned part-way through AppInit2,
//! and the RPC server is already serving by then (StartRPCThreads runs earlier),
//! so reading it from an RPC thread is a data race on a non-atomic object. This
//! is set once the scheduler is ready to take work and cleared before it is told
//! to stop, and the release/acquire pair is what makes reading through it
//! well-defined.
//!
//! Null means "do not schedule": either the scheduler does not exist yet, or
//! shutdown has begun. A caller whose task must actually run should treat null
//! as a refusal rather than carrying on without it.
extern std::atomic<CScheduler*> g_scheduler_handle;

/**
 * Class used by CScheduler clients which may schedule multiple jobs
 * which are required to be run serially. Jobs may not be run on the
 * same thread, but no two jobs will be executed
 * at the same time and memory will be release-acquire consistent
 * (the scheduler will internally do an acquire before invoking a callback
 * as well as a release at the end). In practice this means that a callback
 * B() will be able to observe all of the effects of callback A() which executed
 * before it.
 */
class SingleThreadedSchedulerClient
{
private:
    CScheduler* m_pscheduler;

    RecursiveMutex m_cs_callbacks_pending;
    std::list<std::function<void()>> m_callbacks_pending GUARDED_BY(m_cs_callbacks_pending);
    bool m_are_callbacks_running GUARDED_BY(m_cs_callbacks_pending) = false;

    void MaybeScheduleProcessQueue();
    void ProcessQueue();

public:
    explicit SingleThreadedSchedulerClient(CScheduler* pschedulerIn) : m_pscheduler(pschedulerIn) {}

    /**
     * Add a callback to be executed. Callbacks are executed serially
     * and memory is release-acquire consistent between callback executions.
     * Practically, this means that callbacks can behave as if they are executed
     * in order by a single thread.
     */
    void AddToProcessQueue(std::function<void()> func);

    /**
     * Processes all remaining queue members on the calling thread, blocking until queue is empty
     * Must be called after the CScheduler has no remaining processing threads!
     */
    void EmptyQueue();

    size_t CallbacksPending();
};

#endif
