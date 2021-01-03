// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "version.h"
#include "txdb.h"
#include "rpc/server.h"
#include "rpc/protocol.h"
#include "init.h"
#include "base58.h"
#include "streams.h"
#include "util.h"
#include "gridcoin/backup.h"
#include "gridcoin/staking/difficulty.h"
#include "gridcoin/staking/status.h"
#include "gridcoin/tx_message.h"
#include "wallet/wallet.h"
#include "wallet/walletdb.h"
#include "wallet/ismine.h"

#include <univalue.h>

using namespace std;

int64_t nWalletUnlockTime;
static CCriticalSection cs_nWalletUnlockTime;

extern void ThreadTopUpKeyPool(void* parg);
extern void ThreadCleanWalletPassphrase(void* parg);
extern void TxToJSON(const CTransaction& tx, const uint256 hashBlock, UniValue& entry);

static void accountingDeprecationCheck()
{
    if (!GetBoolArg("-enableaccounts", false))
        throw runtime_error(
            "Accounting API is deprecated and will be removed in future.\n"
            "It can easily result in negative or odd balances if misused or misunderstood, which has happened in the field.\n"
            "If you still want to enable it, add to your config file enableaccounts=1\n");

    if (GetBoolArg("-staking", true))
        throw runtime_error("If you want to use accounting API, staking must be disabled, add to your config file staking=0\n");
}

std::string HelpRequiringPassphrase()
{
    return pwalletMain->IsCrypted()
        ? "\nrequires wallet passphrase to be set with walletpassphrase first"
        : "";
}

void EnsureWalletIsUnlocked()
{
    if (pwalletMain->IsLocked())
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Please enter the wallet passphrase with walletpassphrase first.");
    if (fWalletUnlockStakingOnly)
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Wallet is unlocked for staking only.");
}

void WalletTxToJSON(const CWalletTx& wtx, UniValue& entry)
{
    int confirms = wtx.GetDepthInMainChain();
    entry.pushKV("confirmations", confirms);
    if (wtx.IsCoinBase() || wtx.IsCoinStake())
        entry.pushKV("generated", true);
    if (confirms > 0)
    {
        entry.pushKV("blockhash", wtx.hashBlock.GetHex());
        entry.pushKV("blockindex", wtx.nIndex);
        entry.pushKV("blocktime", (int)(mapBlockIndex[wtx.hashBlock]->nTime));
    }
    entry.pushKV("txid", wtx.GetHash().GetHex());
    entry.pushKV("time", wtx.GetTxTime());
    entry.pushKV("timereceived", (int)wtx.nTimeReceived);
    for (auto const& item : wtx.mapValue)
        entry.pushKV(item.first, item.second);
}

string AccountFromValue(const UniValue& value)
{
    string strAccount = value.get_str();
    if (strAccount == "*")
        throw JSONRPCError(RPC_WALLET_INVALID_ACCOUNT_NAME, "Invalid account name");
    return strAccount;
}

UniValue getinfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "getinfo\n"
                "\n"
                "Returns an object containing various state info.");

    proxyType proxy;
    GetProxy(NET_IPV4, proxy);

    UniValue obj(UniValue::VOBJ);
    UniValue diff(UniValue::VOBJ);

    LOCK2(cs_main, pwalletMain->cs_wallet);

    obj.pushKV("version",       FormatFullVersion());
    obj.pushKV("minor_version", CLIENT_VERSION_MINOR);

    obj.pushKV("protocolversion", PROTOCOL_VERSION);
    obj.pushKV("walletversion", pwalletMain->GetVersion());
    obj.pushKV("balance",       ValueFromAmount(pwalletMain->GetBalance()));
    obj.pushKV("newmint",       ValueFromAmount(pwalletMain->GetNewMint()));
    obj.pushKV("stake",         ValueFromAmount(pwalletMain->GetStake()));
    obj.pushKV("blocks",        nBestHeight);
    obj.pushKV("in_sync",       !OutOfSyncByAge());
    obj.pushKV("timeoffset",    GetTimeOffset());
    obj.pushKV("uptime",        g_timer.GetElapsedTime("uptime", "default") / 1000);
    obj.pushKV("moneysupply",   ValueFromAmount(pindexBest->nMoneySupply));
    obj.pushKV("connections",   (int)vNodes.size());
    obj.pushKV("proxy",         (proxy.first.IsValid() ? proxy.first.ToStringIPPort() : string()));
    obj.pushKV("ip",            addrSeenByPeer.ToStringIP());

    diff.pushKV("current", GRC::GetCurrentDifficulty());
    diff.pushKV("target", GRC::GetTargetDifficulty());
    obj.pushKV("difficulty",    diff);

    obj.pushKV("testnet",       fTestNet);
    obj.pushKV("keypoololdest", pwalletMain->GetOldestKeyPoolTime());
    obj.pushKV("keypoolsize",   (int)pwalletMain->GetKeyPoolSize());
    obj.pushKV("paytxfee",      ValueFromAmount(nTransactionFee));
    obj.pushKV("mininput",      ValueFromAmount(nMinimumInputValue));
    if (pwalletMain->IsCrypted())
        obj.pushKV("unlocked_until", nWalletUnlockTime / 1000);
    obj.pushKV("errors",        GetWarnings("statusbar"));
    return obj;
}

UniValue getwalletinfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "getwalletinfo\n"
                "\n"
                "Displays information about the wallet\n");

    UniValue res(UniValue::VOBJ);

    {
        LOCK2(cs_main, pwalletMain->cs_wallet);

        res.pushKV("walletversion", pwalletMain->GetVersion());
        res.pushKV("balance",       ValueFromAmount(pwalletMain->GetBalance()));
        res.pushKV("newmint",       ValueFromAmount(pwalletMain->GetNewMint()));
        res.pushKV("stake",         ValueFromAmount(pwalletMain->GetStake()));
        res.pushKV("keypoololdest", pwalletMain->GetOldestKeyPoolTime());
        res.pushKV("keypoolsize",   (int)pwalletMain->GetKeyPoolSize());

        if (pwalletMain->IsCrypted())
            res.pushKV("unlocked_until", nWalletUnlockTime / 1000);
    }

    {
        LOCK(g_miner_status.lock);

        bool staking = g_miner_status.nLastCoinStakeSearchInterval && g_miner_status.WeightSum;

        res.pushKV("staking", staking);
        res.pushKV("mining-error", g_miner_status.ReasonNotStaking);
    }

    return res;
}

UniValue getnewpubkey(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
                "getnewpubkey [account]\n"
                "\n"
                "Returns new public key for coinbase generation.\n");

    // Parse the account first so we don't generate a key if there's an error
    string strAccount;
    if (params.size() > 0)
        strAccount = AccountFromValue(params[0]);

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (!pwalletMain->IsLocked())
        pwalletMain->TopUpKeyPool();

    // Generate a new key that is added to wallet
    CPubKey newKey;
    if (!pwalletMain->GetKeyFromPool(newKey, false))
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");
    CKeyID keyID = newKey.GetID();

    pwalletMain->SetAddressBookName(keyID, strAccount);
    vector<unsigned char> vchPubKey = newKey.Raw();

    return HexStr(vchPubKey.begin(), vchPubKey.end());
}


UniValue getnewaddress(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
                "getnewaddress [account]\n"
                "\n"
                "Returns a new Gridcoin address for receiving payments.  "
                "If [account] is specified, it is added to the address book "
                "so payments received with the address will be credited to [account].\n");

    // Parse the account first so we don't generate a key if there's an error
    string strAccount;
    if (params.size() > 0)
        strAccount = AccountFromValue(params[0]);

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (!pwalletMain->IsLocked())
        pwalletMain->TopUpKeyPool();

    // Generate a new key that is added to wallet
    CPubKey newKey;
    if (!pwalletMain->GetKeyFromPool(newKey, false))
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");
    CKeyID keyID = newKey.GetID();

    pwalletMain->SetAddressBookName(keyID, strAccount);

    return CBitcoinAddress(keyID).ToString();
}


CBitcoinAddress GetAccountAddress(string strAccount, bool bForceNew=false)
{
    CWalletDB walletdb(pwalletMain->strWalletFile);

    CAccount account;
    walletdb.ReadAccount(strAccount, account);

    bool bKeyUsed = false;

    // Check if the current key has been used
    if (account.vchPubKey.IsValid())
    {
        CScript scriptPubKey;
        scriptPubKey.SetDestination(account.vchPubKey.GetID());
        for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin();
             it != pwalletMain->mapWallet.end() && account.vchPubKey.IsValid();
             ++it)
        {
            const CWalletTx& wtx = (*it).second;
            for (auto const& txout : wtx.vout)
                if (txout.scriptPubKey == scriptPubKey)
                    bKeyUsed = true;
        }
    }

    // Generate a new key
    if (!account.vchPubKey.IsValid() || bForceNew || bKeyUsed)
    {
        if (!pwalletMain->GetKeyFromPool(account.vchPubKey, false))
            throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");

        pwalletMain->SetAddressBookName(account.vchPubKey.GetID(), strAccount);
        walletdb.WriteAccount(strAccount, account);
    }

    return CBitcoinAddress(account.vchPubKey.GetID());
}

UniValue getaccountaddress(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "getaccountaddress <account>\n"
                "\n"
                "Returns the current Gridcoin address for receiving payments to this account.\n");

    // Parse the account first so we don't generate a key if there's an error
    string strAccount = AccountFromValue(params[0]);

    UniValue ret(UniValue::VSTR);

    LOCK2(cs_main, pwalletMain->cs_wallet);

    ret = GetAccountAddress(strAccount).ToString();

    return ret;
}

UniValue setaccount(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
                "setaccount <gridcoinaddress> <account>\n"
                "\n"
                "Sets the account associated with the given address.\n");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CBitcoinAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Gridcoin address");

    string strAccount;
    if (params.size() > 1)
        strAccount = AccountFromValue(params[1]);

    // Detect when changing the account of an address that is the 'unused current key' of another account:
    if (pwalletMain->mapAddressBook.count(address.Get()))
    {
        string strOldAccount = pwalletMain->mapAddressBook[address.Get()];
        if (address == GetAccountAddress(strOldAccount))
            GetAccountAddress(strOldAccount, true);
    }

    pwalletMain->SetAddressBookName(address.Get(), strAccount);

    return NullUniValue;
}


