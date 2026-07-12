// Copyright (c) 2024 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "key.h"
#include "key_io.h"
#include "main.h"
#include "primitives/transaction.h"
#include "rpc/protocol.h"
#include "rpc/server.h"
#include "wallet/wallet.h"
#include "wallet/walletdb.h"

#include <univalue.h>

#include <boost/test/unit_test.hpp>

#include <stdexcept>
#include <string>
#include <vector>

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

// ----- Phase B: label RPC surface -----

namespace {
UniValue ArgArray(const std::vector<std::string>& args)
{
    UniValue v(UniValue::VARR);
    for (const std::string& a : args) {
        v.push_back(a);
    }
    return v;
}

bool LabelsContain(const UniValue& arr, const std::string& want)
{
    for (size_t i = 0; i < arr.size(); ++i) {
        if (arr[i].get_str() == want) return true;
    }
    return false;
}
} // namespace

// setlabel on an owned address tags it "receive"; getaddressesbylabel returns it with that
// purpose; listlabels lists it; re-setlabel moves the address to the new label.
BOOST_AUTO_TEST_CASE(label_rpcs_roundtrip)
{
    CKey key;
    key.MakeNewKey(false);
    BOOST_REQUIRE(pwalletMain->AddKey(key));   // owned -> purpose "receive"
    const std::string addr = EncodeDestination(CTxDestination(key.GetPubKey().GetID()));

    setlabel(ArgArray({addr, "tabby"}));

    UniValue got = getaddressesbylabel(ArgArray({"tabby"}));
    BOOST_REQUIRE(got.exists(addr));
    BOOST_CHECK_EQUAL(got[addr]["purpose"].get_str(), "receive");

    BOOST_CHECK(LabelsContain(listlabels(UniValue(UniValue::VARR)), "tabby"));

    // Re-label: the address moves to "renamed"; "tabby" then has no addresses.
    setlabel(ArgArray({addr, "renamed"}));
    UniValue moved = getaddressesbylabel(ArgArray({"renamed"}));
    BOOST_CHECK(moved.exists(addr));
    BOOST_CHECK_THROW(getaddressesbylabel(ArgArray({"tabby"})), UniValue);
}

// An unknown label throws RPC_WALLET_INVALID_LABEL_NAME rather than returning an empty object.
BOOST_AUTO_TEST_CASE(getaddressesbylabel_unknown_throws)
{
    BOOST_CHECK_THROW(getaddressesbylabel(ArgArray({"no-such-label-xyz"})), UniValue);
}

// listlabels excludes the empty default label (A.6 stubs) and filters by purpose.
BOOST_AUTO_TEST_CASE(listlabels_excludes_empty_and_filters_purpose)
{
    // An empty-name stub, as GetAccountAddress / default-key init would create.
    CKey kStub;
    kStub.MakeNewKey(false);
    pwalletMain->SetAddressBookName(CTxDestination(kStub.GetPubKey().GetID()), "");

    // A non-owned (sending) address with a real label.
    CKey kSend;
    kSend.MakeNewKey(false);
    const std::string sendAddr = EncodeDestination(CTxDestination(kSend.GetPubKey().GetID()));
    setlabel(ArgArray({sendAddr, "sendlabel"}));   // not owned -> purpose "send"

    UniValue all = listlabels(UniValue(UniValue::VARR));
    BOOST_CHECK(!LabelsContain(all, ""));            // empty label never surfaces
    BOOST_CHECK(LabelsContain(all, "sendlabel"));

    // Filtering by "receive" must exclude the send-purpose label.
    UniValue recv = listlabels(ArgArray({"receive"}));
    BOOST_CHECK(!LabelsContain(recv, "sendlabel"));
}

// ----- Phase C: account RPC deprecation -----

// The account RPCs that have a label analogue keep working WITHOUT -enableaccounts: setaccount
// bridges to a label (and tags purpose), getaccount returns the label, getaddressesbyaccount
// lists the addresses. (-enableaccounts defaults off in the test fixture.)
BOOST_AUTO_TEST_CASE(account_bridge_rpcs_still_work)
{
    CKey key;
    key.MakeNewKey(false);
    BOOST_REQUIRE(pwalletMain->AddKey(key));   // owned -> purpose "receive"
    const CTxDestination dest = CTxDestination(key.GetPubKey().GetID());
    const std::string addr = EncodeDestination(dest);

    BOOST_CHECK_NO_THROW(setaccount(ArgArray({addr, "acct-x"})));
    {
        LOCK(pwalletMain->cs_wallet);
        BOOST_CHECK_EQUAL(pwalletMain->mapAddressBook[dest].name, "acct-x");
        BOOST_CHECK_EQUAL(pwalletMain->mapAddressBook[dest].purpose, "receive");
    }

    BOOST_CHECK_EQUAL(getaccount(ArgArray({addr})).get_str(), "acct-x");

    UniValue addrs = getaddressesbyaccount(ArgArray({"acct-x"}));
    BOOST_REQUIRE(addrs.isArray());
    bool found = false;
    for (size_t i = 0; i < addrs.size(); ++i) {
        if (addrs[i].get_str() == addr) found = true;
    }
    BOOST_CHECK(found);
}

