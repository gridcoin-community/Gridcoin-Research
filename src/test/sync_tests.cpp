// Copyright (c) 2012-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <sync.h>

#include <mutex>
#include <stdexcept>

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

BOOST_AUTO_TEST_SUITE_END()
