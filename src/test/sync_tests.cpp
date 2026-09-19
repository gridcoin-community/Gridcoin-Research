// Copyright (c) 2012-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <sync.h>

#include <mutex>
#include <stdexcept>
#include <string>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(sync_tests)

BOOST_AUTO_TEST_CASE(potential_deadlock_detected)
{
    #ifdef DEBUG_LOCKORDER
    bool prev_lockorder_abort = GetLockOrderDebugAbort();
    bool prev_lockorder_throw_exception = GetLockOrderDebugThrowException();

    SetLockOrderDebugAbort(false);
    SetLockOrderDebugThrowException(true);

    #endif

    CCriticalSection mutex1, mutex2;
    {
        LOCK2(mutex1, mutex2);
    }
    BOOST_CHECK(LockStackEmpty());
    bool error_thrown = false;
    try {
        LOCK2(mutex2, mutex1);
    } catch (const std::logic_error& e) {
        BOOST_CHECK_EQUAL(e.what(), "potential deadlock detected: mutex1 -> mutex2 -> mutex1");
        error_thrown = true;
    }
    BOOST_CHECK(LockStackEmpty());
    #ifdef DEBUG_LOCKORDER
    BOOST_CHECK(error_thrown);
    #else
    BOOST_CHECK(!error_thrown);
    #endif

    #ifdef DEBUG_LOCKORDER
    SetLockOrderDebugAbort(prev_lockorder_abort);
    SetLockOrderDebugThrowException(prev_lockorder_throw_exception);
    #endif
}

#ifdef DEBUG_LOCKORDER
namespace {
//! Route the checker's verdict to an exception for the duration of a case.
struct ThrowOnReport
{
    const bool m_abort;
    const bool m_throw;
    ThrowOnReport() : m_abort(GetLockOrderDebugAbort()), m_throw(GetLockOrderDebugThrowException())
    {
        SetLockOrderDebugAbort(false);
        SetLockOrderDebugThrowException(true);
    }
    ~ThrowOnReport()
    {
        SetLockOrderDebugAbort(m_abort);
        SetLockOrderDebugThrowException(m_throw);
    }
};

//! Run \p f and return the checker's message, or the empty string if it
//! reported nothing.
template <typename F>
std::string ReportFrom(F&& f)
{
    try {
        f();
    } catch (const std::logic_error& e) {
        return e.what();
    }
    return "";
}
} // anonymous namespace

// A plain try-lock stays in the hierarchy: the reverse order is reported even
// though a try-lock cannot wait, as upstream chose (bitcoin/bitcoin#9674).
BOOST_AUTO_TEST_CASE(plain_try_lock_in_the_reverse_order_is_still_reported)
{
    ThrowOnReport guard;

    {
        CCriticalSection mutex1, mutex2;
        {
            LOCK(mutex1);
            TRY_LOCK(mutex2, taken);
            BOOST_REQUIRE(taken.owns_lock());
        }
        BOOST_CHECK_EQUAL(ReportFrom([&] { LOCK2(mutex2, mutex1); }),
                          "potential deadlock detected: mutex1 -> mutex2 -> mutex1");
        BOOST_CHECK(LockStackEmpty());
    }
    {
        CCriticalSection mutex1, mutex2;
        {
            LOCK2(mutex1, mutex2);
        }
        BOOST_CHECK_EQUAL(ReportFrom([&] {
            LOCK(mutex2);
            TRY_LOCK(mutex1, taken);
            BOOST_REQUIRE(taken.owns_lock());
        }), "potential deadlock detected: mutex1 -> mutex2 -> mutex1");
        BOOST_CHECK(LockStackEmpty());
    }
}

// TRY_LOCK_ORDER_EXEMPT is outside the hierarchy: no order is recorded into it
// and none checked, so the same two shapes are silent.
BOOST_AUTO_TEST_CASE(order_exempt_try_lock_in_the_reverse_order_is_not_reported)
{
    ThrowOnReport guard;

    {
        CCriticalSection mutex1, mutex2;
        {
            LOCK(mutex1);
            TRY_LOCK_ORDER_EXEMPT(mutex2, taken);
            BOOST_REQUIRE(taken.owns_lock());
        }
        BOOST_CHECK(LockStackEmpty());
        BOOST_CHECK_EQUAL(ReportFrom([&] { LOCK2(mutex2, mutex1); }), "");
        BOOST_CHECK(LockStackEmpty());
    }
    {
        CCriticalSection mutex1, mutex2;
        {
            LOCK2(mutex1, mutex2);
        }
        BOOST_CHECK_EQUAL(ReportFrom([&] {
            LOCK(mutex2);
            TRY_LOCK_ORDER_EXEMPT(mutex1, taken);
            BOOST_REQUIRE(taken.owns_lock());
        }), "");
        BOOST_CHECK(LockStackEmpty());
    }
}

// The lock an order-exempt try acquired is held like any other: a blocking lock
// taken while it is held is a real order, and its reverse is a real deadlock.
BOOST_AUTO_TEST_CASE(blocking_lock_taken_while_an_order_exempt_try_lock_is_held_is_still_ordered)
{
    ThrowOnReport guard;

    CCriticalSection mutex1, mutex2;
    {
        TRY_LOCK_ORDER_EXEMPT(mutex2, taken);
        BOOST_REQUIRE(taken.owns_lock());
        LOCK(mutex1);
    }
    BOOST_CHECK(LockStackEmpty());
    BOOST_CHECK_EQUAL(ReportFrom([&] { LOCK2(mutex1, mutex2); }),
                      "potential deadlock detected: mutex2 -> mutex1 -> mutex2");
    BOOST_CHECK(LockStackEmpty());
}

// Seeing a pair by an order-exempt try-lock first must not stop the same pair
// from being recorded, and later reported, when it is taken with blocking locks.
BOOST_AUTO_TEST_CASE(a_blocking_order_is_recorded_after_the_same_exempt_order)
{
    ThrowOnReport guard;

    CCriticalSection mutex1, mutex2;
    {
        LOCK(mutex1);
        TRY_LOCK_ORDER_EXEMPT(mutex2, taken);
        BOOST_REQUIRE(taken.owns_lock());
    }
    {
        LOCK2(mutex1, mutex2);
    }
    BOOST_CHECK(LockStackEmpty());
    BOOST_CHECK_EQUAL(ReportFrom([&] { LOCK2(mutex2, mutex1); }),
                      "potential deadlock detected: mutex1 -> mutex2 -> mutex1");
    BOOST_CHECK(LockStackEmpty());
}
#endif // DEBUG_LOCKORDER

BOOST_AUTO_TEST_SUITE_END()
