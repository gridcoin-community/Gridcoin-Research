// Copyright (c) 2009-2012 Bitcoin Developers
// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "wallet/walletdump.h"

#include "chain.h"
#include "clientversion.h"
#include "key.h"
#include "key_io.h"
#include "logging.h"
#include "rpc/protocol.h"
#include "rpc/server.h"
#include "support/cleanse.h"
#include "sync.h"
#include "tinyformat.h"
#include "util/strencodings.h"
#include "util/string.h"
#include "util/time.h"
#include "wallet/wallet.h"

#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <algorithm>
#include <ctime>
#include <locale>
#include <map>
#include <sstream>

// The dump's text and its parse, moved here from wallet/rpcdump.cpp so that the
// file transfer's stream and upload use what dumpwallet and importwallet use.
namespace {

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

std::string EncodeDumpTime(int64_t nTime) {
    return DateTimeStrFormat("%Y-%m-%dT%H:%M:%SZ", nTime);
}

std::string EncodeDumpString(const std::string &str) {
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

//! The refusal for a key \p wallet holds but cannot read: the keys are withheld
//! from this thread, or the key does not decrypt.
UniValue UnreadableKeyError(const CWallet& wallet, const CKeyID& id)
{
    return JSONRPCError(wallet.CanUseKeys() ? RPC_WALLET_ERROR : RPC_WALLET_UNLOCK_NEEDED,
                        "dumpwallet: cannot read the key for " + EncodeDestination(id));
}

//! The refusal when the calling thread may not use \p wallet's keys at all.
void RequireKeys(const CWallet& wallet)
{
    if (!wallet.CanUseKeys()) throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "keys are not available");
}

//! Overwrite \p text before it is freed.
void Cleanse(std::string& text)
{
    if (!text.empty()) memory_cleanse(text.data(), text.size());
    text.clear();
}

} // namespace

WalletDumpGenerator::WalletDumpGenerator(CWallet& wallet) : m_wallet(wallet)
{
    LOCK2(cs_main, m_wallet.cs_wallet);

    RequireKeys(m_wallet);

    std::map<CKeyID, int64_t> mapKeyBirth;

    m_wallet.GetKeyBirthTimes(mapKeyBirth);

    m_wallet.GetAllReserveKeys(m_key_pool);

    // sort time/key pairs
    for (std::map<CKeyID, int64_t>::const_iterator it = mapKeyBirth.begin(); it != mapKeyBirth.end(); it++) {
        m_keys.push_back(std::make_pair(it->second, it->first));
    }
    mapKeyBirth.clear();
    std::sort(m_keys.begin(), m_keys.end());

    m_header.push_back(strprintf("# Wallet dump created by Gridcoin %s\n", FormatFullVersion()));
    m_header.push_back(strprintf("# * Created on %s\n", EncodeDumpTime(GetTime())));
    m_header.push_back(strprintf("# * Best block at time of backup was %i (%s),\n", nBestHeight, hashBestChain.ToString()));
    m_header.push_back(strprintf("#   mined on %s\n", EncodeDumpTime(pindexBest->nTime)));
    m_header.push_back("\n");

    for (const std::string& line : m_header) {
        m_header_bytes += line.size();
    }

    m_master_id = m_wallet.GetHDChain().masterKeyID;
}

