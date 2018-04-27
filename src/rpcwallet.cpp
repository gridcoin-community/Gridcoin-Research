// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "version.h"
#include "txdb.h"
#include "wallet.h"
#include "walletdb.h"
#include "bitcoinrpc.h"
#include "init.h"
#include "base58.h"
#include "util.h"
#include "backup.h"

using namespace json_spirit;
using namespace std;

int64_t nWalletUnlockTime;
static CCriticalSection cs_nWalletUnlockTime;

extern void ThreadTopUpKeyPool(void* parg);

double CoinToDouble(double surrogate);
std::string ExtractXML(std::string XMLdata, std::string key, std::string key_end);

extern void ThreadCleanWalletPassphrase(void* parg);

extern void TxToJSON(const CTransaction& tx, const uint256 hashBlock, json_spirit::Object& entry);
extern int64_t GetEarliestWalletTransaction();

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

bool IsPoR2(double amt)
{
    std::string sAmt = RoundToString(amt,8);
    if (sAmt.length() > 8)
    {
        std::string suffix = sAmt.substr(sAmt.length()-4,4);
        if (suffix == "0124" || suffix=="0123")
        {
            return true;
        }
    }
    return false;
}

void WalletTxToJSON(const CWalletTx& wtx, Object& entry)
{
    int confirms = wtx.GetDepthInMainChain();
    entry.push_back(Pair("confirmations", confirms));
    if (wtx.IsCoinBase() || wtx.IsCoinStake())
        entry.push_back(Pair("generated", true));
    if (confirms > 0)
    {
        entry.push_back(Pair("blockhash", wtx.hashBlock.GetHex()));
        entry.push_back(Pair("blockindex", wtx.nIndex));
        entry.push_back(Pair("blocktime", (int)(mapBlockIndex[wtx.hashBlock]->nTime)));
    }
    entry.push_back(Pair("txid", wtx.GetHash().GetHex()));
    entry.push_back(Pair("time", wtx.GetTxTime()));
    entry.push_back(Pair("timereceived", (int)wtx.nTimeReceived));
    for (auto const& item : wtx.mapValue)
        entry.push_back(Pair(item.first, item.second));
}

string AccountFromValue(const Value& value)
{
    string strAccount = value.get_str();
    if (strAccount == "*")
        throw JSONRPCError(RPC_WALLET_INVALID_ACCOUNT_NAME, "Invalid account name");
    return strAccount;
}

Value getinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "getinfo\n"
                "\n"
                "Returns an object containing various state info.");

    proxyType proxy;
    GetProxy(NET_IPV4, proxy);

    Object obj, diff;

    LOCK2(cs_main, pwalletMain->cs_wallet);

    obj.push_back(Pair("version",       FormatFullVersion()));
    obj.push_back(Pair("minor_version", CLIENT_VERSION_MINOR));

    obj.push_back(Pair("protocolversion", PROTOCOL_VERSION));
    obj.push_back(Pair("walletversion", pwalletMain->GetVersion()));
    obj.push_back(Pair("balance",       ValueFromAmount(pwalletMain->GetBalance())));
    obj.push_back(Pair("newmint",       ValueFromAmount(pwalletMain->GetNewMint())));
    obj.push_back(Pair("stake",         ValueFromAmount(pwalletMain->GetStake())));
    obj.push_back(Pair("blocks",        nBestHeight));
    obj.push_back(Pair("timeoffset",    GetTimeOffset()));
    obj.push_back(Pair("moneysupply",   ValueFromAmount(pindexBest->nMoneySupply)));
    obj.push_back(Pair("connections",   (int)vNodes.size()));
    obj.push_back(Pair("proxy",         (proxy.first.IsValid() ? proxy.first.ToStringIPPort() : string())));
    obj.push_back(Pair("ip",            addrSeenByPeer.ToStringIP()));

    diff.push_back(Pair("proof-of-work",  GetDifficulty()));
    diff.push_back(Pair("proof-of-stake", GetDifficulty(GetLastBlockIndex(pindexBest, true))));
    obj.push_back(Pair("difficulty",    diff));

    obj.push_back(Pair("testnet",       fTestNet));
    obj.push_back(Pair("keypoololdest", pwalletMain->GetOldestKeyPoolTime()));
    obj.push_back(Pair("keypoolsize",   (int)pwalletMain->GetKeyPoolSize()));
    obj.push_back(Pair("paytxfee",      ValueFromAmount(nTransactionFee)));
    obj.push_back(Pair("mininput",      ValueFromAmount(nMinimumInputValue)));
    if (pwalletMain->IsCrypted())
        obj.push_back(Pair("unlocked_until", nWalletUnlockTime / 1000));
    obj.push_back(Pair("errors",        GetWarnings("statusbar")));
    return obj;
}

