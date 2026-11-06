// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "fs.h"
#include "init.h"
#include "test/state_guard.h"
#include "util/system.h"
#include "util/win_acl.h"
#include "util/winservice.h"

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace util::winsvc;
namespace winacl = util::winacl;

namespace {

winacl::Sid SidOf(const std::string& text)
{
    const std::optional<winacl::Sid> sid = winacl::ParseSidString(text);
    BOOST_REQUIRE_MESSAGE(sid.has_value(), "'" << text << "' does not parse");
    return *sid;
}

//! NT SERVICE\gridcoinresearchd: S-1-5-80 and the SHA-1 of the upper-case name in UTF-16LE as five
//! little-endian sub-authorities, which gives TrustedInstaller's SID for "TrustedInstaller".
const char* const SERVICE_SID = "S-1-5-80-3522172824-109097731-499206491-3348895147-1114477730";

bool HasText(const std::string& text, const std::string& part)
{
    return text.find(part) != std::string::npos;
}

bool Flag(std::vector<const char*> args)
{
    args.insert(args.begin(), "gridcoinresearchd");
    return HasServiceFlag(static_cast<int>(args.size()), args.data());
}

//! Makes the core look like a Windows service until the scope ends.
struct ServiceMode
{
    ServiceMode() { SetRunningAsWindowsServiceForTesting(true); }
    ~ServiceMode() { SetRunningAsWindowsServiceForTesting(false); }
};

//! A fresh, empty data directory with -datadir pointing at it, for one case. The guard,
//! constructed before -datadir is set, restores the fixture's settings when the case ends.
struct ScratchDataDir
{
    fs::path path{fs::temp_directory_path() / fs::unique_path("winservice_tests_%%%%-%%%%-%%%%")};
    grc_test::StateGuard guard;

    ScratchDataDir()
    {
        fs::create_directories(path);
        gArgs.ForceSetArg("-datadir", path.string());
        gArgs.ClearPathCache();
    }

    ~ScratchDataDir()
    {
        gArgs.ClearPathCache();
        boost::system::error_code ec;
        fs::remove_all(path, ec);
    }
};

//! The service manager and the node's shutdown flag, as fakes for a ServiceRunner.
struct FakeScm
{
    std::mutex mutex;
    std::condition_variable cv;
    std::vector<SvcStatus> reports;
    std::atomic<bool> shutdown{false};
    std::atomic<int> requests{0};
    //! Called with each report, as the runner makes it: under the runner's lock.
    std::function<void(const SvcStatus&)> on_report;

    ServiceRunner::ReportFunction Report()
    {
        return [this](const SvcStatus& status) {
            {
                std::lock_guard<std::mutex> lock(mutex);
                reports.push_back(status);
            }
            cv.notify_all();
            if (on_report) on_report(status);
        };
    }

    std::function<bool()> Requested()
    {
        return [this] { return shutdown.load(); };
    }

    std::function<void()> Request()
    {
        return [this] {
            ++requests;
            shutdown = true;
        };
    }

    std::vector<SvcStatus> Reports()
    {
        std::lock_guard<std::mutex> lock(mutex);
        return reports;
    }