UniValue getaccount(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "getaccount <gridcoinaddress>\n"
                "\n"
                "Returns the account associated with the given address.\n");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CBitcoinAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Gridcoin address");

    string strAccount;

    map<CTxDestination, string>::iterator mi = pwalletMain->mapAddressBook.find(address.Get());
    if (mi != pwalletMain->mapAddressBook.end() && !(*mi).second.empty())
        strAccount = (*mi).second;
    return strAccount;
}


UniValue getaddressesbyaccount(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "getaddressesbyaccount <account>\n"
                "\n"
                "Returns the list of addresses for the given account.\n");

    string strAccount = AccountFromValue(params[0]);

    // Find all addresses that have the given account
    UniValue ret(UniValue::VARR);

    LOCK2(cs_main, pwalletMain->cs_wallet);

    for (auto const& item : pwalletMain->mapAddressBook)
    {
        const CBitcoinAddress& address = item.first;
        const string& strName = item.second;
        if (strName == strAccount)
            ret.push_back(address.ToString());
    }
    return ret;
}

UniValue sendtoaddress(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 5)
        throw runtime_error(
                "sendtoaddress <gridcoinaddress> <amount> [comment] [comment-to] [message]\n"
                "\n"
                "<amount> is a real and is rounded to the nearest 0.000001\n"
                "[comment] a comment used to store what the transaction is for.\n"
                "         This is not part of the transaction, just kept in your wallet.\n"
                "[comment_to] a comment to store the name of the person or organization\n"
                "             to which you're sending the transaction. This is not part of the \n"
                "             transaction, just kept in your wallet.\n"
                "[message] Optional message to add to the receiver.\n"
                + HelpRequiringPassphrase());

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CBitcoinAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Gridcoin address");

    // Amount
    int64_t nAmount = AmountFromValue(params[1]);

    // Wallet comments
    CWalletTx wtx;
    if (params.size() > 2 && !params[2].isNull() && !params[2].get_str().empty())
        wtx.mapValue["comment"] = params[2].get_str();
    if (params.size() > 3 && !params[3].isNull() && !params[3].get_str().empty())
        wtx.mapValue["to"]      = params[3].get_str();
    if (params.size() > 4 && !params[4].isNull() && !params[4].get_str().empty())
        wtx.vContracts.emplace_back(
            GRC::MakeContract<GRC::TxMessage>(GRC::ContractAction::ADD, params[4].get_str()));

    if (pwalletMain->IsLocked())
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Please enter the wallet passphrase with walletpassphrase first.");

    string strError = pwalletMain->SendMoneyToDestination(address.Get(), nAmount, wtx);
    if (!strError.empty())
        throw JSONRPCError(RPC_WALLET_ERROR, strError);

    return wtx.GetHash().GetHex();
}

UniValue listaddressgroupings(const UniValue& params, bool fHelp)
{
    if (fHelp)
        throw runtime_error(
                "listaddressgroupings\n"
                "\n"
                "Lists groups of addresses which have had their common ownership\n"
                "made public by common use as inputs or as the resulting change\n"
                "in past transactions\n");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    UniValue jsonGroupings(UniValue::VARR);
    map<CTxDestination, int64_t> balances = pwalletMain->GetAddressBalances();
    for (auto const& grouping : pwalletMain->GetAddressGroupings())
    {
        UniValue jsonGrouping(UniValue::VARR);
        for (auto const& address : grouping)
        {
            UniValue addressInfo(UniValue::VARR);
            addressInfo.push_back(CBitcoinAddress(address).ToString());
            addressInfo.push_back(ValueFromAmount(balances[address]));
            {
                if (pwalletMain->mapAddressBook.find(CBitcoinAddress(address).Get()) != pwalletMain->mapAddressBook.end())
                    addressInfo.push_back(pwalletMain->mapAddressBook.find(CBitcoinAddress(address).Get())->second);
            }
            jsonGrouping.push_back(addressInfo);
        }
        jsonGroupings.push_back(jsonGrouping);
    }
    return jsonGroupings;
}

UniValue signmessage(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error(
                "signmessage <Gridcoinaddress> <message>\n"
                "\n"
                "Sign a message with the private key of an address\n");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    string strAddress = params[0].get_str();
    string strMessage = params[1].get_str();

    CBitcoinAddress addr(strAddress);
    if (!addr.IsValid())
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid address");

    CKeyID keyID;
    if (!addr.GetKeyID(keyID))
        throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to key");

    CKey key;
    if (!pwalletMain->GetKey(keyID, key))
        throw JSONRPCError(RPC_WALLET_ERROR, "Private key not available");

    CDataStream ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    vector<unsigned char> vchSig;
    if (!key.SignCompact(Hash(ss.begin(), ss.end()), vchSig))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Sign failed");

    return EncodeBase64(&vchSig[0], vchSig.size());
}

UniValue verifymessage(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 3)
        throw runtime_error(
                "verifymessage <Gridcoinaddress> <signature> <message>\n"
                "\n"
                "Verify a signed message\n");

    string strAddress  = params[0].get_str();
    string strSign     = params[1].get_str();
    string strMessage  = params[2].get_str();

    LOCK(cs_main);

    CBitcoinAddress addr(strAddress);
    if (!addr.IsValid())
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid address");

    CKeyID keyID;
    if (!addr.GetKeyID(keyID))
        throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to key");

    bool fInvalid = false;
    vector<unsigned char> vchSig = DecodeBase64(strSign.c_str(), &fInvalid);

    if (fInvalid)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Malformed base64 encoding");

    CDataStream ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    CKey key;
    if (!key.SetCompactSignature(Hash(ss.begin(), ss.end()), vchSig))
        return false;

    return (key.GetPubKey().GetID() == keyID);
}


UniValue getreceivedbyaddress(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
                "getreceivedbyaddress <Gridcoinaddress> [minconf=1]\n"
                "\n"
                "Returns the total amount received by <Gridcoinaddress> in transactions with at least [minconf] confirmations.\n");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // Bitcoin address
    CBitcoinAddress address = CBitcoinAddress(params[0].get_str());
    CScript scriptPubKey;
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Gridcoin address");
    scriptPubKey.SetDestination(address.Get());
    if (IsMine(*pwalletMain,scriptPubKey) == ISMINE_NO)
        return (double)0.0;

    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();

    // Tally
    int64_t nAmount = 0;
    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
    {
        const CWalletTx& wtx = (*it).second;
        if (wtx.IsCoinBase() || wtx.IsCoinStake() || !IsFinalTx(wtx))
            continue;

        for (auto const& txout : wtx.vout)
            if (txout.scriptPubKey == scriptPubKey)
                if (wtx.GetDepthInMainChain() >= nMinDepth)
                    nAmount += txout.nValue;
    }

    return  ValueFromAmount(nAmount);
}


void GetAccountAddresses(string strAccount, set<CTxDestination>& setAddress)
{
    for (auto const& item : pwalletMain->mapAddressBook)
    {
        const CTxDestination& address = item.first;
        const string& strName = item.second;
        if (strName == strAccount)
            setAddress.insert(address);
    }
}

UniValue getreceivedbyaccount(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
                "getreceivedbyaccount <account> [minconf=1]\n"
                "\n"
                "Returns the total amount received by addresses with <account> in transactions with at least [minconf] confirmations.\n");

    accountingDeprecationCheck();

    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // Get the set of pub keys assigned to account
    string strAccount = AccountFromValue(params[0]);
    set<CTxDestination> setAddress;
    GetAccountAddresses(strAccount, setAddress);

    // Tally
    int64_t nAmount = 0;
    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
    {
        const CWalletTx& wtx = (*it).second;
        if (wtx.IsCoinBase() || wtx.IsCoinStake() || !IsFinalTx(wtx))
            continue;

        for (auto const& txout : wtx.vout)
        {
            CTxDestination address;
            if (ExtractDestination(txout.scriptPubKey, address) && (IsMine(*pwalletMain, address) != ISMINE_NO) && setAddress.count(address))
                if (wtx.GetDepthInMainChain() >= nMinDepth)
                    nAmount += txout.nValue;
        }
    }

    return (double)nAmount / (double)COIN;
}

int64_t GetAccountBalance(CWalletDB& walletdb, const string& strAccount, int nMinDepth, const isminefilter& filter = ISMINE_SPENDABLE)
{
    int64_t nBalance = 0;

    // Tally wallet transactions
    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
    {
        const CWalletTx& wtx = (*it).second;
        if (!IsFinalTx(wtx) || wtx.GetDepthInMainChain() < 0)
            continue;

        int64_t nReceived, nSent, nFee;
        wtx.GetAccountAmounts(strAccount, nReceived, nSent, nFee, filter);

        if (nReceived != 0 && wtx.GetDepthInMainChain() >= nMinDepth && wtx.GetBlocksToMaturity() == 0)
            nBalance += nReceived;
        nBalance -= nSent + nFee;
    }

    // Tally internal accounting entries
    nBalance += walletdb.GetAccountCreditDebit(strAccount);

    return nBalance;
}

int64_t GetAccountBalance(const string& strAccount, int nMinDepth, const isminefilter& filter = ISMINE_SPENDABLE)
{
    CWalletDB walletdb(pwalletMain->strWalletFile);
    return GetAccountBalance(walletdb, strAccount, nMinDepth, filter);
}


UniValue getbalance(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 3)
         throw runtime_error(
                "getbalance ( \"account\" minconf includeWatchonly )\n"
                "\n"
                "\nIf account is not specified, returns the server's total available balance.\n"
                "If account is specified, returns the balance in the account.\n"
                "Note that the account \"\" is not the same as leaving the parameter out.\n"
                "The server total may be different to the balance in the default \"\" account.\n"
                "\nArguments:\n"
                "1. \"account\"      (string, optional) The selected account, or \"*\" for entire wallet. It may be the default account using \"\".\n"
                "2. minconf          (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
                "3. includeWatchonly (bool, optional, default=false) Also include balance in watchonly addresses (see 'importaddress')\n"
                "\nResult:\n"
                "amount              (numeric) The total amount in btc received for this account.\n"
                "\nExamples:\n"
                "\nThe total amount in the server across all accounts\n"
                "\nThe total amount in the server across all accounts, with at least 5 confirmations\n"
                "\nThe total amount in the default account with at least 1 confirmation\n"
                "\nThe total amount in the account named tabby with at least 6 confirmations\n"
                "\nAs a json rpc call\n"
                );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (params.size() == 0)
        return  ValueFromAmount(pwalletMain->GetBalance());

    int nMinDepth = 1;
    isminefilter filter = ISMINE_SPENDABLE;

    if (params.size() > 1)
    {
        nMinDepth = params[1].get_int();
        if (params.size() > 2)
        {
            if (params[2].get_bool()) filter = filter | ISMINE_WATCH_ONLY;
        }
    }

    if (params[0].get_str() == "*")
    {
        // Calculate total balance a different way from GetBalance()
        // (GetBalance() sums up all unspent TxOuts)
        // getbalance and getbalance '*' 0 should return the same number.
        int64_t nBalance = 0;

        for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
        {
            const CWalletTx& wtx = it->second;
            if (!wtx.IsTrusted())
                continue;

            int64_t allFee;
            string strSentAccount;
            list<COutputEntry> listReceived;
            list<COutputEntry> listSent;

            wtx.GetAmounts(listReceived, listSent, allFee, strSentAccount, filter);

            if (wtx.GetDepthInMainChain() >= nMinDepth && wtx.GetBlocksToMaturity() == 0)
            {
                for (auto const& r : listReceived)
                {
                    nBalance += r.amount;
                }
            }
            for (auto const& s : listSent)
            {
                nBalance -= s.amount;
            }

            nBalance -= allFee;
        }

    return ValueFromAmount(nBalance);
    }

    accountingDeprecationCheck();

    string strAccount = AccountFromValue(params[0]);

    int64_t nBalance = GetAccountBalance(strAccount, nMinDepth, filter);

    return ValueFromAmount(nBalance);
}

