#include <boost/test/unit_test.hpp>

#include "support/allocators/secure.h"
#include "init.h"
#include "main.h"
#include "util.h"

BOOST_AUTO_TEST_SUITE(allocator_tests)

// Dummy memory page locker for platform independent tests
static const void *last_lock_addr, *last_unlock_addr;
static size_t last_lock_len, last_unlock_len;
class TestLocker
{
public:
    bool Lock(const void *addr, size_t len)
    {
        last_lock_addr = addr;
        last_lock_len = len;
        return true;
    }
    bool Unlock(const void *addr, size_t len)
    {
        last_unlock_addr = addr;
        last_unlock_len = len;
        return true;
    }
};

/** Mock LockedPageAllocator for testing */
class TestLockedPageAllocator: public LockedPageAllocator
{
public:
    TestLockedPageAllocator(int count_in, int lockedcount_in): count(count_in), lockedcount(lockedcount_in) {}
    void* AllocateLocked(size_t len, bool *lockingSuccess)
    {
        *lockingSuccess = false;
        if (count > 0) {
            --count;

            if (lockedcount > 0) {
                --lockedcount;
                *lockingSuccess = true;
            }

            return reinterpret_cast<void*>(0x08000000 + (count<<24)); // Fake address, do not actually use this memory
        }
        return 0;
    }
    void FreeLocked(void* addr, size_t len)
    {
    }
    size_t GetLimit()
    {
        return std::numeric_limits<size_t>::max();
    }
private:
    int count;
    int lockedcount;
};

BOOST_AUTO_TEST_CASE(lockedpool_tests_mock)
{
    // Test over three virtual arenas, of which one will succeed being locked
    std::unique_ptr<LockedPageAllocator> x(new TestLockedPageAllocator(3, 1));
    LockedPool pool(std::move(x));
    BOOST_CHECK(pool.stats().total == 0);
    BOOST_CHECK(pool.stats().locked == 0);

    void *a0 = pool.alloc(LockedPool::ARENA_SIZE / 2);
    BOOST_CHECK(a0);
    BOOST_CHECK(pool.stats().locked == LockedPool::ARENA_SIZE);
    void *a1 = pool.alloc(LockedPool::ARENA_SIZE / 2);
    BOOST_CHECK(a1);
    void *a2 = pool.alloc(LockedPool::ARENA_SIZE / 2);
    BOOST_CHECK(a2);
    void *a3 = pool.alloc(LockedPool::ARENA_SIZE / 2);
    BOOST_CHECK(a3);
    void *a4 = pool.alloc(LockedPool::ARENA_SIZE / 2);
    BOOST_CHECK(a4);
    void *a5 = pool.alloc(LockedPool::ARENA_SIZE / 2);
    BOOST_CHECK(a5);
    // We've passed a count of three arenas, so this allocation should fail
    void *a6 = pool.alloc(16);
    BOOST_CHECK(!a6);

    pool.free(a0);
    pool.free(a2);
    pool.free(a4);
    pool.free(a1);
    pool.free(a3);
    pool.free(a5);
    BOOST_CHECK(pool.stats().total == 3*LockedPool::ARENA_SIZE);
    BOOST_CHECK(pool.stats().locked == LockedPool::ARENA_SIZE);
    BOOST_CHECK(pool.stats().used == 0);
}

// These tests used the live LockedPoolManager object, this is also used
// by other tests so the conditions are somewhat less controllable and thus the
// tests are somewhat more error-prone.
BOOST_AUTO_TEST_CASE(lockedpool_tests_live)
{
    LockedPoolManager &pool = LockedPoolManager::Instance();
    LockedPool::Stats initial = pool.stats();

    void *a0 = pool.alloc(16);
    BOOST_CHECK(a0);
    // Test reading and writing the allocated memory
    *((uint32_t*)a0) = 0x1234;
    BOOST_CHECK(*((uint32_t*)a0) == 0x1234);

    pool.free(a0);
    try { // Test exception on double-free
        pool.free(a0);
        BOOST_CHECK(0);
    } catch(std::runtime_error &)
    {
    }
    // If more than one new arena was allocated for the above tests, something is wrong
    BOOST_CHECK(pool.stats().total <= (initial.total + LockedPool::ARENA_SIZE));
    // Usage must be back to where it started
    BOOST_CHECK(pool.stats().used == initial.used);
}

BOOST_AUTO_TEST_SUITE_END()
