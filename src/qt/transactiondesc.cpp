#include "transactiondesc.h"
#include "clientmodel.h"
#include "guiutil.h"
#include "bitcoinunits.h"
#include "main.h"
#include "wallet/wallet.h"
#include "txdb.h"
#include "ui_interface.h"
#include "base58.h"
#include "bitcoingui.h"
#include "util.h"

#include <QInputDialog>
#include <QPushButton>
#include <QMessageBox>
#include <string>

std::vector<std::pair<std::string, std::string>> GetTxStakeBoincHashInfo(const CMerkleTx& mtx);
std::vector<std::pair<std::string, std::string>> GetTxNormalBoincHashInfo(const CMerkleTx& mtx);

QString ToQString(std::string s)
{
    QString str1 = QString::fromUtf8(s.c_str());

    return str1;
}

QString TransactionDesc::FormatTxStatus(const CWalletTx& wtx)
{
    AssertLockHeld(cs_main);

    if (!IsFinalTx(wtx, nBestHeight + 1))
    {
        if (wtx.nLockTime < LOCKTIME_THRESHOLD)
            return tr("Open for %n more block(s)", "", wtx.nLockTime - nBestHeight);

        else
            return tr("Open until %1").arg(GUIUtil::dateTimeStr(wtx.nLockTime));
    }

    else
    {
        int nDepth = wtx.GetDepthInMainChain();

        if (nDepth < 0)
            return tr("conflicted");

        else if (GetAdjustedTime() - wtx.nTimeReceived > 2 * 60 && wtx.GetRequestCount() == 0)
            return tr("%1/offline").arg(nDepth);

        else if (nDepth < 10)
            return tr("%1/unconfirmed").arg(nDepth);

        else
            return tr("%1 confirmations").arg(nDepth);
    }
}

std::string PubKeyToGRCAddress(const CScript& scriptPubKey)
{
    txnouttype type;
    std::vector<CTxDestination> addresses;
    int nRequired;

    if (!ExtractDestinations(scriptPubKey, type, addresses, nRequired))
    {
        return "";
    }

    std::string grcaddress = "";

    for (auto const& addr : addresses)
        grcaddress = CBitcoinAddress(addr).ToString();

    return grcaddress;
}

