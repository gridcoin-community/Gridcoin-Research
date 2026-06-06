// Copyright (c) 2009-2012 Bitcoin Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "clientversion.h"
#include "fs.h"
#include "init.h" // for pwalletMain
#include <key_io.h>
#include "rpc/server.h"
#include "rpc/protocol.h"
#include "rpc/util.h"
#include "node/ui_interface.h"

#include <stdexcept>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/algorithm/string.hpp>
#include <util/string.h>

using namespace std;

void EnsureWalletIsUnlocked();

namespace bt = boost::posix_time;

// Extended DecodeDumpTime implementation, see this page for details:
// https://stackoverflow.com/questions/3786201/parsing-of-date-time-from-string-boost
const std::locale formats[] = {
    std::locale(std::locale::classic(),new bt::time_input_facet("%Y-%m-%dT%H:%M:%SZ")),
    std::locale(std::locale::classic(),new bt::time_input_facet("%Y-%m-%d %H:%M:%S")),
    std::locale(std::locale::classic(),new bt::time_input_facet("%Y/%m/%d %H:%M:%S")),
    std::locale(std::locale::classic(),new bt::time_input_facet("%d.%m.%Y %H:%M:%S")),
    std::locale(std::locale::classic(),new bt::time_input_facet("%Y-%m-%d"))
};

std::time_t pt_to_time_t(const bt::ptime& pt)
{
    bt::ptime timet_start(boost::gregorian::date(1970,1,1));
    bt::time_duration diff = pt - timet_start;
    return diff.ticks()/bt::time_duration::rep_type::ticks_per_second;
}

int64_t DecodeDumpTime(const std::string& s)
{
    bt::ptime pt;

    for (const auto& format : formats)
    {
        std::istringstream is(s);
        is.imbue(format);
        is >> pt;
        if(pt != bt::ptime()) break;
    }

    return pt_to_time_t(pt);
}

std::string static EncodeDumpTime(int64_t nTime) {
    return DateTimeStrFormat("%Y-%m-%dT%H:%M:%SZ", nTime);
}

std::string static EncodeDumpString(const std::string &str) {
    std::stringstream ret;
    for (unsigned char c : str) {
        if (c <= 32 || c >= 128 || c == '%') {
            ret << '%' << HexStr({&c, 1});
        } else {
            ret << c;
        }
    }
    return ret.str();
}

std::string DecodeDumpString(const std::string &str) {
    std::stringstream ret;
    for (unsigned int pos = 0; pos < str.length(); pos++) {
        unsigned char c = str[pos];
        if (c == '%' && pos+2 < str.length()) {
            c = (((str[pos+1]>>6)*9+((str[pos+1]-'0')&15)) << 4) |
                ((str[pos+2]>>6)*9+((str[pos+2]-'0')&15));
            pos += 2;
        }
        ret << c;
    }
    return ret.str();
}

class CTxDump
{
public:
    CBlockIndex *pindex;
    int64_t nValue;
    bool fSpent;
    CWalletTx* ptx;
    int nOut;
    CTxDump(CWalletTx* ptx = nullptr, int nOut = -1)
    {
        pindex = nullptr;
        nValue = 0;
        fSpent = false;
        this->ptx = ptx;
        this->nOut = nOut;
    }
};