UniValue getbalancedetail(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
         throw runtime_error(
                "getbalancedetail ( minconf includeWatchonly )\n"
                "\n"
                "\nArguments:\n"
                "1. minconf          (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
                "2. includeWatchonly (bool, optional, default=false) Also include balance in watchonly addresses (see 'importaddress')\n"
                "\nResult:\n"
                "detailed list       (JSON) A list of outputs similar to listtransactions that compose the entire balance.\n"
                );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    int nMinDepth = 1;
    isminefilter filter = ISMINE_SPENDABLE;

    if (params.size() > 0)
    {
        nMinDepth = params[0].get_int();
        if (params.size() > 1)
        {
            if (params[1].get_bool()) filter = filter | ISMINE_WATCH_ONLY;
        }
    }

    UniValue ret(UniValue::VOBJ);
    UniValue items(UniValue::VARR);

    // Calculate total balance a different way from GetBalance()
    // (GetBalance() sums up all unspent TxOuts)
    int64_t nBalance = 0;
    int64_t totalFee = 0;

    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
    {
        const CWalletTx& wtx = it->second;
        if (!wtx.IsTrusted())
            continue;

        int64_t allFee;
        string strSentAccount;
        list<COutputEntry> listReceived;
        list<COutputEntry> listSent;

        wtx.GetAmounts(listReceived, listSent, allFee, strSentAccount, filter);

        uint256 txid = wtx.GetHash();

        if (wtx.GetDepthInMainChain() >= nMinDepth && wtx.GetBlocksToMaturity() == 0)
        {
            for (auto const& r : listReceived)
            {
                std::string addressStr;

                CBitcoinAddress addr;
                addressStr = addr.Set(r.destination) ? addr.ToString() : std::string {};

                nBalance += r.amount;

                UniValue item(UniValue::VOBJ);

                item.pushKV("timestamp", (int64_t) wtx.nTime);
                item.pushKV("txid", txid.ToString());
                item.pushKV("address", addressStr);
                item.pushKV("amount", ValueFromAmount(r.amount));

                items.push_back(item);
            }
        }

        for (auto const& s : listSent)
        {
            std::string addressStr;

            CBitcoinAddress addr;
            addressStr = addr.Set(s.destination) ? addr.ToString() : std::string {};

            nBalance -= s.amount;

            UniValue item(UniValue::VOBJ);

            item.pushKV("timestamp", (int64_t) wtx.nTime);
            item.pushKV("txid", txid.ToString());
            item.pushKV("address", addressStr);
            item.pushKV("amount", ValueFromAmount(-s.amount));

            items.push_back(item);
        }

        nBalance -= allFee;
        totalFee += allFee;

        UniValue item(UniValue::VOBJ);

        if (allFee)
        {
            item.pushKV("timestamp", (int64_t) wtx.nTime);
            item.pushKV("txid", txid.ToString());
            item.pushKV("address", std::string {});
            item.pushKV("fee", ValueFromAmount(allFee));

            items.push_back(item);
        }
    }

    ret.pushKV("balance", ValueFromAmount(nBalance));
    ret.pushKV("fees", ValueFromAmount(totalFee));
    ret.pushKV("list", items);

    return ret;
}


UniValue getunconfirmedbalance(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error("getunconfirmedbalance\n"
                            "\n"
                            "returns the unconfirmed balance in the wallet\n");

    return ValueFromAmount(pwalletMain->GetUnconfirmedBalance());
}



UniValue movecmd(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 3 || params.size() > 5)
        throw runtime_error(
                "move <fromaccount> <toaccount> <amount> [minconf=1] [comment]\n"
                "\n"
                "Move from one account in your wallet to another.\n");

    accountingDeprecationCheck();

    string strFrom = AccountFromValue(params[0]);
    string strTo = AccountFromValue(params[1]);
    int64_t nAmount = AmountFromValue(params[2]);

    if (params.size() > 3)
        // unused parameter, used to be nMinDepth, keep type-checking it though
        (void)params[3].get_int();
    string strComment;
    if (params.size() > 4)
        strComment = params[4].get_str();

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CWalletDB walletdb(pwalletMain->strWalletFile);
    if (!walletdb.TxnBegin())
        throw JSONRPCError(RPC_DATABASE_ERROR, "database error");

    int64_t nNow = GetAdjustedTime();

    // Debit
    CAccountingEntry debit;
    debit.nOrderPos = pwalletMain->IncOrderPosNext(&walletdb);
    debit.strAccount = strFrom;
    debit.nCreditDebit = -nAmount;
    debit.nTime = nNow;
    debit.strOtherAccount = strTo;
    debit.strComment = strComment;
    walletdb.WriteAccountingEntry(debit);

    // Credit
    CAccountingEntry credit;
    credit.nOrderPos = pwalletMain->IncOrderPosNext(&walletdb);
    credit.strAccount = strTo;
    credit.nCreditDebit = nAmount;
    credit.nTime = nNow;
    credit.strOtherAccount = strFrom;
    credit.strComment = strComment;
    walletdb.WriteAccountingEntry(credit);

    if (!walletdb.TxnCommit())
        throw JSONRPCError(RPC_DATABASE_ERROR, "database error");

    return true;
}


UniValue sendfrom(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 3 || params.size() > 7)
        throw runtime_error(
                "sendfrom <account> <gridcoinaddress> <amount> [minconf=1] [comment] [comment-to] [message]\n"
                "\n"
                "<account> account to send from.\n"
                "<gridcoinaddress> address to send to.\n"
                "<amount> is a real and is rounded to the nearest 0.000001\n"
                "[minconf] only use the balance confirmed at least this many times."
                "[comment] a comment used to store what the transaction is for.\n"
                "         This is not part of the transaction, just kept in your wallet.\n"
                "[comment_to] a comment to store the name of the person or organization\n"
                "             to which you're sending the transaction. This is not part of the \n"
                "             transaction, just kept in your wallet.\n"
                "[message] Optional message to add to the receiver.\n"
                + HelpRequiringPassphrase());

    string strAccount = AccountFromValue(params[0]);

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CBitcoinAddress address(params[1].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Gridcoin address");
    int64_t nAmount = AmountFromValue(params[2]);

    int nMinDepth = 1;
    if (params.size() > 3)
        nMinDepth = params[3].get_int();

    CWalletTx wtx;
    wtx.strFromAccount = strAccount;
    if (params.size() > 4 && !params[4].isNull() && !params[4].get_str().empty())
        wtx.mapValue["comment"] = params[4].get_str();
    if (params.size() > 5 && !params[5].isNull() && !params[5].get_str().empty())
        wtx.mapValue["to"]      = params[5].get_str();
    if (params.size() > 6 && !params[6].isNull() && !params[6].get_str().empty())
        wtx.vContracts.emplace_back(
            GRC::MakeContract<GRC::TxMessage>(GRC::ContractAction::ADD, params[6].get_str()));

    EnsureWalletIsUnlocked();

    // Check funds
    int64_t nBalance = GetAccountBalance(strAccount, nMinDepth);
    if (nAmount > nBalance)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Account has insufficient funds");

    // Send
    string strError = pwalletMain->SendMoneyToDestination(address.Get(), nAmount, wtx);
    if (!strError.empty())
        throw JSONRPCError(RPC_WALLET_ERROR, strError);

    return wtx.GetHash().GetHex();
}

UniValue sendmany(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 4)
        throw runtime_error(
                "sendmany <fromaccount> {address:amount,...} [minconf=1] [comment]\n"
                "\n"
                "<fromaccount> Specify account name to use or use '' to use all addresses in wallet\n"
                "\n"
                "amounts are double-precision floating point numbers\n"
                + HelpRequiringPassphrase());

    string strAccount = AccountFromValue(params[0]);
    bool bFromAccount = false;

    if (!strAccount.empty())
        bFromAccount = true;

    UniValue sendTo = params[1].get_obj();
    int nMinDepth = 1;
    if (params.size() > 2)
        nMinDepth = params[2].get_int();

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CWalletTx wtx;

    if (bFromAccount)
        wtx.strFromAccount = strAccount;

    if (params.size() > 3 && !params[3].isNull() && !params[3].get_str().empty())
        wtx.mapValue["comment"] = params[3].get_str();

    set<CBitcoinAddress> setAddress;
    vector<pair<CScript, int64_t> > vecSend;
    vector<string> addrList = sendTo.getKeys();

    int64_t totalAmount = 0;
    for (auto const& name_: addrList)
    {
        CBitcoinAddress address(name_);
        if (!address.IsValid())
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, string("Invalid Gridcoin address: ")+name_);

        if (setAddress.count(address))
            throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, duplicated address: ")+name_);
        setAddress.insert(address);

        CScript scriptPubKey;
        scriptPubKey.SetDestination(address.Get());
        int64_t nAmount = AmountFromValue(sendTo[name_]);

        totalAmount += nAmount;

        vecSend.push_back(make_pair(scriptPubKey, nAmount));
    }

    EnsureWalletIsUnlocked();

    // Check funds & Support non-account sendmany
    int64_t nBalance = 0;

    if (bFromAccount)
        nBalance = GetAccountBalance(strAccount, nMinDepth);

    else
    {
        isminefilter filter = ISMINE_SPENDABLE;

        for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
        {
            const CWalletTx& wtx = (*it).second;
            if (!wtx.IsTrusted())
                continue;

            int64_t allFee;
            string strSentAccount;
            list<COutputEntry> listReceived;
            list<COutputEntry> listSent;
            wtx.GetAmounts(listReceived, listSent, allFee, strSentAccount, filter);
            if (wtx.GetDepthInMainChain() >= nMinDepth && wtx.GetBlocksToMaturity() == 0)
            {
                for (auto const& r : listReceived)
                    nBalance += r.amount;
            }
            for (auto const& r : listSent)
                nBalance -= r.amount;
            nBalance -= allFee;
        }
    }

    if (totalAmount > nBalance)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Account has insufficient funds");

    // Send
    CReserveKey keyChange(pwalletMain);
    int64_t nFeeRequired = 0;
    bool fCreated = pwalletMain->CreateTransaction(vecSend, wtx, keyChange, nFeeRequired);
    if (!fCreated)
    {
        if (totalAmount + nFeeRequired > pwalletMain->GetBalance())
            throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient funds");
        throw JSONRPCError(RPC_WALLET_ERROR, "Transaction creation failed");
    }
    if (!pwalletMain->CommitTransaction(wtx, keyChange))
        throw JSONRPCError(RPC_WALLET_ERROR, "Transaction commit failed");

    return wtx.GetHash().GetHex();
}

