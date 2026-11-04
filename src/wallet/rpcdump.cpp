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
#include "support/cleanse.h"
#include "wallet/walletdump.h"

#include <stdexcept>

using namespace std;

void EnsureWalletIsUnlocked();

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

static const RPCHelpMan importprivkey_help{
    "importprivkey",
    "Add a private key (as returned by dumpprivkey) to your wallet.\n"
    "Requires wallet passphrase to be set with walletpassphrase first if wallet is encrypted.\n"
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
const RPCHelpMan& importprivkey_helpman() { return importprivkey_help; }

UniValue importprivkey(const UniValue& params)
{
    // Same enforce-first shape as importwallet, dumpprivkey and dumpwallet.
    // Without it a locked wallet failed later inside AddKey with
    // RPC_WALLET_ERROR, after MarkDirty and a mapKeyMetadata entry had
    // already been written, and a re-import of a key the wallet already held
    // returned success without ever consulting the lock. The check runs with
    // cs_wallet already held (walletlock takes the same lock before Lock()),
    // so the lock state cannot flip between this check and AddKey below.
    LOCK2(cs_main, pwalletMain->cs_wallet);
    EnsureWalletIsUnlocked();

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

    CKeyID vchAddress = key.GetPubKey().GetID();
    {
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

static const RPCHelpMan importwallet_help{
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
const RPCHelpMan& importwallet_helpman() { return importwallet_help; }

UniValue importwallet(const UniValue& params)
{
    fs::path PathForImport = fs::path(params[0].get_str());
    fs::path DefaultPathDataDir = GetDataDir();

    // If provided filename does not have a path, then append parent path, otherwise leave alone.
    if (PathForImport.parent_path().empty())
        PathForImport = DefaultPathDataDir / PathForImport;

    fsbridge::ifstream file;
    file.open(PathForImport);
    if (!file.is_open())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot open wallet dump file");

    return ImportWalletFromStream(*pwalletMain, file);
}


static const RPCHelpMan dumpprivkey_help{
    "dumpprivkey",
    "Reveals the private key corresponding to <gridcoinaddress>.\n"
    "Requires wallet passphrase to be set with walletpassphrase first if wallet is encrypted.",
    {
        {"gridcoinaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "Address of the requested key."},
        {"dump_hex", RPCArg::Type::BOOL, RPCArg::Optional::OMITTED,
            "If true, also include the private and public keys as hex strings in the JSON output. "
            "Default: false (only the base58 WIF is returned)."},
    },
    RPCResult{RPCResult::Type::ANY, "",
        "When dump_hex is false (default), returns the base58 WIF private key as a string. "
        "When dump_hex is true, returns a JSON object with base58 and hex representations."},
    RPCExamples{
        HelpExampleCli("dumpprivkey", "\"SBMNrHuBvrbGTwqCnZHZhQpsbTyYYmY2Dz\"") +
        HelpExampleCli("dumpprivkey", "\"SBMNrHuBvrbGTwqCnZHZhQpsbTyYYmY2Dz\" true") +
        HelpExampleRpc("dumpprivkey", "\"SBMNrHuBvrbGTwqCnZHZhQpsbTyYYmY2Dz\", true")},
};
const RPCHelpMan& dumpprivkey_helpman() { return dumpprivkey_help; }

UniValue dumpprivkey(const UniValue& params)
{
    // Under cs_wallet before the unlock-state check, as importprivkey is
    // (#3332): walletpassphrase and walletlock change the lock state and the
    // staking-only flag under cs_wallet, so the check and the key access below
    // are one step rather than a window another RPC thread can change.
    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    CTxDestination address = DecodeDestination(params[0].get_str());
    if (!IsValidDestination(address))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Gridcoin address");

    CKeyID* keyID = std::get_if<CKeyID>(&address);
    if (!keyID)
        throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to a key");
    CKey vchSecret;
    if (!pwalletMain->GetKey(*keyID, vchSecret))
        throw JSONRPCError(RPC_WALLET_ERROR, "Private key for address " + params[0].get_str() + " is not known");


    if (params.size() == 2 && params[1].isBool() && params[1].get_bool()) {
        // Use the key read above. A second read could find the wallet locked
        // again by the timed relock, which takes only the keystore lock.
        UniValue result(UniValue::VOBJ);

        result.pushKV("private_key", EncodeSecret(vchSecret));
        result.pushKV("private_key_hex", HexStr(vchSecret.GetPrivKey()));
        result.pushKV("public_key_hex", HexStr(vchSecret.GetPubKey()));

        return result;
    }

    return EncodeSecret(vchSecret);
}

static const RPCHelpMan dumpwallet_help{
    "dumpwallet",
    "Dumps all wallet keys in a human-readable format into the specified file.\n"
    "If a path is not specified in the filename, the data directory is used.\n"
    "The file must not already exist.\n"
    "Requires wallet passphrase to be set with walletpassphrase first if wallet is encrypted.",
    {
        {"filename", RPCArg::Type::STR, RPCArg::Optional::NO, "Filename to dump the wallet to."},
    },
    RPCResult{RPCResult::Type::NONE, "", ""},
    RPCExamples{
        HelpExampleCli("dumpwallet", "\"wallet.dump\"") +
        HelpExampleRpc("dumpwallet", "\"wallet.dump\"")},
};
const RPCHelpMan& dumpwallet_helpman() { return dumpwallet_help; }

namespace {

//! Removes a dump file that was not written to the end, however the writer
//! leaves: a dump with keys missing must not stay on disk looking complete.
//! Only a regular file is removed. A symbolic link or a special file at the
//! path is left as it is.
class PartialDumpRemover
{
public:
    PartialDumpRemover(fsbridge::ofstream& file, const fs::path& path) : m_file(file), m_path(path) {}

    ~PartialDumpRemover()
    {
        if (m_complete) return;

        m_file.close();

        // A destructor may not throw, and a file that cannot be removed leaves
        // nothing better to do here.
        try {
            if (fs::is_regular_file(fs::symlink_status(m_path))) fs::remove(m_path);
        } catch (const std::exception&) {
        }
    }

    PartialDumpRemover(const PartialDumpRemover&) = delete;
    PartialDumpRemover& operator=(const PartialDumpRemover&) = delete;

    void Complete() { m_complete = true; }

private:
    fsbridge::ofstream& m_file;
    const fs::path m_path;
    bool m_complete{false};
};

} // namespace

void WriteWalletDump(CWallet& wallet, const std::string& filename)
{
    fs::path PathForDump = fs::path(filename);
    fs::path DefaultPathDataDir = GetDataDir();

    // If provided filename does not have a path, then append parent path, otherwise leave alone.
    if (PathForDump.parent_path().empty())
        PathForDump = DefaultPathDataDir / PathForDump;

    // Nothing at the path is written over: a bare name lands in the data
    // directory, among the node's own files. A symbolic link counts, also one
    // that leads nowhere, and so does a path that cannot be examined. Refused
    // before the file is opened, so nothing there is truncated or removed.
    boost::system::error_code ec;
    if (fs::symlink_status(PathForDump, ec).type() != fs::file_not_found) {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
                           "File already exists at location. Please delete or rename old file first.");
    }

    fsbridge::ofstream file;
    file.open(PathForDump);
    if (!file.is_open())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot open wallet dump file");

    PartialDumpRemover remover(file, PathForDump);

    // Held across the generator's calls, as across the whole dump before it, so
    // the file is one picture of the wallet.
    LOCK2(cs_main, wallet.cs_wallet);

    WalletDumpGenerator generator(wallet);

    // Room for a whole chunk and a line past it up front, so the buffer does
    // not grow, which would leave a copy of the keys in it behind. A line
    // longer than that is appended to an empty buffer, which copies nothing.
    std::string lines;
    lines.reserve(64 * 1024 + 4096);
    try {
        for (bool more = true; more;) {
            more = generator.Next(lines, 64 * 1024);
            file << lines;
            memory_cleanse(lines.data(), lines.size());
            lines.clear();
        }
    } catch (...) {
        if (!lines.empty()) memory_cleanse(lines.data(), lines.size());
        throw;
    }
    file.close();

    // The close flushes what is still buffered. A write that failed leaves a
    // dump with its end cut off, which the remover takes away.
    if (file.fail()) {
        throw JSONRPCError(RPC_WALLET_ERROR, "dumpwallet: cannot write the dump file");
    }

    remover.Complete();
}

UniValue dumpwallet(const UniValue& params)
{
    EnsureWalletIsUnlocked();

    WriteWalletDump(*pwalletMain, params[0].get_str());

    return NullUniValue;
}