UniValue importprivkey(const UniValue& params, bool fHelp)
{
    static const RPCHelpMan help{
        "importprivkey",
        "Add a private key (as returned by dumpprivkey) to your wallet.\n"
        "\n"
        "WARNING: when rescan is true, a full rescan of the blockchain will occur. This can take up to 20 minutes.",
        {
            {"gridcoinprivkey", RPCArg::Type::STR, RPCArg::Optional::NO, "Base58 WIF private key."},
            {"label", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "Optional label for the imported address."},
            {"rescan", RPCArg::Type::BOOL, RPCArg::Optional::OMITTED,
                "Whether to rescan the blockchain after import. Default: true."},
        },
        RPCResult{RPCResult::Type::NONE, "", ""},
        RPCExamples{
            HelpExampleCli("importprivkey", "\"<wif>\"") +
            HelpExampleCli("importprivkey", "\"<wif>\" \"label\" false") +
            HelpExampleRpc("importprivkey", "\"<wif>\", \"label\", false")},
    };
    if (fHelp || !help.IsValidNumArgs(params.size()))
        throw runtime_error(help.ToString());

    string strSecret = params[0].get_str();
    string strLabel = "";
    if (params.size() > 1)
        strLabel = params[1].get_str();

     // Whether to perform rescan after import
    bool fRescan = true;
    if (params.size() > 2)
        fRescan = params[2].get_bool();

    CKey key = DecodeSecret(strSecret);

    if (!key.IsValid())
    {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid private key");
    }

    if (fWalletUnlockStakingOnly)
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Wallet is unlocked for staking only.");

    CKeyID vchAddress = key.GetPubKey().GetID();
    {
        LOCK2(cs_main, pwalletMain->cs_wallet);

        pwalletMain->MarkDirty();

        // Don't throw error in case a key is already there
        if (pwalletMain->HaveKey(vchAddress))
            return NullUniValue;

        pwalletMain->mapKeyMetadata[vchAddress].nCreateTime = 1;

        if (!pwalletMain->AddKey(key))
            throw JSONRPCError(RPC_WALLET_ERROR, "Error adding key to wallet");

        // whenever a key is imported, we need to scan the whole chain
        pwalletMain->nTimeFirstKey = 1; // 0 would be considered 'no value'
        pwalletMain->SetAddressBookName(vchAddress, strLabel);

        if (fRescan)
        {
            pwalletMain->ScanForWalletTransactions(pindexGenesisBlock, true);
            pwalletMain->ReacceptWalletTransactions();
        }
    }

    return NullUniValue;
}

UniValue importwallet(const UniValue& params, bool fHelp)
{
    static const RPCHelpMan help{
        "importwallet",
        "Imports keys from a wallet dump file (see dumpwallet).\n"
        "If a path is not specified in the filename, the data directory is used.\n"
        "Requires wallet passphrase to be set with walletpassphrase first if wallet is encrypted.",
        {
            {"filename", RPCArg::Type::STR, RPCArg::Optional::NO, "Filename of the wallet dump to import."},
        },
        RPCResult{RPCResult::Type::NONE, "", ""},
        RPCExamples{
            HelpExampleCli("importwallet", "\"wallet.dump\"") +
            HelpExampleRpc("importwallet", "\"wallet.dump\"")},
    };
    if (fHelp || !help.IsValidNumArgs(params.size()))
        throw runtime_error(help.ToString());

    fs::path PathForImport = fs::path(params[0].get_str());
    fs::path DefaultPathDataDir = GetDataDir();

    // If provided filename does not have a path, then append parent path, otherwise leave alone.
    if (PathForImport.parent_path().empty())
        PathForImport = DefaultPathDataDir / PathForImport;

    fsbridge::ifstream file;
    file.open(PathForImport);
    if (!file.is_open())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot open wallet dump file");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    int64_t nTimeBegin = pindexBest->nTime;

    bool fGood = true;
    bool found_hd_seed = false;

    while (file.good()) {
        std::string line;
        std::getline(file, line);
        if (line.empty() || line[0] == '#')
            continue;

        std::vector<std::string> vstr;
        vstr = SplitString(line, ' ');
        if (vstr.size() < 2)
            continue;

        CKey key = DecodeSecret(vstr[0]);
        if (!key.IsValid())
            continue;

        CKeyID keyid = key.GetPubKey().GetID();

        if (pwalletMain->HaveKey(keyid)) {
            LogPrintf("Skipping import of %s (key already present)", EncodeDestination(keyid));
            continue;
        }
        int64_t nTime = DecodeDumpTime(vstr[1]);
        std::string strLabel;
        bool fLabel = true;
        for (unsigned int nStr = 2; nStr < vstr.size(); nStr++) {
            if (boost::algorithm::starts_with(vstr[nStr], "#"))
                break;
            if (vstr[nStr] == "change=1")
                fLabel = false;
            if (vstr[nStr] == "reserve=1")
                fLabel = false;
            if (vstr[nStr] == "hdmaster=1") {
                found_hd_seed = true;
            }
            if (boost::algorithm::starts_with(vstr[nStr], "label=")) {
                strLabel = DecodeDumpString(vstr[nStr].substr(6));
                fLabel = true;
            }
        }
        LogPrintf("Importing %s...", EncodeDestination(keyid));
        if (!pwalletMain->AddKey(key)) {
            fGood = false;
            continue;
        }
        pwalletMain->mapKeyMetadata[keyid].nCreateTime = nTime;
        if (fLabel)
            pwalletMain->SetAddressBookName(keyid, strLabel);
        nTimeBegin = std::min(nTimeBegin, nTime);
    }
    file.close();

    CBlockIndex *pindex = pindexBest;
    while (pindex && pindex->pprev && pindex->nTime > nTimeBegin - 7200)
        pindex = pindex->pprev;

    if (!pwalletMain->nTimeFirstKey || nTimeBegin < pwalletMain->nTimeFirstKey)
        pwalletMain->nTimeFirstKey = nTimeBegin;

    LogPrintf("Rescanning last %i blocks", pindexBest->nHeight - pindex->nHeight + 1);
    pwalletMain->ScanForWalletTransactions(pindex);
    pwalletMain->ReacceptWalletTransactions();
    pwalletMain->MarkDirty();

    if (!fGood)
        throw JSONRPCError(RPC_WALLET_ERROR, "Error adding some keys to wallet");

    if (found_hd_seed) {
        return "Warning: Encountered and imported inactive HD seed during the import. Use the 'sethdseed false <key>'"
               "RPC command if you wish to activate it.";
    }

    return NullUniValue;
}