UniValue addmultisigaddress(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 3)
    {
        string msg = "addmultisigaddress <nrequired> <'[\"key\",\"key\"]'> [account]\n"
                     "\n"
                     "Add a nrequired-to-sign multisignature address to the wallet\n"
                     "each key is a Gridcoin address or hex-encoded public key\n"
                     "If [account] is specified, assign address to [account].\n";
        throw runtime_error(msg);
    }

    int nRequired = params[0].get_int();
    const UniValue& keys = params[1].get_array();
    string strAccount;
    if (params.size() > 2)
        strAccount = AccountFromValue(params[2]);

    // Gather public keys
    if (nRequired < 1)
        throw runtime_error("a multisignature address must require at least one key to redeem");
    if ((int)keys.size() < nRequired)
        throw runtime_error(
            strprintf("not enough keys supplied "
                      "(got %" PRIszu " keys, but need at least %d to redeem)", keys.size(), nRequired));

    if (keys.size() > 16)       throw runtime_error("Number of addresses involved in the multisignature address creation > 16\nReduce the number");

    std::vector<CKey> pubkeys;
    pubkeys.resize(keys.size());

    LOCK2(cs_main, pwalletMain->cs_wallet);

    for (unsigned int i = 0; i < keys.size(); i++)
    {
        const std::string& ks = keys[i].get_str();

        // Case 1: Bitcoin address and we have full public key:
        CBitcoinAddress address(ks);
        if (address.IsValid())
        {
            CKeyID keyID;
            if (!address.GetKeyID(keyID))
                throw runtime_error(
                    strprintf("%s does not refer to a key",ks));
            CPubKey vchPubKey;
            if (!pwalletMain->GetPubKey(keyID, vchPubKey))
                throw runtime_error(
                    strprintf("no full public key for address %s",ks));
            if (!vchPubKey.IsValid() || !pubkeys[i].SetPubKey(vchPubKey))
                throw runtime_error(" Invalid public key: "+ks);
        }

        // Case 2: hex public key
        else if (IsHex(ks))
        {
            CPubKey vchPubKey(ParseHex(ks));
            if (!vchPubKey.IsValid() || !pubkeys[i].SetPubKey(vchPubKey))
                throw runtime_error(" Invalid public key: "+ks);
        }
        else
        {
            throw runtime_error(" Invalid public key: "+ks);
        }
    }

    // Construct using pay-to-script-hash:
    CScript inner;
    inner.SetMultisig(nRequired, pubkeys);
    CScriptID innerID = inner.GetID();
    if (!pwalletMain->AddCScript(inner))
        throw runtime_error("AddCScript() failed");

    pwalletMain->SetAddressBookName(innerID, strAccount);
    return CBitcoinAddress(innerID).ToString();
}

UniValue addredeemscript(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
    {
        string msg = "addredeemscript <redeemScript> [account]\n"
                     "\n"
                     "Add a P2SH address with a specified redeemScript to the wallet.\n"
                     "If [account] is specified, assign address to [account].\n";
        throw runtime_error(msg);
    }

    string strAccount;
    if (params.size() > 1)
        strAccount = AccountFromValue(params[1]);

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // Construct using pay-to-script-hash:
    vector<unsigned char> innerData = ParseHexV(params[0], "redeemScript");
    CScript inner(innerData.begin(), innerData.end());
    CScriptID innerID = inner.GetID();
    if (!pwalletMain->AddCScript(inner))
        throw runtime_error("AddCScript() failed");

    pwalletMain->SetAddressBookName(innerID, strAccount);
    return CBitcoinAddress(innerID).ToString();
}

struct tallyitem
{
    int64_t nAmount;
    int nConf;
    vector<uint256> txids;
    bool fIsWatchonly;

    tallyitem()
    {
        nAmount = 0;
        nConf = std::numeric_limits<int>::max();
        fIsWatchonly = false;
    }
};

UniValue ListReceived(const UniValue& params, bool fByAccounts)
{
    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 0)
        nMinDepth = params[0].get_int();

    // Whether to include empty accounts
    bool fIncludeEmpty = false;
    isminefilter filter = ISMINE_SPENDABLE;
    if(params.size() > 2)
         if(params[2].get_bool())
             filter = filter | ISMINE_WATCH_ONLY;

    if (params.size() > 1)
        fIncludeEmpty = params[1].get_bool();

    // Tally
    map<CBitcoinAddress, tallyitem> mapTally;
    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
    {
        const CWalletTx& wtx = (*it).second;

        if (wtx.IsCoinBase() || wtx.IsCoinStake() || !IsFinalTx(wtx))
            continue;

        int nDepth = wtx.GetDepthInMainChain();
        if (nDepth < nMinDepth)
            continue;
        for (auto const& txout : wtx.vout)
        {
            CTxDestination address;
            // if (!ExtractDestination(txout.scriptPubKey, address) || !IsMine(*pwalletMain, address))                continue;
            if (!ExtractDestination(txout.scriptPubKey, address))
                 continue;

            isminefilter mine = IsMine(*pwalletMain, address);
            if( (!mine) & filter)
                  continue;

            tallyitem& item = mapTally[address];
            item.nAmount += txout.nValue;
            item.nConf = min(item.nConf, nDepth);
            item.txids.push_back(wtx.GetHash());
            if (mine & ISMINE_WATCH_ONLY)
              item.fIsWatchonly = true;

        }
    }

    // Reply
    UniValue ret(UniValue::VARR);
    map<string, tallyitem> mapAccountTally;

    for (auto const& item : pwalletMain->mapAddressBook)
    {
        const CBitcoinAddress& address = item.first;
        const string& strAccount = item.second;
        map<CBitcoinAddress, tallyitem>::iterator it = mapTally.find(address);
        if (it == mapTally.end() && !fIncludeEmpty)
            continue;

        int64_t nAmount = 0;

        int nConf = std::numeric_limits<int>::max();
        bool fIsWatchonly = false;
        if (it != mapTally.end())
        {
             nAmount = (*it).second.nAmount;
             nConf = (*it).second.nConf;
             fIsWatchonly = (*it).second.fIsWatchonly;
        }

        if (fByAccounts)
        {
            tallyitem& item = mapAccountTally[strAccount];
            item.nAmount += nAmount;
            item.nConf = min(item.nConf, nConf);
            item.fIsWatchonly = fIsWatchonly;
        }
        else
        {
            UniValue obj(UniValue::VOBJ);
            if(fIsWatchonly)
                  obj.pushKV("involvesWatchonly", true);
            obj.pushKV("address",       address.ToString());
            obj.pushKV("account",       strAccount);
            obj.pushKV("amount",        ValueFromAmount(nAmount));
            obj.pushKV("confirmations", (nConf == std::numeric_limits<int>::max() ? 0 : nConf));

            UniValue transactions(UniValue::VARR);
            if(it != mapTally.end())
            {
                for (const uint256& _item : (*it).second.txids)
                {
                    transactions.push_back(_item.GetHex());
                }
			}
            obj.pushKV("txids", transactions);
            ret.push_back(obj);
        }
    }

    if (fByAccounts)
    {
        for (map<string, tallyitem>::iterator it = mapAccountTally.begin(); it != mapAccountTally.end(); ++it)
        {
            int64_t nAmount = (*it).second.nAmount;
            int nConf = (*it).second.nConf;
            UniValue obj(UniValue::VOBJ);
            if((*it).second.fIsWatchonly)
                 obj.pushKV("involvesWatchonly", true);
            obj.pushKV("account",       (*it).first);
            obj.pushKV("amount",        ValueFromAmount(nAmount));
            obj.pushKV("confirmations", (nConf == std::numeric_limits<int>::max() ? 0 : nConf));
            ret.push_back(obj);
        }
    }

    return ret;
}

UniValue listreceivedbyaddress(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 3)
        throw runtime_error(
                "listreceivedbyaddress ( minconf includeempty includeWatchonly)\n"
                "\nList balances by receiving address.\n"
                "\nArguments:\n"
                "1. minconf       (numeric, optional, default=1) The minimum number of confirmations before payments are included.\n"
                "2. includeempty  (numeric, optional, default=false) Whether to include addresses that haven't received any payments.\n"
                "3. includeWatchonly (bool, optional, default=false) Whether to include watchonly addresses (see 'importaddress').\n"
                "\nResult:\n"
                "[\n"
                "  {\n"
                "    \"involvesWatchonly\" : \"true\",    (bool) Only returned if imported addresses were involved in transaction\n"
                "    \"address\" : \"receivingaddress\",  (string) The receiving address\n"
                "    \"account\" : \"accountname\",       (string) The account of the receiving address. The default account is \"\".\n"
                "    \"amount\" : x.xxx,                  (numeric) The total amount in btc received by the address\n"
                "\nExamples:\n"
                );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    return ListReceived(params, false);
}

