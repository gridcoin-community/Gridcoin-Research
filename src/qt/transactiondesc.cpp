#include "transactiondesc.h"
#include "guiutil.h"
#include "bitcoinunits.h"

#include <QString>

using GRC::WalletTxDetail;

//! Translated maturity notice, kept verbatim (and under the same tr() context)
//! from the pre-DTO renderer so its shipped translations survive.
static QString GeneratedMaturityNotice()
{
    return TransactionDesc::tr("Gridcoin generated coins must mature 110 blocks before they can be spent. When you generated this block, it was broadcast to the network to be added to the block chain. If it fails to get into the chain, its state will change to \"not accepted\" and it won't be spendable. This may occasionally happen if another node generates a block within a few seconds of yours.");
}

QString TransactionDesc::FormatTxStatus(const WalletTxDetail& detail)
{
    switch (detail.finality)
    {
    case WalletTxDetail::Finality::OpenForBlocks:
        return tr("Open for %n more block(s)", "", detail.finality_value);
    case WalletTxDetail::Finality::OpenUntilTime:
        return tr("Open until %1").arg(GUIUtil::dateTimeStr(detail.finality_value));
    case WalletTxDetail::Finality::Conflicted:
        return tr("conflicted");
    case WalletTxDetail::Finality::Offline:
        return tr("%1/offline").arg(detail.depth);
    case WalletTxDetail::Finality::Unconfirmed:
        return tr("%1/unconfirmed").arg(detail.depth);
    case WalletTxDetail::Finality::Confirmed:
        return tr("%1 confirmations").arg(detail.depth);
    }

    return QString{};
}

//! Translated source label for a generated (coinstake) transaction.
static QString GeneratedSource(GRC::MinedType type)
{
    switch (type)
    {
    case GRC::MinedType::POS:                  return TransactionDesc::tr("Mined - PoS");
    case GRC::MinedType::POR:                  return TransactionDesc::tr("Mined - PoS+RR");
    case GRC::MinedType::ORPHANED:             return TransactionDesc::tr("Mined - Orphaned");
    case GRC::MinedType::POS_SIDE_STAKE_RCV:   return TransactionDesc::tr("PoS Side Stake Received");
    case GRC::MinedType::POR_SIDE_STAKE_RCV:   return TransactionDesc::tr("PoS+RR Side Stake Received");
    case GRC::MinedType::POS_SIDE_STAKE_SEND:  return TransactionDesc::tr("PoS Side Stake Sent");
    case GRC::MinedType::POR_SIDE_STAKE_SEND:  return TransactionDesc::tr("PoS+RR Side Stake Sent");
    case GRC::MinedType::MRC_RCV:              return TransactionDesc::tr("MRC Payment Received");
    case GRC::MinedType::MRC_SEND:             return TransactionDesc::tr("MRC Payment Sent");
    case GRC::MinedType::SUPERBLOCK:           return TransactionDesc::tr("Mined - Superblock");
    default:                                   return TransactionDesc::tr("Mined - Unknown");
    }
}

