// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/guiipcworkertests.h"

#include "qt/guiipcworker.h"
#include "sync.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
using namespace std::chrono_literals;

using Role = GuiIpcWorker::Role;

//! The future is ready within ten seconds, so a regression fails rather than hangs.
bool ReadySoon(const std::future<void>& future)
{
    return future.valid() && future.wait_for(10s) == std::future_status::ready;
}

//! The thread \p worker runs its jobs on.
std::thread::id ThreadOf(GuiIpcWorker& worker)
{
    std::thread::id id;
    const std::future<void> done = worker.Post([&id] { id = std::this_thread::get_id(); });
    if (!ReadySoon(done)) return {};
    return id;
}

//! Sets its flag when it is destroyed, which a job that holds the only reference to it does when
//! the job is destroyed. It takes its time first, so that a future made ready before the job was
//! destroyed would be seen before the flag.
class Token
{
public:
    explicit Token(std::atomic<bool>& destroyed) : m_destroyed(destroyed) {}

    ~Token()
    {
        std::this_thread::sleep_for(100ms);
        m_destroyed = true;
    }

    Token(const Token&) = delete;
    Token& operator=(const Token&) = delete;

private:
    std::atomic<bool>& m_destroyed;
};
} // namespace

// Every job runs on one thread, not the GUI's, in the order posted, and one that throws does not
// end that thread. The worker is the process's worker for its role while it lives.
void GuiIpcWorkerTests::runsJobsInOrderOnOneThread()
{
    const std::thread::id gui_thread = std::this_thread::get_id();
    QVERIFY(GuiIpcWorker::Current(Role::Modal) == nullptr);
    QVERIFY(GuiIpcWorker::Current(Role::CoinLoad) == nullptr);

    {
        // Before the worker, so that they outlive any job it still runs as it goes.
        Mutex mutex;
        std::vector<int> order;
        std::vector<std::thread::id> threads;
        std::vector<std::future<void>> done;

        GuiIpcWorker worker(Role::Modal);
        QVERIFY(GuiIpcWorker::Current(Role::Modal) == &worker);
        QVERIFY(GuiIpcWorker::Current(Role::CoinLoad) == nullptr);

        for (int i = 0; i < 3; ++i) {
            done.push_back(worker.Post([&, i] {
                LOCK(mutex);
                order.push_back(i);
                threads.push_back(std::this_thread::get_id());
            }));
        }

        done.push_back(worker.Post([] { throw std::runtime_error("a job that throws"); }));
        done.push_back(worker.Post([&] {
            LOCK(mutex);
            threads.push_back(std::this_thread::get_id());
        }));

        for (const std::future<void>& future : done) {
            QVERIFY(ReadySoon(future));
        }

        LOCK(mutex);
        QVERIFY(order == (std::vector<int>{0, 1, 2}));
        QVERIFY(threads.size() == 4);
        QVERIFY(threads[0] != gui_thread);
        QVERIFY(std::all_of(threads.begin(), threads.end(), [&](const std::thread::id& id) { return id == threads[0]; }));
    }

    QVERIFY(GuiIpcWorker::Current(Role::Modal) == nullptr);
}

// The two roles run on threads of their own, so a coin load runs while a modal job is still busy.
// A second worker for a role is not the process's worker for it.
void GuiIpcWorkerTests::eachRoleHasItsOwnThread()
{
    // Before the workers, so that they outlive any job a worker still runs as it goes.
    std::promise<void> release;
    std::shared_future<void> released = release.get_future().share();
    std::atomic<bool> modal_busy{false};
    std::thread::id coin_thread;

    GuiIpcWorker modal(Role::Modal);
    GuiIpcWorker coin_load(Role::CoinLoad);
    QVERIFY(GuiIpcWorker::Current(Role::Modal) == &modal);
    QVERIFY(GuiIpcWorker::Current(Role::CoinLoad) == &coin_load);

    {
        GuiIpcWorker second(Role::Modal);
        QVERIFY(GuiIpcWorker::Current(Role::Modal) == &modal);
    }
    QVERIFY(GuiIpcWorker::Current(Role::Modal) == &modal);

    const std::future<void> modal_done = modal.Post([&modal_busy, released] {
        modal_busy = true;
        released.wait_for(10s);
    });

    const std::future<void> coin_done = coin_load.Post([&coin_thread] { coin_thread = std::this_thread::get_id(); });

    // The coin load finished while the modal job still waits.
    QVERIFY(ReadySoon(coin_done));
    QVERIFY(modal_busy.load());
    QVERIFY(modal_done.valid());
    QVERIFY(modal_done.wait_for(0s) != std::future_status::ready);

    release.set_value();
    QVERIFY(ReadySoon(modal_done));

    const std::thread::id modal_thread = ThreadOf(modal);
    QVERIFY(modal_thread != std::thread::id{});
    QVERIFY(coin_thread != std::thread::id{});
    QVERIFY(modal_thread != coin_thread);
    QVERIFY(modal_thread != std::this_thread::get_id());
    QVERIFY(coin_thread != std::this_thread::get_id());
}

// A job's future is ready only once the job has run and has been destroyed with what it captured;
// also when the job threw.
void GuiIpcWorkerTests::futureIsReadyOnceTheJobIsGone()
{
    std::atomic<bool> destroyed{false};
    std::atomic<bool> destroyed_after_throw{false};
    std::atomic<bool> ran{false};
    std::future<void> done;

    GuiIpcWorker worker(Role::Modal);

    {
        auto token = std::make_shared<Token>(destroyed);
        done = worker.Post([token = std::move(token), &ran] { ran = true; });
    }

    QVERIFY(ReadySoon(done));
    QVERIFY(ran.load());
    QVERIFY(destroyed.load());

    {
        auto token = std::make_shared<Token>(destroyed_after_throw);
        done = worker.Post([token = std::move(token)] { throw std::runtime_error("a job that throws"); });
    }

    QVERIFY(ReadySoon(done));
    QVERIFY(destroyed_after_throw.load());
}

// The worker is joined by its destructor, which runs the jobs still queued first.
void GuiIpcWorkerTests::runsQueuedJobsBeforeItGoes()
{
    std::atomic<int> ran{0};
    std::vector<std::future<void>> done;

    {
        GuiIpcWorker worker(Role::CoinLoad);
        done.push_back(worker.Post([] { std::this_thread::sleep_for(50ms); }));

        for (int i = 0; i < 5; ++i) {
            done.push_back(worker.Post([&ran] { ++ran; }));
        }
    }

    QCOMPARE(ran.load(), 5);

    for (const std::future<void>& future : done) {
        QVERIFY(future.valid());
        QVERIFY(future.wait_for(0s) == std::future_status::ready);
    }
}
