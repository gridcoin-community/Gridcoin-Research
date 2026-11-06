// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#ifdef WIN32
#include "chainparams.h"
#include "chainparamsbase.h"
#include "logging.h"
#include "test/state_guard.h"
#include "test/wine.h"
#include "tinyformat.h"
#include "util/dir_permissions.h"
#include "util/system.h"
#include "util/win_acl.h"
#include "util/winservice.h"
#include "wallet/autounlock.h"
#include "wallet/autounlock_blob.h"
#include "wallet/autounlock_dpapi.h"
#include "wallet/autounlock_store.h"

#include <chrono>
#include <functional>
#include <iterator>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <windows.h>
#include <wincrypt.h>
#endif

// The DPAPI store and fetch exist only on Windows, so elsewhere this suite is empty.
BOOST_AUTO_TEST_SUITE(autounlock_dpapi_tests)

#ifdef WIN32
namespace {
using namespace util::winacl;

//! Whether these cases can rely on Windows' own access control. Wine keeps no protected
//! DACL and has no real DPAPI key separation, so under it they only log.
bool SkipUnderWine()
{
    if (!grc_test::RunningUnderWine()) return false;
    BOOST_TEST_MESSAGE("Wine has no Windows security model for files and DPAPI; skipping");
    return true;
}

//! A fresh data directory with -datadir pointing at it, for one case. The guard,
//! constructed before -datadir is set, restores the fixture's when the case ends.
struct TempDataDir
{
    fs::path path{fs::temp_directory_path() / fs::unique_path("autounlock_dpapi_%%%%-%%%%-%%%%")};
    grc_test::StateGuard guard;

    TempDataDir()
    {
        fs::create_directories(path);
        gArgs.ForceSetArg("-datadir", path.string());
        gArgs.ClearPathCache();
    }

    ~TempDataDir()
    {
        boost::system::error_code ec;
        fs::remove_all(path, ec);
    }