Value getwalletinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "getwalletinfo\n"
                "\n"
                "Displays information about the wallet\n");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    Object res;

    res.push_back(Pair("walletversion", pwalletMain->GetVersion()));
    res.push_back(Pair("balance",       ValueFromAmount(pwalletMain->GetBalance())));
    res.push_back(Pair("newmint",       ValueFromAmount(pwalletMain->GetNewMint())));
    res.push_back(Pair("stake",         ValueFromAmount(pwalletMain->GetStake())));
    res.push_back(Pair("keypoololdest", pwalletMain->GetOldestKeyPoolTime()));
    res.push_back(Pair("keypoolsize",   (int)pwalletMain->GetKeyPoolSize()));

    if (pwalletMain->IsCrypted())
        res.push_back(Pair("unlocked_until", nWalletUnlockTime / 1000));

    return res;
}

Value getnewpubkey(const Array& params, bool fHelp)
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


Value getnewaddress(const Array& params, bool fHelp)
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

Value getaccountaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "getaccountaddress <account>\n"
                "\n"
                "Returns the current Gridcoin address for receiving payments to this account.\n");

    // Parse the account first so we don't generate a key if there's an error
    string strAccount = AccountFromValue(params[0]);

    Value ret;

    LOCK2(cs_main, pwalletMain->cs_wallet);

    ret = GetAccountAddress(strAccount).ToString();

    return ret;
}



Value setaccount(const Array& params, bool fHelp)
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

    return Value::null;
}


Value getaccount(const Array& params, bool fHelp)
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


Value getaddressesbyaccount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "getaddressesbyaccount <account>\n"
                "\n"
                "Returns the list of addresses for the given account.\n");

    string strAccount = AccountFromValue(params[0]);

    // Find all addresses that have the given account
    Array ret;

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

Value sendtoaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 4)
        throw runtime_error(
                "sendtoaddress <gridcoinaddress> <amount> [comment] [comment-to]\n"
                "\n"
                "<amount> is a real and is rounded to the nearest 0.000001\n"
                + HelpRequiringPassphrase());

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CBitcoinAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Gridcoin address");

    // Amount
    int64_t nAmount = AmountFromValue(params[1]);

    // Wallet comments
    CWalletTx wtx;
    if (params.size() > 2 && params[2].type() != null_type && !params[2].get_str().empty())
        wtx.mapValue["comment"] = params[2].get_str();
    if (params.size() > 3 && params[3].type() != null_type && !params[3].get_str().empty())
        wtx.mapValue["to"]      = params[3].get_str();

    if (pwalletMain->IsLocked())
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Please enter the wallet passphrase with walletpassphrase first.");

    string strError = pwalletMain->SendMoneyToDestination(address.Get(), nAmount, wtx);
    if (!strError.empty())
        throw JSONRPCError(RPC_WALLET_ERROR, strError);

    return wtx.GetHash().GetHex();
}

Value listaddressgroupings(const Array& params, bool fHelp)
{
    if (fHelp)
        throw runtime_error(
                "listaddressgroupings\n"
                "\n"
                "Lists groups of addresses which have had their common ownership\n"
                "made public by common use as inputs or as the resulting change\n"
                "in past transactions\n");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    Array jsonGroupings;
    map<CTxDestination, int64_t> balances = pwalletMain->GetAddressBalances();
    for (auto const& grouping : pwalletMain->GetAddressGroupings())
    {
        Array jsonGrouping;
        for (auto const& address : grouping)
        {
            Array addressInfo;
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

Value signmessage(const Array& params, bool fHelp)
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

Value verifymessage(const Array& params, bool fHelp)
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


Value getreceivedbyaddress(const Array& params, bool fHelp)
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
    if (!IsMine(*pwalletMain,scriptPubKey))
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

Value getreceivedbyaccount(const Array& params, bool fHelp)
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
            if (ExtractDestination(txout.scriptPubKey, address) && IsMine(*pwalletMain, address) && setAddress.count(address))
                if (wtx.GetDepthInMainChain() >= nMinDepth)
                    nAmount += txout.nValue;
        }
    }

    return (double)nAmount / (double)COIN;
}




int64_t GetEarliestWalletTransaction()
{
        int64_t nTime = 999999999999;
        const isminefilter& filter = MINE_SPENDABLE;
        for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
        {
            const CWalletTx& wtx = (*it).second;
            if (!wtx.IsTrusted())     continue;

            string strSentAccount;
            list<pair<CTxDestination, int64_t> > listReceived;
            list<pair<CTxDestination, int64_t> > listSent;
            int64_t totalFees;

            wtx.GetAmounts(listReceived, listSent, totalFees, strSentAccount, filter);
            if (wtx.GetDepthInMainChain() >= 0 && wtx.GetBlocksToMaturity() == 0)
            {
                if (wtx.nTime < nTime && wtx.nTime > 0) nTime = wtx.nTime;
            }
        }
        return  nTime;


}

