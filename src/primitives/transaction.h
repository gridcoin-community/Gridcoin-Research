// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_PRIMITIVES_TRANSACTION_H
#define BITCOIN_PRIMITIVES_TRANSACTION_H

#include "amount.h"
#include "fwd.h"
#include "gridcoin/contract/contract.h"
#include "script/script.h"
#include "serialize.h"

#include <stdexcept>

struct CMutableTransaction;

/** An inpoint - a combination of a transaction and an index n into its vin */
class CInPoint
{
public:
    CTransaction* ptx;
    unsigned int n;

    CInPoint() { SetNull(); }
    CInPoint(CTransaction* ptxIn, unsigned int nIn) { ptx = ptxIn; n = nIn; }
    void SetNull() { ptx = nullptr; n = (unsigned int)-1; }
    bool IsNull() const { return (ptx == nullptr && n == (unsigned int)-1); }
};



/** An outpoint - a combination of a transaction hash and an index n into its vout */
class COutPoint
{
public:
    uint256 hash;
    unsigned int n;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(hash);
        READWRITE(n);
    }

    COutPoint() { SetNull(); }
    COutPoint(uint256 hashIn, unsigned int nIn) { hash = hashIn; n = nIn; }
    void SetNull() { hash.SetNull(); n = (unsigned int) -1; }
    bool IsNull() const { return (hash.IsNull() && n == (unsigned int) -1); }

    friend bool operator<(const COutPoint& a, const COutPoint& b)
    {
        return (a.hash < b.hash || (a.hash == b.hash && a.n < b.n));
    }

    friend bool operator==(const COutPoint& a, const COutPoint& b)
    {
        return (a.hash == b.hash && a.n == b.n);
    }

    friend bool operator!=(const COutPoint& a, const COutPoint& b)
    {
        return !(a == b);
    }

    std::string ToString() const;
};




/** An input of a transaction.  It contains the location of the previous
 * transaction's output that it claims and a signature that matches the
 * output's public key.
 */
class CTxIn
{
public:
    COutPoint prevout;
    CScript scriptSig;
    unsigned int nSequence;

    CTxIn()
    {
        nSequence = std::numeric_limits<unsigned int>::max();
    }

    explicit CTxIn(COutPoint prevoutIn, CScript scriptSigIn=CScript(), unsigned int nSequenceIn=std::numeric_limits<unsigned int>::max())
    {
        prevout = prevoutIn;
        scriptSig = scriptSigIn;
        nSequence = nSequenceIn;
    }

    CTxIn(uint256 hashPrevTx, unsigned int nOut, CScript scriptSigIn=CScript(), unsigned int nSequenceIn=std::numeric_limits<unsigned int>::max())
    {
        prevout = COutPoint(hashPrevTx, nOut);
        scriptSig = scriptSigIn;
        nSequence = nSequenceIn;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(prevout);
        READWRITE(scriptSig);
        READWRITE(nSequence);
    }

    bool IsFinal() const
    {
        return (nSequence == std::numeric_limits<unsigned int>::max());
    }

    friend bool operator==(const CTxIn& a, const CTxIn& b)
    {
        return (a.prevout   == b.prevout &&
                a.scriptSig == b.scriptSig &&
                a.nSequence == b.nSequence);
    }

    friend bool operator!=(const CTxIn& a, const CTxIn& b)
    {
        return !(a == b);
    }

    std::string ToStringShort() const;
    std::string ToString() const;
};

/* BIP68 — Relative lock-time using consensus-enforced sequence numbers.
 *
 * These constants define the meaning of the nSequence field when
 * CTransaction::nVersion >= 2.
 */

/** Setting nSequence to this value for every input in a transaction
 *  disables nLockTime. */
static const uint32_t SEQUENCE_FINAL = 0xffffffff;

/** If this flag is set, the input's nSequence is NOT interpreted as a
 *  relative lock-time. */
static const uint32_t SEQUENCE_LOCKTIME_DISABLE_FLAG = (1U << 31);

/** If nSequence encodes a relative lock-time and this flag is set, the
 *  relative lock-time has units of 512 seconds, otherwise it specifies
 *  blocks. */