UniValue listreceivedbyaccount(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 3)
        throw runtime_error(
                "listreceivedbyaccount ( minconf includeempty includeWatchonly)\n"
                "\nList balances by account.\n"
                "\nArguments:\n"
                "1. minconf      (numeric, optional, default=1) The minimum number of confirmations before payments are included.\n"
                "2. includeempty (boolean, optional, default=false) Whether to include accounts that haven't received any payments.\n"
                "3. includeWatchonly (bool, optional, default=false) Whether to include watchonly addresses (see 'importaddress').\n"
                "\nResult:\n"
                "[\n"
                "  {\n"
                "    \"involvesWatchonly\" : \"true\",    (bool) Only returned if imported addresses were involved in transaction\n"
                "    \"account\" : \"accountname\",  (string) The account name of the receiving account\n"
                "    \"amount\" : x.xxx,             (numeric) The total amount received by addresses with this account\n"
                "    \"confirmations\" : n           (numeric) The number of confirmations of the most recent transaction included\n"
                );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    accountingDeprecationCheck();

    return ListReceived(params, true);
}

 void ListTransactions(const CWalletTx& wtx, const string& strAccount, int nMinDepth,
                       bool fLong, UniValue& ret, const isminefilter& filter = ISMINE_SPENDABLE,
                       bool stakes_only = false)
 {
    int64_t nFee;
    string strSentAccount;
    list<COutputEntry> listReceived;
    list<COutputEntry> listSent;

    wtx.GetAmounts(listReceived, listSent, nFee, strSentAccount, filter);

    bool fAllAccounts = (strAccount == string("*") || strAccount.empty());
    bool involvesWatchonly = wtx.IsFromMe(ISMINE_WATCH_ONLY);

    // List: Sent
    if ((!listSent.empty() || nFee != 0) && (fAllAccounts || strAccount == strSentAccount) && !stakes_only)
    {
        for (auto const& s : listSent)
        {
            UniValue entry(UniValue::VOBJ);
            if(involvesWatchonly || (::IsMine(*pwalletMain, s.destination) & ISMINE_WATCH_ONLY))
                            entry.pushKV("involvesWatchonly", true);
            entry.pushKV("account", strSentAccount);

            std::string addressStr;

            CBitcoinAddress addr;
            addressStr = addr.Set(s.destination) ? addr.ToString() : std::string {};

            entry.pushKV("address", addressStr);

            if (wtx.IsCoinBase() || wtx.IsCoinStake())
            {
                if (wtx.GetDepthInMainChain() < 1)
                    entry.pushKV("category", "orphan");
                else if (wtx.GetBlocksToMaturity() > 0)
                    entry.pushKV("category", "immature");
                else
                    entry.pushKV("category", "generate");

                MinedType gentype = GetGeneratedType(pwalletMain, wtx.GetHash(), s.vout);

                switch (gentype)
                {
                    case MinedType::POR                 :    entry.pushKV("Type", "POR");                     break;
                    case MinedType::POS                 :    entry.pushKV("Type", "POS");                     break;
                    case MinedType::ORPHANED            :    entry.pushKV("Type", "ORPHANED");                break;
                    case MinedType::POS_SIDE_STAKE_RCV  :    entry.pushKV("Type", "POS SIDE STAKE RECEIVED"); break;
                    case MinedType::POR_SIDE_STAKE_RCV  :    entry.pushKV("Type", "POR SIDE STAKE RECEIVED"); break;
                    case MinedType::POS_SIDE_STAKE_SEND :    entry.pushKV("Type", "POS SIDE STAKE SENT");     break;
                    case MinedType::POR_SIDE_STAKE_SEND :    entry.pushKV("Type", "POR SIDE STAKE SENT");     break;
                    default                             :    entry.pushKV("Type", "UNKNOWN");                 break;
                }
            }
            else
            {
                entry.pushKV("category", "send");
                entry.pushKV("fee", ValueFromAmount(-nFee));
            }

            entry.pushKV("amount", ValueFromAmount(-s.amount));
            //  entry.pushKV("vout", s.vout);
            if (fLong) WalletTxToJSON(wtx, entry);
            ret.push_back(entry);
        }
    }

    // List: Received
    if (listReceived.size() > 0 && wtx.GetDepthInMainChain() >= nMinDepth)
    {
        for (auto const& r : listReceived)
        {
            string account;

            if (pwalletMain->mapAddressBook.count(r.destination))
                account = pwalletMain->mapAddressBook[r.destination];

            if (fAllAccounts || (account == strAccount))
            {
                UniValue entry(UniValue::VOBJ);
                if(involvesWatchonly || (::IsMine(*pwalletMain, r.destination) & ISMINE_WATCH_ONLY))
                          entry.pushKV("involvesWatchonly", true);
                entry.pushKV("account", account);

                std::string addressStr;

                CBitcoinAddress addr;
                addressStr = addr.Set(r.destination) ? addr.ToString() : std::string {};

                entry.pushKV("address", addressStr);

                if (wtx.IsCoinBase() || wtx.IsCoinStake())
                {
                    if (wtx.GetDepthInMainChain() < 1)
                        entry.pushKV("category", "orphan");
                    else if (wtx.GetBlocksToMaturity() > 0)
                        entry.pushKV("category", "immature");
                    else
                        entry.pushKV("category", "generate");

                    MinedType gentype = GetGeneratedType(pwalletMain, wtx.GetHash(), r.vout);

                    switch (gentype)
                    {
                        case MinedType::POR                 :    entry.pushKV("Type", "POR");                     break;
                        case MinedType::POS                 :    entry.pushKV("Type", "POS");                     break;
                        case MinedType::ORPHANED            :    entry.pushKV("Type", "ORPHANED");                break;
                        case MinedType::POS_SIDE_STAKE_RCV  :    entry.pushKV("Type", "POS SIDE STAKE RECEIVED"); break;
                        case MinedType::POR_SIDE_STAKE_RCV  :    entry.pushKV("Type", "POR SIDE STAKE RECEIVED"); break;
                        case MinedType::POS_SIDE_STAKE_SEND :    entry.pushKV("Type", "POS SIDE STAKE SENT");     break;
                        case MinedType::POR_SIDE_STAKE_SEND :    entry.pushKV("Type", "POR SIDE STAKE SENT");     break;
                        default                             :    entry.pushKV("Type", "UNKNOWN");                 break;
                    }

                    // Skip posting this entry if stakes only is desired and not an actual stake.
                    if (stakes_only && gentype != MinedType::POR && gentype != MinedType::POS) continue;
                }
                else
                {
                    // Skip posting this entry for non-stake receives.
                    if (stakes_only) continue;

                    entry.pushKV("category", "receive");
                }
                entry.pushKV("fee", ValueFromAmount(-nFee));
                entry.pushKV("amount", ValueFromAmount(r.amount));
                if (fLong) WalletTxToJSON(wtx, entry);
                ret.push_back(entry);
            }
        }
    }
}

void AcentryToJSON(const CAccountingEntry& acentry, const string& strAccount, UniValue& ret)
{
    bool fAllAccounts = (strAccount == string("*"));

    if (fAllAccounts || acentry.strAccount == strAccount)
    {
        UniValue entry(UniValue::VOBJ);
        entry.pushKV("account", acentry.strAccount);
        entry.pushKV("category", "move");
        entry.pushKV("time", acentry.nTime);
        entry.pushKV("amount", ValueFromAmount(acentry.nCreditDebit));
        entry.pushKV("otheraccount", acentry.strOtherAccount);
        entry.pushKV("comment", acentry.strComment);
        ret.push_back(entry);
    }
}

