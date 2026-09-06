// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/psgttoastdamptests.h"

#include "qt/psgttoastdamp.h"

#include <string>
#include <vector>

//!
//! \file psgttoastdamptests.cpp
//! \brief The damp that keeps a pending multisig spend from toasting once per
//! co-signer's revision.
//!
//! The pool-changed notification carries a revision hash, and every co-signer
//! who signs ahead of this wallet produces one. walletMustSignRevision answers
//! true for each of them until this wallet signs, so the toast decision cannot
//! come from that predicate alone: it has to resolve the revision back to the
//! spend -- the unsigned transaction, which every signature revision shares and
//! an initiator supersede changes -- which is what these cases drive.
//!

namespace {
//! Unsigned-transaction hashes: the identity of a pending spend.
const std::string SPEND_A("a1b2c3d4e5f60718293a4b5c6d7e8f9012345678a1b2c3d4e5f60718293a4b5c");
const std::string SPEND_B("fedcba98765432100123456789abcdef01234567fedcba987654321001234567");
const std::string SPEND_C("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");

//! Three co-signers have revised spend A; B has one revision.
std::vector<PSGTToastDamp::Entry> Pool()
{
    return {
        {"rev-a-1", SPEND_A},
        {"rev-a-2", SPEND_A},
        {"rev-a-3", SPEND_A},
        {"rev-b-1", SPEND_B},
    };
}
} // anonymous namespace

void PSGTToastDampTests::oneSpendAnnouncesOnceAcrossItsRevisions()
{
    PSGTToastDamp damp;
    const std::vector<PSGTToastDamp::Entry> pool = Pool();

    // Three revisions of the same spend reach the handler, all of them still
    // needing this wallet. Without the damp all three would toast.
    QVERIFY(damp.ShouldToastRevision(pool, "rev-a-1"));
    QVERIFY(!damp.ShouldToastRevision(pool, "rev-a-2"));
    QVERIFY(!damp.ShouldToastRevision(pool, "rev-a-3"));

    QCOMPARE(damp.AnnouncedCount(), static_cast<std::size_t>(1));
}

void PSGTToastDampTests::separateSpendsEachAnnounce()
{
    PSGTToastDamp damp;
    const std::vector<PSGTToastDamp::Entry> pool = Pool();

    // Damping is per spend, not a global mute: a second multisig waiting on
    // this wallet is still announced.
    QVERIFY(damp.ShouldToastRevision(pool, "rev-a-1"));
    QVERIFY(damp.ShouldToastRevision(pool, "rev-b-1"));
    QVERIFY(!damp.ShouldToastRevision(pool, "rev-a-2"));
    QVERIFY(!damp.ShouldToastRevision(pool, "rev-b-1"));

    QCOMPARE(damp.AnnouncedCount(), static_cast<std::size_t>(2));
}

void PSGTToastDampTests::anUnknownRevisionIsAnnouncedNotSwallowed()
{
    PSGTToastDamp damp;
    const std::vector<PSGTToastDamp::Entry> pool = Pool();

    // A revision that does not resolve to a spend has nothing to damp on.
    // walletMustSignRevision gates the caller first and already answers false
    // for a revision the pool has dropped, so this is the narrow race between
    // those two calls -- but announcing is the safe side of a lookup that
    // failed: silence decided by a failure would repeat.
    QVERIFY(damp.ShouldToastRevision(pool, "rev-gone"));
    QVERIFY(damp.ShouldToastRevision(pool, "rev-gone"));

    QCOMPARE(damp.AnnouncedCount(), static_cast<std::size_t>(0));
}

void PSGTToastDampTests::removalForgetsOnlyTheEntriesThatLeft()
{
    PSGTToastDamp damp;
    const std::vector<PSGTToastDamp::Entry> pool = Pool();

    damp.ShouldToastRevision(pool, "rev-a-1");
    damp.ShouldToastRevision(pool, "rev-b-1");
    QCOMPARE(damp.AnnouncedCount(), static_cast<std::size_t>(2));

    // A leaves the pool, B stays.
    const std::vector<PSGTToastDamp::Entry> after = {{"rev-b-1", SPEND_B}};
    damp.Prune(after);

    QCOMPARE(damp.AnnouncedCount(), static_cast<std::size_t>(1));
    QVERIFY(!damp.ShouldToastRevision(after, "rev-b-1"));
}

void PSGTToastDampTests::aResubmittedSpendAnnouncesAgain()
{
    PSGTToastDamp damp;
    const std::vector<PSGTToastDamp::Entry> pool = Pool();

    QVERIFY(damp.ShouldToastRevision(pool, "rev-a-1"));
    QVERIFY(!damp.ShouldToastRevision(pool, "rev-a-2"));

    // The spend is removed, so the pool is empty.
    damp.Prune(std::vector<PSGTToastDamp::Entry>());
    QCOMPARE(damp.AnnouncedCount(), static_cast<std::size_t>(0));

    // The same transaction submitted again is a new request, not the old one.
    QVERIFY(damp.ShouldToastRevision(pool, "rev-a-1"));
}

void PSGTToastDampTests::aSupersedeUnderTheSameImageAnnouncesAgain()
{
    PSGTToastDamp damp;

    // Spend A waits for this wallet: announced once.
    const std::vector<PSGTToastDamp::Entry> before = {{"rev-a-1", SPEND_A}};
    QVERIFY(damp.ShouldToastRevision(before, "rev-a-1"));
    QCOMPARE(damp.AnnouncedCount(), static_cast<std::size_t>(1));

    // The initiator supersedes it with a DIFFERENT transaction from the same
    // arrangement. The pool keeps one entry per image, so this arrives as an
    // update under the same image with a revision of the new unsigned
    // transaction, and the old spend is gone from the pool. It is a new
    // request and must be announced -- the image would have been the wrong
    // key, the transaction hash is the right one.
    const std::vector<PSGTToastDamp::Entry> after = {{"rev-c-1", SPEND_C}};
    QVERIFY(damp.ShouldToastRevision(after, "rev-c-1"));
    QVERIFY(!damp.ShouldToastRevision(after, "rev-c-1"));

    // The superseded spend was forgotten along the way, so reverting to it is
    // announced again too.
    QCOMPARE(damp.AnnouncedCount(), static_cast<std::size_t>(1));
    QVERIFY(damp.ShouldToastRevision(before, "rev-a-1"));
}
