// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "key.h"
#include "sync.h"
#include "test/state_guard.h"
#include "util/system.h"
#include "wallet/autounlock_blob.h"
#include "wallet/autounlock_store.h"
#include "wallet/wallet.h"

#include <boost/test/unit_test.hpp>

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
//! A file in the layout with each header field as given, whether or not it is valid, and
//! \p dpapi_bytes bytes of blob after it. \p length overrides the network length byte.
std::vector<unsigned char> RawFile(const std::string& magic, unsigned char scope, const std::string& network,
                                   size_t dpapi_bytes, std::optional<unsigned char> length = std::nullopt)
{
    std::vector<unsigned char> bytes(magic.begin(), magic.end());
    bytes.push_back(scope);
    bytes.push_back(length.value_or(static_cast<unsigned char>(network.size())));
    bytes.insert(bytes.end(), network.begin(), network.end());
    bytes.insert(bytes.end(), dpapi_bytes, 0xA5);
    return bytes;
}

AutoUnlockBlob MakeBlob(BlobScope scope, const std::string& network, std::vector<unsigned char> dpapi)
{
    AutoUnlockBlob blob;
    blob.scope = scope;
    blob.network = network;
    blob.dpapi = std::move(dpapi);
    return blob;
}

//! What the fake store was asked to do.
struct StoreCalls
{
    int stores{0};
    int removes{0};
    SecureString last;
};

//! A store that records its calls and answers as told.
class FakeStore final : public AutoUnlockSecretStore
{
public:
    enum class Outcome { Succeeds, Fails, Throws };

    FakeStore(std::shared_ptr<StoreCalls> calls, bool configured, Outcome store_outcome, Outcome remove_outcome)
        : m_calls(std::move(calls)), m_configured(configured), m_store_outcome(store_outcome),
          m_remove_outcome(remove_outcome)
    {
    }

    bool Store(const SecureString& passphrase, std::string& error) override
    {
        ++m_calls->stores;
        m_calls->last = passphrase;

        return Answer(m_store_outcome, error);
    }

    bool Remove(std::string& error) override
    {
        ++m_calls->removes;

        return Answer(m_remove_outcome, error);
    }

    AutoUnlockStoreStatus Status() override
    {
        AutoUnlockStoreStatus status;
        status.supported = true;
        status.configured = m_configured;
        status.scope = "machine";
        return status;
    }

private:
    static bool Answer(Outcome outcome, std::string& error)
    {
        if (outcome == Outcome::Throws) throw std::runtime_error("the fake store throws");
        if (outcome == Outcome::Fails) {
            error = "the fake store refuses";
            return false;
        }
        return true;
    }

    std::shared_ptr<StoreCalls> m_calls;
    bool m_configured{false};
    Outcome m_store_outcome{Outcome::Succeeds};
    Outcome m_remove_outcome{Outcome::Succeeds};
};

//! Installs a fake store for one scope and restores the platform's after it.
struct FakeStoreInstalled
{
    std::shared_ptr<StoreCalls> calls{std::make_shared<StoreCalls>()};

    FakeStoreInstalled(bool configured, FakeStore::Outcome store_outcome,
                       FakeStore::Outcome remove_outcome = FakeStore::Outcome::Succeeds)
    {
        SetAutoUnlockSecretStoreForTesting(
            std::make_unique<FakeStore>(calls, configured, store_outcome, remove_outcome));
    }

    ~FakeStoreInstalled()
    {
        SetAutoUnlockSecretStoreForTesting(nullptr);
    }
};

//! An encrypted wallet holding one key, in memory.
void MakeEncryptedWallet(CWallet& wallet, const SecureString& passphrase)
{
    CKey key;
    key.MakeNewKey(true);
    {
        LOCK(wallet.cs_wallet);
        BOOST_REQUIRE(wallet.AddKey(key));
    }
    BOOST_REQUIRE(wallet.EncryptWallet(passphrase));
}
} // namespace

BOOST_AUTO_TEST_SUITE(autounlock_blob_tests)