static const uint32_t SEQUENCE_LOCKTIME_TYPE_FLAG = (1U << 22);

/** If nSequence encodes a relative lock-time, this mask is applied to
 *  extract that lock-time from the sequence field. */
static const uint32_t SEQUENCE_LOCKTIME_MASK = 0x0000ffff;

/** In order to use the same number of bits to encode roughly the same
 *  wall-clock duration, and target a granularity of 512 seconds, time-based
 *  relative lock-times are measured in units of 2^SEQUENCE_LOCKTIME_GRANULARITY
 *  seconds. */
static const int SEQUENCE_LOCKTIME_GRANULARITY = 9; // 512-second units

/** An output of a transaction.  It contains the public key that the next input
 * must be able to sign with to claim it.
 */
class CTxOut
{
public:
    CAmount nValue;
    CScript scriptPubKey;

    CTxOut()
    {
        SetNull();
    }

    CTxOut(CAmount nValueIn, CScript scriptPubKeyIn)
    {
        nValue = nValueIn;
        scriptPubKey = scriptPubKeyIn;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(nValue);
        READWRITE(scriptPubKey);
    }

    void SetNull()
    {
        nValue = -1;
        scriptPubKey.clear();
    }

    bool IsNull()
    {
        return (nValue == -1);
    }

    void SetEmpty()
    {
        nValue = 0;
        scriptPubKey.clear();
    }

    bool IsEmpty() const
    {
        return (nValue == 0 && scriptPubKey.empty());
    }

    uint256 GetHash() const
    {
        return SerializeHash(*this);
    }

    friend bool operator==(const CTxOut& a, const CTxOut& b)
    {
        return (a.nValue       == b.nValue &&
                a.scriptPubKey == b.scriptPubKey);
    }

    friend bool operator!=(const CTxOut& a, const CTxOut& b)
    {
        return !(a == b);
    }

    std::string ToStringShort() const;
    std::string ToString() const;
};

/** The basic transaction that is broadcasted on the network and contained in
 * blocks.  A transaction can contain multiple inputs and outputs.
 *
 * All serialized fields are const to enable reliable hash caching. Use
 * CMutableTransaction to build a transaction, then convert to CTransaction.
 */
class CTransaction
{
public:
    static const int CURRENT_VERSION = 2;
    const int nVersion;
    const unsigned int nTime;
    const std::vector<CTxIn> vin;
    const std::vector<CTxOut> vout;
    const unsigned int nLockTime;
    const std::string hashBoinc;
    const std::vector<GRC::Contract> vContracts;

private:
    /** Lazy cache for parsed legacy v1 contracts (from hashBoinc). */
    mutable std::vector<GRC::Contract> m_legacy_parsed_contracts;

    /** Cached transaction hash, computed once at construction.
     * Declared after serialized fields to ensure correct initialization order.
     */
    const uint256 hash;

    /** Compute the hash from serialized data. */
    uint256 ComputeHash() const;

public:

    /** Default constructor: empty transaction with current version/time. */
    CTransaction();

    /** Copy and move constructors. */
    CTransaction(const CTransaction& tx);
    CTransaction(CTransaction&& tx) noexcept;

    /** Convert from a CMutableTransaction. */
    CTransaction(const CMutableTransaction& tx);
    CTransaction(CMutableTransaction&& tx);

    /** Assignment via placement-new (needed for vector/map element assignment). */
    CTransaction& operator=(const CTransaction& other)
    {
        this->~CTransaction();
        ::new (this) CTransaction(other);
        return *this;
    }

    CTransaction& operator=(CTransaction&& other) noexcept
    {
        this->~CTransaction();
        ::new (this) CTransaction(std::move(other));
        return *this;
    }

    template<typename Stream>
    void Serialize(Stream& s) const
    {
        ::Serialize(s, nVersion);
        ::Serialize(s, nTime);
        ::Serialize(s, vin);
        ::Serialize(s, vout);
        ::Serialize(s, nLockTime);

        if (nVersion >= 2) {
            ::Serialize(s, vContracts);
        } else {
            ::Serialize(s, hashBoinc);
        }
    }