QString TransactionDesc::toHTML(const WalletTxDetail& detail)
{
    if (!detail.found) {
        return QString();
    }

    QString strHTML;
    strHTML.reserve(9250);
    strHTML += "<html><font face='verdana, arial, helvetica, sans-serif'>";

    strHTML += "<b>" + tr("Status") + ":</b> " + FormatTxStatus(detail);

    if (detail.requests != -1)
    {
        if (detail.requests == 0)
            strHTML += tr(", has not been successfully broadcast yet");

        else if (detail.requests > 0)
            strHTML += tr(", broadcast through %n node(s)", "", detail.requests);
    }

    strHTML += "<br>";
    strHTML += "<b>" + tr("Date") + ":</b> " + (detail.time ? GUIUtil::dateTimeStr(detail.time) : "") + "<br>";

    // From / Source
    switch (detail.source)
    {
    case WalletTxDetail::Source::CoinBase:
        strHTML += "<b>" + tr("Source") + ":</b> " + tr("Generated in CoinBase") + "<br>";
        break;
    case WalletTxDetail::Source::CoinStake:
        strHTML += "<b>" + tr("Source") + ":</b> " + GeneratedSource(detail.generated_type) + "<br>";
        break;
    case WalletTxDetail::Source::OnlineFrom:
        strHTML += "<b>" + tr("From") + ":</b> " + GUIUtil::HtmlEscape(detail.from_value) + "<br>";
        break;
    case WalletTxDetail::Source::Other:
        if (detail.has_own_credit_line)
        {
            strHTML += "<b>" + tr("From") + ":</b> " + tr("unknown") + "<br>";
            strHTML += "<b>" + tr("To") + ":</b> ";
            strHTML += GUIUtil::HtmlEscape(detail.own_credit_address);

            if (!detail.own_credit_label.empty())
                strHTML += " (" + tr("own address") + ", " + tr("label") + ": " + GUIUtil::HtmlEscape(detail.own_credit_label) + ")";

            else
                strHTML += " (" + tr("own address") + ")";

            strHTML += "<br>";
        }
        break;
    }

    // To (online transaction)
    if (detail.has_to)
    {
        strHTML += "<b>" + tr("To") + ":</b> ";

        if (!detail.to_label.empty())
            strHTML += GUIUtil::HtmlEscape(detail.to_label) + " ";

        strHTML += GUIUtil::HtmlEscape(detail.to_value) + "<br>";
    }

    // Amount
    switch (detail.amount_form)
    {
    case WalletTxDetail::AmountForm::CoinbaseImmature:
        strHTML += "<b>" + tr("Credit") + ":</b> ";

        if (detail.in_main_chain)
            strHTML += BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, detail.unmatured) + " (" + tr("matures in %n more block(s)", "", detail.matures_in) + ")";

        else
            strHTML += "(" + tr("not accepted") + ")";

        strHTML += "<br>";
        break;

    case WalletTxDetail::AmountForm::NetCredit:
        strHTML += "<b>" + tr("Credit") + ":</b> " + BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, detail.net) + "<br>";
        break;

    case WalletTxDetail::AmountForm::FromMe:
        for (const auto& debit : detail.debits)
        {
            if (debit.has_address)
            {
                strHTML += "<b>" + tr("To") + ":</b> ";

                if (!debit.label.empty())
                    strHTML += GUIUtil::HtmlEscape(debit.label) + " ";

                strHTML += GUIUtil::HtmlEscape(debit.address);
                strHTML += "<br>";
            }

            strHTML += "<b>" + tr("Debit") + ":</b> " + BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, -debit.amount) + "<br>";
        }

        if (detail.to_self)
        {
            strHTML += "<b>" + tr("Debit") + ":</b> " + BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, -detail.self_value) + "<br>";
            strHTML += "<b>" + tr("Credit") + ":</b> " + BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, detail.self_value) + "<br>";
        }

        if (detail.has_fee)
            strHTML += "<b>" + tr("Transaction fee") + ":</b> " + BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, -detail.fee) + "<br>";
        break;

    case WalletTxDetail::AmountForm::Mixed:
        for (int64_t debit : detail.mixed_debits)
            strHTML += "<b>" + tr("Debit") + ":</b> " + BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, -debit) + "<br>";

        for (int64_t credit : detail.mixed_credits)
            strHTML += "<b>" + tr("Credit") + ":</b> " + BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, credit) + "<br>";
        break;
    }

    strHTML += "<b>" + tr("Net amount") + ":</b> " + BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, detail.net, true) + "<br>";

    if (!detail.map_message.empty())
        strHTML += "<br><b>" + tr("Message") + ":</b><br>" + GUIUtil::HtmlEscape(detail.map_message, true) + "<br>";

    if (!detail.map_comment.empty())
        strHTML += "<br><b>" + tr("Comment") + ":</b><br>" + GUIUtil::HtmlEscape(detail.map_comment, true) + "<br>";

    strHTML += "<b>" + tr("TX ID") + ":</b> " + QString::fromStdString(detail.txid) + "<br>";

    if (detail.has_block_hash)
        strHTML += "<b>" + tr("Block Hash") + ":</b> " + QString::fromStdString(detail.block_hash) + "<br>";
    else
        strHTML += "<b>" + tr("Block Hash") + ":</b> Not yet in chain<br>";

    if (!detail.contract_message.empty())
    {
        strHTML += "<br>";
        strHTML += "<b>" + tr("Message") + ":</b> ";
        strHTML += GUIUtil::HtmlEscape(detail.contract_message);
        strHTML += "<br>";
    }

    if (detail.is_generated)
    {
        strHTML += "<hr><br><b>" + tr("Transaction Stake Data") + "</b><br><br>";

        for (const auto& info : detail.stake_info)
        {
            strHTML += "<b>";
            strHTML += QString::fromStdString(info.first);
            strHTML += ": </b>";
            strHTML += QString::fromStdString(info.second);
            strHTML += "<br>";
        }

        strHTML += "<br><br>" + GeneratedMaturityNotice() + "<br>";
    }

    // The always-rendered debits/credits + raw-data section.
    strHTML += "<hr><br><b>" + tr("Transaction Debits/Credits") + "</b><br><br>";

    for (int64_t debit : detail.verbose_debits)
        strHTML += "<b>" + tr("Debit") + ":</b> " + BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, -debit) + "<br>";

    for (int64_t credit : detail.verbose_credits)
        strHTML += "<b>" + tr("Credit") + ":</b> " + BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, credit) + "<br>";

    strHTML += "<br><b>" + tr("Transaction Data") + "</b><br><br>";
    strHTML += GUIUtil::HtmlEscape(detail.tx_dump, true);

    strHTML += "<br><b>" + tr("Transaction Inputs") + "</b>";
    strHTML += "<ul>";

    for (const auto& input : detail.inputs)
    {
        strHTML += "<li>";

        if (input.has_address)
            strHTML += QString::fromStdString(input.address);

        strHTML += " " + tr("Amount") + "=" + BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, input.amount);
        strHTML += " IsMine=" + (input.is_mine ? tr("true") : tr("false")) + "</li>";
    }

    strHTML += "</ul>";

    strHTML += "</font></html>";

    return strHTML;
}