BOOST_AUTO_TEST_CASE(blob_roundtrip_both_scopes)
{
    for (const BlobScope scope : {BlobScope::LocalMachine, BlobScope::ServiceAccount}) {
        const AutoUnlockBlob blob = MakeBlob(scope, "main", {0x01, 0x00, 0xFF, 0x7E});
        const std::vector<unsigned char> bytes = SerializeAutoUnlockBlob(blob);

        // The header, byte for byte, then the blob.
        BOOST_REQUIRE_EQUAL(bytes.size(), 8U + 1U + 1U + 4U + 4U);
        BOOST_CHECK(std::string(bytes.begin(), bytes.begin() + 8) == "GRCAUB01");
        BOOST_CHECK_EQUAL(bytes[8], static_cast<unsigned char>(scope));
        BOOST_CHECK_EQUAL(bytes[9], 4U);
        BOOST_CHECK(std::string(bytes.begin() + 10, bytes.begin() + 14) == "main");
        BOOST_CHECK(std::vector<unsigned char>(bytes.begin() + 14, bytes.end()) == blob.dpapi);

        const std::optional<AutoUnlockBlob> parsed = ParseAutoUnlockBlob(bytes);
        BOOST_REQUIRE(parsed.has_value());
        BOOST_CHECK(parsed->scope == scope);
        BOOST_CHECK_EQUAL(parsed->network, "main");
        BOOST_CHECK(parsed->dpapi == blob.dpapi);
    }

    // The longest network name the layout holds.
    const AutoUnlockBlob longest = MakeBlob(BlobScope::LocalMachine, std::string(16, 'z'), {0x42});
    const std::optional<AutoUnlockBlob> parsed = ParseAutoUnlockBlob(SerializeAutoUnlockBlob(longest));
    BOOST_REQUIRE(parsed.has_value());
    BOOST_CHECK_EQUAL(parsed->network, longest.network);
}

BOOST_AUTO_TEST_CASE(blob_rejects_bad_magic)
{
    BOOST_REQUIRE(ParseAutoUnlockBlob(RawFile("GRCAUB01", 1, "main", 8)).has_value());

    BOOST_CHECK(!ParseAutoUnlockBlob(RawFile("GRCAUB02", 1, "main", 8)));
    BOOST_CHECK(!ParseAutoUnlockBlob(RawFile("grcaub01", 1, "main", 8)));
    BOOST_CHECK(!ParseAutoUnlockBlob(RawFile("XRCAUB01", 1, "main", 8)));

    // Shorter than the header.
    const std::vector<unsigned char> whole = RawFile("GRCAUB01", 1, "main", 8);
    for (size_t size = 0; size <= 14; ++size) {
        BOOST_CHECK(!ParseAutoUnlockBlob(Span<const unsigned char>(whole.data(), size)));
    }
}

BOOST_AUTO_TEST_CASE(blob_rejects_unknown_scope)
{
    BOOST_REQUIRE(ParseAutoUnlockBlob(RawFile("GRCAUB01", 1, "main", 8)).has_value());
    BOOST_REQUIRE(ParseAutoUnlockBlob(RawFile("GRCAUB01", 2, "main", 8)).has_value());

    BOOST_CHECK(!ParseAutoUnlockBlob(RawFile("GRCAUB01", 0, "main", 8)));
    BOOST_CHECK(!ParseAutoUnlockBlob(RawFile("GRCAUB01", 3, "main", 8)));
    BOOST_CHECK(!ParseAutoUnlockBlob(RawFile("GRCAUB01", 0xFF, "main", 8)));

    BOOST_CHECK(SerializeAutoUnlockBlob(MakeBlob(static_cast<BlobScope>(0), "main", {1})).empty());
    BOOST_CHECK(SerializeAutoUnlockBlob(MakeBlob(static_cast<BlobScope>(3), "main", {1})).empty());
}

BOOST_AUTO_TEST_CASE(blob_rejects_empty_dpapi)
{
    // One byte of blob is enough.
    BOOST_REQUIRE(ParseAutoUnlockBlob(RawFile("GRCAUB01", 1, "main", 1)).has_value());
    BOOST_REQUIRE(!SerializeAutoUnlockBlob(MakeBlob(BlobScope::LocalMachine, "main", {1})).empty());

    BOOST_CHECK(!ParseAutoUnlockBlob(RawFile("GRCAUB01", 1, "main", 0)));
    BOOST_CHECK(SerializeAutoUnlockBlob(MakeBlob(BlobScope::LocalMachine, "main", {})).empty());

    // A network length that runs past the end of the file.
    BOOST_CHECK(!ParseAutoUnlockBlob(RawFile("GRCAUB01", 1, "main", 0, 6)));
}

