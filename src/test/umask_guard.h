// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_TEST_UMASK_GUARD_H
#define GRIDCOIN_TEST_UMASK_GUARD_H

#ifndef WIN32
#include <sys/stat.h>
#include <sys/types.h>

namespace grc_test {

//! Sets the process umask to \p mask for one scope and puts the old one back however the
//! scope ends, a failed BOOST_REQUIRE included.
class UmaskGuard
{
public:
    explicit UmaskGuard(mode_t mask) : m_saved(::umask(mask)) {}
    ~UmaskGuard() { ::umask(m_saved); }

    UmaskGuard(const UmaskGuard&) = delete;
    UmaskGuard& operator=(const UmaskGuard&) = delete;

private:
    const mode_t m_saved;
};

} // namespace grc_test
#endif // !WIN32

#endif // GRIDCOIN_TEST_UMASK_GUARD_H