// The account RPCs with no label analogue are hard-gated behind -enableaccounts, which is off
// by default in the fixture, so they must throw. getaccountaddress uses the lighter address
// gate; sendfrom uses the full accounting gate -- both reject before touching their params.
BOOST_AUTO_TEST_CASE(account_only_rpcs_gated_without_flag)
{
    BOOST_CHECK_THROW(getaccountaddress(ArgArray({""})), std::runtime_error);
    BOOST_CHECK_THROW(sendfrom(ArgArray({"someaccount", "dummy", "1.0"})), std::runtime_error);
}

// The received-by-label twins work WITHOUT -enableaccounts (their account twins are
// hard-gated). This case pins the gating and the JSON shape on an unfunded wallet; the
// tally semantics are covered by received_by_label_tallies_owned_outputs_only below.
BOOST_AUTO_TEST_CASE(received_by_label_rpcs_work_flag_free)
{
    CKey key;
    key.MakeNewKey(false);
    BOOST_REQUIRE(pwalletMain->AddKey(key));   // owned -> purpose "receive"
    const std::string addr = EncodeDestination(CTxDestination(key.GetPubKey().GetID()));
    setlabel(ArgArray({addr, "recvlabel"}));

    // The gated account twins throw without -enableaccounts; the label twins must not.
    BOOST_CHECK_THROW(getreceivedbyaccount(ArgArray({"recvlabel"})), std::runtime_error);
    BOOST_CHECK_THROW(listreceivedbyaccount(UniValue(UniValue::VARR)), std::runtime_error);

    // Empty test wallet: an existing label (and an unknown one) both tally to zero.
    BOOST_CHECK_EQUAL(getreceivedbylabel(ArgArray({"recvlabel"})).get_real(), 0.0);
    BOOST_CHECK_EQUAL(getreceivedbylabel(ArgArray({"no-such-label-xyz"})).get_real(), 0.0);

    // "*" is not a valid label name (mirrors LabelFromValue everywhere else).
    BOOST_CHECK_THROW(getreceivedbylabel(ArgArray({"*"})), UniValue);

    // includeempty=true so the unfunded label still gets a row; rows are keyed "label",
    // not "account".
    UniValue lrArgs(UniValue::VARR);
    lrArgs.push_back(UniValue(1));
    lrArgs.push_back(UniValue(true));
    UniValue rows = listreceivedbylabel(lrArgs);
    BOOST_REQUIRE(rows.isArray());
    bool found = false;
    for (size_t i = 0; i < rows.size(); ++i) {
        BOOST_CHECK(rows[i].exists("label"));
        BOOST_CHECK(!rows[i].exists("account"));
        if (rows[i]["label"].get_str() == "recvlabel") found = true;
    }
    BOOST_CHECK(found);
}