BOOST_AUTO_TEST_CASE(blob_rejects_oversize)
{
    const size_t header = 8 + 1 + 1 + 4;

    const std::vector<unsigned char> largest = RawFile("GRCAUB01", 2, "main", AUTOUNLOCK_BLOB_MAX_BYTES - header);
    BOOST_REQUIRE_EQUAL(largest.size(), AUTOUNLOCK_BLOB_MAX_BYTES);
    BOOST_CHECK(ParseAutoUnlockBlob(largest).has_value());

    const std::vector<unsigned char> over = RawFile("GRCAUB01", 2, "main", AUTOUNLOCK_BLOB_MAX_BYTES + 1 - header);
    BOOST_REQUIRE_EQUAL(over.size(), 65537U);
    BOOST_CHECK(!ParseAutoUnlockBlob(over));

    BOOST_CHECK_EQUAL(SerializeAutoUnlockBlob(MakeBlob(BlobScope::ServiceAccount, "main",
                                                       std::vector<unsigned char>(AUTOUNLOCK_BLOB_MAX_BYTES - header, 1)))
                          .size(),
                      AUTOUNLOCK_BLOB_MAX_BYTES);
    BOOST_CHECK(SerializeAutoUnlockBlob(MakeBlob(BlobScope::ServiceAccount, "main",
                                                 std::vector<unsigned char>(AUTOUNLOCK_BLOB_MAX_BYTES + 1 - header, 1)))
                    .empty());
}

BOOST_AUTO_TEST_CASE(blob_rejects_bad_network)
{
    for (const std::string& network : {std::string(), std::string("MAIN"), std::string("ma in"), std::string(17, 'a'),
                                       std::string("main1"), std::string("t\xC3\xA9st")}) {
        BOOST_CHECK_MESSAGE(!ParseAutoUnlockBlob(RawFile("GRCAUB01", 1, network, 8)), "network '" << network << "'");
        BOOST_CHECK_MESSAGE(SerializeAutoUnlockBlob(MakeBlob(BlobScope::LocalMachine, network, {1})).empty(),
                            "network '" << network << "'");
    }

    BOOST_CHECK(ParseAutoUnlockBlob(RawFile("GRCAUB01", 1, "regtest", 8)).has_value());
    BOOST_CHECK(ParseAutoUnlockBlob(RawFile("GRCAUB01", 1, "test", 8)).has_value());
}

BOOST_AUTO_TEST_CASE(entropy_binds_network)
{
    const std::string expected = "gridcoin-autounlock-v1:main";
    BOOST_CHECK(AutoUnlockEntropy("main") == std::vector<unsigned char>(expected.begin(), expected.end()));
    BOOST_CHECK(AutoUnlockEntropy("main") != AutoUnlockEntropy("test"));
}

BOOST_AUTO_TEST_CASE(blob_path_rules)
{
    grc_test::StateGuard guard;

    const fs::path expected = GetDataDir() / "autounlock" / "passphrase.dpapi";
    std::string error;

    gArgs.ClearForcedArg("-autounlockblob");
    std::optional<fs::path> path = AutoUnlockBlobPath(error);
    BOOST_REQUIRE(path.has_value());
    BOOST_CHECK(*path == expected);
    BOOST_CHECK(error.empty());

    // Given without a value.
    gArgs.ForceSetArg("-autounlockblob", "");
    path = AutoUnlockBlobPath(error);
    BOOST_REQUIRE(path.has_value());
    BOOST_CHECK(*path == expected);

    // Negated, as -noautounlockblob on the command line leaves it. Its value reads as "0",
    // which would otherwise be a relative path.
    gArgs.ClearForcedArg("-autounlockblob");
    gArgs.LockSettings([](util::Settings& settings) {
        settings.command_line_options["autounlockblob"] = {util::SettingsValue(false)};
    });
    BOOST_REQUIRE(gArgs.IsArgNegated("-autounlockblob"));
    path = AutoUnlockBlobPath(error);
    BOOST_REQUIRE_MESSAGE(path.has_value(), error);
    BOOST_CHECK(*path == expected);
    BOOST_CHECK(error.empty());
    gArgs.LockSettings([](util::Settings& settings) { settings.command_line_options.erase("autounlockblob"); });

#ifdef WIN32
    const fs::path absolute("C:\\Gridcoin\\elsewhere\\passphrase.dpapi");
#else
    const fs::path absolute("/var/lib/gridcoin/elsewhere/passphrase.dpapi");
#endif
    gArgs.ForceSetArg("-autounlockblob", absolute.string());
    path = AutoUnlockBlobPath(error);
    BOOST_REQUIRE(path.has_value());
    BOOST_CHECK(*path == absolute);

    gArgs.ForceSetArg("-autounlockblob", "relative/passphrase.dpapi");
    path = AutoUnlockBlobPath(error);
    BOOST_CHECK(!path.has_value());
    BOOST_CHECK_MESSAGE(error.find("relative/passphrase.dpapi") != std::string::npos, error);

#ifdef WIN32
    // From the root of whatever drive is current, which is not absolute either.
    gArgs.ForceSetArg("-autounlockblob", "\\Gridcoin\\passphrase.dpapi");
    BOOST_CHECK(!AutoUnlockBlobPath(error).has_value());
#endif
}