int64_t GetAccountBalance(CWalletDB& walletdb, const string& strAccount, int nMinDepth, const isminefilter& filter = MINE_SPENDABLE)
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

int64_t GetAccountBalance(const string& strAccount, int nMinDepth, const isminefilter& filter = MINE_SPENDABLE)
{
    CWalletDB walletdb(pwalletMain->strWalletFile);
    return GetAccountBalance(walletdb, strAccount, nMinDepth, filter);
}


Value getbalance(const Array& params, bool fHelp)
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
    isminefilter filter = MINE_SPENDABLE;
    if (params.size() > 1)
    {
        nMinDepth = params[1].get_int();
        if(params.size() > 2)
        {
            if(params[2].get_bool())
                 filter = filter | MINE_WATCH_ONLY;
        }
    }

    if (params[0].get_str() == "*") {
        // Calculate total balance a different way from GetBalance()
        // (GetBalance() sums up all unspent TxOuts)
        // getbalance and getbalance '*' 0 should return the same number.
        int64_t nBalance = 0;
        for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
        {
            const CWalletTx& wtx = (*it).second;
            if (!wtx.IsTrusted())
                continue;

            int64_t allFee;
            string strSentAccount;
            list<pair<CTxDestination, int64_t> > listReceived;
            list<pair<CTxDestination, int64_t> > listSent;
            wtx.GetAmounts(listReceived, listSent, allFee, strSentAccount, filter);
            if (wtx.GetDepthInMainChain() >= nMinDepth && wtx.GetBlocksToMaturity() == 0)
            {
                for (auto const& r : listReceived)
                    nBalance += r.second;
            }
            for (auto const& r : listSent)
                nBalance -= r.second;
            nBalance -= allFee;
        }
        return  ValueFromAmount(nBalance);
    }

    accountingDeprecationCheck();

    string strAccount = AccountFromValue(params[0]);

    int64_t nBalance = GetAccountBalance(strAccount, nMinDepth, filter);

    return ValueFromAmount(nBalance);
}


Value movecmd(const Array& params, bool fHelp)
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


Value sendfrom(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 3 || params.size() > 6)
        throw runtime_error(
                "sendfrom <fromaccount> <toGridcoinaddress> <amount> [minconf=1] [comment] [comment-to]\n"
                "\n"
                "<amount> is a real and is rounded to the nearest 0.000001\n"
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
    if (params.size() > 4 && params[4].type() != null_type && !params[4].get_str().empty())
        wtx.mapValue["comment"] = params[4].get_str();
    if (params.size() > 5 && params[5].type() != null_type && !params[5].get_str().empty())
        wtx.mapValue["to"]      = params[5].get_str();

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


Value sendmany(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 4)
        throw runtime_error(
                "sendmany <fromaccount> {address:amount,...} [minconf=1] [comment]\n"
                "\n"
                "amounts are double-precision floating point numbers\n"
                + HelpRequiringPassphrase());

    string strAccount = AccountFromValue(params[0]);
    Object sendTo = params[1].get_obj();
    int nMinDepth = 1;
    if (params.size() > 2)
        nMinDepth = params[2].get_int();

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CWalletTx wtx;
    wtx.strFromAccount = strAccount;
    if (params.size() > 3 && params[3].type() != null_type && !params[3].get_str().empty())
        wtx.mapValue["comment"] = params[3].get_str();

    set<CBitcoinAddress> setAddress;
    vector<pair<CScript, int64_t> > vecSend;

    int64_t totalAmount = 0;
    for (auto const& s : sendTo)
    {
        CBitcoinAddress address(s.name_);
        if (!address.IsValid())
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, string("Invalid Gridcoin address: ")+s.name_);

        if (setAddress.count(address))
            throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, duplicated address: ")+s.name_);
        setAddress.insert(address);

        CScript scriptPubKey;
        scriptPubKey.SetDestination(address.Get());
        int64_t nAmount = AmountFromValue(s.value_);

        totalAmount += nAmount;

        vecSend.push_back(make_pair(scriptPubKey, nAmount));
    }

    EnsureWalletIsUnlocked();

    // Check funds
    int64_t nBalance = GetAccountBalance(strAccount, nMinDepth);
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

Value addmultisigaddress(const Array& params, bool fHelp)
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
    const Array& keys = params[1].get_array();
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

Value addredeemscript(const Array& params, bool fHelp)
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

    std::string sKey;
    std::string sDetail;
    std::string sContract;
    vector<uint256> txids;
    bool fIsWatchonly;
    vector<std::string> sContracts;

    tallyitem()
    {
        nAmount = 0;
        nConf = std::numeric_limits<int>::max();
        fIsWatchonly = false;
        sKey = "";
        sDetail = "";
        sContract = "";
    }
};

