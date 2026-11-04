// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/sendcoinsdialogtests.h"

#include "qt/bitcoinunits.h"
#include "qt/sendcoinsdialog.h"
#include "qt/walletmodel.h"

#include <QList>
#include <QString>

namespace {
const QString ADDRESS_A = QStringLiteral("S9FYyGQK14zzXFnSqPLuXSGREfDjDr2G9s");
const QString ADDRESS_B = QStringLiteral("SBvCq9hD7WtYpRk3mXnJ5fLgZ2aEu8VsQo");
const QString ADDRESS_CHANGE = QStringLiteral("S7rQnXz4HcMd2LwTq8VbYe5GfJk3PaUh6N");
constexpr qint64 FEE_RATE = 100000;

QList<SendCoinsRecipient> MakeRecipients()
{
    SendCoinsRecipient a;
    a.address = ADDRESS_A;
    a.label = QStringLiteral("Alice <b>");
    a.amount = 100000000;

    SendCoinsRecipient b;
    b.address = ADDRESS_B;
    b.amount = 200000000;
    b.fSubtractFeeFromAmount = true;

    return {a, b};
}
} // namespace

void SendCoinsDialogTests::confirmationShowsEveryFullAddressAndTheFeeRate()
{
    const QString text = SendCoinsDialog::FormatSendConfirmation(MakeRecipients(), FEE_RATE, QString());

    QVERIFY(text.contains(ADDRESS_A));
    QVERIFY(text.contains(ADDRESS_B));
    QVERIFY(text.contains(QStringLiteral("Alice &lt;b&gt;")));
    QVERIFY(text.contains(BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, FEE_RATE)));
    QVERIFY(text.contains(QStringLiteral("per kB")));
    QVERIFY(!text.contains(QStringLiteral("Change to")));
}

void SendCoinsDialogTests::confirmationShowsACustomChangeAddress()
{
    const QString text = SendCoinsDialog::FormatSendConfirmation(MakeRecipients(), FEE_RATE, ADDRESS_CHANGE);

    QVERIFY(text.contains(QStringLiteral("Change to:")));
    QVERIFY(text.contains(ADDRESS_CHANGE));
    QVERIFY(text.contains(ADDRESS_A));
    QVERIFY(text.contains(ADDRESS_B));
}

void SendCoinsDialogTests::confirmationEscapesMarkupInTheAddresses()
{
    // The text is rich text, so markup in an address or the change address is
    // shown as written rather than rendered.
    QList<SendCoinsRecipient> recipients = MakeRecipients();
    recipients[0].address = QStringLiteral("S9FY<i>recipient</i>");

    const QString text = SendCoinsDialog::FormatSendConfirmation(
        recipients, FEE_RATE, QStringLiteral("S7rQ<img src=x>change"));

    QVERIFY(text.contains(QStringLiteral("S9FY&lt;i&gt;recipient&lt;/i&gt;")));
    QVERIFY(!text.contains(QStringLiteral("<i>recipient")));
    QVERIFY(text.contains(QStringLiteral("S7rQ&lt;img src=x&gt;change")));
    QVERIFY(!text.contains(QStringLiteral("<img")));
}