BOOST_AUTO_TEST_CASE(passphrase_change_reencrypts_only_when_configured)
{
    const SecureString changed("autounlock-store-new-passphrase");

    {
        FakeStoreInstalled fake(/*configured=*/true, FakeStore::Outcome::Succeeds);
        OnWalletPassphraseChanged(changed);
        BOOST_CHECK_EQUAL(fake.calls->stores, 1);
        BOOST_CHECK(fake.calls->last == changed);
        BOOST_CHECK_EQUAL(fake.calls->removes, 0);
    }

    {
        FakeStoreInstalled fake(/*configured=*/false, FakeStore::Outcome::Succeeds);
        OnWalletPassphraseChanged(changed);
        BOOST_CHECK_EQUAL(fake.calls->stores, 0);
        BOOST_CHECK_EQUAL(fake.calls->removes, 0);
    }

    // A store that fails leaves the old passphrase stored, which could no longer unlock
    // the wallet, so it is removed.
    {
        FakeStoreInstalled fake(/*configured=*/true, FakeStore::Outcome::Fails);
        BOOST_CHECK_NO_THROW(OnWalletPassphraseChanged(changed));
        BOOST_CHECK_EQUAL(fake.calls->stores, 1);
        BOOST_CHECK_EQUAL(fake.calls->removes, 1);
    }

    {
        FakeStoreInstalled fake(/*configured=*/true, FakeStore::Outcome::Throws);
        BOOST_CHECK_NO_THROW(OnWalletPassphraseChanged(changed));
        BOOST_CHECK_EQUAL(fake.calls->stores, 1);
        BOOST_CHECK_EQUAL(fake.calls->removes, 1);
    }

    // The removal failing as well is logged, never thrown.
    for (const FakeStore::Outcome remove : {FakeStore::Outcome::Fails, FakeStore::Outcome::Throws}) {
        FakeStoreInstalled fake(/*configured=*/true, FakeStore::Outcome::Fails, remove);
        BOOST_CHECK_NO_THROW(OnWalletPassphraseChanged(changed));
        BOOST_CHECK_EQUAL(fake.calls->stores, 1);
        BOOST_CHECK_EQUAL(fake.calls->removes, 1);
    }

    // Through the wallet: a change that succeeds stores the new passphrase once, and one
    // that is refused stores nothing. The refusal is answered after
    // FailedPassphraseDelay(1), half a second.
    {
        FakeStoreInstalled fake(/*configured=*/true, FakeStore::Outcome::Succeeds);
        CWallet wallet;
        const SecureString original("autounlock-store-old-passphrase");
        MakeEncryptedWallet(wallet, original);

        BOOST_REQUIRE(wallet.ChangeWalletPassphrase(original, changed));
        BOOST_CHECK_EQUAL(fake.calls->stores, 1);
        BOOST_CHECK(fake.calls->last == changed);

        BOOST_CHECK(!wallet.ChangeWalletPassphrase(SecureString("autounlock-store-wrong"), original));
        BOOST_CHECK_EQUAL(fake.calls->stores, 1);
        BOOST_CHECK_EQUAL(fake.calls->removes, 0);
    }

    // Uninstalled, the platform's store is back: none, except on Windows.
#ifndef WIN32
    BOOST_CHECK(GetAutoUnlockSecretStore() == nullptr);
#else
    BOOST_CHECK(GetAutoUnlockSecretStore() != nullptr);
#endif
}

BOOST_AUTO_TEST_SUITE_END()
