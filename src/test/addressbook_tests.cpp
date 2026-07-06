// Copyright (c) 2024 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "key.h"
#include "key_io.h"
#include "wallet/wallet.h"
#include "wallet/walletdb.h"

#include <boost/test/unit_test.hpp>

#include <string>

extern CWallet* pwalletMain;

namespace {
//! Make a fresh, deterministic-enough destination for a test by minting a new key.
CTxDestination NewDestination()
{
    CKey key;
    key.MakeNewKey(false);
    return CTxDestination(key.GetPubKey().GetID());
}

//! Reload the persisted address book from the (mock) wallet DB into a throwaway
//! CWallet and return the entry for `dest`. Exercises the real load path
//! (CWalletDB::ReadKeyValue), so it verifies what actually round-trips to disk.
bool ReloadEntry(const CTxDestination& dest, CAddressBookData& out)
{
    CWallet w("wallet.dat");
    bool fFirstRun;
    w.LoadWallet(fFirstRun);

    LOCK(w.cs_wallet);
    auto mi = w.mapAddressBook.find(dest);
    if (mi == w.mapAddressBook.end()) {
        return false;
    }
    out = mi->second;
    return true;
}
} // namespace

BOOST_AUTO_TEST_SUITE(addressbook_tests)

// A default-constructed entry has an empty label and the "unknown" purpose. This is the
// default that old wallets (which carry only "name" records) inherit on load.
BOOST_AUTO_TEST_CASE(addressbookdata_defaults)
{
    CAddressBookData data;
    BOOST_CHECK(data.name.empty());
    BOOST_CHECK_EQUAL(data.purpose, "unknown");
}

// Setting a label and a purpose persists both, and they survive a reload independently.
BOOST_AUTO_TEST_CASE(name_and_purpose_roundtrip)
{
    const CTxDestination dest = NewDestination();

    BOOST_CHECK(pwalletMain->SetAddressBookName(dest, "alice"));
    BOOST_CHECK(pwalletMain->SetAddressBookPurpose(dest, "receive"));

    // In-memory state.
    {
        LOCK(pwalletMain->cs_wallet);
        BOOST_CHECK_EQUAL(pwalletMain->mapAddressBook[dest].name, "alice");
        BOOST_CHECK_EQUAL(pwalletMain->mapAddressBook[dest].purpose, "receive");
    }

    // Persisted state.
    CAddressBookData reloaded;
    BOOST_REQUIRE(ReloadEntry(dest, reloaded));
    BOOST_CHECK_EQUAL(reloaded.name, "alice");
    BOOST_CHECK_EQUAL(reloaded.purpose, "receive");
}

// Backward compat: a wallet that has only a legacy "name" record (no "purpose") loads with
// the label intact and the purpose defaulting to "unknown" -- no migration write required.
BOOST_AUTO_TEST_CASE(legacy_name_only_defaults_purpose_unknown)
{
    const CTxDestination dest = NewDestination();
    const std::string strAddress = EncodeDestination(dest);

    // Write only the legacy "name" record, exactly as a pre-disentanglement wallet would.
    BOOST_CHECK(CWalletDB(pwalletMain->strWalletFile).WriteName(strAddress, "legacy-label"));

    CAddressBookData reloaded;
    BOOST_REQUIRE(ReloadEntry(dest, reloaded));
    BOOST_CHECK_EQUAL(reloaded.name, "legacy-label");
    BOOST_CHECK_EQUAL(reloaded.purpose, "unknown");
}

// DelAddressBookName must erase BOTH the name and purpose records, so neither survives a
// reload (a ghost label would otherwise reappear from disk).
BOOST_AUTO_TEST_CASE(del_erases_name_and_purpose)
{
    const CTxDestination dest = NewDestination();

    BOOST_CHECK(pwalletMain->SetAddressBookName(dest, "bob"));
    BOOST_CHECK(pwalletMain->SetAddressBookPurpose(dest, "send"));

    CAddressBookData before;
    BOOST_REQUIRE(ReloadEntry(dest, before));

    BOOST_CHECK(pwalletMain->DelAddressBookName(dest));

    // Gone from memory...
    {
        LOCK(pwalletMain->cs_wallet);
        BOOST_CHECK(pwalletMain->mapAddressBook.find(dest) == pwalletMain->mapAddressBook.end());
    }

    // ...and gone from disk (both records).
    CAddressBookData after;
    BOOST_CHECK(!ReloadEntry(dest, after));
}

// The NotifyAddressBookChanged signal carries the purpose to subscribers. This pins the new
// 6-arg arity (the highest build-risk item of the disentanglement) at the wallet layer,
// without involving the Qt event loop.
BOOST_AUTO_TEST_CASE(notify_carries_purpose)
{
    const CTxDestination dest = NewDestination();

    std::string captured_label;
    std::string captured_purpose;
    ChangeType captured_status = CT_NEW;
    int calls = 0;

    auto conn = pwalletMain->NotifyAddressBookChanged.connect(
        [&](CWallet*, const CTxDestination&, const std::string& label, bool,
            const std::string& purpose, ChangeType status) {
            captured_label = label;
            captured_purpose = purpose;
            captured_status = status;
            ++calls;
        });

    // dest has no prior entry, so setting its purpose creates the mapAddressBook entry: the
    // signal must report CT_NEW (not CT_UPDATED), matching SetAddressBookName, so a GUI
    // subscriber inserts the row rather than updating one it never saw.
    pwalletMain->SetAddressBookPurpose(dest, "receive");

    conn.disconnect();

    BOOST_CHECK_EQUAL(calls, 1);
    BOOST_CHECK_EQUAL(captured_purpose, "receive");
    BOOST_CHECK(captured_status == CT_NEW);
}

BOOST_AUTO_TEST_SUITE_END()