    fs::path BlobPath() const { return path / "autounlock" / "passphrase.dpapi"; }
};

//! Installs a read-back hook for one scope.
struct ReadBackHook
{
    explicit ReadBackHook(std::function<void(const fs::path&)> hook)
    {
        SetAutoUnlockReadBackHookForTesting(std::move(hook));
    }
    ~ReadBackHook() { SetAutoUnlockReadBackHookForTesting({}); }
};

//! Makes the core look like a Windows service, or not, until the scope ends.
struct ServiceMode
{
    explicit ServiceMode(bool service) { util::winsvc::SetRunningAsWindowsServiceForTesting(service); }
    ~ServiceMode() { util::winsvc::SetRunningAsWindowsServiceForTesting(false); }
};

std::vector<unsigned char> ReadAll(const fs::path& path)
{
    fsbridge::ifstream file(path, std::ios::binary);
    BOOST_REQUIRE(file.is_open());
    return std::vector<unsigned char>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

//! What the autounlock file at \p path holds.
AutoUnlockBlob StoredBlob(const fs::path& path)
{
    const std::optional<AutoUnlockBlob> blob = ParseAutoUnlockBlob(ReadAll(path));
    BOOST_REQUIRE(blob.has_value());
    return *blob;
}

//! The owner-only list with \p trustee also granted \p rights.
AclSpec OwnerOnlyPlus(const Sid& trustee, uint32_t rights)
{
    AclSpec spec = util::OwnerOnlyAclSpec(/*inheritable=*/false);
    spec.aces.push_back({trustee, rights, Inherit::None});
    return spec;
}

bool Contains(const std::string& text, const std::string& part)
{
    return text.find(part) != std::string::npos;
}

//! Why this logon cannot encrypt and decrypt with its own DPAPI key (CryptProtectData
//! without CRYPTPROTECT_LOCAL_MACHINE), or nullopt when it can. A logon without a password,
//! such as one by ssh key, may not have the key's master key available.
std::optional<std::string> OwnDpapiKeyUnusable()
{
    unsigned char byte = 0x5a;
    DATA_BLOB plain{sizeof(byte), &byte};
    DATA_BLOB encrypted{};
    if (!::CryptProtectData(&plain, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &encrypted)) {
        const DWORD error = ::GetLastError();
        return strprintf("CryptProtectData failed with error %u", static_cast<unsigned>(error));
    }

    DATA_BLOB decrypted{};
    const bool round_trip =
        ::CryptUnprotectData(&encrypted, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &decrypted);
    const DWORD error = ::GetLastError();
    ::LocalFree(encrypted.pbData);
    if (!round_trip) return strprintf("CryptUnprotectData failed with error %u", static_cast<unsigned>(error));

    ::LocalFree(decrypted.pbData);
    return std::nullopt;
}

} // namespace

BOOST_AUTO_TEST_CASE(dpapi_store_fetch_roundtrip_local_machine)
{
    if (SkipUnderWine()) return;

    TempDataDir datadir;
    const fs::path path = datadir.BlobPath();
    DpapiBlobStore store;
    std::string error;

    // The platform's store is this one.
    BOOST_CHECK(dynamic_cast<DpapiBlobStore*>(GetAutoUnlockSecretStore()) != nullptr);

    AutoUnlockStoreStatus status = store.Status();
    BOOST_CHECK(status.supported);
    BOOST_CHECK(!status.configured);
    BOOST_CHECK_EQUAL(status.scope, "machine");
    BOOST_CHECK_EQUAL(status.location, fsbridge::LongPathString(path));

    const SecureString passphrase("dpapi-roundtrip-passphrase \xC3\xA9");
    BOOST_REQUIRE_MESSAGE(store.Store(passphrase, error) == AutoUnlockStoreOutcome::Verified, error);

    status = store.Status();
    BOOST_CHECK(status.configured);
    BOOST_CHECK_MESSAGE(status.problem.empty(), status.problem);

    const AutoUnlockBlob blob = StoredBlob(path);
    BOOST_CHECK(blob.scope == BlobScope::LocalMachine);
    BOOST_CHECK_EQUAL(blob.network, Params().NetworkIDString());

    // Owner-only from the start, and nothing left beside it.
    const std::optional<std::string> reason = CheckMatchesSpec(ReadAcl(path), util::OwnerOnlyAclSpec(false));
    BOOST_CHECK_MESSAGE(!reason.has_value(), reason.value_or(""));
    BOOST_CHECK(!fs::exists(path.string() + ".tmp"));

    std::optional<SecureString> fetched = FetchAutoUnlockFromDpapiBlob(path, error);
    BOOST_REQUIRE_MESSAGE(fetched.has_value(), error);
    BOOST_CHECK(*fetched == passphrase);

    // Storing again replaces it.
    const SecureString replacement("dpapi-roundtrip-replacement");
    BOOST_REQUIRE_MESSAGE(store.Store(replacement, error) == AutoUnlockStoreOutcome::Verified, error);
    fetched = FetchAutoUnlockFromDpapiBlob(path, error);
    BOOST_REQUIRE_MESSAGE(fetched.has_value(), error);
    BOOST_CHECK(*fetched == replacement);

    // Removed, autounlock is not set up; removing again is no error.
    BOOST_REQUIRE_MESSAGE(store.Remove(error), error);
    BOOST_CHECK(!fs::exists(path));
    BOOST_CHECK(!store.Status().configured);
    BOOST_CHECK(!FetchAutoUnlockFromDpapiBlob(path, error).has_value());
    BOOST_CHECK_MESSAGE(Contains(error, "not set up"), error);
    BOOST_CHECK_MESSAGE(store.Remove(error), error);
}

BOOST_AUTO_TEST_CASE(dpapi_fetch_refuses_loose_acl)
{
    if (SkipUnderWine()) return;

    TempDataDir datadir;
    const fs::path path = datadir.BlobPath();
    DpapiBlobStore store;
    std::string error;

    BOOST_REQUIRE_MESSAGE(store.Store(SecureString("dpapi-loose-acl"), error) == AutoUnlockStoreOutcome::Verified,
                          error);
    ApplyAcl(path, OwnerOnlyPlus(wellknown::Users(), FILE_GENERIC_READ));

    BOOST_CHECK(!FetchAutoUnlockFromDpapiBlob(path, error).has_value());
    BOOST_CHECK_MESSAGE(Contains(error, "readable by other accounts"), error);
    BOOST_CHECK_MESSAGE(Contains(error, "turn autounlock off and set it up again"), error);

    const AutoUnlockStoreStatus status = store.Status();
    BOOST_CHECK(status.configured);
    BOOST_CHECK_MESSAGE(Contains(status.problem, "readable by other accounts"), status.problem);
}

// An account that may change the file's access control list can grant itself read, which
// the reader check alone does not see.
BOOST_AUTO_TEST_CASE(dpapi_fetch_refuses_foreign_writer)
{
    if (SkipUnderWine()) return;

    TempDataDir datadir;
    const fs::path path = datadir.BlobPath();
    DpapiBlobStore store;
    std::string error;

    BOOST_REQUIRE_MESSAGE(store.Store(SecureString("dpapi-foreign-writer"), error) == AutoUnlockStoreOutcome::Verified,
                          error);
    ApplyAcl(path, OwnerOnlyPlus(wellknown::Users(), WRITE_DAC | READ_CONTROL | SYNCHRONIZE));

    const std::vector<Sid> allowed{CurrentTokenUser(), wellknown::System(), wellknown::Administrators()};
    BOOST_REQUIRE(!FindForeignReader(ReadAcl(path), allowed).has_value());

    BOOST_CHECK(!FetchAutoUnlockFromDpapiBlob(path, error).has_value());
    BOOST_CHECK_MESSAGE(Contains(error, "can be changed by other accounts"), error);
    BOOST_CHECK_MESSAGE(Contains(error, "turn autounlock off and set it up again"), error);
}

BOOST_AUTO_TEST_CASE(dpapi_fetch_refuses_wrong_network)
{
    if (SkipUnderWine()) return;

    TempDataDir datadir;
    const fs::path path = datadir.BlobPath();
    DpapiBlobStore store;
    std::string error;

    BOOST_REQUIRE_EQUAL(Params().NetworkIDString(), CBaseChainParams::MAIN);
    BOOST_REQUIRE_MESSAGE(store.Store(SecureString("dpapi-wrong-network"), error) == AutoUnlockStoreOutcome::Verified,
                          error);

    // The guard puts mainnet back.
    SelectParams(CBaseChainParams::TESTNET);

    BOOST_CHECK(!FetchAutoUnlockFromDpapiBlob(path, error).has_value());
    BOOST_CHECK_MESSAGE(Contains(error, "network"), error);
    BOOST_CHECK_MESSAGE(Contains(error, "main"), error);
}

// A file set up for the other mode is refused before it is decrypted, in both directions.
// The service's file is made here from the machine's by changing its scope, so the case does
// not need this account's own DPAPI key, which a logon without a password (an ssh key, or
// S4U, as the same-account Core-Start task logs on) may not be able to use.
BOOST_AUTO_TEST_CASE(dpapi_fetch_refuses_scope_mismatch)
{
    if (SkipUnderWine()) return;

    TempDataDir datadir;
    const fs::path path = datadir.BlobPath();
    DpapiBlobStore store;
    std::string error;

    BOOST_REQUIRE_MESSAGE(store.Store(SecureString("dpapi-scope-machine"), error) == AutoUnlockStoreOutcome::Verified,
                          error);
    AutoUnlockBlob blob = StoredBlob(path);
    BOOST_REQUIRE(blob.scope == BlobScope::LocalMachine);

    {
        ServiceMode service(true);
        BOOST_CHECK(!FetchAutoUnlockFromDpapiBlob(path, error).has_value());
        BOOST_CHECK_MESSAGE(Contains(error, "Windows service"), error);

        const AutoUnlockStoreStatus status = store.Status();
        BOOST_CHECK_EQUAL(status.scope, "service");
        BOOST_CHECK(status.configured);
        BOOST_CHECK(!status.problem.empty());
    }

    // What Store() writes as a service, but for the DPAPI blob, which is not read before the
    // scope is. Written over the file in place, so it keeps the owner-only list Store() gave it.
    blob.scope = BlobScope::ServiceAccount;
    const std::vector<unsigned char> service_file = SerializeAutoUnlockBlob(blob);
    BOOST_REQUIRE(!service_file.empty());
    {
        fsbridge::ofstream file(path, std::ios::binary | std::ios::trunc);
        BOOST_REQUIRE(file.is_open());
        file.write(reinterpret_cast<const char*>(service_file.data()), static_cast<std::streamsize>(service_file.size()));
        BOOST_REQUIRE(file.good());
    }
    BOOST_REQUIRE(StoredBlob(path).scope == BlobScope::ServiceAccount);

    BOOST_CHECK(!FetchAutoUnlockFromDpapiBlob(path, error).has_value());
    BOOST_CHECK_MESSAGE(Contains(error, "Windows service"), error);
}

// Stored as a service, the file is for the service account's DPAPI key, and is read back
// with it. This account's own key stands in for the service account's here. Where this logon
// cannot use its own key, the case says so as a warning and checks nothing; the product does
// not use that key outside the service mode.
BOOST_AUTO_TEST_CASE(dpapi_service_scope_round_trip)
{
    if (SkipUnderWine()) return;

    if (const std::optional<std::string> unusable = OwnDpapiKeyUnusable()) {
        BOOST_WARN_MESSAGE(false, "this logon cannot use its own DPAPI key (" << *unusable
                                      << "); the service scope's round trip is not checked");
        return;
    }

    TempDataDir datadir;
    const fs::path path = datadir.BlobPath();
    DpapiBlobStore store;
    std::string error;

    ServiceMode service(true);
    const SecureString passphrase("dpapi-scope-service");
    BOOST_REQUIRE_MESSAGE(store.Store(passphrase, error) == AutoUnlockStoreOutcome::Verified, error);
    BOOST_CHECK(StoredBlob(path).scope == BlobScope::ServiceAccount);

    const std::optional<SecureString> fetched = FetchAutoUnlockFromDpapiBlob(path, error);
    BOOST_REQUIRE_MESSAGE(fetched.has_value(), error);
    BOOST_CHECK(*fetched == passphrase);
}

BOOST_AUTO_TEST_CASE(dpapi_fetch_missing_or_oversize_file)
{
    if (SkipUnderWine()) return;

    TempDataDir datadir;
    const fs::path path = datadir.BlobPath();
    std::string error;

    BOOST_CHECK(!FetchAutoUnlockFromDpapiBlob(path, error).has_value());
    BOOST_CHECK_MESSAGE(Contains(error, "autounlock is not set up"), error);

    // One byte over the limit, owner-only so that only its size is wrong.
    std::string dir_error;
    BOOST_REQUIRE_MESSAGE(util::CreateOwnerOnlyDirectory(path.parent_path(), dir_error), dir_error);
    {
        fsbridge::ofstream file(path, std::ios::binary);
        BOOST_REQUIRE(file.is_open());
        file << std::string(AUTOUNLOCK_BLOB_MAX_BYTES + 1, 'x');
    }
    util::ApplyOwnerOnlyDacl(path, /*inheritable=*/false);

    BOOST_CHECK(!FetchAutoUnlockFromDpapiBlob(path, error).has_value());
    BOOST_CHECK_MESSAGE(Contains(error, "larger than"), error);
}

// A file that cannot be read back because another process holds it is kept: what was
// written is there. One that the read-back refuses for what it is, here for an access
// control list that lets other accounts read it, is removed.
BOOST_AUTO_TEST_CASE(dpapi_store_keeps_a_file_it_cannot_read_back)
{
    if (SkipUnderWine()) return;

    TempDataDir datadir;
    const fs::path path = datadir.BlobPath();
    DpapiBlobStore store;
    std::string error;

    // Held without sharing from the moment it is in place until Store() returns, so that
    // each read-back attempt, three over 3 s, meets a sharing violation.
    HANDLE held = INVALID_HANDLE_VALUE;
    const SecureString passphrase("dpapi-unverified");
    AutoUnlockStoreOutcome stored = AutoUnlockStoreOutcome::Failed;
    {
        ReadBackHook hook([&held](const fs::path& in_place) {
            held = ::CreateFileW(in_place.wstring().c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING,
                                 FILE_ATTRIBUTE_NORMAL, nullptr);
        });
        stored = store.Store(passphrase, error);
    }
    BOOST_REQUIRE(held != INVALID_HANDLE_VALUE);
    ::CloseHandle(held);

    // Kept, and reported as not checked.
    BOOST_REQUIRE_MESSAGE(stored == AutoUnlockStoreOutcome::Unverified, error);
    BOOST_CHECK(fs::exists(path));
    const std::optional<SecureString> fetched = FetchAutoUnlockFromDpapiBlob(path, error);
    BOOST_REQUIRE_MESSAGE(fetched.has_value(), error);
    BOOST_CHECK(*fetched == passphrase);

    {
        ReadBackHook hook([](const fs::path& in_place) {
            ApplyAcl(in_place, OwnerOnlyPlus(wellknown::Users(), FILE_GENERIC_READ));
        });
        BOOST_CHECK(store.Store(SecureString("dpapi-refused-on-read-back"), error) == AutoUnlockStoreOutcome::Failed);
    }
    BOOST_CHECK_MESSAGE(Contains(error, "readable by other accounts"), error);
    BOOST_CHECK_MESSAGE(Contains(error, "so it was removed"), error);
    BOOST_CHECK(!fs::exists(path));
}

// FetchAutoUnlockPassphrase() reads the file only when -autounlockblob is given and not
// negated. Given without a value, with no file at the default place, it logs one line.
BOOST_AUTO_TEST_CASE(fetch_follows_autounlockblob)
{
    if (SkipUnderWine()) return;

    TempDataDir datadir;
    std::vector<std::string> lines;
    const auto callback = LogInstance().PushBackCallback([&lines](const std::string& line) {
        if (Contains(line, "autounlock")) lines.push_back(line);
    });
    struct CallbackRemover {
        decltype(callback) it;
        ~CallbackRemover() { LogInstance().DeleteCallback(it); }
    } const remove_callback{callback};

    gArgs.ClearForcedArg("-autounlockblob");
    BOOST_CHECK(!FetchAutoUnlockPassphrase().has_value());
    BOOST_CHECK_EQUAL(lines.size(), 0U);

    gArgs.LockSettings([](util::Settings& settings) {
        settings.command_line_options["autounlockblob"] = {util::SettingsValue(false)};
    });
    BOOST_REQUIRE(gArgs.IsArgNegated("-autounlockblob"));
    BOOST_CHECK(!FetchAutoUnlockPassphrase().has_value());
    BOOST_CHECK_EQUAL(lines.size(), 0U);
    gArgs.LockSettings([](util::Settings& settings) { settings.command_line_options.erase("autounlockblob"); });

    gArgs.ForceSetArg("-autounlockblob", "");
    BOOST_CHECK(!FetchAutoUnlockPassphrase().has_value());
    BOOST_REQUIRE_EQUAL(lines.size(), 1U);
    const std::string expected = "autounlock is not set up (" + fsbridge::LongPathString(datadir.BlobPath())
                               + "); the wallet stays locked";
    BOOST_CHECK_MESSAGE(Contains(lines[0], expected), lines[0]);
}

// A file held open without read sharing is retried: here it is released after half a
// second, so the second attempt, one second after the first, reads it.
BOOST_AUTO_TEST_CASE(dpapi_fetch_retries_a_sharing_violation)
{
    if (SkipUnderWine()) return;

    TempDataDir datadir;
    const fs::path path = datadir.BlobPath();
    DpapiBlobStore store;
    std::string error;

    const SecureString passphrase("dpapi-sharing-violation");
    BOOST_REQUIRE_MESSAGE(store.Store(passphrase, error) == AutoUnlockStoreOutcome::Verified, error);

    const HANDLE held = ::CreateFileW(path.wstring().c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING,
                                      FILE_ATTRIBUTE_NORMAL, nullptr);
    BOOST_REQUIRE(held != INVALID_HANDLE_VALUE);

    std::thread release([held]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        ::CloseHandle(held);
    });

    const auto start = std::chrono::steady_clock::now();
    const std::optional<SecureString> fetched = FetchAutoUnlockFromDpapiBlob(path, error);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    release.join();

    BOOST_REQUIRE_MESSAGE(fetched.has_value(), error);
    BOOST_CHECK(*fetched == passphrase);
    BOOST_CHECK(elapsed >= std::chrono::seconds(1));
}
#endif // WIN32

BOOST_AUTO_TEST_SUITE_END()
