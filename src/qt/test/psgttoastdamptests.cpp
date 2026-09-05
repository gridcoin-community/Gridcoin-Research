// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/psgttoastdamptests.h"

#include "qt/psgttoastdamp.h"

#include <string>
#include <vector>

//!
//! \file psgttoastdamptests.cpp
//! \brief The damp that keeps a multisig arrangement from toasting once per
//! co-signer's revision.
//!
//! The pool-changed notification carries a revision hash, and every co-signer
//! who signs ahead of this wallet produces one. walletMustSignRevision answers
//! true for each of them until this wallet signs, so the toast decision cannot
//! come from that predicate alone: it has to resolve the revision back to the
//! arrangement, which is what these cases drive.
//!

namespace {
const std::string ARRANGEMENT_A("a1b2c3d4e5f60718293a4b5c6d7e8f9012345678");
const std::string ARRANGEMENT_B("fedcba98765432100123456789abcdef01234567");

//! Three co-signers have revised arrangement A; B has one revision.
std::vector<PSGTToastDamp::Entry> Pool()
{
    return {
        {"rev-a-1", ARRANGEMENT_A},
        {"rev-a-2", ARRANGEMENT_A},
        {"rev-a-3", ARRANGEMENT_A},
        {"rev-b-1", ARRANGEMENT_B},
    };
}
} // anonymous namespace

void PSGTToastDampTests::oneArrangementAnnouncesOnceAcrossItsRevisions()
{
    PSGTToastDamp damp;
    const std::vector<PSGTToastDamp::Entry> pool = Pool();

    // Three revisions of the same arrangement reach the handler, all of them
    // still needing this wallet. Without the damp all three would toast.
    QVERIFY(damp.ShouldToastRevision(pool, "rev-a-1"));
    QVERIFY(!damp.ShouldToastRevision(pool, "rev-a-2"));
    QVERIFY(!damp.ShouldToastRevision(pool, "rev-a-3"));

    QCOMPARE(damp.AnnouncedCount(), static_cast<std::size_t>(1));
}

void PSGTToastDampTests::separateArrangementsEachAnnounce()
{
    PSGTToastDamp damp;
    const std::vector<PSGTToastDamp::Entry> pool = Pool();

    // Damping is per arrangement, not a global mute: a second multisig waiting
    // on this wallet is still announced.
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

    // A revision that does not resolve to an arrangement has nothing to damp
    // on. walletMustSignRevision gates the caller first and already answers
    // false for a revision the pool has dropped, so this is the narrow race
    // between those two calls -- but announcing is the safe side of a lookup
    // that failed: silence decided by a failure would repeat.
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
    const std::vector<PSGTToastDamp::Entry> after = {{"rev-b-1", ARRANGEMENT_B}};
    damp.Prune(after);

    QCOMPARE(damp.AnnouncedCount(), static_cast<std::size_t>(1));
    QVERIFY(!damp.ShouldToastRevision(after, "rev-b-1"));
}

void PSGTToastDampTests::aResubmittedArrangementAnnouncesAgain()
{
    PSGTToastDamp damp;
    const std::vector<PSGTToastDamp::Entry> pool = Pool();

    QVERIFY(damp.ShouldToastRevision(pool, "rev-a-1"));
    QVERIFY(!damp.ShouldToastRevision(pool, "rev-a-2"));

    // The arrangement is removed, so the pool is empty.
    damp.Prune(std::vector<PSGTToastDamp::Entry>());
    QCOMPARE(damp.AnnouncedCount(), static_cast<std::size_t>(0));

    // The same image submitted again is a new request, not the old one.
    QVERIFY(damp.ShouldToastRevision(pool, "rev-a-1"));
}