Value ListReceived(const Array& params, bool fByAccounts)
{
    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 0)
        nMinDepth = params[0].get_int();

    // Whether to include empty accounts
    bool fIncludeEmpty = false;
    isminefilter filter = MINE_SPENDABLE;
    if(params.size() > 2)
         if(params[2].get_bool())
             filter = filter | MINE_WATCH_ONLY;

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
            std::string sContract = wtx.hashBoinc + "<TXID>" + wtx.GetHash().GetHex() + "</TXID>";
            item.sContracts.push_back(sContract);
            if (mine & MINE_WATCH_ONLY)
              item.fIsWatchonly = true;

        }
    }

    // Reply
    Array ret;
    map<string, tallyitem> mapAccountTally;

    for (auto const& item : pwalletMain->mapAddressBook)
    {
        const CBitcoinAddress& address = item.first;
        const string& strAccount = item.second;
        map<CBitcoinAddress, tallyitem>::iterator it = mapTally.find(address);
        if (it == mapTally.end() && !fIncludeEmpty)
            continue;

        int64_t nAmount = 0;
        std::string sKey = "";
        std::string sContract = "";
        std::string sDetail = "";

        int nConf = std::numeric_limits<int>::max();
        bool fIsWatchonly = false;
        if (it != mapTally.end())
        {
                 nAmount   = (*it).second.nAmount;
                 nConf     = (*it).second.nConf;
                 sKey      = (*it).second.sKey;
                 sContract = (*it).second.sContract;
                 sDetail   = (*it).second.sDetail;
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
            Object obj;
            if(fIsWatchonly)
                  obj.push_back(Pair("involvesWatchonly", true));
            obj.push_back(Pair("address",       address.ToString()));
            obj.push_back(Pair("account",       strAccount));
            obj.push_back(Pair("amount",        ValueFromAmount(nAmount)));
            obj.push_back(Pair("confirmations", (nConf == std::numeric_limits<int>::max() ? 0 : nConf)));
            obj.push_back(Pair("tx_count",      (uint64_t) it->second.sContracts.size()));

            // Add support for contract or message information appended to the TX itself
            Object oTX;
            if(it != mapTally.end())
            {
                for (auto const& sContract : (*it).second.sContracts)
                {
                    std::string sTxId = ExtractXML(sContract,"<TXID>","</TXID>");
                    std::string sKey = ExtractXML(sContract,"<KEY>","</KEY>");
                    std::string sDetail = ExtractXML(sContract,"<DETAIL>","</DETAIL>");
                    oTX.push_back(Pair("txid", sTxId));
                    Object oSubTx;
                    if (!sDetail.empty())
                    {
                        oSubTx.push_back(Pair("key", sKey));
                        oSubTx.push_back(Pair("detail",sDetail));
                        oTX.push_back(Pair("Contract",oSubTx));
        	        }
                }
			}
            obj.push_back(Pair("txids", oTX));
            ret.push_back(obj);
        }
    }

    if (fByAccounts)
    {
        for (map<string, tallyitem>::iterator it = mapAccountTally.begin(); it != mapAccountTally.end(); ++it)
        {
            int64_t nAmount = (*it).second.nAmount;
            int nConf = (*it).second.nConf;
            Object obj;
            if((*it).second.fIsWatchonly)
                 obj.push_back(Pair("involvesWatchonly", true));
            obj.push_back(Pair("account",       (*it).first));
            obj.push_back(Pair("amount",        ValueFromAmount(nAmount)));
            obj.push_back(Pair("confirmations", (nConf == std::numeric_limits<int>::max() ? 0 : nConf)));
            ret.push_back(obj);
        }
    }

    return ret;
}