QString TransactionDesc::toHTML(CWallet *wallet, CWalletTx &wtx, unsigned int vout)
{
    QString strHTML;

    LOCK2(cs_main, wallet->cs_wallet);

    strHTML.reserve(9250);
    strHTML += "<html><font face='verdana, arial, helvetica, sans-serif'>";

    int64_t nTime = wtx.GetTxTime();
    int64_t nCredit = wtx.GetCredit();
    int64_t nDebit = wtx.GetDebit();
    int64_t nNet = nCredit - nDebit;
    int nRequests = wtx.GetRequestCount();

    strHTML += "<b>" + tr("Status") + ":</b> " + FormatTxStatus(wtx);

    if (nRequests != -1)
    {
        if (nRequests == 0)
            strHTML += tr(", has not been successfully broadcast yet");

        else if (nRequests > 0)
            strHTML += tr(", broadcast through %n node(s)", "", nRequests);
    }

    strHTML += "<br>";
    strHTML += "<b>" + tr("Date") + ":</b> " + (nTime ? GUIUtil::dateTimeStr(nTime) : "") + "<br>";

    // From
    if (wtx.IsCoinBase())
        strHTML += "<b>" + tr("Source") + ":</b> " + tr("Generated in CoinBase") + "<br>";

    else if (wtx.IsCoinStake())
    {
        // Update support for Side Stake and correctly show POS/POR as well
        strHTML += "<b>" + tr("Source") + ":</b> ";

        MinedType gentype = GetGeneratedType(wallet, wtx.GetHash(), vout);

        switch (gentype)
        {
        case MinedType::POS:
            strHTML += tr("MINED - POS");
            break;
        case MinedType::POR:
            strHTML += tr("MINED - POR");
            break;
        case MinedType::ORPHANED:
            strHTML += tr("MINED - ORPHANED");
            break;
        case MinedType::POS_SIDE_STAKE_RCV:
            strHTML += tr("POS SIDE STAKE RECEIVED");
            break;
        case MinedType::POR_SIDE_STAKE_RCV:
            strHTML += tr("POR SIDE STAKE RECEIVED");
            break;
        case MinedType::POS_SIDE_STAKE_SEND:
            strHTML += tr("POS SIDE STAKE SENT");
            break;
        case MinedType::POR_SIDE_STAKE_SEND:
            strHTML += tr("POR SIDE STAKE SENT");
            break;
        case MinedType::SUPERBLOCK:
            strHTML += tr("SUPERBLOCK");
            break;
        default:
            strHTML += tr("MINED - UNKNOWN");
            break;
        }

        strHTML += "<br>";
    }
    // Online transaction
    else if (wtx.mapValue.count("from") && !wtx.mapValue["from"].empty())
        strHTML += "<b>" + tr("From") + ":</b> " + GUIUtil::HtmlEscape(wtx.mapValue["from"]) + "<br>";

    else
    {
        // Offline transaction
        if (nNet > 0)
        {
            // Credit
            for (auto const& txout : wtx.vout)
            {
                if (wallet->IsMine(txout) != ISMINE_NO)
                {
                    CTxDestination address;

                    if (ExtractDestination(txout.scriptPubKey, address) && (IsMine(*wallet, address) != ISMINE_NO))
                    {
                        if (wallet->mapAddressBook.count(address))
                        {
                            strHTML += "<b>" + tr("From") + ":</b> " + tr("unknown") + "<br>";
                            strHTML += "<b>" + tr("To") + ":</b> ";
                            strHTML += GUIUtil::HtmlEscape(CBitcoinAddress(address).ToString());

                            if (!wallet->mapAddressBook[address].empty())
                                strHTML += " (" + tr("own address") + ", " + tr("label") + ": " + GUIUtil::HtmlEscape(wallet->mapAddressBook[address]) + ")";

                            else
                                strHTML += " (" + tr("own address") + ")";

                            strHTML += "<br>";
                        }
                    }

                    break;
                }
            }
        }
    }

    // To
    if (wtx.mapValue.count("to") && !wtx.mapValue["to"].empty())
    {
        // Online transaction
        std::string strAddress = wtx.mapValue["to"];

        strHTML += "<b>" + tr("To") + ":</b> ";

        CTxDestination dest = CBitcoinAddress(strAddress).Get();

        if (wallet->mapAddressBook.count(dest) && !wallet->mapAddressBook[dest].empty())
            strHTML += GUIUtil::HtmlEscape(wallet->mapAddressBook[dest]) + " ";

        strHTML += GUIUtil::HtmlEscape(strAddress) + "<br>";
    }

    // Amount
    if (wtx.IsCoinBase() && nCredit == 0)
    {
        // Coinbase
        int64_t nUnmatured = 0;

        for (auto const& txout : wtx.vout)
            nUnmatured += wallet->GetCredit(txout);

        strHTML += "<b>" + tr("Credit") + ":</b> ";

        if (wtx.IsInMainChain())
            strHTML += BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, nUnmatured)+ " (" + tr("matures in %n more block(s)", "", wtx.GetBlocksToMaturity()) + ")";

        else
            strHTML += "(" + tr("not accepted") + ")";

        strHTML += "<br>";
    }

    else if (nNet > 0)
    {
        // Credit
        strHTML += "<b>" + tr("Credit") + ":</b> " + BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, nNet) + "<br>";
    }

    else
    {
        bool fAllFromMe = true;

        for (auto const& txin : wtx.vin)
            fAllFromMe = fAllFromMe && (wallet->IsMine(txin) != ISMINE_NO);

        bool fAllToMe = true;

        for (auto const& txout : wtx.vout)
            fAllToMe = fAllToMe && (wallet->IsMine(txout) != ISMINE_NO);

        if (fAllFromMe)
        {
            // Debit
            for (auto const& txout : wtx.vout)
            {
                if (wallet->IsMine(txout) != ISMINE_NO)
                    continue;

                if (!wtx.mapValue.count("to") || wtx.mapValue["to"].empty())
                {
                    // Offline transaction
                    CTxDestination address;

                    if (ExtractDestination(txout.scriptPubKey, address))
                    {
                        strHTML += "<b>" + tr("To") + ":</b> ";

                        if (wallet->mapAddressBook.count(address) && !wallet->mapAddressBook[address].empty())
                            strHTML += GUIUtil::HtmlEscape(wallet->mapAddressBook[address]) + " ";

                        strHTML += GUIUtil::HtmlEscape(CBitcoinAddress(address).ToString());
                        strHTML += "<br>";
                    }
                }

                strHTML += "<b>" + tr("Debit") + ":</b> " + BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, -txout.nValue) + "<br>";
            }

            if (fAllToMe)
            {
                // Payment to self
                int64_t nChange = wtx.GetChange();
                int64_t nValue = nCredit - nChange;

                strHTML += "<b>" + tr("Debit") + ":</b> " + BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, -nValue) + "<br>";
                strHTML += "<b>" + tr("Credit") + ":</b> " + BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, nValue) + "<br>";
            }

            int64_t nTxFee = nDebit - wtx.GetValueOut();

            if (nTxFee > 0)
                strHTML += "<b>" + tr("Transaction fee") + ":</b> " + BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, -nTxFee) + "<br>";
        }

        else
        {
            // Mixed debit transaction
            for (auto const& txin : wtx.vin)

                if (wallet->IsMine(txin) != ISMINE_NO)
                    strHTML += "<b>" + tr("Debit") + ":</b> " + BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, -wallet->GetDebit(txin)) + "<br>";

            for (auto const& txout : wtx.vout)

                if (wallet->IsMine(txout) != ISMINE_NO)
                    strHTML += "<b>" + tr("Credit") + ":</b> " + BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, wallet->GetCredit(txout)) + "<br>";
        }
    }

    strHTML += "<b>" + tr("Net amount") + ":</b> " + BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, nNet, true) + "<br>";

    // Message
    if (wtx.mapValue.count("message") && !wtx.mapValue["message"].empty())
        strHTML += "<br><b>" + tr("Message") + ":</b><br>" + GUIUtil::HtmlEscape(wtx.mapValue["message"], true) + "<br>";

    if (wtx.mapValue.count("comment") && !wtx.mapValue["comment"].empty())
        strHTML += "<br><b>" + tr("Comment") + ":</b><br>" + GUIUtil::HtmlEscape(wtx.mapValue["comment"], true) + "<br>";

    strHTML += "<b>" + tr("TX ID") + ":</b> " + wtx.GetHash().ToString().c_str() + "<br>";

    std::string sHashBlock = wtx.hashBlock.ToString();

    if (wtx.hashBlock.IsNull())
        strHTML += "<b>" + tr("Block Hash") + ":</b> Not yet in chain<br>";

    else
        strHTML += "<b>" + tr("Block Hash") + ":</b> " + sHashBlock.c_str() + "<br>";

    const std::string tx_message = wtx.GetMessage();

    if (!tx_message.empty())
    {
        strHTML += "<br>";
        strHTML += "<b>" + tr("Message") + ":</b> ";
        strHTML += GUIUtil::HtmlEscape(tx_message);
        strHTML += "<br>";
    }

    if (wtx.IsCoinBase() || wtx.IsCoinStake())
    {
        strHTML += "<hr><br><b>" + tr("Transaction Stake Data") + "</b><br><br>";

        std::vector<std::pair<std::string, std::string>> vTxStakeInfoIn = GetTxStakeBoincHashInfo(wtx);

        for (auto const& vTxStakeInfo : vTxStakeInfoIn)
        {
            strHTML += "<b>";
            strHTML += vTxStakeInfo.first.c_str();
            strHTML += ": </b>";
            strHTML += vTxStakeInfo.second.c_str();
            strHTML += "<br>";
        }

        strHTML += "<br><br>" + tr("Gridcoin generated coins must mature 110 blocks before they can be spent. When you generated this block, it was broadcast to the network to be added to the block chain. If it fails to get into the chain, its state will change to \"not accepted\" and it won't be spendable. This may occasionally happen if another node generates a block within a few seconds of yours.") + "<br>";
    }

    if (LogInstance().WillLogCategory(BCLog::LogFlags::VERBOSE) || true)
    {
        strHTML += "<hr><br><b>" + tr("Transaction Debits/Credits") + "</b><br><br>";

        for (auto const& txin : wtx.vin)

            if (wallet->IsMine(txin) != ISMINE_NO)
                strHTML += "<b>" + tr("Debit") + ":</b> " + BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, -wallet->GetDebit(txin)) + "<br>";

        for (auto const& txout : wtx.vout)

            if (wallet->IsMine(txout) != ISMINE_NO)
                strHTML += "<b>" + tr("Credit") + ":</b> " + BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, wallet->GetCredit(txout)) + "<br>";

        strHTML += "<br><b>" + tr("Transaction Data") + "</b><br><br>";
        strHTML += GUIUtil::HtmlEscape(wtx.ToString(), true);

        CTxDB txdb("r"); // To fetch source txouts

        strHTML += "<br><b>" + tr("Transaction Inputs") + "</b>";
        strHTML += "<ul>";

        for (auto const& txin : wtx.vin)
        {
            COutPoint prevout = txin.prevout;

            CTransaction prev;

            if(txdb.ReadDiskTx(prevout.hash, prev))
            {
                if (prevout.n < prev.vout.size())
                {
                    strHTML += "<li>";

                    //Inputs: 7-31-2015
                    const CTxOut &vout = prev.vout[prevout.n];

                    CTxDestination address;

                    if (ExtractDestination(vout.scriptPubKey, address))
                        strHTML += QString::fromStdString(CBitcoinAddress(address).ToString());

                    strHTML += " " + tr("Amount") + "=" + BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, vout.nValue);
                    strHTML += " IsMine=" + ((wallet->IsMine(vout) != ISMINE_NO) ? tr("true") : tr("false")) + "</li>";
                }
            }
        }

        strHTML += "</ul>";
    }

    strHTML += "</font></html>";

    return strHTML;
}
