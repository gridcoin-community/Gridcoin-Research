// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/transaction.h>


std::string COutPoint::ToString() const
{
    return strprintf("COutPoint(%s, %u)", hash.ToString().substr(0,10), n);
}


std::string CTxIn::ToStringShort() const
{
    return strprintf(" %s %d", prevout.hash.ToString(), prevout.n);
}


std::string CTxIn::ToString() const
{
    std::string str;
    str += "CTxIn(";
    str += prevout.ToString();
    if (prevout.IsNull())
        str += strprintf(", coinbase %s", HexStr(scriptSig));
    else
        str += strprintf(", scriptSig=%s", scriptSig.ToString().substr(0,24));
    if (nSequence != std::numeric_limits<unsigned int>::max())
        str += strprintf(", nSequence=%u", nSequence);
    str += ")";
    return str;
}


std::string CTxOut::ToStringShort() const
{
    return strprintf(" out %s %s", FormatMoney(nValue), scriptPubKey.ToString(true));
}


std::string CTxOut::ToString() const
{
    if (IsEmpty()) return "CTxOut(empty)";
    return strprintf("CTxOut(nValue=%s, scriptPubKey=%s)", FormatMoney(nValue), scriptPubKey.ToString());
}


std::string CTransaction::ToStringShort() const
{
    std::string str;
    str += strprintf("%s %s", GetHash().ToString(), IsCoinBase()? "base" : (IsCoinStake()? "stake" : "user"));
    return str;
}


std::string CTransaction::ToString() const
{
    std::string str;
    str += IsCoinBase()? "Coinbase" : (IsCoinStake()? "Coinstake" : "CTransaction");
    str += strprintf("(hash=%s, nTime=%d, ver=%d, vin.size=%" PRIszu ", vout.size=%" PRIszu ", nLockTime=%d)\n",
        GetHash().ToString().substr(0,10),
        nTime,
        nVersion,
        vin.size(),
        vout.size(),
        nLockTime);
    for (unsigned int i = 0; i < vin.size(); i++)
        str += "    " + vin[i].ToString() + "\n";
    for (unsigned int i = 0; i < vout.size(); i++)
        str += "    " + vout[i].ToString() + "\n";
    return str;
}


void CTransaction::print() const
{
    LogPrintf("%s", ToString());
}


bool CTransaction::IsNewerThan(const CTransaction& old) const
{
    if (vin.size() != old.vin.size())
        return false;
    for (unsigned int i = 0; i < vin.size(); i++)
        if (vin[i].prevout != old.vin[i].prevout)
            return false;

    bool fNewer = false;
    unsigned int nLowest = std::numeric_limits<unsigned int>::max();
    for (unsigned int i = 0; i < vin.size(); i++)
    {
        if (vin[i].nSequence != old.vin[i].nSequence)
        {
            if (vin[i].nSequence <= nLowest)
            {
                fNewer = false;
                nLowest = vin[i].nSequence;
            }
            if (old.vin[i].nSequence < nLowest)
            {
                fNewer = true;
                nLowest = old.vin[i].nSequence;
            }
        }
    }
    return fNewer;
}


const std::vector<GRC::Contract>& CTransaction::GetContracts() const
{
    if (nVersion == 1 && vContracts.empty() && GRC::Contract::Detect(hashBoinc)) {
        REF(vContracts).emplace_back(GRC::Contract::Parse(hashBoinc));
    }

    return vContracts;
}


std::vector<GRC::Contract> CTransaction::PullContracts()
{
    GetContracts(); // Populate vContracts for legacy transactions.

    return std::move(vContracts);
}