Value listreceivedbyaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 3)
        throw runtime_error(
                "listreceivedbyaddress ( minconf includeempty includeWatchonly)\n"
                "\nList balances by receiving address.\n"
                "\nArguments:\n"
                "1. minconf       (numeric, optional, default=1) The minimum number of confirmations before payments are included.\n"
                "2. includeempty  (numeric, optional, dafault=false) Whether to include addresses that haven't received any payments.\n"
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

Value listreceivedbyaccount(const Array& params, bool fHelp)
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

static void MaybePushAddress(Object & entry, const CTxDestination &dest)
{
    CBitcoinAddress addr;
    if (addr.Set(dest))
    {
            entry.push_back(Pair("address", addr.ToString()));
    }
    else
    {
            std::string     grcaddress = CBitcoinAddress(addr).ToString();
            entry.push_back(Pair("address?", grcaddress));
    }
}



 void ListTransactions2(const CWalletTx& wtx, const string& strAccount, int nMinDepth, bool fLong, Array& ret, CTxDB& txdb, const isminefilter& filter=MINE_SPENDABLE)
 {
    int64_t nFee;
    string strSentAccount;
    list<COutputEntry> listReceived;
    list<COutputEntry> listSent;

    wtx.GetAmounts2(listReceived, listSent, nFee, strSentAccount, true, txdb, filter);

    bool fAllAccounts = (strAccount == string("*") || strAccount.empty());
    bool involvesWatchonly = wtx.IsFromMe(MINE_WATCH_ONLY);

    // R Halford - Upgrade Bitcoin's ListTransactions to work with Gridcoin
    // Ensure CoinStake addresses are deserialized, convert CoinStake split stake rewards to subsidies, Show POR vs Interest breakout

    // List: Sent
    if ((!listSent.empty() || nFee != 0) && (fAllAccounts || strAccount == strSentAccount))
    {
        for (auto const& s : listSent)
        {
            Object entry;
            // CTxDestination dest = address.Get();
            if(involvesWatchonly || (::IsMine(*pwalletMain, s.destination) & MINE_WATCH_ONLY))
                            entry.push_back(Pair("involvesWatchonly", true));
            entry.push_back(Pair("account", strSentAccount));
            MaybePushAddress(entry, s.destination);
            entry.push_back(Pair("category", "send"));
            entry.push_back(Pair("amount", ValueFromAmount(-s.amount)));
            //  entry.push_back(Pair("vout", s.vout));
            entry.push_back(Pair("fee", ValueFromAmount(-nFee)));
            if (fLong)
                WalletTxToJSON(wtx, entry);
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
                Object entry;
                if(involvesWatchonly || (::IsMine(*pwalletMain, r.destination) & MINE_WATCH_ONLY))
                          entry.push_back(Pair("involvesWatchonly", true));
                entry.push_back(Pair("account", account));
                MaybePushAddress(entry, r.destination);
                if (wtx.IsCoinBase() || wtx.IsCoinStake())
                {
                    if (wtx.GetDepthInMainChain() < 1)
                        entry.push_back(Pair("category", "orphan"));
                    else if (wtx.GetBlocksToMaturity() > 0)
                        entry.push_back(Pair("category", "immature"));
                    else
                        entry.push_back(Pair("category", "generate"));

                    std::string type = IsPoR2(CoinToDouble(r.amount)) ? "POR" : "Interest";
                    {
                        entry.push_back(Pair("Type", type));
                    }

                }
                else
                {
                    entry.push_back(Pair("category", "receive"));
                }
                entry.push_back(Pair("fee", ValueFromAmount(-nFee)));
                entry.push_back(Pair("amount", ValueFromAmount(r.amount)));
                if (fLong)
                    WalletTxToJSON(wtx, entry);
                ret.push_back(entry);
            }
        }
    }
}

void ListTransactions(const CWalletTx& wtx, const string& strAccount, int nMinDepth, bool fLong, Array& ret, const isminefilter& filter=MINE_SPENDABLE)
{
    int64_t nFee;
    string strSentAccount;
    list<pair<CTxDestination, int64_t> > listReceived;
    list<pair<CTxDestination, int64_t> > listSent;
    //const isminefilter& filter = MINE_SPENDABLE;
    wtx.GetAmounts(listReceived, listSent, nFee, strSentAccount, filter);

    bool fAllAccounts = (strAccount == string("*") || strAccount.empty() || strAccount == "");
    bool involvesWatchonly = wtx.IsFromMe(MINE_WATCH_ONLY);
    // Sent
    if ((!wtx.IsCoinStake()) && (!listSent.empty() || nFee != 0) && (fAllAccounts || strAccount == strSentAccount))
    {
        for (auto const& s : listSent)
        {
            Object entry;
            if(involvesWatchonly || (::IsMine(*pwalletMain, s.first) & MINE_WATCH_ONLY))
                     entry.push_back(Pair("involvesWatchonly", true));
            entry.push_back(Pair("account", strSentAccount));
            MaybePushAddress(entry, s.first);
            entry.push_back(Pair("category", "send"));
            entry.push_back(Pair("amount", ValueFromAmount(-s.second)));
            entry.push_back(Pair("fee", ValueFromAmount(-nFee)));
            if (fLong)
                WalletTxToJSON(wtx, entry);
            ret.push_back(entry);
        }
    }

    // Received
    if (listReceived.size() > 0 && wtx.GetDepthInMainChain() >= nMinDepth)
    {
        bool stop = false;
        for (auto const& r : listReceived)
        {
            string account;
            if (pwalletMain->mapAddressBook.count(r.first))
                account = pwalletMain->mapAddressBook[r.first];
            if (true || (account == strAccount))
            {
                Object entry;
                if(involvesWatchonly || (::IsMine(*pwalletMain, r.first) & MINE_WATCH_ONLY))
                           entry.push_back(Pair("involvesWatchonly", true));
                entry.push_back(Pair("account", account));
                MaybePushAddress(entry, r.first);
                if (wtx.IsCoinBase() || wtx.IsCoinStake())
                {
                    if (wtx.GetDepthInMainChain() < 1)
                    {
                        entry.push_back(Pair("category", "orphan"));
                    }
                    else if (wtx.GetBlocksToMaturity() > 0)
                    {
                        entry.push_back(Pair("category", "immature"));
                    }
                    else
                    {
                        entry.push_back(Pair("category", "generate"));

                    }
                    std::string type = IsPoR2(-nFee) ? "POR" : "Interest";
                    {
                        entry.push_back(Pair("Type", type));
                    }
                }
                else
                {
                    entry.push_back(Pair("category", "receive"));
                }
                if (!wtx.IsCoinStake())
                    entry.push_back(Pair("amount", ValueFromAmount(r.second)));
                else
                {
                    entry.push_back(Pair("amount", ValueFromAmount(-nFee)));
                    stop = true; // only one coinstake output
                }
                if (fLong)
                    WalletTxToJSON(wtx, entry);
                ret.push_back(entry);
            }
            if (stop)                 break;
        }
    }
}