UniValue dumpprivkey(const UniValue& params, bool fHelp)
{
    static const RPCHelpMan help{
        "dumpprivkey",
        "Reveals the private key corresponding to <gridcoinaddress>.\n"
        "Requires wallet passphrase to be set with walletpassphrase first if wallet is encrypted.",
        {
            {"gridcoinaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "Address of the requested key."},
            {"dump_hex", RPCArg::Type::BOOL, RPCArg::Default{false},
                "If true, also include the private and public keys as hex strings in the JSON output."},
        },
        RPCResults{
            RPCResult{RPCResult::Type::STR, "",
                "Base58 WIF private key (dump_hex=false)."},
            RPCResult{RPCResult::Type::OBJ, "", "Multiple key encodings (dump_hex=true).",
                {
                    {RPCResult::Type::STR, "private_key", "Base58 WIF private key."},
                    {RPCResult::Type::STR_HEX, "private_key_hex", "Hex-encoded private key."},
                    {RPCResult::Type::STR_HEX, "public_key_hex", "Hex-encoded public key."},
                }},
        },
        RPCExamples{
            HelpExampleCli("dumpprivkey", "\"S1Example\"") +
            HelpExampleCli("dumpprivkey", "\"S1Example\" true") +
            HelpExampleRpc("dumpprivkey", "\"S1Example\", true")},
    };
    if (fHelp || !help.IsValidNumArgs(params.size()))
        throw runtime_error(help.ToString());

    EnsureWalletIsUnlocked();

    CTxDestination address = DecodeDestination(params[0].get_str());
    if (!IsValidDestination(address))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Gridcoin address");
    if (fWalletUnlockStakingOnly)
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Wallet is unlocked for staking only.");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CKeyID* keyID = std::get_if<CKeyID>(&address);
    if (!keyID)
        throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to a key");
    CKey vchSecret;
    if (!pwalletMain->GetKey(*keyID, vchSecret))
        throw JSONRPCError(RPC_WALLET_ERROR, "Private key for address " + params[0].get_str() + " is not known");


    if (params.size() == 2 && params[1].isBool() && params[1].get_bool()) {
        CKey key_out;
        pwalletMain->GetKey(*keyID, key_out);

        UniValue result(UniValue::VOBJ);

        result.pushKV("private_key", EncodeSecret(vchSecret));
        result.pushKV("private_key_hex", HexStr(key_out.GetPrivKey()));
        result.pushKV("public_key_hex", HexStr(key_out.GetPubKey()));

        return result;
    }

    return EncodeSecret(vchSecret);
}

UniValue dumpwallet(const UniValue& params, bool fHelp)
{
    static const RPCHelpMan help{
        "dumpwallet",
        "Dumps all wallet keys in a human-readable format into the specified file.\n"
        "If a path is not specified in the filename, the data directory is used.\n"
        "Requires wallet passphrase to be set with walletpassphrase first if wallet is encrypted.",
        {
            {"filename", RPCArg::Type::STR, RPCArg::Optional::NO, "Filename to dump the wallet to."},
        },
        RPCResult{RPCResult::Type::NONE, "", ""},
        RPCExamples{
            HelpExampleCli("dumpwallet", "\"wallet.dump\"") +
            HelpExampleRpc("dumpwallet", "\"wallet.dump\"")},
    };
    if (fHelp || !help.IsValidNumArgs(params.size()))
        throw runtime_error(help.ToString());

    EnsureWalletIsUnlocked();

    fs::path PathForDump = fs::path(params[0].get_str());
    fs::path DefaultPathDataDir = GetDataDir();

    // If provided filename does not have a path, then append parent path, otherwise leave alone.
    if (PathForDump.parent_path().empty())
        PathForDump = DefaultPathDataDir / PathForDump;

    fsbridge::ofstream file;
    file.open(PathForDump);
    if (!file.is_open())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot open wallet dump file");

    std::map<CKeyID, int64_t> mapKeyBirth;

    std::set<CKeyID> setKeyPool;

    LOCK2(cs_main, pwalletMain->cs_wallet);

    pwalletMain->GetKeyBirthTimes(mapKeyBirth);

    pwalletMain->GetAllReserveKeys(setKeyPool);

    // sort time/key pairs
    std::vector<std::pair<int64_t, CKeyID> > vKeyBirth;
    for (std::map<CKeyID, int64_t>::const_iterator it = mapKeyBirth.begin(); it != mapKeyBirth.end(); it++) {
        vKeyBirth.push_back(std::make_pair(it->second, it->first));
    }
    mapKeyBirth.clear();
    std::sort(vKeyBirth.begin(), vKeyBirth.end());

    // produce output
    file << strprintf("# Wallet dump created by Gridcoin %s\n", FormatFullVersion());
    file << strprintf("# * Created on %s\n", EncodeDumpTime(GetTime()));
    file << strprintf("# * Best block at time of backup was %i (%s),\n", nBestHeight, hashBestChain.ToString());
    file << strprintf("#   mined on %s\n", EncodeDumpTime(pindexBest->nTime));
    file << "\n";

    // add the base58check encoded extended master if the wallet uses HD
    CKeyID masterKeyID = pwalletMain->GetHDChain().masterKeyID;
    if (!masterKeyID.IsNull())
    {
        CKey key;
        if (pwalletMain->GetKey(masterKeyID, key))
        {
            CExtKey masterKey;
            masterKey.SetSeed(key);

            file << "# extended private masterkey: " << EncodeExtKey(masterKey) << "\n\n";
        }
    }
    for (std::vector<std::pair<int64_t, CKeyID> >::const_iterator it = vKeyBirth.begin(); it != vKeyBirth.end(); it++) {
        const CKeyID &keyid = it->second;
        std::string strTime = EncodeDumpTime(it->first);
        std::string strAddr = EncodeDestination(keyid);

        CKey key;
        if (pwalletMain->GetKey(keyid, key)) {
            file << strprintf("%s %s ", EncodeSecret(key), strTime);
            if (pwalletMain->mapAddressBook.count(keyid)) {
                file << strprintf("label=%s", EncodeDumpString(pwalletMain->mapAddressBook[keyid]));
            } else if (keyid == masterKeyID) {
                file << "hdmaster=1";
            } else if (setKeyPool.count(keyid)) {
                file << "reserve=1";
            } else {
                file << "change=1";
            }
            file << strprintf(" # addr=%s%s\n", strAddr, (pwalletMain->mapKeyMetadata[keyid].hdKeypath.size() > 0 ? " hdkeypath="+pwalletMain->mapKeyMetadata[keyid].hdKeypath : ""));
        }
    }
    file << "\n";
    file << "# End of dump\n";
    file.close();
    return NullUniValue;
}
