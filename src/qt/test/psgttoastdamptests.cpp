// Copyright (c) 2026 The Gridcoin developers
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
//! who signs ahead of this wallet produces one (at most m-1 of them for an
//! m-of-n, since the pool never holds a complete revision). walletMustSignRevision
//! answers true for each of them until this wallet signs, so the toast decision
//! cannot come from that predicate alone: it has to resolve the revision back to
//! the spend -- the unsigned transaction, which every signature revision shares
//! and an initiator supersede changes -- which is what these cases drive.
//!
//! The pool keeps one entry per image, so successive revisions of one spend
//! never coexist: each case feeds the damp the pool as it stands at each
//! notification, one row per image.
//!

namespace {
//! Unsigned-transaction hashes: the identity of a pending spend.
const std::string SPEND_A("a1b2c3d4e5f60718293a4b5c6d7e8f9012345678a1b2c3d4e5f60718293a4b5c");
const std::string SPEND_B("fedcba98765432100123456789abcdef01234567fedcba987654321001234567");
const std::string SPEND_C("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");

using Pool = std::vector<PSGTToastDamp::Entry>;

constexpr bool FIRST = true;   //!< CT_NEW: first revision in an empty image slot.
constexpr bool UPDATE = false; //!< CT_UPDATED: a later revision replacing it.
} // anonymous namespace

void PSGTToastDampTests::oneSpendAnnouncesOnceAcrossItsRevisions()
{
    PSGTToastDamp damp;

    // The initiator's revision lands, then two co-signers each replace it with
    // a revision carrying one more signature. All three still need this
    // wallet. Without the damp all three would toast.
    QVERIFY(damp.ShouldToastRevision(Pool{{"rev-a-1", SPEND_A}}, "rev-a-1", FIRST));
    QVERIFY(!damp.ShouldToastRevision(Pool{{"rev-a-2", SPEND_A}}, "rev-a-2", UPDATE));
    QVERIFY(!damp.ShouldToastRevision(Pool{{"rev-a-3", SPEND_A}}, "rev-a-3", UPDATE));

    QCOMPARE(damp.AnnouncedCount(), static_cast<std::size_t>(1));
}

void PSGTToastDampTests::separateSpendsEachAnnounce()
{
    PSGTToastDamp damp;

    // Damping is per spend, not a global mute: a second multisig waiting on
    // this wallet is still announced, and each keeps damping its own updates.
    QVERIFY(damp.ShouldToastRevision(Pool{{"rev-a-1", SPEND_A}}, "rev-a-1", FIRST));
    QVERIFY(damp.ShouldToastRevision(Pool{{"rev-a-1", SPEND_A}, {"rev-b-1", SPEND_B}}, "rev-b-1", FIRST));
    QVERIFY(!damp.ShouldToastRevision(Pool{{"rev-a-2", SPEND_A}, {"rev-b-1", SPEND_B}}, "rev-a-2", UPDATE));
    QVERIFY(!damp.ShouldToastRevision(Pool{{"rev-a-2", SPEND_A}, {"rev-b-2", SPEND_B}}, "rev-b-2", UPDATE));

    QCOMPARE(damp.AnnouncedCount(), static_cast<std::size_t>(2));
}

void PSGTToastDampTests::anUnknownRevisionIsAnnouncedNotSwallowed()
{
    PSGTToastDamp damp;
    const Pool pool{{"rev-a-1", SPEND_A}};

    // A revision that does not resolve to a spend has nothing to damp on.
    // walletMustSignRevision gates the caller first and already answers false
    // for a revision the pool has dropped, so this is the narrow race between
    // those two calls -- but announcing is the safe side of a lookup that
    // failed: silence decided by a failure would repeat.
    QVERIFY(damp.ShouldToastRevision(pool, "rev-gone", UPDATE));
    QVERIFY(damp.ShouldToastRevision(pool, "rev-gone", UPDATE));

    QCOMPARE(damp.AnnouncedCount(), static_cast<std::size_t>(0));
}