UniValue listtransactions(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 4)
        throw runtime_error(
                "listtransactions ( \"account\" count from includeWatchonly)\n"
                "\nReturns up to 'count' most recent transactions skipping the first 'from' transactions for account 'account'.\n"
                "\nArguments:\n"
                "1. \"account\"    (string, optional) The account name. If not included, it will list all transactions for all accounts.\n"
                "                                     If \"\" is set, it will list transactions for the default account.\n"
                "2. count          (numeric, optional, default=10) The number of transactions to return\n"
                "3. from           (numeric, optional, default=0) The number of transactions to skip\n"
                "4. includeWatchonly (bool, optional, default=false) Include transactions to watchonly addresses (see 'importaddress')\n"
                "                                     If \"\" is set true, it will list sent transactions as well\n"
                "\nResult:\n"
                "[\n"
                "  {\n"
                "    \"account\":\"accountname\",       (string) The account name associated with the transaction. \n"
                "                                                It will be \"\" for the default account.\n"
                "    \"address\":\"bitcoinaddress\",    (string) The bitcoin address of the transaction. Not present for \n"
                "                                                move transactions (category = move).\n"
                "    \"category\":\"send|receive|move\", (string) The transaction category. 'move' is a local (off blockchain)\n"
                "                                                transaction between accounts, and not associated with an address,\n"
                "                                                transaction id or block. 'send' and 'receive' transactions are \n"
                "                                                associated with an address, transaction id and block details\n"
                "    \"amount\": x.xxx,          (numeric) The amount in btc. This is negative for the 'send' category, and for the\n"
                "                                         'move' category for moves outbound. It is positive for the 'receive' category,\n"
                "                                         and for the 'move' category for inbound funds.\n"
                "    \"fee\": x.xxx,             (numeric) The amount of the fee in btc. This is negative and only available for the \n"
                "                                         'send' category of transactions.\n"
                "    \"confirmations\": n,       (numeric) The number of confirmations for the transaction. Available for 'send' and \n"
                "                                         'receive' category of transactions.\n"
                "    \"blockhash\": \"hashvalue\", (string) The block hash containing the transaction. Available for 'send' and 'receive'\n"
                "                                          category of transactions.\n"
                "    \"blockindex\": n,          (numeric) The block index containing the transaction. Available for 'send' and 'receive'\n"
                "                                          category of transactions.\n"
                "    \"txid\": \"transactionid\", (string) The transaction id. Available for 'send' and 'receive' category of transactions.\n"
                "    \"walletconflicts\" : [\n"
                "        \"conflictid\",  (string) Ids of transactions, including equivalent clones, that re-spend a txid input.\n"
                "    ],\n"
                "    \"respendsobserved\" : [\n"
                "        \"respendid\",  (string) Ids of transactions, NOT equivalent clones, that re-spend a txid input. \"Double-spends.\"\n"
                "    ],\n"
                "    \"time\": xxx,              (numeric) The transaction time in seconds since epoch (midnight Jan 1 1970 GMT).\n"
                "    \"timereceived\": xxx,      (numeric) The time received in seconds since epoch (midnight Jan 1 1970 GMT). Available \n"
                "                                          for 'send' and 'receive' category of transactions.\n"
                "    \"comment\": \"...\",       (string) If a comment is associated with the transaction.\n"
                "    \"otheraccount\": \"accountname\",  (string) For the 'move' category of transactions, the account the funds came \n"
                "                                          from (for receiving funds, positive amounts), or went to (for sending funds,\n"
                "                                          negative amounts).\n"
                "  }\n"
                "]\n"

                "\nExamples:\n"
                "\nList the most recent 10 transactions in the systems\n"
                "\nList the most recent 10 transactions for the tabby account\n"
                "\nList transactions 100 to 120 from the tabby account\n"
                "\nAs a json rpc call\n"
                );

    string strAccount = "*";
    int nCount = 10;
    int nFrom = 0;
    isminefilter filter = ISMINE_SPENDABLE;
    if (params.size() > 0)
    {
        strAccount = params[0].get_str();
        if (params.size() > 1)
        {
            nCount = params[1].get_int();
            if (params.size() > 2)
            {
                nFrom = params[2].get_int();
                if(params.size() > 3)
                {
                    if(params[3].get_bool())
                        filter = filter | ISMINE_WATCH_ONLY;
                }
            }
        }
    }
    if (nCount < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative count");
    if (nFrom < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative from");

    UniValue ret(UniValue::VARR);

    LOCK2(cs_main, pwalletMain->cs_wallet);

    std::list<CAccountingEntry> acentries;
    CWallet::TxItems txOrdered = pwalletMain->OrderedTxItems(acentries, strAccount);

    // iterate backwards until we have nCount items to return:
    for (CWallet::TxItems::reverse_iterator it = txOrdered.rbegin(); it != txOrdered.rend(); ++it)
    {
        CWalletTx *const pwtx = (*it).second.first;
        if (pwtx != 0)
            ListTransactions(*pwtx, strAccount, 0, true, ret, filter);
        CAccountingEntry *const pacentry = (*it).second.second;
        if (pacentry != 0)
            AcentryToJSON(*pacentry, strAccount, ret);

        if ((int)ret.size() >= (nCount+nFrom)) break;
    }
    // ret is newest to oldest

    if (nFrom > (int)ret.size())
        nFrom = ret.size();
    if ((nFrom + nCount) > (int)ret.size())
        nCount = ret.size() - nFrom;

    std::vector<UniValue> arrTmp = ret.getValues();

    std::vector<UniValue>::iterator first = arrTmp.begin();
    std::advance(first, nFrom);
    std::vector<UniValue>::iterator last = arrTmp.begin();
    std::advance(last, nFrom+nCount);

    if (last != arrTmp.end()) arrTmp.erase(last, arrTmp.end());
    if (first != arrTmp.begin()) arrTmp.erase(arrTmp.begin(), first);

    std::reverse(arrTmp.begin(), arrTmp.end()); // Return oldest to newest
    ret.clear();
    ret.setArray();
    ret.push_backV(arrTmp);

    return ret;
}

UniValue liststakes(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
                "liststakes ( count )\n"
                "\n"
                "Returns count most recent stakes."
                );

    string strAccount = "*";
    int nCount = 10;
    isminefilter filter = ISMINE_SPENDABLE;
    if (params.size() > 0)
    {
        nCount = params[0].get_int();
    }

    if (nCount < 0)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative count");
    }

    UniValue ret_superset(UniValue::VARR);
    UniValue ret(UniValue::VARR);

    LOCK2(cs_main, pwalletMain->cs_wallet);

    std::list<CAccountingEntry> acentries;
    CWallet::TxItems txOrdered = pwalletMain->OrderedTxItems(acentries, strAccount);

    // iterate backwards until we have at least nCount items to return:
    for (CWallet::TxItems::reverse_iterator it = txOrdered.rbegin(); it != txOrdered.rend(); ++it)
    {
        CWalletTx *const pwtx = it->second.first;
        if (pwtx != 0)
            ListTransactions(*pwtx, strAccount, 0, true, ret_superset, filter, true);
        CAccountingEntry *const pacentry = it->second.second;
        if (pacentry != 0)
            AcentryToJSON(*pacentry, strAccount, ret_superset);

        if ((int)ret_superset.size() >= nCount) break;
    }
    // ret is newest to oldest, for the stake listings, we will leave in that order.
    std::vector<UniValue> arrTmp = ret_superset.getValues();

    for (const auto& iter : arrTmp)
    {
        ret.push_back(iter);
    }

    return ret;
}

UniValue listaccounts(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
                "listaccounts ( minconf includeWatchonly)\n"
                "\n"
                "Returns UniValue that has account names as keys, account balances as values."
                "1. minconf          (numeric, optional, default=1) Only onclude transactions with at least this many confirmations\n"
                "2. includeWatchonly (bool, optional, default=false) Include balances in watchonly addresses (see 'importaddress')\n"
                "\nResult:\n"
                "{                      (json object where keys are account names, and values are numeric balances\n"
                "  \"account\": x.xxx,  (numeric) The property name is the account name, and the value is the total balance for the account.\n"
                );

    accountingDeprecationCheck();

    int nMinDepth = 1;
    isminefilter includeWatchonly = ISMINE_SPENDABLE;
    if (params.size() > 0)
    {
         nMinDepth = params[0].get_int();
         if(params.size() > 1)
             if(params[1].get_bool())
                 includeWatchonly = includeWatchonly | ISMINE_WATCH_ONLY;
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);

    map<string, int64_t> mapAccountBalances;
    for (auto const& entry : pwalletMain->mapAddressBook) {
       if (IsMine(*pwalletMain, entry.first) & includeWatchonly) // This address belongs to me
            mapAccountBalances[entry.second] = 0;
    }

    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
    {
        const CWalletTx& wtx = (*it).second;
        int64_t nFee;
        string strSentAccount;
        list<COutputEntry> listReceived;
        list<COutputEntry> listSent;
        int nDepth = wtx.GetDepthInMainChain();
        if (nDepth < 0)
            continue;
        wtx.GetAmounts(listReceived, listSent, nFee, strSentAccount, includeWatchonly);
        mapAccountBalances[strSentAccount] -= nFee;
        for (auto const& s : listSent)
            mapAccountBalances[strSentAccount] -= s.amount;
        if (nDepth >= nMinDepth && wtx.GetBlocksToMaturity() == 0)
        {
            for (auto const& r : listReceived)
                if (pwalletMain->mapAddressBook.count(r.destination))
                    mapAccountBalances[pwalletMain->mapAddressBook[r.destination]] += r.amount;
                else
                    mapAccountBalances[""] += r.amount;
        }
    }

    list<CAccountingEntry> acentries;
    CWalletDB(pwalletMain->strWalletFile).ListAccountCreditDebit("*", acentries);
    for (auto const& entry : acentries)
        mapAccountBalances[entry.strAccount] += entry.nCreditDebit;

    UniValue ret(UniValue::VOBJ);
    for (auto const& accountBalance : mapAccountBalances) {
        ret.pushKV(accountBalance.first, ValueFromAmount(accountBalance.second));
    }
    return ret;
}

UniValue listsinceblock(const UniValue& params, bool fHelp)
{
    if (fHelp)
        throw runtime_error(
                "listsinceblock ( \"blockhash\" target-confirmations includeWatchonly)\n"
                "\nGet all transactions in blocks since block [blockhash], or all transactions if omitted\n"
                "\nArguments:\n"
                "1. \"blockhash\"   (string, optional) The block hash to list transactions since\n"
                "2. target-confirmations:    (numeric, optional) The confirmations required, must be 1 or more\n"
                "3. includeWatchonly:        (bool, optional, default=false) Include transactions to watchonly addresses (see 'importaddress')"
                "\nResult:\n"
                "{\n"
                "  \"transactions\": [\n"
                );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CBlockIndex *pindex = NULL;
    int target_confirms = 1;
    isminefilter filter = ISMINE_SPENDABLE;
    if (params.size() > 0)
    {
        uint256 blockId;

        blockId.SetHex(params[0].get_str());
        BlockMap::iterator it = mapBlockIndex.find(blockId);
        if (it != mapBlockIndex.end())
            pindex = it->second;

        if (params.size() > 1)
        {
            target_confirms = params[1].get_int();

            if (target_confirms < 1)
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter");

            if(params.size() > 2)
            {
                if(params[2].get_bool())
                    filter = filter | ISMINE_WATCH_ONLY;
            }
        }
    }

    int depth = pindex ? (1 + nBestHeight - pindex->nHeight) : -1;
    UniValue transactions(UniValue::VARR);

    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); it++)
    {
        CWalletTx tx = (*it).second;

        if (depth == -1 || tx.GetDepthInMainChain() < depth)
            ListTransactions(tx, "*", 0, true, transactions, filter);
    }

    int target_height = pindexBest->nHeight + 1 - target_confirms;
    CBlockIndex *block;
        for (block = pindexBest;
             block && block->nHeight > target_height;
             block = block->pprev)  { }
    uint256 lastblock = block ? block->GetBlockHash() : uint256();

    UniValue ret(UniValue::VOBJ);
    ret.pushKV("transactions", transactions);
    ret.pushKV("lastblock", lastblock.GetHex());

    return ret;

}

UniValue gettransaction(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
                "gettransaction \"txid\" ( includeWatchonly )\n"
                "\nGet detailed information about in-wallet transaction <txid>\n"
                "\nArguments:\n"
                "1. \"txid\"    (string, required) The transaction id\n"
                "2. \"includeWatchonly\"    (bool, optional, default=false) Whether to include watchonly addresses in balance calculation and details[]\n"
                "\nResult:\n"
                "{\n"
                "  \"amount\" : x.xxx,        (numeric) The transaction amount in grc\n"
                );

    uint256 hash;
    hash.SetHex(params[0].get_str());
    isminefilter filter = ISMINE_SPENDABLE;
    if(params.size() > 1)
         if(params[1].get_bool())
             filter = filter | ISMINE_WATCH_ONLY;
    UniValue entry(UniValue::VOBJ);

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (pwalletMain->mapWallet.count(hash))
    {
        const CWalletTx& wtx = pwalletMain->mapWallet[hash];

        TxToJSON(wtx, uint256(), entry);

        int64_t nCredit = wtx.GetCredit();
        int64_t nDebit = wtx.GetDebit();
        int64_t nNet = nCredit - nDebit;

        bool IsFee = wtx.IsFromMe() && !wtx.IsCoinBase() && !wtx.IsCoinStake();

        int64_t nFee = (IsFee ? wtx.GetValueOut() - nDebit : 0);

        if (IsFee)
            entry.pushKV("fee", ValueFromAmount(nFee));

        entry.pushKV("amount", ValueFromAmount(nNet - nFee));

        WalletTxToJSON(wtx, entry);

        UniValue details(UniValue::VARR);
        ListTransactions(pwalletMain->mapWallet[hash], "*", 0, false, details, filter);
        entry.pushKV("details", details);
    }
    else
    {
        CTransaction tx;
        uint256 hashBlock;
        if (GetTransaction(hash, tx, hashBlock))
        {
            TxToJSON(tx, uint256(), entry);
            if (hashBlock.IsNull())
                entry.pushKV("confirmations", 0);
            else
            {
                entry.pushKV("blockhash", hashBlock.GetHex());
                BlockMap::iterator mi = mapBlockIndex.find(hashBlock);
                if (mi != mapBlockIndex.end() && (*mi).second)
                {
                    CBlockIndex* pindex = (*mi).second;
                    if (pindex->IsInMainChain())
                        entry.pushKV("confirmations", 1 + nBestHeight - pindex->nHeight);
                    else
                        entry.pushKV("confirmations", 0);
                }
            }
        }
        else
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available about transaction");
    }

    return entry;
}

