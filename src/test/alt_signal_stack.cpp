// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

//! Install an adequately sized alternate signal stack before Boost.Test can
//! install an undersized one.
//!
//! Boost.Test installs an alternate signal stack sized BOOST_TEST_ALT_STACK_SIZE,
//! which is SIGSTKSZ. glibc 2.34+ made SIGSTKSZ dynamic -- sysconf(_SC_SIGSTKSZ),
//! which resolves to what the kernel actually requires. **musl hardcodes 8192.**
//!
//! The kernel derives its minimum from the CPU's XSAVE area and publishes it as
//! auxv AT_MINSIGSTKSZ. Measured, that value tracks the XSAVE size plus roughly
//! 940 bytes of signal-frame overhead:
//!
//!     Xeon E5-2687W v2 (AVX)     XSAVE   832 -> AT_MINSIGSTKSZ  1776
//!     Core i9-13900K   (AVX2)    XSAVE  2696 -> AT_MINSIGSTKSZ  3632
//!     CI runner (observed)                   -> AT_MINSIGSTKSZ 11952
//!
//! 11952 implies an XSAVE area near 11 KB, which only AMX reaches (tile data is
//! 8192 bytes on top of AVX-512's ~2.7 KB). So on musl, any host with AMX-class
//! register state -- Sapphire Rapids and later -- has a kernel minimum above
//! 8192, sigaltstack(2) rejects Boost's buffer with ENOMEM, and the binary dies
//! before running a single test:
//!
//!     Test setup error: system_error produced by: exp: Out of memory
//!
//! (Both halves of that message mislead. `exp` is a Boost bug --
//! BOOST_TEST_SYS_ASSERT stringizes the literal token `exp` rather than the
//! condition, so all six of its call sites report the same name. And it is an
//! undersized buffer, never memory pressure.)
//!
//! **Why the --use_alt_stack=no runtime switch does not fix it.** That parameter
//! is applied in exactly one place, unit_test_monitor_t::execute_and_translate,
//! which runs test cases and fixtures. The failing call happens earlier, from
//! framework::init() -- which is where the command line is parsed -- under an
//! execution_monitor still holding its constructor default of true. Confirmed by
//! backtrace: the install survives with the switch set. The switch does suppress
//! the per-test-case installs, so it is not inert; it simply cannot reach the one
//! that matters.
//!
//! **Why installing our own works.** Boost queries before it installs and only
//! installs when no alternate stack is active:
//!
//!     BOOST_TEST_SYS_ASSERT( ::sigaltstack( 0, &sigstk ) != -1 );
//!     if( sigstk.ss_flags & SS_DISABLE ) { ...install SIGSTKSZ bytes... }
//!
//! A stack installed before main() therefore makes Boost skip its own install for
//! that scope -- including framework::init(), which is the one that aborts. It
//! needs no Boost recompilation, so it works under BOOST_TEST_DYN_LINK where
//! -DBOOST_TEST_DISABLE_ALT_STACK is inert. Checked against 1.84 (Alpine's) and
//! 1.86.
//!
//! **This alone is not enough, and --use_alt_stack=no alone is not either.**
//! Boost's signal_handler DESTRUCTOR disables the alternate stack
//! unconditionally -- it is not guarded by the alt_stack argument -- so it tears
//! down whatever is installed, ours included, at the end of the first monitored
//! scope. Every later test case then finds no stack active and gets Boost's own
//! SIGSTKSZ-sized install.
//!
//! Measured with strace on this suite, counting only installs (ss_flags == 0):
//!
//!     flag only          our stack 0   boost 1   <- framework::init, aborts on musl
//!     pre-install only   our stack 1   boost 6   <- one per test case
//!     BOTH               our stack 1   boost 0   <- what we want
//!
//! So src/test/CMakeLists.txt keeps passing --use_alt_stack=no alongside this
//! file. The two cover disjoint phases; removing either reopens the failure.

#include <test/alt_signal_stack.h>

#if defined(__linux__)

#include <csignal>
#include <cstddef>
#include <vector>

#include <sys/auxv.h>

//! musl exposes getauxval() but does not pull in <linux/auxvec.h>, which is where
//! AT_MINSIGSTKSZ lives. The value is ABI, not a build-time choice.
#ifndef AT_MINSIGSTKSZ
#define AT_MINSIGSTKSZ 51
#endif

namespace {
//! Must outlive every signal that could be delivered, i.e. the process. A
//! function-local static gets that lifetime without depending on the
//! initialisation order of other translation units.
std::vector<char>& AltSignalStackStorage()
{
    static std::vector<char> storage;
    return storage;
}

//! Runs during static initialisation, so before main() and therefore before
//! Boost's framework::init(). Its value is unused; the side effect is the point.
AltSignalStackReport& MutableReport()
{
    static AltSignalStackReport report;
    return report;
}

//! Runs during static initialisation, before main() and therefore before Boost's
//! framework::init(). The value is never read; the side effect is the point, and
//! [[maybe_unused]] keeps a translation-unit const from drawing
//! -Wunused-const-variable on toolchains that warn about it.
[[maybe_unused]] const bool g_alt_signal_stack_installed = [] {
    AltSignalStackReport& report = MutableReport();
    report.attempted = true;

    // Nothing below may escape this lambda.
    //
    // Dynamic initialisation is not a context that tolerates exceptions: one
    // escaping here calls std::terminate and kills the binary before a single
    // test runs, which is exactly the failure this file exists to prevent.
    // resize() can throw std::bad_alloc, so record a failure rather than
    // propagate one.
    try {
    // What the kernel requires on THIS cpu, not what a header hardcodes. Zero
    // means the kernel did not publish it, in which case the constants below
    // are all we have.
    std::size_t wanted = static_cast<std::size_t>(::getauxval(AT_MINSIGSTKSZ));

    if (wanted < static_cast<std::size_t>(SIGSTKSZ)) {
        wanted = static_cast<std::size_t>(SIGSTKSZ);
    }

    if (wanted < static_cast<std::size_t>(MINSIGSTKSZ)) {
        wanted = static_cast<std::size_t>(MINSIGSTKSZ);
    }

    // Headroom over the bare minimum. The kernel's figure is what it takes to
    // DELIVER a signal; a handler that does any work of its own needs more, and
    // Boost's runs a longjmp and formats a message.
    wanted += 8192;

    AltSignalStackStorage().resize(wanted);

    stack_t alt_stack{};
    alt_stack.ss_sp = AltSignalStackStorage().data();
    alt_stack.ss_size = AltSignalStackStorage().size();
    alt_stack.ss_flags = 0;

    // Deliberately not fatal. If this fails the suite is no worse off than
    // before -- Boost will try its own and either succeed or produce the setup
    // error this exists to prevent. Aborting here would turn a recoverable
    // situation into an unconditional one.
    const bool ok = ::sigaltstack(&alt_stack, nullptr) == 0;

    report.installed = ok;
    report.requested = wanted;
    report.kernel_minimum = static_cast<std::size_t>(::getauxval(AT_MINSIGSTKSZ));

    return ok;

    } catch (...) {
        // Same reasoning as the sigaltstack failure above: without this file the
        // suite was no better off, so record the failure and let Boost proceed.
        report.installed = false;
        return false;
    }
}();
} // namespace

const AltSignalStackReport& GetAltSignalStackReport()
{
    return MutableReport();
}

#else // !__linux__

const AltSignalStackReport& GetAltSignalStackReport()
{
    static const AltSignalStackReport report;
    return report;
}

#endif // __linux__