    //! Whether a report in \p state came within ten seconds.
    bool WaitFor(SvcState state)
    {
        std::unique_lock<std::mutex> lock(mutex);
        return cv.wait_for(lock, std::chrono::seconds{10}, [&] {
            return std::any_of(reports.begin(), reports.end(), [&](const SvcStatus& s) { return s.state == state; });
        });
    }
};

//! A runner on \p scm that looks for a shutdown every 5 ms and reports a stop's progress every
//! 10 ms. Returned as a prvalue, which C++17 constructs in place: a runner cannot be moved.
ServiceRunner MakeRunner(FakeScm& scm)
{
    return ServiceRunner(scm.Report(), scm.Requested(), scm.Request(), std::chrono::milliseconds{5},
                         std::chrono::milliseconds{10});
}

//! Waits, for at most ten seconds, until \p flag is set: a core that runs until it is stopped.
bool RunUntil(const std::atomic<bool>& flag)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
    while (!flag) {
        if (std::chrono::steady_clock::now() > deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    return true;
}

//! The checks every run's reports must pass: Start() and InitBegun() first, one Stopped report
//! and it last.
void CheckFrame(const std::vector<SvcStatus>& reports)
{
    BOOST_REQUIRE_GE(reports.size(), 3u);
    BOOST_CHECK(reports[0].state == SvcState::StartPending);
    BOOST_CHECK(reports[1].state == SvcState::Running);
    BOOST_CHECK(reports.back().state == SvcState::Stopped);
    BOOST_CHECK_EQUAL(std::count_if(reports.begin(), reports.end(),
                                    [](const SvcStatus& s) { return s.state == SvcState::Stopped; }),
                      1);
}

} // namespace

BOOST_AUTO_TEST_SUITE(winservice_tests)

BOOST_AUTO_TEST_CASE(status_start_then_running_accepts_stop_and_preshutdown)
{
    ServiceStatusMachine machine;

    const SvcStatus start = machine.Start();
    BOOST_CHECK(start.state == SvcState::StartPending);
    BOOST_CHECK_EQUAL(start.controls_accepted, 0u);
    BOOST_CHECK_EQUAL(start.checkpoint, 1u);
    BOOST_CHECK_EQUAL(start.wait_hint_ms, START_WAIT_HINT_MS);

    const SvcStatus running = machine.InitBegun();
    BOOST_CHECK(running.state == SvcState::Running);
    BOOST_CHECK_EQUAL(running.controls_accepted, ACCEPT_STOP | ACCEPT_PRESHUTDOWN);
    BOOST_CHECK_EQUAL(running.checkpoint, 0u);
    BOOST_CHECK_EQUAL(running.wait_hint_ms, 0u);
    BOOST_CHECK_EQUAL(running.win32_exit_code, 0u);
    BOOST_CHECK(!machine.ScmRequestedStop());
}

BOOST_AUTO_TEST_CASE(status_scm_stop_exits_zero)
{
    ServiceStatusMachine machine;
    machine.Start();
    machine.InitBegun();

    const std::optional<SvcStatus> stopping = machine.Control(CONTROL_STOP);
    BOOST_REQUIRE(stopping.has_value());
    BOOST_CHECK(stopping->state == SvcState::StopPending);
    BOOST_CHECK_EQUAL(stopping->controls_accepted, 0u);
    BOOST_CHECK_EQUAL(stopping->wait_hint_ms, STOP_WAIT_HINT_MS);
    BOOST_CHECK(machine.ScmRequestedStop());

    // The watcher then sees the shutdown it requested, which changes nothing.
    const SvcStatus observed = machine.ShutdownObserved();
    BOOST_CHECK(observed.state == SvcState::StopPending);
    BOOST_CHECK_EQUAL(observed.checkpoint, stopping->checkpoint);
    BOOST_CHECK(machine.ScmRequestedStop());

    const SvcStatus stopped = machine.Exit(/*app_ok=*/true);
    BOOST_CHECK(stopped.state == SvcState::Stopped);
    BOOST_CHECK_EQUAL(stopped.controls_accepted, 0u);
    BOOST_CHECK_EQUAL(stopped.checkpoint, 0u);
    BOOST_CHECK_EQUAL(stopped.wait_hint_ms, 0u);
    BOOST_CHECK_EQUAL(stopped.win32_exit_code, 0u);
    BOOST_CHECK_EQUAL(stopped.specific_exit_code, 0u);
}

BOOST_AUTO_TEST_CASE(status_preshutdown_same_as_stop)
{
    ServiceStatusMachine by_stop;
    by_stop.Start();
    by_stop.InitBegun();
    ServiceStatusMachine by_preshutdown;
    by_preshutdown.Start();
    by_preshutdown.InitBegun();

    const std::optional<SvcStatus> stop = by_stop.Control(CONTROL_STOP);
    const std::optional<SvcStatus> preshutdown = by_preshutdown.Control(CONTROL_PRESHUTDOWN);
    BOOST_REQUIRE(stop.has_value() && preshutdown.has_value());
    BOOST_CHECK(preshutdown->state == stop->state);
    BOOST_CHECK_EQUAL(preshutdown->controls_accepted, stop->controls_accepted);
    BOOST_CHECK_EQUAL(preshutdown->checkpoint, stop->checkpoint);
    BOOST_CHECK_EQUAL(preshutdown->wait_hint_ms, stop->wait_hint_ms);
    BOOST_CHECK(by_preshutdown.ScmRequestedStop());

    const SvcStatus stopped = by_preshutdown.Exit(/*app_ok=*/true);
    BOOST_CHECK_EQUAL(stopped.win32_exit_code, 0u);
    BOOST_CHECK_EQUAL(stopped.specific_exit_code, 0u);
}

// The RPC stop and the GUI's stop over IPC end the core on its own. It exits with a
// service-specific error, so that the failure actions restart it.
BOOST_AUTO_TEST_CASE(status_self_shutdown_exits_service_specific_1)
{
    ServiceStatusMachine machine;
    machine.Start();
    machine.InitBegun();

    const SvcStatus observed = machine.ShutdownObserved();
    BOOST_CHECK(observed.state == SvcState::StopPending);
    BOOST_CHECK_EQUAL(observed.controls_accepted, 0u);
    BOOST_CHECK_EQUAL(observed.wait_hint_ms, STOP_WAIT_HINT_MS);
    BOOST_CHECK(!machine.ScmRequestedStop());

    const SvcStatus stopped = machine.Exit(/*app_ok=*/true);
    BOOST_CHECK(stopped.state == SvcState::Stopped);
    BOOST_CHECK_EQUAL(stopped.win32_exit_code, ERROR_SERVICE_SPECIFIC);
    BOOST_CHECK_EQUAL(stopped.specific_exit_code, EXIT_SELF_STOP);
}

BOOST_AUTO_TEST_CASE(status_init_failure_exits_service_specific_2)
{
    ServiceStatusMachine machine;
    machine.Start();
    machine.InitBegun();

    const SvcStatus stopped = machine.Exit(/*app_ok=*/false);
    BOOST_CHECK(stopped.state == SvcState::Stopped);
    BOOST_CHECK_EQUAL(stopped.win32_exit_code, ERROR_SERVICE_SPECIFIC);
    BOOST_CHECK_EQUAL(stopped.specific_exit_code, EXIT_INIT_FAILED);

    // A start that failed after the core began to stop on its own counts the same.
    ServiceStatusMachine stopped_self;
    stopped_self.Start();
    stopped_self.InitBegun();
    stopped_self.ShutdownObserved();
    BOOST_CHECK_EQUAL(stopped_self.Exit(/*app_ok=*/false).specific_exit_code, EXIT_INIT_FAILED);
}

// A stop the service manager asks for during the start ends the start, which then returns
// false. The service stays stopped.
BOOST_AUTO_TEST_CASE(status_scm_stop_during_failed_init_exits_zero)
{
    ServiceStatusMachine machine;
    machine.Start();
    machine.InitBegun();
    BOOST_REQUIRE(machine.Control(CONTROL_STOP).has_value());
    machine.ShutdownObserved();
    machine.Tick();

    const SvcStatus stopped = machine.Exit(/*app_ok=*/false);
    BOOST_CHECK(stopped.state == SvcState::Stopped);
    BOOST_CHECK_EQUAL(stopped.win32_exit_code, 0u);
    BOOST_CHECK_EQUAL(stopped.specific_exit_code, 0u);
}

// A service started under any account but its own virtual one stops with its own code, which
// event 7024 shows, whoever asked for a stop meanwhile.
BOOST_AUTO_TEST_CASE(status_wrong_account_exits_service_specific_3)
{
    BOOST_CHECK_EQUAL(EXIT_WRONG_ACCOUNT, 3u);

    ServiceStatusMachine machine;
    machine.Start();
    machine.InitBegun();
    machine.RefuseStart(EXIT_WRONG_ACCOUNT);
    const SvcStatus stopped = machine.Exit(/*app_ok=*/false);
    BOOST_CHECK(stopped.state == SvcState::Stopped);
    BOOST_CHECK_EQUAL(stopped.win32_exit_code, ERROR_SERVICE_SPECIFIC);
    BOOST_CHECK_EQUAL(stopped.specific_exit_code, EXIT_WRONG_ACCOUNT);

    ServiceStatusMachine stopped_by_scm;
    stopped_by_scm.Start();
    stopped_by_scm.InitBegun();
    BOOST_REQUIRE(stopped_by_scm.Control(CONTROL_STOP).has_value());
    stopped_by_scm.RefuseStart(EXIT_WRONG_ACCOUNT);
    BOOST_CHECK_EQUAL(stopped_by_scm.Exit(/*app_ok=*/false).specific_exit_code, EXIT_WRONG_ACCOUNT);
}

// A start refused with 0, such as one asked for help, exits 0 and stays stopped, even though no
// one asked for a stop.
BOOST_AUTO_TEST_CASE(status_start_refused_with_zero_exits_zero)
{
    ServiceStatusMachine machine;
    machine.Start();
    machine.InitBegun();
    machine.RefuseStart(0);
    const SvcStatus stopped = machine.Exit(/*app_ok=*/false);
    BOOST_CHECK(stopped.state == SvcState::Stopped);
    BOOST_CHECK_EQUAL(stopped.win32_exit_code, 0u);
    BOOST_CHECK_EQUAL(stopped.specific_exit_code, 0u);
}

BOOST_AUTO_TEST_CASE(status_ticks_monotonic)
{
    ServiceStatusMachine machine;
    machine.Start();
    machine.InitBegun();

    // Not stopping: nothing to report progress on.
    const SvcStatus idle = machine.Tick();
    BOOST_CHECK(idle.state == SvcState::Running);
    BOOST_CHECK_EQUAL(idle.checkpoint, 0u);
    BOOST_CHECK_EQUAL(idle.wait_hint_ms, 0u);

    uint32_t last = machine.ShutdownObserved().checkpoint;
    for (int i = 0; i < 3; ++i) {
        const SvcStatus tick = machine.Tick();
        BOOST_CHECK(tick.state == SvcState::StopPending);
        BOOST_CHECK_GT(tick.checkpoint, last);
        BOOST_CHECK_EQUAL(tick.wait_hint_ms, STOP_WAIT_HINT_MS);
        last = tick.checkpoint;
    }

    // Stopped is final.
    machine.Exit(/*app_ok=*/true);
    const SvcStatus after = machine.Tick();
    BOOST_CHECK(after.state == SvcState::Stopped);
    BOOST_CHECK_EQUAL(after.checkpoint, 0u);
}

BOOST_AUTO_TEST_CASE(status_unknown_control_not_implemented)
{
    ServiceStatusMachine machine;
    machine.Start();
    machine.InitBegun();

    // SERVICE_CONTROL_PAUSE, and SERVICE_CONTROL_SHUTDOWN, which a service that accepts
    // PRESHUTDOWN is not sent.
    BOOST_CHECK(!machine.Control(0x2).has_value());
    BOOST_CHECK(!machine.Control(0x5).has_value());
    BOOST_CHECK(!machine.ScmRequestedStop());

    const std::optional<SvcStatus> interrogated = machine.Control(CONTROL_INTERROGATE);
    BOOST_REQUIRE(interrogated.has_value());
    BOOST_CHECK(interrogated->state == SvcState::Running);
    BOOST_CHECK(!machine.ScmRequestedStop());
}

BOOST_AUTO_TEST_CASE(service_flag_detection)
{
    BOOST_CHECK(Flag({"-service"}));
    BOOST_CHECK(Flag({"--service"}));
    BOOST_CHECK(Flag({"-service=1"}));
    BOOST_CHECK(!Flag({"-service=0"}));
    BOOST_CHECK(!Flag({"-servicex"}));

    BOOST_CHECK(!Flag({}));
    BOOST_CHECK(!Flag({"-multiprocess", "-datadir=C:\\x"}));
    // As ArgsManager reads a boolean: an empty value is true, a non-number is 0.
    BOOST_CHECK(Flag({"-service="}));
    BOOST_CHECK(Flag({"-service=2"}));
    BOOST_CHECK(!Flag({"-service=yes"}));
    // Windows options are case-insensitive and may start with '/'.
    BOOST_CHECK(Flag({"-SERVICE"}));
    BOOST_CHECK(Flag({"/service"}));
    // Negation, and the last of several.
    BOOST_CHECK(!Flag({"-noservice"}));
    BOOST_CHECK(Flag({"-noservice=0"}));
    BOOST_CHECK(!Flag({"-service", "-noservice"}));
    BOOST_CHECK(Flag({"-noservice", "-service"}));
    // A network section is not the option.
    BOOST_CHECK(!Flag({"-main.service"}));
    // Nothing after a command or a lone "-" is an option.
    BOOST_CHECK(!Flag({"getinfo", "-service"}));
    BOOST_CHECK(!Flag({"-", "-service"}));
    BOOST_CHECK(Flag({"-testnet", "-service", "-conf=C:\\ProgramData\\Gridcoin\\conf\\testnet.conf"}));
}

BOOST_AUTO_TEST_CASE(service_account_classification)
{
    BOOST_CHECK(ClassifyServiceAccount(SidOf(SERVICE_SID)) == ServiceAccountKind::VirtualServiceAccount);
    BOOST_CHECK(ClassifyServiceAccount(winacl::wellknown::TrustedInstaller())
                == ServiceAccountKind::VirtualServiceAccount);
    BOOST_CHECK(ClassifyServiceAccount(winacl::wellknown::System()) == ServiceAccountKind::LocalSystem);
    BOOST_CHECK(ClassifyServiceAccount(SidOf("S-1-5-19")) == ServiceAccountKind::LocalService);
    BOOST_CHECK(ClassifyServiceAccount(SidOf("S-1-5-20")) == ServiceAccountKind::NetworkService);

    // A user account, and an administrator's.
    BOOST_CHECK(ClassifyServiceAccount(SidOf("S-1-5-21-1004336348-1177238915-682003330-1001"))
                == ServiceAccountKind::Other);
    BOOST_CHECK(ClassifyServiceAccount(SidOf("S-1-5-21-1004336348-1177238915-682003330-500"))
                == ServiceAccountKind::Other);
    // NT SERVICE\ALL SERVICES is a group of every service, not one service's account; a
    // SID under 80 with any other number of sub-authorities is no virtual account either.
    BOOST_CHECK(ClassifyServiceAccount(SidOf("S-1-5-80-0")) == ServiceAccountKind::Other);
    BOOST_CHECK(ClassifyServiceAccount(SidOf("S-1-5-80-1-2-3-4")) == ServiceAccountKind::Other);
    BOOST_CHECK(ClassifyServiceAccount(SidOf("S-1-5-80-1-2-3-4-5-6")) == ServiceAccountKind::Other);
    // The same sub-authorities under another authority.
    BOOST_CHECK(ClassifyServiceAccount(SidOf("S-1-16-80-3522172824-109097731-499206491-3348895147-1114477730"))
                == ServiceAccountKind::Other);
    // Virtual accounts of other kinds (IIS application pools, S-1-5-82) are not services'.
    BOOST_CHECK(ClassifyServiceAccount(SidOf("S-1-5-82-1-2-3-4-5")) == ServiceAccountKind::Other);
    BOOST_CHECK(ClassifyServiceAccount(winacl::Sid{}) == ServiceAccountKind::Other);
}

BOOST_AUTO_TEST_CASE(service_account_refusal_names_the_account_and_the_fix)
{
    BOOST_CHECK(!ServiceAccountRefusal(SidOf(SERVICE_SID), "NT SERVICE\\gridcoinresearchd", "gridcoinresearchd"));

    const std::optional<std::string> system =
        ServiceAccountRefusal(winacl::wellknown::System(), "NT AUTHORITY\\SYSTEM", "gridcoinresearchd");
    BOOST_REQUIRE(system.has_value());
    BOOST_CHECK_MESSAGE(HasText(*system, "LocalSystem"), *system);
    BOOST_CHECK_MESSAGE(HasText(*system, "NT AUTHORITY\\SYSTEM"), *system);
    BOOST_CHECK_MESSAGE(HasText(*system, "S-1-5-18"), *system);
    BOOST_CHECK_MESSAGE(HasText(*system, "NT SERVICE\\gridcoinresearchd"), *system);
    BOOST_CHECK_MESSAGE(HasText(*system, "gridcoin-service.exe install"), *system);

    const std::optional<std::string> local =
        ServiceAccountRefusal(SidOf("S-1-5-19"), "NT AUTHORITY\\LOCAL SERVICE", "gridcointestnetd");
    BOOST_REQUIRE(local.has_value());
    BOOST_CHECK_MESSAGE(HasText(*local, "LocalService"), *local);
    BOOST_CHECK_MESSAGE(HasText(*local, "NT SERVICE\\gridcointestnetd"), *local);

    BOOST_CHECK(ServiceAccountRefusal(SidOf("S-1-5-20"), "", "gridcoinresearchd").has_value());

    // A user account without a name it can be shown by.
    const std::optional<std::string> user =
        ServiceAccountRefusal(SidOf("S-1-5-21-1004336348-1177238915-682003330-1001"), "", "gridcoinresearchd");
    BOOST_REQUIRE(user.has_value());
    BOOST_CHECK_MESSAGE(HasText(*user, "S-1-5-21-1004336348-1177238915-682003330-1001"), *user);
    BOOST_CHECK_MESSAGE(HasText(*user, "NT SERVICE\\gridcoinresearchd"), *user);
}

BOOST_AUTO_TEST_CASE(token_description_lists_privileges_restriction_and_integrity)
{
    BOOST_CHECK_EQUAL(IntegrityLevelName(0x0000), "Untrusted");
    BOOST_CHECK_EQUAL(IntegrityLevelName(0x1000), "Low");
    BOOST_CHECK_EQUAL(IntegrityLevelName(0x2000), "Medium");
    BOOST_CHECK_EQUAL(IntegrityLevelName(0x2100), "Medium Plus");
    BOOST_CHECK_EQUAL(IntegrityLevelName(0x3000), "High");
    BOOST_CHECK_EQUAL(IntegrityLevelName(0x4000), "System");
    BOOST_CHECK_EQUAL(IntegrityLevelName(0x5000), "Protected Process");
    BOOST_CHECK_EQUAL(IntegrityLevelName(0x2010), "0x2010");

    TokenFacts facts;
    facts.user = std::string("NT SERVICE\\gridcoinresearchd (") + SERVICE_SID + ")";
    facts.enabled_privileges = {"SeChangeNotifyPrivilege"};
    facts.restricting_sids = 1;
    facts.write_restricted = true;
    facts.integrity_rid = 0x4000;

    const std::string text = DescribeTokenFacts(facts);
    BOOST_CHECK_MESSAGE(HasText(text, "NT SERVICE\\gridcoinresearchd"), text);
    BOOST_CHECK_MESSAGE(HasText(text, "enabled privileges: SeChangeNotifyPrivilege;"), text);
    BOOST_CHECK_MESSAGE(HasText(text, "disabled privileges: none"), text);
    BOOST_CHECK_MESSAGE(HasText(text, "write-restricted: yes"), text);
    BOOST_CHECK_MESSAGE(HasText(text, "restricting SIDs: 1"), text);
    BOOST_CHECK_MESSAGE(HasText(text, "integrity: System"), text);

    facts.enabled_privileges = {"SeChangeNotifyPrivilege", "SeImpersonatePrivilege"};
    facts.disabled_privileges = {"SeShutdownPrivilege"};
    facts.restricting_sids = 0;
    facts.write_restricted = false;
    facts.integrity_rid = 0x2000;
    const std::string wide = DescribeTokenFacts(facts);
    BOOST_CHECK_MESSAGE(HasText(wide, "enabled privileges: SeChangeNotifyPrivilege, SeImpersonatePrivilege;"), wide);
    BOOST_CHECK_MESSAGE(HasText(wide, "disabled privileges: SeShutdownPrivilege"), wide);
    BOOST_CHECK_MESSAGE(HasText(wide, "write-restricted: no"), wide);
    BOOST_CHECK_MESSAGE(HasText(wide, "integrity: Medium"), wide);

    facts.enabled_privileges.clear();
    BOOST_CHECK_MESSAGE(HasText(DescribeTokenFacts(facts), "enabled privileges: none"), DescribeTokenFacts(facts));
}

// Windows sends CTRL_LOGOFF_EVENT to every service whenever any user signs out. It must not
// stop the service; every other event still stops the node, as in a console.
BOOST_AUTO_TEST_CASE(logoff_does_not_stop_a_service)
{
    BOOST_CHECK(!ConsoleEventStopsNode(CONSOLE_LOGOFF_EVENT, /*service=*/true));
    BOOST_CHECK(ConsoleEventStopsNode(CONSOLE_LOGOFF_EVENT, /*service=*/false));

    // CTRL_C_EVENT, CTRL_BREAK_EVENT, CTRL_CLOSE_EVENT and CTRL_SHUTDOWN_EVENT.
    for (const uint32_t event : {0u, 1u, 2u, 6u}) {
        BOOST_CHECK_MESSAGE(ConsoleEventStopsNode(event, /*service=*/true), event);
        BOOST_CHECK_MESSAGE(ConsoleEventStopsNode(event, /*service=*/false), event);
    }
}

BOOST_AUTO_TEST_CASE(image_directory_allowed_writers)
{
    const winacl::Sid self = SidOf("S-1-5-21-1004336348-1177238915-682003330-1001");
    const std::vector<winacl::Sid> admin_only{winacl::wellknown::System(), winacl::wellknown::Administrators(),
                                              winacl::wellknown::TrustedInstaller()};

    // The service's executable may be changed only by the administrators' accounts, not by
    // the service's own account.
    std::vector<winacl::Sid> service = ImageDirectoryAllowed(/*service=*/true, SidOf(SERVICE_SID));
    std::sort(service.begin(), service.end());
    std::vector<winacl::Sid> expected = admin_only;
    std::sort(expected.begin(), expected.end());
    BOOST_CHECK(service == expected);

    // A -multiprocess core that is not a service also allows the account it runs as.
    std::vector<winacl::Sid> multiprocess = ImageDirectoryAllowed(/*service=*/false, self);
    std::sort(multiprocess.begin(), multiprocess.end());
    expected.push_back(self);
    std::sort(expected.begin(), expected.end());
    BOOST_CHECK(multiprocess == expected);
}

BOOST_AUTO_TEST_CASE(image_directory_message_names_the_override)
{
    const fs::path exe("C:\\Apps\\Gridcoin\\daemon\\gridcoinresearchd.exe");
    const std::string reason = "'C:\\Apps\\Gridcoin\\daemon': its DACL lets S-1-5-32-545 change it (access mask 0x00000002)";

    // Each mode states its own rule: the service's executable only the administrators' accounts
    // may change, a -multiprocess core's this account too.
    const std::string service = ImageDirectoryMessage(exe, reason, /*service=*/true, /*allow_unsafe=*/false);
    BOOST_CHECK_MESSAGE(HasText(service, exe.string()), service);
    BOOST_CHECK_MESSAGE(HasText(service, "S-1-5-32-545"), service);
    BOOST_CHECK_MESSAGE(HasText(service, "-allowunsafeimagedir"), service);
    BOOST_CHECK_MESSAGE(HasText(service, "-service"), service);
    BOOST_CHECK_MESSAGE(HasText(service, "other than SYSTEM, Administrators and TrustedInstaller"), service);
    BOOST_CHECK_MESSAGE(HasText(service, "not even the service's own account"), service);

    const std::string multiprocess = ImageDirectoryMessage(exe, reason, /*service=*/false, /*allow_unsafe=*/false);
    BOOST_CHECK_MESSAGE(HasText(multiprocess, "-multiprocess"), multiprocess);
    BOOST_CHECK_MESSAGE(HasText(multiprocess, "other than this one, SYSTEM, Administrators and TrustedInstaller"),
                        multiprocess);
    BOOST_CHECK_MESSAGE(!HasText(multiprocess, "-service"), multiprocess);
    BOOST_CHECK_MESSAGE(HasText(multiprocess, "-allowunsafeimagedir"), multiprocess);

    const std::string allowed = ImageDirectoryMessage(exe, reason, /*service=*/true, /*allow_unsafe=*/true);
    BOOST_CHECK_MESSAGE(HasText(allowed, "WARNING"), allowed);
    BOOST_CHECK_MESSAGE(HasText(allowed, "S-1-5-32-545"), allowed);
    BOOST_CHECK_MESSAGE(HasText(allowed, "-allowunsafeimagedir"), allowed);
    BOOST_CHECK(allowed != service);
}

// The run against a fake service manager: an SCM stop is reported in order and exits 0, and
// once the stopped report is made nothing more is reported or requested.
BOOST_AUTO_TEST_CASE(runner_scm_stop_is_reported_in_order)
{
    FakeScm scm;
    ServiceRunner runner = MakeRunner(scm);

    // Before the first report a control does nothing.
    BOOST_CHECK_EQUAL(runner.OnControl(CONTROL_INTERROGATE), HANDLER_OK);
    BOOST_CHECK(scm.Reports().empty());

    bool ran_ok = false;
    std::thread service([&] { ran_ok = runner.Run([&] { return RunUntil(scm.shutdown); }); });
    BOOST_REQUIRE(scm.WaitFor(SvcState::Running));

    BOOST_CHECK_EQUAL(runner.OnControl(0x2), HANDLER_NOT_IMPLEMENTED);
    BOOST_CHECK_EQUAL(runner.OnControl(CONTROL_STOP), HANDLER_OK);
    service.join();

    BOOST_CHECK(ran_ok);
    BOOST_CHECK_EQUAL(scm.requests.load(), 1);
    const std::vector<SvcStatus> reports = scm.Reports();
    CheckFrame(reports);
    BOOST_CHECK(reports[2].state == SvcState::StopPending);
    BOOST_CHECK_EQUAL(reports[2].controls_accepted, 0u);
    BOOST_CHECK_EQUAL(reports.back().win32_exit_code, 0u);
    BOOST_CHECK_EQUAL(reports.back().specific_exit_code, 0u);

    // After the stopped report: nothing reported, nothing requested.
    BOOST_CHECK_EQUAL(runner.OnControl(CONTROL_STOP), HANDLER_OK);
    BOOST_CHECK_EQUAL(runner.OnControl(CONTROL_PRESHUTDOWN), HANDLER_OK);
    BOOST_CHECK_EQUAL(runner.OnControl(CONTROL_INTERROGATE), HANDLER_OK);
    BOOST_CHECK_EQUAL(scm.Reports().size(), reports.size());
    BOOST_CHECK_EQUAL(scm.requests.load(), 1);
}

// The core's own stop: the watcher reports it, then its progress, and the exit is 1066/1.
BOOST_AUTO_TEST_CASE(runner_self_stop_reports_progress_and_exits_1)
{
    FakeScm scm;
    ServiceRunner runner = MakeRunner(scm);

    std::thread service([&] {
        runner.Run([&] {
            const bool stopped = RunUntil(scm.shutdown);
            // The shutdown takes a while, long enough for several progress reports.
            std::this_thread::sleep_for(std::chrono::milliseconds{80});
            return stopped;
        });
    });
    BOOST_REQUIRE(scm.WaitFor(SvcState::Running));
    // The RPC stop: the flag set by the core itself, not through the service manager.
    scm.shutdown = true;
    service.join();

    BOOST_CHECK_EQUAL(scm.requests.load(), 0);
    const std::vector<SvcStatus> reports = scm.Reports();
    CheckFrame(reports);
    std::vector<uint32_t> checkpoints;
    for (const SvcStatus& status : reports) {
        if (status.state == SvcState::StopPending) checkpoints.push_back(status.checkpoint);
    }
    BOOST_REQUIRE_GE(checkpoints.size(), 3u);
    BOOST_CHECK_EQUAL(checkpoints.front(), 1u);
    for (size_t i = 1; i < checkpoints.size(); ++i) BOOST_CHECK_GT(checkpoints[i], checkpoints[i - 1]);
    BOOST_CHECK_EQUAL(reports.back().win32_exit_code, ERROR_SERVICE_SPECIFIC);
    BOOST_CHECK_EQUAL(reports.back().specific_exit_code, EXIT_SELF_STOP);
}

// The exit code is settled before the stopped report: after it, the process may end at once.
BOOST_AUTO_TEST_CASE(runner_settles_the_exit_before_the_stopped_report)
{
    FakeScm scm;
    ServiceRunner runner = MakeRunner(scm);

    std::vector<std::string> order;
    std::mutex order_mutex;
    scm.on_report = [&](const SvcStatus& status) {
        std::lock_guard<std::mutex> lock(order_mutex);
        order.push_back(status.state == SvcState::Stopped ? "stopped" : "report");
    };

    std::optional<bool> settled;
    BOOST_CHECK(!runner.Run([] { return false; }, [&](bool app_ok) {
        std::lock_guard<std::mutex> lock(order_mutex);
        settled = app_ok;
        order.push_back("exit settled");
    }));

    BOOST_REQUIRE(settled.has_value());
    BOOST_CHECK(!*settled);
    const std::vector<std::string> expected{"report", "report", "exit settled", "stopped"};
    BOOST_CHECK_EQUAL_COLLECTIONS(order.begin(), order.end(), expected.begin(), expected.end());
}

// A start that fails, one that throws, and one refused for its account.
BOOST_AUTO_TEST_CASE(runner_failed_and_refused_starts)
{
    {
        FakeScm scm;
        ServiceRunner runner = MakeRunner(scm);
        BOOST_CHECK(!runner.Run([] { return false; }));
        const std::vector<SvcStatus> reports = scm.Reports();
        CheckFrame(reports);
        BOOST_CHECK_EQUAL(reports.size(), 3u);
        BOOST_CHECK_EQUAL(reports.back().win32_exit_code, ERROR_SERVICE_SPECIFIC);
        BOOST_CHECK_EQUAL(reports.back().specific_exit_code, EXIT_INIT_FAILED);
    }
    {
        FakeScm scm;
        ServiceRunner runner = MakeRunner(scm);
        BOOST_CHECK(!runner.Run([]() -> bool { throw std::runtime_error("init threw"); }));
        const std::vector<SvcStatus> reports = scm.Reports();
        CheckFrame(reports);
        BOOST_CHECK_EQUAL(reports.back().specific_exit_code, EXIT_INIT_FAILED);
    }
    {
        FakeScm scm;
        ServiceRunner runner = MakeRunner(scm);
        BOOST_CHECK(!runner.Run([&] {
            runner.RefuseStart(EXIT_WRONG_ACCOUNT);
            return false;
        }));
        const std::vector<SvcStatus> reports = scm.Reports();
        CheckFrame(reports);
        BOOST_CHECK_EQUAL(reports.back().win32_exit_code, ERROR_SERVICE_SPECIFIC);
        BOOST_CHECK_EQUAL(reports.back().specific_exit_code, EXIT_WRONG_ACCOUNT);
    }
}

// A control that reaches the handler while the stopped report is being made waits for it, and
// then does nothing: no second report, no shutdown request, as the process may already be ending.
BOOST_AUTO_TEST_CASE(runner_reports_nothing_after_stopped)
{
    FakeScm scm;
    ServiceRunner runner = MakeRunner(scm);

    std::thread late;
    std::atomic<bool> entering{false};
    std::atomic<uint32_t> late_result{999};
    std::atomic<bool> waited_during_report{false};
    scm.on_report = [&](const SvcStatus& status) {
        // The handler on the dispatcher's thread. The stopped report waits until the control is
        // on its way, then a little longer, so that the control reaches the runner's lock while
        // this report holds it.
        if (status.state == SvcState::Stopped && !late.joinable()) {
            late = std::thread([&] {
                entering = true;
                late_result = runner.OnControl(CONTROL_STOP);
            });
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
            while (!entering && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::yield();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{40});
            // Still waiting: the control could not act while this report held the lock.
            waited_during_report = entering && late_result == 999;
        }
    };

    BOOST_CHECK(!runner.Run([] { return false; }));
    BOOST_REQUIRE(late.joinable());
    late.join();
    BOOST_CHECK(waited_during_report.load());

    BOOST_CHECK_EQUAL(late_result.load(), HANDLER_OK);
    BOOST_CHECK_EQUAL(scm.requests.load(), 0);
    BOOST_CHECK(!scm.shutdown.load());
    const std::vector<SvcStatus> reports = scm.Reports();
    CheckFrame(reports);
    BOOST_CHECK_EQUAL(reports.size(), 3u);
}

// The service's configuration is written by gridcoin-service.exe install, owned by the
// administrators, so a service that finds none stops and says so, and writes none itself.
BOOST_AUTO_TEST_CASE(missing_conf_in_service_mode_is_fatal)
{
    ScratchDataDir datadir;
    ServiceMode service;

    const fs::path conf = GetConfigFile();
    BOOST_REQUIRE(conf.parent_path() == datadir.path);
    BOOST_REQUIRE(!fs::exists(fs::symlink_status(conf)));

    std::string error;
    BOOST_CHECK(!PrepareConfigFile(error));
    BOOST_CHECK_MESSAGE(HasText(error, "gridcoin-service.exe install"), error);
    BOOST_CHECK_MESSAGE(HasText(error, conf.string()), error);

    BOOST_CHECK(!fs::exists(fs::symlink_status(conf)));
    BOOST_CHECK(fs::is_empty(datadir.path));
}

BOOST_AUTO_TEST_SUITE_END()