UniValue getrawwallettransaction(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "getrawwallettransaction <txid>\n"
                "\n"
                "Get a string that is serialized, hex-encoded data for <txid> "
                "from the wallet.\n");

    const uint256 hash = uint256S(params[0].get_str());

    LOCK2(cs_main, pwalletMain->cs_wallet);

    const auto iter = pwalletMain->mapWallet.find(hash);

    if (iter == pwalletMain->mapWallet.end()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Transaction not in wallet");
    }

    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << static_cast<const CTransaction&>(iter->second);

    return HexStr(ssTx.begin(), ssTx.end());
}


UniValue backupwallet(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error(
                "backupwallet\n"
                "\n"
                "Backup your wallet and config files.\n");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    bool bWalletBackupResults = GRC::BackupWallet(*pwalletMain, GRC::GetBackupFilename("wallet.dat"));
    bool bConfigBackupResults = GRC::BackupConfigFile(GRC::GetBackupFilename("gridcoinresearch.conf"));

    std::vector<std::string> backup_file_type;

    backup_file_type.push_back("wallet.dat");
    backup_file_type.push_back("gridcoinresearch.conf");

    std::vector<std::string> files_removed;
    UniValue u_files_removed(UniValue::VARR);

    bool bMaintainBackupResults = GRC::MaintainBackups(GRC::GetBackupPath(), backup_file_type, 0, 0, files_removed);

    for (const auto& iter : files_removed)
    {
        u_files_removed.push_back(iter);
    }

    UniValue ret(UniValue::VOBJ);
    ret.pushKV("Backup wallet success", bWalletBackupResults);
    ret.pushKV("Backup config success", bConfigBackupResults);
    ret.pushKV("Maintain backup file retention success", bMaintainBackupResults);
    ret.pushKV("Number of files removed", (int64_t) files_removed.size());
    ret.pushKV("Files removed", u_files_removed);

    return ret;
}

UniValue maintainbackups(const UniValue& params, bool fHelp)
{
    if (fHelp || (params.size() != 0 && params.size() != 2)
            || (params.size() == 2 && (params[0].get_int() < 0 || params[1].get_int() < 0)))
        throw runtime_error(
                "maintainbackups ( \"retention by number\" \"retention by days\" )\n"
                "\nArguments:\n"
                "1. \"retention by number\" (non-negative integer, optional) The number of files to retain\n"
                "2. \"retention by days\"   (non-negative integer, optional) The number of days to retain\n"
                "These must be specified as a pair if provided.\n"
                "To run this command, -maintainbackupretention must be set as an argument during Gridcoin\n"
                "startup or given in the config file with maintainbackupretention=1.\n"
                "WARNING: The default values for number and days is 365 for each. Please ensure this is\n"
                "what is desired before you execute this command. Note the command will also use\n"
                "the corresponding walletbackupretainnumfiles= and walletbackupretainnumdays= specified\n"
                "in the config file unless overridden by supplied arguments here. Finally, this function\n"
                "will not allow both values to be set less than 7 to prevent disastrous unintended\n"
                "consequences, and will clamp the values at 7 instead.\n"
                "\n"
                "Maintain backup retention.\n");

    unsigned int retention_by_num = 0;
    unsigned int retention_by_days = 0;

    if (params.size() == 2)
    {
         retention_by_num = params[0].get_int();
         retention_by_days = params[1].get_int();
    }

    std::vector<std::string> backup_file_type;

    backup_file_type.push_back("wallet.dat");
    backup_file_type.push_back("gridcoinresearch.conf");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    std::vector<std::string> files_removed;
    UniValue u_files_removed(UniValue::VARR);

    bool bMaintainBackupResults = GRC::MaintainBackups(GRC::GetBackupPath(), backup_file_type,
                                              retention_by_num, retention_by_days, files_removed);

    for (const auto& iter : files_removed)
    {
        u_files_removed.push_back(iter);
    }

    UniValue ret(UniValue::VOBJ);
    ret.pushKV("Maintain backup file retention success", bMaintainBackupResults);
    ret.pushKV("Number of files removed", (int64_t) files_removed.size());
    ret.pushKV("Files removed", u_files_removed);

    return ret;
}