// The label tally counts only outputs the wallet owns (a labeled send-to address that
// receives coins must NOT be counted), honors minconf, and listreceivedbylabel rows carry
// the "label" key LAST (Bitcoin's serialized order: amount, confirmations, label).
BOOST_AUTO_TEST_CASE(received_by_label_tallies_owned_outputs_only)
{
    // Owned, labeled address -> its output is tallied.
    CKey key;
    key.MakeNewKey(false);
    BOOST_REQUIRE(pwalletMain->AddKey(key));
    const CTxDestination dest = CTxDestination(key.GetPubKey().GetID());
    setlabel(ArgArray({EncodeDestination(dest), "fundedlabel"}));

    // Non-owned, labeled (purpose "send") address paid by the SAME tx -> never tallied.
    const CTxDestination otherDest = NewDestination();   // key not added to the wallet
    setlabel(ArgArray({EncodeDestination(otherDest), "othersfunds"}));

    // One unconfirmed wallet tx paying both. The fixture has no chain, so depth comes from
    // the global mempool (a wtx in neither reports depth -1); visible only at minconf=0.
    CMutableTransaction mtx;
    mtx.vout.resize(2);
    mtx.vout[0].nValue = 5 * COIN;
    mtx.vout[0].scriptPubKey.SetDestination(dest);
    mtx.vout[1].nValue = 7 * COIN;
    mtx.vout[1].scriptPubKey.SetDestination(otherDest);
    CTransaction tx(mtx);
    const uint256 hash = tx.GetHash();
    {
        LOCK2(cs_main, pwalletMain->cs_wallet);
        CWalletTx wtx(pwalletMain, tx);
        wtx.SetTxState(TxStateInMempool{});
        pwalletMain->mapWallet[hash] = wtx;
        // addUnchecked's contract requires the caller hold mempool.cs (it does no
        // internal locking, unlike mempool.remove below); acquire it after cs_wallet
        // per the canonical lock order.
        LOCK(mempool.cs);
        mempool.addUnchecked(hash, CTxMemPoolEntry(
            tx, /*fee=*/0, /*time=*/0, /*height=*/0,
            ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION)));
    }

    UniValue grArgs(UniValue::VARR);
    grArgs.push_back("fundedlabel");
    grArgs.push_back(UniValue(0));
    BOOST_CHECK_EQUAL(getreceivedbylabel(grArgs).get_real(), 5.0);

    // The default minconf=1 must not see the unconfirmed tx.
    BOOST_CHECK_EQUAL(getreceivedbylabel(ArgArray({"fundedlabel"})).get_real(), 0.0);

    // The non-owned address's 7-coin output must not be attributed to its label.
    UniValue goArgs(UniValue::VARR);
    goArgs.push_back("othersfunds");
    goArgs.push_back(UniValue(0));
    BOOST_CHECK_EQUAL(getreceivedbylabel(goArgs).get_real(), 0.0);

    // listreceivedbylabel agrees with getreceivedbylabel on both labels, and every row ends
    // with the "label" key.
    UniValue lrArgs(UniValue::VARR);
    lrArgs.push_back(UniValue(0));
    lrArgs.push_back(UniValue(true));
    UniValue rows = listreceivedbylabel(lrArgs);
    BOOST_REQUIRE(rows.isArray());
    double funded = -1.0;
    double others = -1.0;
    for (size_t i = 0; i < rows.size(); ++i) {
        BOOST_CHECK_EQUAL(rows[i].getKeys().back(), "label");
        if (rows[i]["label"].get_str() == "fundedlabel") funded = rows[i]["amount"].get_real();
        if (rows[i]["label"].get_str() == "othersfunds") others = rows[i]["amount"].get_real();
    }
    BOOST_CHECK_EQUAL(funded, 5.0);
    BOOST_CHECK_EQUAL(others, 0.0);

    // Remove the injected tx so later cases (and suites) see the wallet they expect.
    {
        LOCK2(cs_main, pwalletMain->cs_wallet);
        mempool.remove(tx);
        pwalletMain->mapWallet.erase(hash);
    }
}

// migratelabels backfills purpose on entries that loaded as "unknown" (owned -> "receive"),
// reports a non-zero updated count, and is idempotent (a second run updates nothing).
BOOST_AUTO_TEST_CASE(migratelabels_backfills_purpose)
{
    CKey key;
    key.MakeNewKey(false);
    BOOST_REQUIRE(pwalletMain->AddKey(key));   // owned -> should become "receive"
    const CTxDestination dest = CTxDestination(key.GetPubKey().GetID());

    // Simulate a pre-label entry: a name with the default "unknown" purpose.
    pwalletMain->SetAddressBookName(dest, "legacy-acct");
    {
        LOCK(pwalletMain->cs_wallet);
        BOOST_REQUIRE_EQUAL(pwalletMain->mapAddressBook[dest].purpose, "unknown");
    }

    UniValue res = migratelabels(UniValue(UniValue::VARR));
    BOOST_CHECK(res["updated"].get_int() >= 1);
    {
        LOCK(pwalletMain->cs_wallet);
        BOOST_CHECK_EQUAL(pwalletMain->mapAddressBook[dest].purpose, "receive");
    }

    // Idempotent: the first run backfills EVERY "unknown" entry in the (shared) wallet, so a
    // second back-to-back run must find nothing left to do and report updated == 0. Asserting
    // the count -- not just the target's purpose -- is what actually catches a non-idempotent
    // implementation (e.g. one that re-tagged already-classified entries).
    UniValue res2 = migratelabels(UniValue(UniValue::VARR));
    BOOST_CHECK_EQUAL(res2["updated"].get_int(), 0);
    {
        LOCK(pwalletMain->cs_wallet);
        BOOST_CHECK_EQUAL(pwalletMain->mapAddressBook[dest].purpose, "receive");
    }
}

BOOST_AUTO_TEST_SUITE_END()