bool WalletDumpGenerator::LineHere(std::string& line)
{
    for (;;) {
        switch (m_part) {
        case Part::Header:
            if (m_index < m_header.size()) {
                line = m_header[m_index];
                return true;
            }
            m_part = Part::Master;
            m_index = 0;
            break;

        case Part::Master:
            // add the base58check encoded extended master if the wallet uses HD
            if (!m_master_id.IsNull()) {
                CKey key;
                if (m_wallet.GetKey(m_master_id, key)) {
                    CExtKey masterKey;
                    masterKey.SetSeed(key);

                    // Reserved in full before the key is copied in, so the
                    // line never grows and leaves no copy of it behind.
                    static const std::string prefix = "# extended private masterkey: ";
                    std::string encoded = EncodeExtKey(masterKey);
                    line.clear();
                    line.reserve(prefix.size() + encoded.size() + 1);
                    line += prefix;
                    line += encoded;
                    line += "\n";
                    Cleanse(encoded);
                    return true;
                }

                // A master the wallet holds but cannot read fails the dump, as
                // any other key does below.
                if (m_wallet.HaveKey(m_master_id)) throw UnreadableKeyError(m_wallet, m_master_id);
            }

            // No master line, and so no blank line after it.
            m_part = Part::Keys;
            m_index = 0;
            break;

        case Part::MasterBlank:
            line = "\n";
            return true;

        case Part::Keys:
            while (m_index < m_keys.size()) {
                const CKeyID& keyid = m_keys[m_index].second;
                std::string strTime = EncodeDumpTime(m_keys[m_index].first);
                std::string strAddr = EncodeDestination(keyid);

                // The birth times also list keys that have metadata but are not
                // in the wallet, as a failed import can leave; there is no key
                // to dump. A key the wallet holds and cannot read fails the
                // dump: leaving it out would write a dump that looks complete
                // and is not.
                CKey key;
                if (!m_wallet.GetKey(keyid, key)) {
                    if (!m_wallet.HaveKey(keyid)) {
                        ++m_index;
                        continue;
                    }

                    throw UnreadableKeyError(m_wallet, keyid);
                }

                // The line is strprintf("%s %s ", secret, time) and then the
                // rest. The rest holds no secret and is built first; the line
                // is then reserved in full before the secret is copied in, so
                // it never grows and leaves no copy of the secret behind. The
                // secret is in no buffer but this line and its own, which is
                // cleansed.
                std::string tail = " " + strTime + " ";
                if (m_wallet.mapAddressBook.count(keyid)) {
                    tail += strprintf("label=%s", EncodeDumpString(m_wallet.mapAddressBook[keyid].name));
                } else if (keyid == m_master_id) {
                    tail += "hdmaster=1";
                } else if (m_key_pool.count(keyid)) {
                    tail += "reserve=1";
                } else {
                    tail += "change=1";
                }
                tail += strprintf(" # addr=%s%s\n", strAddr, (m_wallet.mapKeyMetadata[keyid].hdKeypath.size() > 0 ? " hdkeypath="+m_wallet.mapKeyMetadata[keyid].hdKeypath : ""));

                std::string secret = EncodeSecret(key);
                line.clear();
                line.reserve(secret.size() + tail.size());
                line += secret;
                line += tail;
                Cleanse(secret);
                return true;
            }
            m_part = Part::Blank;
            break;

        case Part::Blank:
            line = "\n";
            return true;

        case Part::Trailer:
            line = "# End of dump\n";
            return true;

        case Part::Done:
            return false;
        }
    }
}

void WalletDumpGenerator::Advance()
{
    switch (m_part) {
    case Part::Header:
    case Part::Keys:
        ++m_index;
        break;
    case Part::Master:
        m_part = Part::MasterBlank;
        break;
    case Part::MasterBlank:
        m_part = Part::Keys;
        m_index = 0;
        break;
    case Part::Blank:
        m_part = Part::Trailer;
        break;
    case Part::Trailer:
        m_part = Part::Done;
        break;
    case Part::Done:
        break;
    }
}

bool WalletDumpGenerator::Next(std::string& out, size_t max_bytes)
{
    LOCK2(cs_main, m_wallet.cs_wallet);

    if (m_part == Part::Done) return false;

    // Each call, since a capability scope that let the caller use the keys
    // may have closed since the last.
    RequireKeys(m_wallet);

    size_t appended = 0;
    std::string line;

    while (LineHere(line)) {
        // At least one line a call, however long.
        if (appended > 0 && appended + line.size() > max_bytes) {
            Cleanse(line);
            break;
        }

        out += line;
        appended += line.size();
        Cleanse(line);

        const bool trailer = m_part == Part::Trailer;
        Advance();
        if (trailer) return false;
    }

    return true;
}

uint64_t WalletDumpGenerator::SizeHint() const
{
    return m_header_bytes + 160 * static_cast<uint64_t>(m_keys.size());
}

UniValue ImportWalletFromStream(CWallet& wallet, std::istream& file)
{
    LOCK2(cs_main, wallet.cs_wallet);

    EnsureWalletIsUnlocked(wallet);

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

        if (wallet.HaveKey(keyid)) {
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
        if (!wallet.AddKey(key)) {
            fGood = false;
            continue;
        }
        wallet.mapKeyMetadata[keyid].nCreateTime = nTime;
        if (fLabel)
            wallet.SetAddressBookName(keyid, strLabel);
        nTimeBegin = std::min(nTimeBegin, nTime);
    }

    CBlockIndex *pindex = pindexBest;
    while (pindex && pindex->pprev && pindex->nTime > nTimeBegin - 7200)
        pindex = pindex->pprev;

    if (!wallet.nTimeFirstKey || nTimeBegin < wallet.nTimeFirstKey)
        wallet.nTimeFirstKey = nTimeBegin;

    LogPrintf("Rescanning last %i blocks", pindexBest->nHeight - pindex->nHeight + 1);
    wallet.ScanForWalletTransactions(pindex);
    wallet.ReacceptWalletTransactions();
    wallet.MarkDirty();

    if (!fGood)
        throw JSONRPCError(RPC_WALLET_ERROR, "Error adding some keys to wallet");

    if (found_hd_seed) {
        return "Warning: Encountered and imported inactive HD seed during the import. Use the 'sethdseed false <key>'"
               "RPC command if you wish to activate it.";
    }

    return NullUniValue;
}