void AcentryToJSON(const CAccountingEntry& acentry, const string& strAccount, Array& ret)
{
    bool fAllAccounts = (strAccount == string("*"));

    if (fAllAccounts || acentry.strAccount == strAccount)
    {
        Object entry;
        entry.push_back(Pair("account", acentry.strAccount));
        entry.push_back(Pair("category", "move"));
        entry.push_back(Pair("time", acentry.nTime));
        entry.push_back(Pair("amount", ValueFromAmount(acentry.nCreditDebit)));
        entry.push_back(Pair("otheraccount", acentry.strOtherAccount));
        entry.push_back(Pair("comment", acentry.strComment));
        ret.push_back(entry);
    }
}





Value listtransactions(const Array& params, bool fHelp)
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
    isminefilter filter = MINE_SPENDABLE;
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
                        filter = filter | MINE_WATCH_ONLY;
                }
            }
        }
    }
    if (nCount < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative count");
    if (nFrom < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative from");

    Array ret;

    LOCK2(cs_main, pwalletMain->cs_wallet);

    std::list<CAccountingEntry> acentries;
    CWallet::TxItems txOrdered = pwalletMain->OrderedTxItems(acentries, strAccount);
    CTxDB txdb("r");

    // iterate backwards until we have nCount items to return:
    for (CWallet::TxItems::reverse_iterator it = txOrdered.rbegin(); it != txOrdered.rend(); ++it)
    {
        CWalletTx *const pwtx = (*it).second.first;
        if (pwtx != 0)
            ListTransactions2(*pwtx, strAccount, 0, true, ret, txdb, filter);
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
    Array::iterator first = ret.begin();
    std::advance(first, nFrom);
    Array::iterator last = ret.begin();
    std::advance(last, nFrom+nCount);

    if (last != ret.end()) ret.erase(last, ret.end());
    if (first != ret.begin()) ret.erase(ret.begin(), first);

    std::reverse(ret.begin(), ret.end()); // Return oldest to newest

    return ret;
}

Value listaccounts(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
                "listaccounts ( minconf includeWatchonly)\n"
                "\n"
                "Returns Object that has account names as keys, account balances as values."
                "1. minconf          (numeric, optional, default=1) Only onclude transactions with at least this many confirmations\n"
                "2. includeWatchonly (bool, optional, default=false) Include balances in watchonly addresses (see 'importaddress')\n"
                "\nResult:\n"
                "{                      (json object where keys are account names, and values are numeric balances\n"
                "  \"account\": x.xxx,  (numeric) The property name is the account name, and the value is the total balance for the account.\n"
                );

    accountingDeprecationCheck();

    int nMinDepth = 1;
    isminefilter includeWatchonly = MINE_SPENDABLE;
    if (params.size() > 0)
    {
         nMinDepth = params[0].get_int();
         if(params.size() > 1)
             if(params[1].get_bool())
                 includeWatchonly = includeWatchonly | MINE_WATCH_ONLY;
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
        list<pair<CTxDestination, int64_t> > listReceived;
        list<pair<CTxDestination, int64_t> > listSent;
        int nDepth = wtx.GetDepthInMainChain();
        if (nDepth < 0)
            continue;
        wtx.GetAmounts(listReceived, listSent, nFee, strSentAccount, includeWatchonly);
        mapAccountBalances[strSentAccount] -= nFee;
        for (auto const& s : listSent)
            mapAccountBalances[strSentAccount] -= s.second;
        if (nDepth >= nMinDepth && wtx.GetBlocksToMaturity() == 0)
        {
            for (auto const& r : listReceived)
                if (pwalletMain->mapAddressBook.count(r.first))
                    mapAccountBalances[pwalletMain->mapAddressBook[r.first]] += r.second;
                else
                    mapAccountBalances[""] += r.second;
        }
    }

    list<CAccountingEntry> acentries;
    CWalletDB(pwalletMain->strWalletFile).ListAccountCreditDebit("*", acentries);
    for (auto const& entry : acentries)
        mapAccountBalances[entry.strAccount] += entry.nCreditDebit;

    Object ret;
    for (auto const& accountBalance : mapAccountBalances) {
        ret.push_back(Pair(accountBalance.first, ValueFromAmount(accountBalance.second)));
    }
    return ret;
}

Value listsinceblock(const Array& params, bool fHelp)
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
    isminefilter filter = MINE_SPENDABLE;
    if (params.size() > 0)
    {
        uint256 blockId = 0;

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
                    filter = filter | MINE_WATCH_ONLY;
            }
        }
    }

    int depth = pindex ? (1 + nBestHeight - pindex->nHeight) : -1;
    Array transactions;

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
    uint256 lastblock = block ? block->GetBlockHash() : 0;

    Object ret;
    ret.push_back(Pair("transactions", transactions));
    ret.push_back(Pair("lastblock", lastblock.GetHex()));

    return ret;

}

