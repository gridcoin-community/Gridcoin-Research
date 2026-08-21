add_executable(test_gridcoin-qt
    test_main.cpp
    bitcoinunitstests.cpp
    uritests.cpp
    sidestakemodeltests.cpp
)

set_target_properties(test_gridcoin-qt PROPERTIES
    AUTOMOC ON
)

target_link_libraries(test_gridcoin-qt PRIVATE
${RUNTIME_LIBS}
    ${QT}::Test
    ${QT}::Widgets
    ${QT}::Gui
    ${QT}::Core
    gridcoinqt
    gridcoin_util
    )

# --use_alt_stack=no is not optional on musl.
#
# Boost.Test installs an alternate signal stack at framework startup, sized
# BOOST_TEST_ALT_STACK_SIZE == SIGSTKSZ, and sigaltstack(2) returns ENOMEM when
# that is below the kernel's minimum. The kernel derives its minimum from the
# CPU's XSAVE area and publishes it as auxv AT_MINSIGSTKSZ. glibc 2.34+ made
# SIGSTKSZ dynamic -- sysconf(_SC_SIGSTKSZ), i.e. that same value -- but musl
# hardcodes 8192. On a CPU whose requirement exceeds 8192 the test binary
# therefore dies before running a single test:
#
#   Test setup error: system_error produced by: exp: Out of memory
#
# measured on a CI runner reporting AT_MINSIGSTKSZ = 11952 with 14.5 GB of its
# 16 GB free -- an undersized stack, never memory pressure. It presented as an
# intermittent Alpine failure only because it depends on which physical CPU the
# job lands on; hosts reporting ~3632 can never reproduce it.
#
# It cannot be fixed at compile time here: BOOST_TEST_DYN_LINK means
# execution_monitor.ipp is compiled inside libboost_unit_test_framework, so
# -DBOOST_TEST_DISABLE_ALT_STACK would be ignored. The runtime switch is the
# supported control. The cost is that Boost can no longer report a
# stack-overflow SIGSEGV from a separate stack, which is a small price for a
# suite that starts at all.
add_test(NAME gridcoin_qt_tests COMMAND test_gridcoin-qt --use_alt_stack=no)
