// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_TEST_FILE_SIZE_LIMIT_GUARD_H
#define GRIDCOIN_TEST_FILE_SIZE_LIMIT_GUARD_H

#ifndef WIN32
#include <signal.h>
#include <sys/resource.h>

namespace grc_test {

//! Limits the size of a file this process writes (RLIMIT_FSIZE) to \p limit bytes, so a write
//! past it fails with EFBIG, as on a full disk, and ignores the SIGXFSZ such a write raises.
//! Restores the limit and the signal's disposition on every exit path.
class FileSizeLimitGuard
{
public:
    explicit FileSizeLimitGuard(rlim_t limit)
    {
        struct sigaction ignore;
        ignore.sa_handler = SIG_IGN;
        sigemptyset(&ignore.sa_mask);
        ignore.sa_flags = 0;

        m_signal_saved = ::sigaction(SIGXFSZ, &ignore, &m_old_action) == 0;
        m_limit_saved = ::getrlimit(RLIMIT_FSIZE, &m_old_limit) == 0;

        if (m_limit_saved) {
            struct rlimit lim = m_old_limit;
            lim.rlim_cur = limit;
            ::setrlimit(RLIMIT_FSIZE, &lim);
        }
    }

    ~FileSizeLimitGuard() { Restore(); }

    FileSizeLimitGuard(const FileSizeLimitGuard&) = delete;
    FileSizeLimitGuard& operator=(const FileSizeLimitGuard&) = delete;

    void Restore()
    {
        if (m_limit_saved) {
            ::setrlimit(RLIMIT_FSIZE, &m_old_limit);
            m_limit_saved = false;
        }

        if (m_signal_saved) {
            ::sigaction(SIGXFSZ, &m_old_action, nullptr);
            m_signal_saved = false;
        }
    }

private:
    struct rlimit m_old_limit{};
    struct sigaction m_old_action{};
    bool m_limit_saved{false};
    bool m_signal_saved{false};
};

} // namespace grc_test
#endif // !WIN32

#endif // GRIDCOIN_TEST_FILE_SIZE_LIMIT_GUARD_H