Value gettransaction(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
                "gettransaction \"txid\"\n"
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
    isminefilter filter = MINE_SPENDABLE;
    if(params.size() > 1)
         if(params[1].get_bool())
             filter = filter | MINE_WATCH_ONLY;
    Object entry;

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (pwalletMain->mapWallet.count(hash))
    {
        const CWalletTx& wtx = pwalletMain->mapWallet[hash];

        TxToJSON(wtx, 0, entry);

        int64_t nCredit = wtx.GetCredit();
        int64_t nDebit = wtx.GetDebit();
        int64_t nNet = nCredit - nDebit;
        int64_t nFee = (wtx.IsFromMe() ? wtx.GetValueOut() - nDebit : 0);

        entry.push_back(Pair("amount", ValueFromAmount(nNet - nFee)));
        if (wtx.IsFromMe())
            entry.push_back(Pair("fee", ValueFromAmount(nFee)));

        WalletTxToJSON(wtx, entry);

        Array details;
        ListTransactions(pwalletMain->mapWallet[hash], "*", 0, false, details, filter);
        entry.push_back(Pair("details", details));
    }
    else
    {
        CTransaction tx;
        uint256 hashBlock = 0;
        if (GetTransaction(hash, tx, hashBlock))
        {
            TxToJSON(tx, 0, entry);
            if (hashBlock == 0)
                entry.push_back(Pair("confirmations", 0));
            else
            {
                entry.push_back(Pair("blockhash", hashBlock.GetHex()));
                BlockMap::iterator mi = mapBlockIndex.find(hashBlock);
                if (mi != mapBlockIndex.end() && (*mi).second)
                {
                    CBlockIndex* pindex = (*mi).second;
                    if (pindex->IsInMainChain())
                        entry.push_back(Pair("confirmations", 1 + nBestHeight - pindex->nHeight));
                    else
                        entry.push_back(Pair("confirmations", 0));
                }
            }
        }
        else
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available about transaction");
    }

    return entry;
}


Value backupwallet(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error(
                "backupwallet\n"
                "\n"
                "Backup your wallet and config files.\n");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    bool bWalletBackupResults = BackupWallet(*pwalletMain, GetBackupFilename("wallet.dat"));
    bool bConfigBackupResults = BackupConfigFile(GetBackupFilename("gridcoinresearch.conf"));
    Object ret;
    ret.push_back(Pair("Backup wallet success", bWalletBackupResults));
    ret.push_back(Pair("Backup config success", bConfigBackupResults));
    return ret;
}


Value keypoolrefill(const Array& params, bool fHelp)
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

    return Value::null;
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

Value walletpassphrase(const Array& params, bool fHelp)
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

    int64_t nSleepTime = params[1].get_int64();
    if (nSleepTime <= 0 || nSleepTime >= std::numeric_limits<int64_t>::max() / 1000000000)
        throw runtime_error("timeout is out of bounds");

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

    return Value::null;
}


Value walletpassphrasechange(const Array& params, bool fHelp)
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

    return Value::null;
}


Value walletlock(const Array& params, bool fHelp)
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

    return Value::null;
}


Value encryptwallet(const Array& params, bool fHelp)
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

class DescribeAddressVisitor : public boost::static_visitor<Object>
{
public:
    Object operator()(const CNoDestination &dest) const { return Object(); }

    Object operator()(const CKeyID &keyID) const {
        Object obj;
        CPubKey vchPubKey;
        pwalletMain->GetPubKey(keyID, vchPubKey);
        obj.push_back(Pair("isscript", false));
        obj.push_back(Pair("pubkey", HexStr(vchPubKey.Raw())));
        obj.push_back(Pair("iscompressed", vchPubKey.IsCompressed()));
        return obj;
    }

