// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_QMLCALLRUNNER_H
#define BITCOIN_QT_QUICK_QMLCALLRUNNER_H

#include <atomic>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <thread>

//! Runs calls off the GUI thread on two lanes and hands each result back to the
//! GUI thread, queued. The serial lane is one thread of its own that runs its
//! calls in order; the pooled lane runs each call on QThreadPool::globalInstance()
//! (never a private pool), so the composition root's waits on the global pool
//! reap it. A call is a `fn` that runs on the worker and a `deliver` that runs
//! on the GUI thread after `fn` returned; `deliver` is dropped when the runner
//! was closed or drained, or moved to a new generation, after the call was
//! posted. A call on either lane never waits on a core message, because the
//! QML front end delivers every core message queued.
//!
//! A call that throws is classified by the DisconnectPolicy. `deliver` runs on
//! the GUI thread as an ordinary queued call; the runner does not catch an
//! exception from it, which reaches the application's notify() as one from any
//! event handler does. Closing is one-way: nothing reopens a runner, so a test
//! or a front end builds a new one.
class QmlCallRunner
{
public:
    //! How a call that throws is classified. matches(what) true means the
    //! daemon connection is gone: notify(what) is called once for each call
    //! that throws so, on the worker thread, and that call's deliver is
    //! dropped. Any other exception a call's fn throws while the runner is
    //! open is handed to fail(ep) on the GUI thread, queued, and that call's
    //! deliver is dropped. fail must not throw: the call into it is delivered
    //! by Qt, and an exception does not cross Qt's event delivery on every
    //! platform.
    struct DisconnectPolicy
    {
        std::function<bool(const std::string&)> matches;
        std::function<void(const std::string&)> notify;
        std::function<void(std::exception_ptr)> fail;
    };

    //! Starts the serial thread. Throws std::invalid_argument if any policy
    //! function is empty.
    explicit QmlCallRunner(DisconnectPolicy policy);
    //! Closes the runner, moves it to a new generation and joins the serial
    //! thread. It does not wait for pooled jobs: they hold the shared state and
    //! their deliveries are dropped.
    ~QmlCallRunner();
    QmlCallRunner(const QmlCallRunner&) = delete;
    QmlCallRunner& operator=(const QmlCallRunner&) = delete;

    //! Queues `fn` on the serial lane, in order, and returns true; `deliver`
    //! runs on the GUI thread after `fn` returned, if the runner is still open
    //! and in the generation current at this call. Returns false, and queues
    //! nothing, once the runner is closed or drained.
    [[nodiscard]] bool postSerial(std::function<void()> fn, std::function<void()> deliver);
    //! Submits `fn` to QThreadPool::globalInstance() and returns true; it
    //! starts at once unless the pool is saturated. `fn` is skipped, and
    //! `deliver` dropped, if the runner was closed or moved to a new generation
    //! before it started. Returns false, and submits nothing, once the runner is
    //! closed or drained.
    [[nodiscard]] bool postPooled(std::function<void()> fn, std::function<void()> deliver);
    //! Moves to a new generation: queued calls and results of earlier ones are
    //! dropped, calls posted afterwards run as normal.
    void bumpGeneration();
    //! Marks the runner closed; one-way. Every later post is refused, and a
    //! call that throws anything but a lost connection from then on is logged
    //! and not handed to fail().
    void close() noexcept;
    //! GUI thread. Closes the runner, moves it to a new generation, clears the
    //! serial queue and waits until no serial call is in flight. It never waits
    //! for the pooled lane.
    void drain();
    //! True once close() or drain() ran.
    bool closed() const;

private:
    struct State;
    std::shared_ptr<State> m_state;
    std::thread m_serial_thread;
    void SerialLoop();
    static bool RunGuarded(const std::shared_ptr<State>& state, const std::function<void()>& fn);
    static void PostDelivery(const std::shared_ptr<State>& state, uint64_t captured, std::function<void()> deliver);
    static void PostFailure(const std::shared_ptr<State>& state, std::exception_ptr ep);
};

#endif // BITCOIN_QT_QUICK_QMLCALLRUNNER_H