    template<typename Stream>
    void Unserialize(Stream& s);

    bool IsNull() const
    {
        return (vin.empty() && vout.empty());
    }

    const uint256& GetHash() const { return hash; }

    bool IsCoinBase() const
    {
        return (vin.size() == 1 && vin[0].prevout.IsNull() && vout.size() >= 1);
    }

    bool IsCoinStake() const
    {
        // ppcoin: the coin stake transaction is marked with the first output empty
        return (vin.size() > 0 && (!vin[0].prevout.IsNull()) && vout.size() >= 2 && vout[0].IsEmpty());
    }

    /** Amount of bitcoins spent by this transaction.
        @return sum of all outputs (note: does not include fees)
     */
    CAmount GetValueOut() const
    {
        CAmount nValueOut = 0;
        for (auto const& txout : vout)
        {
            nValueOut += txout.nValue;
            if (!MoneyRange(txout.nValue) || !MoneyRange(nValueOut))
                throw std::runtime_error("CTransaction::GetValueOut() : value out of range");
        }
        return nValueOut;
    }

    friend bool operator==(const CTransaction& a, const CTransaction& b)
    {
        return a.GetHash() == b.GetHash();
    }

    friend bool operator!=(const CTransaction& a, const CTransaction& b)
    {
        return !(a == b);
    }

    bool IsNewerThan(const CTransaction& old) const;

    std::string ToStringShort() const;
    std::string ToString() const;
    void print() const;

    //!
    //! \brief Get the contracts contained in the transaction.
    //!
    //! \return The set of contracts contained in the transaction. Version 1
    //! transactions can only store one contract.
    //!
    const std::vector<GRC::Contract>& GetContracts() const
    {
        if (nVersion == 1 && vContracts.empty() && GRC::Contract::Detect(hashBoinc)) {
            if (m_legacy_parsed_contracts.empty()) {
                m_legacy_parsed_contracts.emplace_back(GRC::Contract::Parse(hashBoinc));
            }
            return m_legacy_parsed_contracts;
        }

        return vContracts;
    }

    //!
    //! \brief Get the contracts contained in the transaction (by copy).
    //!
    //! \return The set of contracts contained in the transaction.
    //!
    std::vector<GRC::Contract> PullContracts() const
    {
        if (nVersion == 1 && vContracts.empty() && GRC::Contract::Detect(hashBoinc)) {
            return {GRC::Contract::Parse(hashBoinc)};
        }

        return vContracts;
    }

};

/** A mutable version of CTransaction.
 *
 * Use CMutableTransaction to build a transaction, then convert to an immutable
 * CTransaction when done. CTransaction fields are const, so all mutation must
 * happen through CMutableTransaction.
 */
struct CMutableTransaction
{
    static const int CURRENT_VERSION = 2;
    int nVersion;
    unsigned int nTime;
    std::vector<CTxIn> vin;
    std::vector<CTxOut> vout;
    unsigned int nLockTime;
    std::string hashBoinc;
    std::vector<GRC::Contract> vContracts;

    CMutableTransaction();
    explicit CMutableTransaction(const CTransaction& tx);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(nVersion);
        READWRITE(nTime);
        READWRITE(vin);
        READWRITE(vout);
        READWRITE(nLockTime);

        if (nVersion >= 2) {
            READWRITE(vContracts);
        } else {
            READWRITE(hashBoinc);
        }
    }

    /** Compute the hash of this CMutableTransaction. This is computed on the
     * fly, as opposed to GetHash() in CTransaction, which will use a cached
     * result once CTransaction fields become const.
     */
    uint256 GetHash() const;
};

template<typename Stream>
void CTransaction::Unserialize(Stream& s)
{
    CMutableTransaction mtx;
    s >> mtx;
    *this = CTransaction(std::move(mtx));
}

/** Helper function to create a shared transaction reference */
inline CTransactionRef MakeTransactionRef(const CTransaction& tx)
{
    return std::make_shared<const CTransaction>(tx);
}

#endif // BITCOIN_PRIMITIVES_TRANSACTION_H