    Object operator()(const CScriptID &scriptID) const {
        Object obj;
        obj.push_back(Pair("isscript", true));
        CScript subscript;
        pwalletMain->GetCScript(scriptID, subscript);
        std::vector<CTxDestination> addresses;
        txnouttype whichType;
        int nRequired;
        ExtractDestinations(subscript, whichType, addresses, nRequired);
        obj.push_back(Pair("script", GetTxnOutputType(whichType)));
        obj.push_back(Pair("hex", HexStr(subscript.begin(), subscript.end())));
        Array a;
        for (auto const& addr : addresses)
            a.push_back(CBitcoinAddress(addr).ToString());
        obj.push_back(Pair("addresses", a));
        if (whichType == TX_MULTISIG)
            obj.push_back(Pair("sigsrequired", nRequired));
        return obj;
    }
};

Value validateaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "validateaddress <gridcoinaddress>\n"
                "\n"
                "Return information about <gridcoinaddress>.\n");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CBitcoinAddress address(params[0].get_str());
    bool isValid = address.IsValid();

    Object ret;
    ret.push_back(Pair("isvalid", isValid));
    if (isValid)
    {
        CTxDestination dest = address.Get();
        string currentAddress = address.ToString();
        ret.push_back(Pair("address", currentAddress));
        bool fMine = IsMine(*pwalletMain, dest);
        ret.push_back(Pair("ismine", fMine));
        if (fMine) {
            Object detail = boost::apply_visitor(DescribeAddressVisitor(), dest);
            ret.insert(ret.end(), detail.begin(), detail.end());
        }
        if (pwalletMain->mapAddressBook.count(dest))
            ret.push_back(Pair("account", pwalletMain->mapAddressBook[dest]));
    }
    return ret;
}

Value validatepubkey(const Array& params, bool fHelp)
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

    Object ret;
    ret.push_back(Pair("isvalid", isValid));
    if (isValid)
    {
        CTxDestination dest = address.Get();
        string currentAddress = address.ToString();
        ret.push_back(Pair("address", currentAddress));
        bool fMine = IsMine(*pwalletMain, dest);
        ret.push_back(Pair("ismine", fMine));
        ret.push_back(Pair("iscompressed", isCompressed));
        if (fMine) {
            Object detail = boost::apply_visitor(DescribeAddressVisitor(), dest);
            ret.insert(ret.end(), detail.begin(), detail.end());
        }
        if (pwalletMain->mapAddressBook.count(dest))
            ret.push_back(Pair("account", pwalletMain->mapAddressBook[dest]));
    }
    return ret;
}

// ppcoin: reserve balance from being staked for network protection
Value reservebalance(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
                "reservebalance [<reserve> [amount]]\n"
                "\n"
                "<reserve> is true or false to turn balance reserve on or off.\n"
                "<amount> is a real and rounded to cent.\n"
                "Set reserve amount not participating in network protection.\n"
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

    Object result;
    result.push_back(Pair("reserve", (nReserveBalance > 0)));
    result.push_back(Pair("amount", ValueFromAmount(nReserveBalance)));
    return result;
}


// ppcoin: check wallet integrity
Value checkwallet(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error(
                "checkwallet\n"
                "\n"
                "Check wallet for integrity.\n");

    int nMismatchSpent;
    int64_t nBalanceInQuestion;
    pwalletMain->FixSpentCoins(nMismatchSpent, nBalanceInQuestion, true);
    Object result;
    if (nMismatchSpent == 0)
        result.push_back(Pair("wallet check passed", true));
    else
    {
        result.push_back(Pair("mismatched spent coins", nMismatchSpent));
        result.push_back(Pair("amount in question", ValueFromAmount(nBalanceInQuestion)));
    }
    return result;
}


// ppcoin: repair wallet
Value repairwallet(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error(
                "repairwallet\n"
                "\n"
                "Repair wallet if checkwallet reports any problem.\n");

    int nMismatchSpent;
    int64_t nBalanceInQuestion;
    pwalletMain->FixSpentCoins(nMismatchSpent, nBalanceInQuestion);
    Object result;
    if (nMismatchSpent == 0)
        result.push_back(Pair("wallet check passed", true));
    else
    {
        result.push_back(Pair("mismatched spent coins", nMismatchSpent));
        result.push_back(Pair("amount affected by repair", ValueFromAmount(nBalanceInQuestion)));
    }
    return result;
}

// NovaCoin: resend unconfirmed wallet transactions
Value resendtx(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
                "resendtx\n"
                "\n"
                "Re-send unconfirmed transactions.\n"
                );

    ResendWalletTransactions(true);

    return Value::null;
}

// ppcoin: make a public-private key pair
Value makekeypair(const Array& params, bool fHelp)
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
    Object result;
    result.push_back(Pair("PrivateKey", HexStr<CPrivKey::iterator>(vchPrivKey.begin(), vchPrivKey.end())));
    result.push_back(Pair("PublicKey", HexStr(key.GetPubKey().Raw())));
    return result;
}

Value burn(const Array& params, bool fHelp)
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