void PSGTToastDampTests::removalForgetsOnlyTheEntriesThatLeft()
{
    PSGTToastDamp damp;

    damp.ShouldToastRevision(Pool{{"rev-a-1", SPEND_A}}, "rev-a-1", FIRST);
    damp.ShouldToastRevision(Pool{{"rev-a-1", SPEND_A}, {"rev-b-1", SPEND_B}}, "rev-b-1", FIRST);
    QCOMPARE(damp.AnnouncedCount(), static_cast<std::size_t>(2));

    // A leaves the pool, B stays.
    const Pool after{{"rev-b-1", SPEND_B}};
    damp.Prune(after);

    QCOMPARE(damp.AnnouncedCount(), static_cast<std::size_t>(1));
    QVERIFY(!damp.ShouldToastRevision(Pool{{"rev-b-2", SPEND_B}}, "rev-b-2", UPDATE));
}

void PSGTToastDampTests::aResubmittedSpendAnnouncesAgain()
{
    PSGTToastDamp damp;

    QVERIFY(damp.ShouldToastRevision(Pool{{"rev-a-1", SPEND_A}}, "rev-a-1", FIRST));
    QVERIFY(!damp.ShouldToastRevision(Pool{{"rev-a-2", SPEND_A}}, "rev-a-2", UPDATE));

    // The spend is removed, so the pool is empty.
    damp.Prune(Pool{});
    QCOMPARE(damp.AnnouncedCount(), static_cast<std::size_t>(0));

    // The same transaction submitted again is a new request, not the old one.
    QVERIFY(damp.ShouldToastRevision(Pool{{"rev-a-1", SPEND_A}}, "rev-a-1", FIRST));
}

void PSGTToastDampTests::aSupersedeUnderTheSameImageAnnouncesAgain()
{
    PSGTToastDamp damp;

    // Spend A waits for this wallet: announced once.
    const Pool before{{"rev-a-1", SPEND_A}};
    QVERIFY(damp.ShouldToastRevision(before, "rev-a-1", FIRST));
    QCOMPARE(damp.AnnouncedCount(), static_cast<std::size_t>(1));

    // The initiator supersedes it with a DIFFERENT transaction from the same
    // arrangement. The pool keeps one entry per image, so this arrives as an
    // UPDATE under the same image with a revision of the new unsigned
    // transaction, and the old spend is gone from the pool. It is a new
    // request and must be announced -- the image would have been the wrong
    // key, the transaction hash is the right one.
    const Pool after{{"rev-c-1", SPEND_C}};
    QVERIFY(damp.ShouldToastRevision(after, "rev-c-1", UPDATE));
    QVERIFY(!damp.ShouldToastRevision(Pool{{"rev-c-2", SPEND_C}}, "rev-c-2", UPDATE));

    // The superseded spend was forgotten along the way, so reverting to it is
    // announced again too.
    QCOMPARE(damp.AnnouncedCount(), static_cast<std::size_t>(1));
    QVERIFY(damp.ShouldToastRevision(before, "rev-a-1", UPDATE));
}

void PSGTToastDampTests::aFirstRevisionAnnouncesRegardlessOfDeliveryOrder()
{
    PSGTToastDamp damp;

    QVERIFY(damp.ShouldToastRevision(Pool{{"rev-a-1", SPEND_A}}, "rev-a-1", FIRST));

    // The spend is removed and the same transaction is resubmitted before
    // this thread processes the removal, so the removal's prune sees the
    // resubmission live and keeps the key. The resubmission still arrives as
    // the first revision of an empty image slot, and that alone decides it:
    // announced, whatever the damp remembers.
    const Pool resubmitted{{"rev-a-1b", SPEND_A}};
    damp.Prune(resubmitted);
    QCOMPARE(damp.AnnouncedCount(), static_cast<std::size_t>(1));
    QVERIFY(damp.ShouldToastRevision(resubmitted, "rev-a-1b", FIRST));

    // And its later revisions are damped as usual.
    QVERIFY(!damp.ShouldToastRevision(Pool{{"rev-a-2b", SPEND_A}}, "rev-a-2b", UPDATE));
    QCOMPARE(damp.AnnouncedCount(), static_cast<std::size_t>(1));
}