UniValue keypoolrefill(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
                "keypoolrefill [new-size]\n"
                "Fills the keypool.\n"
                + HelpRequiringPassphrase());

    unsigned int nSize = max(GetArg("-keypool", 100), (int64_t)0);
    if (params.size() > 0) {
        if (params[0].get_int() < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected valid size");
        nSize = (unsigned int) params[0].get_int();
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    pwalletMain->TopUpKeyPool(nSize);

    if (pwalletMain->GetKeyPoolSize() < nSize)
        throw JSONRPCError(RPC_WALLET_ERROR, "Error refreshing keypool.");

    return NullUniValue;
}


void ThreadTopUpKeyPool(void* parg)
{
    // Make this thread recognisable as the key-topping-up thread
    RenameThread("grc-key-top");

    pwalletMain->TopUpKeyPool();
}

void ThreadCleanWalletPassphrase(void* parg)
{
    // Make this thread recognisable as the wallet relocking thread
    RenameThread("grc-lock-wa");

    int64_t nMyWakeTime = GetTimeMillis() + *((int64_t*)parg) * 1000;

    ENTER_CRITICAL_SECTION(cs_nWalletUnlockTime);

    if (nWalletUnlockTime == 0)
    {
        nWalletUnlockTime = nMyWakeTime;

        do
        {
            if (nWalletUnlockTime==0)
                break;
            int64_t nToSleep = nWalletUnlockTime - GetTimeMillis();
            if (nToSleep <= 0)
                break;

            LEAVE_CRITICAL_SECTION(cs_nWalletUnlockTime);
            MilliSleep(nToSleep);
            ENTER_CRITICAL_SECTION(cs_nWalletUnlockTime);

        } while(1);

        if (nWalletUnlockTime)
        {
            nWalletUnlockTime = 0;
            pwalletMain->Lock();
        }
    }
    else
    {
        if (nWalletUnlockTime < nMyWakeTime)
            nWalletUnlockTime = nMyWakeTime;
    }

    LEAVE_CRITICAL_SECTION(cs_nWalletUnlockTime);

    delete (int64_t*)parg;
}

UniValue walletpassphrase(const UniValue& params, bool fHelp)
{
    if (pwalletMain->IsCrypted() && (fHelp || params.size() < 2 || params.size() > 3))
        throw runtime_error(
                "walletpassphrase <passphrase> <timeout> [stakingonly]\n"
                "\n"
                "Stores the wallet decryption key in memory for <timeout> seconds.\n"
                "if [stakingonly] is true sending functions are disabled.\n");
    if (fHelp)
        return true;

    if (!pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an unencrypted wallet, but walletpassphrase was called.");

    if (!pwalletMain->IsLocked())
        throw JSONRPCError(RPC_WALLET_ALREADY_UNLOCKED, "Error: Wallet is already unlocked, use walletlock first if need to change unlock settings.");

    // Adapted from Bitcoin (20190511)...
    // Get the timeout
    int64_t nSleepTime = params[1].get_int64();
    // Timeout cannot be negative or zero, otherwise it will relock immediately.
    if (nSleepTime <= 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Timeout cannot be negative or zero.");
    }
    // Clamp timeout
    constexpr int64_t MAX_SLEEP_TIME = 100000000; // larger values trigger a macos/libevent bug?
    if (nSleepTime > MAX_SLEEP_TIME) {
        nSleepTime = MAX_SLEEP_TIME;
        LogPrintf("WARN: walletpassphrase: timeout is too large. Set to limit of 10000000 seconds.");
    }

    // Note that the walletpassphrase is stored in params[0] which is not mlock()ed
    SecureString strWalletPass;
    strWalletPass.reserve(100);
    // Get rid of this .c_str() by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make params[0] mlock()'d to begin with.
    strWalletPass = params[0].get_str().c_str();

    if (strWalletPass.length() > 0)
    {
        LOCK2(cs_main, pwalletMain->cs_wallet);

        if (!pwalletMain->Unlock(strWalletPass))
            throw JSONRPCError(RPC_WALLET_PASSPHRASE_INCORRECT, "Error: The wallet passphrase entered was incorrect.");
    }
    else
        throw runtime_error(
            "walletpassphrase <passphrase> <timeout>\n"
            "Stores the wallet decryption key in memory for <timeout> seconds.");

    NewThread(ThreadTopUpKeyPool, NULL);
    int64_t* pnSleepTime = new int64_t(nSleepTime);
    NewThread(ThreadCleanWalletPassphrase, pnSleepTime);

    // ppcoin: if user OS account compromised prevent trivial sendmoney commands
    if (params.size() > 2)
        fWalletUnlockStakingOnly = params[2].get_bool();
    else
        fWalletUnlockStakingOnly = false;

    return NullUniValue;
}


UniValue walletpassphrasechange(const UniValue& params, bool fHelp)
{
    if (pwalletMain->IsCrypted() && (fHelp || params.size() != 2))
        throw runtime_error(
                "walletpassphrasechange <oldpassphrase> <newpassphrase>\n"
                "\n"
                "Changes the wallet passphrase from <oldpassphrase> to <newpassphrase>.\n");
    if (fHelp)
        return true;

    if (!pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an unencrypted wallet, but walletpassphrasechange was called.");

    // Get rid of these .c_str() calls by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make params[0] mlock()'d to begin with.
    SecureString strOldWalletPass;
    strOldWalletPass.reserve(100);
    strOldWalletPass = params[0].get_str().c_str();

    SecureString strNewWalletPass;
    strNewWalletPass.reserve(100);
    strNewWalletPass = params[1].get_str().c_str();

    if (strOldWalletPass.length() < 1 || strNewWalletPass.length() < 1)
        throw runtime_error(
            "walletpassphrasechange <oldpassphrase> <newpassphrase>\n"
            "Changes the wallet passphrase from <oldpassphrase> to <newpassphrase>\n.");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (!pwalletMain->ChangeWalletPassphrase(strOldWalletPass, strNewWalletPass))
        throw JSONRPCError(RPC_WALLET_PASSPHRASE_INCORRECT, "Error: The wallet passphrase entered was incorrect.");

    return NullUniValue;
}


UniValue walletlock(const UniValue& params, bool fHelp)
{
    if (pwalletMain->IsCrypted() && (fHelp || params.size() != 0))
        throw runtime_error(
                "walletlock\n"
                "\n"
                "Removes the wallet encryption key from memory, locking the wallet.\n"
                "After calling this method, you will need to call walletpassphrase again\n"
                "before being able to call any methods which require the wallet to be unlocked.\n");
    if (fHelp)
        return true;

    if (!pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an unencrypted wallet, but walletlock was called.");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    {
        LOCK(cs_nWalletUnlockTime);
        pwalletMain->Lock();
        nWalletUnlockTime = 0;
    }

    return NullUniValue;
}


UniValue encryptwallet(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "encryptwallet <passphrase>\n"
                "\n"
                "Encrypts the wallet with <passphrase>.\n");

    if (pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an encrypted wallet, but encryptwallet was called.");

    // Get rid of this .c_str() by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make params[0] mlock()'d to begin with.
    SecureString strWalletPass;
    strWalletPass.reserve(100);
    strWalletPass = params[0].get_str().c_str();

    if (strWalletPass.length() < 1)
        throw runtime_error(
            "encryptwallet <passphrase>\n"
            "Encrypts the wallet with <passphrase>.\n");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (!pwalletMain->EncryptWallet(strWalletPass))
        throw JSONRPCError(RPC_WALLET_ENCRYPTION_FAILED, "Error: Failed to encrypt the wallet.");

    // BDB seems to have a bad habit of writing old data into
    // slack space in .dat files; that is bad if the old data is
    // unencrypted private keys. So:
    StartShutdown();
    return "wallet encrypted; Gridcoin server stopping, restart to run with encrypted wallet.  The keypool has been flushed, you need to make a new backup.";
}

class DescribeAddressVisitor : public boost::static_visitor<UniValue>
{
public:
    UniValue operator()(const CNoDestination &dest) const { return UniValue(); }

    UniValue operator()(const CKeyID &keyID) const {
        UniValue obj(UniValue::VOBJ);
        CPubKey vchPubKey;
        pwalletMain->GetPubKey(keyID, vchPubKey);
        obj.pushKV("isscript", false);
        obj.pushKV("pubkey", HexStr(vchPubKey.Raw()));
        obj.pushKV("iscompressed", vchPubKey.IsCompressed());
        return obj;
    }

    UniValue operator()(const CScriptID &scriptID) const {
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("isscript", true);
        CScript subscript;
        pwalletMain->GetCScript(scriptID, subscript);
        std::vector<CTxDestination> addresses;
        txnouttype whichType;
        int nRequired;
        ExtractDestinations(subscript, whichType, addresses, nRequired);
        obj.pushKV("script", GetTxnOutputType(whichType));
        obj.pushKV("hex", HexStr(subscript.begin(), subscript.end()));
        UniValue a(UniValue::VARR);
        for (auto const& addr : addresses)
            a.push_back(CBitcoinAddress(addr).ToString());
        obj.pushKV("addresses", a);
        if (whichType == TX_MULTISIG)
            obj.pushKV("sigsrequired", nRequired);
        return obj;
    }
};

UniValue validateaddress(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "validateaddress <gridcoinaddress>\n"
                "\n"
                "Return information about <gridcoinaddress>.\n");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CBitcoinAddress address(params[0].get_str());
    bool isValid = address.IsValid();

    UniValue ret(UniValue::VOBJ);
    ret.pushKV("isvalid", isValid);
    if (isValid)
    {
        CTxDestination dest = address.Get();
        string currentAddress = address.ToString();
        ret.pushKV("address", currentAddress);
        bool fMine = IsMine(*pwalletMain, dest) != ISMINE_NO;
        ret.pushKV("ismine", fMine);
        if (fMine) {
            UniValue detail = boost::apply_visitor(DescribeAddressVisitor(), dest);
            ret.pushKVs(detail);
        }
        if (pwalletMain->mapAddressBook.count(dest))
            ret.pushKV("account", pwalletMain->mapAddressBook[dest]);
    }
    return ret;
}

UniValue validatepubkey(const UniValue& params, bool fHelp)
{
    if (fHelp || !params.size() || params.size() > 2)
        throw runtime_error(
                "validatepubkey <gridcoinpubkey>\n"
                "\n"
                "Return information about <gridcoinpubkey>.\n");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    std::vector<unsigned char> vchPubKey = ParseHex(params[0].get_str());

    CPubKey pubKey(vchPubKey);

    bool isValid = pubKey.IsValid();
    bool isCompressed = pubKey.IsCompressed();
    CKeyID keyID = pubKey.GetID();

    CBitcoinAddress address;
    address.Set(keyID);

    UniValue ret(UniValue::VOBJ);
    ret.pushKV("isvalid", isValid);
    if (isValid)
    {
        CTxDestination dest = address.Get();
        string currentAddress = address.ToString();
        ret.pushKV("address", currentAddress);
        bool fMine = IsMine(*pwalletMain, dest) != ISMINE_NO;
        ret.pushKV("ismine", fMine);
        ret.pushKV("iscompressed", isCompressed);
        if (fMine) {
            UniValue detail = boost::apply_visitor(DescribeAddressVisitor(), dest);
            ret.pushKVs(detail);
        }
        if (pwalletMain->mapAddressBook.count(dest))
            ret.pushKV("account", pwalletMain->mapAddressBook[dest]);
    }
    return ret;
}

// ppcoin: reserve balance from being staked for network protection
UniValue reservebalance(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
                "reservebalance [<reserve> [amount]]\n"
                "\n"
                "<reserve> is true or false to turn balance reserve on or off.\n"
                "<amount> is a real and rounded to cent.\n"
                "Reserved amount secures a balance in wallet that can be spendable at anytime.\n"
                "However reserve will secure utxo(s) of any size to respect this setting.\n"
                "If no parameters provided current setting is printed.\n");

    if (params.size() > 0)
    {
        bool fReserve = params[0].get_bool();
        if (fReserve)
        {
            if (params.size() == 1)
                throw runtime_error("must provide amount to reserve balance.\n");
            int64_t nAmount = AmountFromValue(params[1]);
            nAmount = (nAmount / CENT) * CENT;  // round to cent
            if (nAmount < 0)
                throw runtime_error("amount cannot be negative.\n");
            nReserveBalance = nAmount;
        }
        else
        {
            if (params.size() > 1)
                throw runtime_error("cannot specify amount to turn off reserve.\n");
            nReserveBalance = 0;
        }
    }

    UniValue result(UniValue::VOBJ);
    result.pushKV("reserve", (nReserveBalance > 0));
    result.pushKV("amount", ValueFromAmount(nReserveBalance));
    return result;
}


// ppcoin: check wallet integrity
UniValue checkwallet(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error(
                "checkwallet\n"
                "\n"
                "Check wallet for integrity.\n");

    int nMismatchSpent;
    int64_t nBalanceInQuestion;
    pwalletMain->FixSpentCoins(nMismatchSpent, nBalanceInQuestion, true);
    UniValue result(UniValue::VOBJ);
    if (nMismatchSpent == 0)
        result.pushKV("wallet check passed", true);
    else
    {
        result.pushKV("mismatched spent coins", nMismatchSpent);
        result.pushKV("amount in question", ValueFromAmount(nBalanceInQuestion));
    }
    return result;
}


// ppcoin: repair wallet
UniValue repairwallet(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error(
                "repairwallet\n"
                "\n"
                "Repair wallet if checkwallet reports any problem.\n");

    int nMismatchSpent;
    int64_t nBalanceInQuestion;
    pwalletMain->FixSpentCoins(nMismatchSpent, nBalanceInQuestion);
    UniValue result(UniValue::VOBJ);
    if (nMismatchSpent == 0)
        result.pushKV("wallet check passed", true);
    else
    {
        result.pushKV("mismatched spent coins", nMismatchSpent);
        result.pushKV("amount affected by repair", ValueFromAmount(nBalanceInQuestion));
    }
    return result;
}

// NovaCoin: resend unconfirmed wallet transactions
UniValue resendtx(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
                "resendtx\n"
                "\n"
                "Re-send unconfirmed transactions.\n"
                );

    ResendWalletTransactions(true);

    return NullUniValue;
}

// ppcoin: make a public-private key pair
UniValue makekeypair(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
                "makekeypair [prefix]\n"
                "\n"
                "Make a public/private key pair.\n"
                "[prefix] is optional preferred prefix for the public key.\n");

    string strPrefix = "";
    if (params.size() > 0)
        strPrefix = params[0].get_str();

    CKey key;
    key.MakeNewKey(false);

    CPrivKey vchPrivKey = key.GetPrivKey();
    UniValue result(UniValue::VOBJ);
    result.pushKV("PrivateKey", HexStr<CPrivKey::iterator>(vchPrivKey.begin(), vchPrivKey.end()));
    result.pushKV("PublicKey", HexStr(key.GetPubKey().Raw()));
    return result;
}

UniValue burn(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
                "burn <amount> [hex string]\n"
                "\n"
                "<amount> is a real and is rounded to the nearest 0.00000001\n"
                + HelpRequiringPassphrase());

    if (pwalletMain->IsLocked())
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Please enter the wallet passphrase with walletpassphrase first.");

    int64_t nAmount = AmountFromValue(params[0]);
    CScript scriptPubKey;

    if (params.size() > 1)
    {
        vector<unsigned char> data;
        if (params[1].get_str().size() > 0)
            data = ParseHexV(params[1], "Data");
        scriptPubKey = CScript() << OP_RETURN << data;
    }
    else
        scriptPubKey = CScript() << OP_RETURN;

    CWalletTx wtx;
    string strError = pwalletMain->SendMoney(scriptPubKey, nAmount, wtx);
    if (!strError.empty())
        throw JSONRPCError(RPC_WALLET_ERROR, strError);

    return wtx.GetHash().GetHex();
}
