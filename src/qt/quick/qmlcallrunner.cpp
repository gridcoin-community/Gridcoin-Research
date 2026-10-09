// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/quick/qmlcallrunner.h>

#include <qt/guilog.h>

#include <QCoreApplication>
#include <QMetaObject>
#include <QThreadPool>

#include <condition_variable>
#include <deque>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <utility>

//! The state a runner shares with the jobs and deliveries it posts, so they
//! never hold the runner itself.
struct QmlCallRunner::State {
    //! One serial-lane call: the work, its delivery and the generation it was
    //! posted in.
    struct Job {
        std::function<void()> fn;
        std::function<void()> deliver;
        uint64_t captured{0};
    };

    //! One-way: set by close() and drain().
    std::atomic<bool> closed{false};
    //! Bumped by bumpGeneration(), drain() and the destructor.
    std::atomic<uint64_t> generation{0};
    DisconnectPolicy policy;

    //! Guards queue, running and stopping.
    std::mutex mutex;
    //! Signalled when the serial queue gains a job or `stopping` is set.
    std::condition_variable work_cv;
    //! Signalled when the serial lane finishes a job.
    std::condition_variable idle_cv;
    std::deque<Job> queue;
    //! A serial call is in flight.
    bool running{false};
    //! The destructor asks the serial thread to end.
    bool stopping{false};
};

QmlCallRunner::QmlCallRunner(DisconnectPolicy policy)
    : m_state(std::make_shared<State>())
{
    if (!policy.matches || !policy.notify) {
        throw std::invalid_argument("QmlCallRunner: the disconnect policy needs both functions");
    }
    m_state->policy = std::move(policy);
    m_serial_thread = std::thread([this] { SerialLoop(); });
}

QmlCallRunner::~QmlCallRunner()
{
    close();
    ++m_state->generation;
    {
        std::lock_guard<std::mutex> lock(m_state->mutex);
        m_state->stopping = true;
    }
    m_state->work_cv.notify_all();
    if (m_serial_thread.joinable()) m_serial_thread.join();
}

void QmlCallRunner::SerialLoop()
{
    const std::shared_ptr<State> state = m_state;
    for (;;) {
        State::Job job;
        {
            std::unique_lock<std::mutex> lock(state->mutex);
            state->work_cv.wait(lock, [&] { return state->stopping || !state->queue.empty(); });
            if (state->queue.empty()) return; // stopping, nothing left
            job = std::move(state->queue.front());
            state->queue.pop_front();
            state->running = true;
        }

        if (!state->closed && state->generation == job.captured) {
            if (RunGuarded(state, job.fn)) {
                PostDelivery(state, job.captured, std::move(job.deliver));
            }
        }

        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->running = false;
        }
        state->idle_cv.notify_all();
    }
}

bool QmlCallRunner::RunGuarded(const std::shared_ptr<State>& state, const std::function<void()>& fn)
{
    // A disconnect is quiet. Any other exception is logged on the worker and,
    // while the runner is open, rethrown on the GUI thread, where the
    // application's exception handling treats it like one from any event
    // handler.
    try {
        fn();
        return true;
    } catch (const std::exception& e) {
        if (state->policy.matches(e.what())) {
            state->policy.notify(e.what());
        } else {
            GUILogPrintf("QmlCallRunner: call threw: %s", e.what());
            if (!state->closed) {
                std::exception_ptr ep = std::current_exception();
                QMetaObject::invokeMethod(QCoreApplication::instance(),
                                          [ep] { std::rethrow_exception(ep); },
                                          Qt::QueuedConnection);
            }
        }
        return false;
    } catch (...) {
        GUILogPrintf("QmlCallRunner: call threw a non-standard exception");
        if (!state->closed) {
            std::exception_ptr ep = std::current_exception();
            QMetaObject::invokeMethod(QCoreApplication::instance(),
                                      [ep] { std::rethrow_exception(ep); },
                                      Qt::QueuedConnection);
        }
        return false;
    }
}

void QmlCallRunner::PostDelivery(const std::shared_ptr<State>& state, uint64_t captured, std::function<void()> deliver)
{
    if (!deliver) return;
    QMetaObject::invokeMethod(QCoreApplication::instance(),
                              [state, captured, deliver = std::move(deliver)] {
                                  if (!state->closed && state->generation == captured) deliver();
                              },
                              Qt::QueuedConnection);
}

bool QmlCallRunner::postSerial(std::function<void()> fn, std::function<void()> deliver)
{
    if (m_state->closed) {
        GUILogPrintf("QmlCallRunner: call refused after close");
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(m_state->mutex);
        m_state->queue.push_back(State::Job{std::move(fn), std::move(deliver), m_state->generation.load()});
    }
    m_state->work_cv.notify_one();
    return true;
}

bool QmlCallRunner::postPooled(std::function<void()> fn, std::function<void()> deliver)
{
    if (m_state->closed) {
        GUILogPrintf("QmlCallRunner: call refused after close");
        return false;
    }
    const std::shared_ptr<State> state = m_state;
    const uint64_t captured = state->generation.load();
    QThreadPool::globalInstance()->start([state, captured, fn = std::move(fn), deliver = std::move(deliver)] {
        if (state->closed || state->generation != captured) return;
        if (RunGuarded(state, fn)) {
            PostDelivery(state, captured, deliver);
        }
    });
    return true;
}

void QmlCallRunner::bumpGeneration()
{
    ++m_state->generation;
}

void QmlCallRunner::close() noexcept
{
    m_state->closed = true;
}

void QmlCallRunner::drain()
{
    m_state->closed = true;
    ++m_state->generation;
    std::unique_lock<std::mutex> lock(m_state->mutex);
    m_state->queue.clear();
    m_state->idle_cv.wait(lock, [this] { return !m_state->running; });
}

bool QmlCallRunner::closed() const
{
    return m_state->closed;
}
