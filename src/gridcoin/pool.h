// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_POOL_H
#define GRIDCOIN_POOL_H

#include "amount.h"
#include "key.h"
#include "pubkey.h"
#include "serialize.h"
#include "uint256.h"

#include "gridcoin/contract/handler.h"
#include "gridcoin/contract/payload.h"
#include "gridcoin/contract/registry_db.h"
#include "gridcoin/cpid.h"
#include "gridcoin/support/enumbytes.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace GRC {

//!
//! \brief Status values for an on-chain pool registration entry.
//!
//! The lifecycle is:
//!
//!   POOL_REGISTER ADD (operator-signed)  -->  PENDING
//!   POOL_APPROVE ADD  (master-key signed) -->  ACTIVE   (must reference a known PENDING/ACTIVE CPID)
//!   POOL_APPROVE REMOVE (master-key)      -->  DELETED  (foundation de-list)
//!   POOL_REGISTER REMOVE (operator-signed) -->  DELETED  (operator self-withdrawal)
//!
enum class PoolStatus : uint8_t
{
    UNKNOWN,
    PENDING,    //!< Operator submitted, awaiting foundation approval.
    ACTIVE,     //!< Approved by foundation; consulted by IsPoolCpid/IsPoolUsername.
    DELETED,    //!< De-listed (foundation) or withdrawn (operator).
    OUT_OF_BOUND
};

//!
//! \brief Runtime entry for an on-chain pool. Carries the data persisted in the
//! pool RegistryDB (history-tracked via m_previous_hash).
//!
class Pool
{
public:
    using Status = EnumByte<PoolStatus>;

    Cpid m_cpid;                //!< The pool's external CPID (registry key).
    std::string m_name;         //!< Display name (e.g. "grcpool.com").
    std::string m_url;          //!< Pool website.
    CPubKey m_operator_key;     //!< Operator key for self-sovereign updates / withdrawal.
    int64_t m_timestamp;        //!< Tx time of the contract that produced this entry.
    uint256 m_hash;             //!< Tx hash of the contract that produced this entry.
    uint256 m_previous_hash;    //!< Tx hash of the prior entry for this CPID (history chain).
    Status m_status;            //!< Current lifecycle status.

    Pool();

    Pool(Cpid cpid, std::string name, std::string url, CPubKey operator_key);

    //!
    //! \brief Sanity check. Does not validate any signature.
    //!
    bool WellFormed() const;

    //!
    //! \brief Registry-template key accessor.
    //!
    Cpid Key() const { return m_cpid; }

    //!
    //! \brief Used by RegistryDB logging.
    //!
    std::pair<std::string, std::string> KeyValueToString() const;

    //!
    //! \brief Returns the (translated) status string of the current m_status.
    //!
    std::string StatusToString() const;

    //!
    //! \brief Returns the (translated or raw) status string for the given enum.
    //!
    std::string StatusToString(const PoolStatus& status, const bool& translated = true) const;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(m_cpid);
        READWRITE(m_name);
        READWRITE(m_url);
        READWRITE(m_operator_key);
        READWRITE(m_timestamp);
        READWRITE(m_hash);
        READWRITE(m_previous_hash);
        READWRITE(m_status);
    }
};

typedef std::shared_ptr<Pool> Pool_ptr;

//!
//! \brief The body of a POOL_REGISTER contract. Operator-self-signed.
//!
//! Operator submits this to register a pool. Lands in PENDING in the registry
//! and does NOT affect IsPoolCpid until a follow-up POOL_APPROVE flips it to
//! ACTIVE. RequiresMasterKey returns false for POOL_REGISTER, so a non-master
//! wallet can broadcast this without rejection by CheckContracts; the
//! operator-key signature in m_signature is what authenticates the submission.
//!
class PoolRegisterPayload : public IContractPayload
{
public:
    //!
    //! \brief Bound for the serialized display name. Discourages spam.
    //!
    static constexpr size_t MAX_NAME_SIZE = 100;

    //!
    //! \brief Bound for the serialized pool URL.
    //!
    static constexpr size_t MAX_URL_SIZE = 500;

    static constexpr uint32_t CURRENT_VERSION = 1;

    uint32_t m_version = CURRENT_VERSION;

    Cpid m_cpid;
    std::string m_name;
    std::string m_url;
    CPubKey m_operator_key;

    //!
    //! \brief Operator-signed bytes proving the operator owns m_operator_key.
    //!
    //! For renewals / withdrawal on a CPID already registered, the signature is
    //! verified against the *existing* entry's operator key, not this payload's
    //! key. This prevents takeover by a third party submitting a new operator
    //! key for someone else's CPID. (See PoolRegistry::Add.)
    //!
    //! The signature covers the serialized payload bytes EXCLUDING this field
    //! (SER_GETHASH exclusion below), so re-signing the same payload doesn't
    //! change its contract hash.
    //!
    std::vector<uint8_t> m_signature;

    PoolRegisterPayload();

    PoolRegisterPayload(Cpid cpid, std::string name, std::string url, CPubKey operator_key);

    GRC::ContractType ContractType() const override
    {
        return GRC::ContractType::POOL_REGISTER;
    }

    bool WellFormed(const ContractAction action) const override;

    std::string LegacyKeyString() const override;

    std::string LegacyValueString() const override;

    CAmount RequiredBurnAmount() const override
    {
        // Matches BeaconPayload to discourage spam PENDING submissions.
        return COIN / 2;
    }

    //!
    //! \brief Sign the payload bytes with the operator's private key.
    //!
    //! \return false if signing failed.
    //!
    bool Sign(CKey& operator_private_key);

    //!
    //! \brief Verify m_signature against the supplied pubkey.
    //!
    //! Callers pass either this payload's m_operator_key (new CPID) or the
    //! existing entry's m_operator_key (CPID renewal / withdrawal).
    //!
    bool VerifySignature(const CPubKey& expected_key) const;

    ADD_CONTRACT_PAYLOAD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(
        Stream& s,
        Operation ser_action,
        const ContractAction contract_action)
    {
        READWRITE(m_version);
        READWRITE(m_cpid);
        READWRITE(LIMITED_STRING(m_name, MAX_NAME_SIZE));
        READWRITE(LIMITED_STRING(m_url, MAX_URL_SIZE));
        READWRITE(m_operator_key);

        // Exclude the signature from the hash so the operator can sign the
        // payload after constructing it. The CPubKey IS in the hash, so the
        // signature is bound to a specific operator key.
        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(m_signature);
        }
    }
};

//!
//! \brief The body of a POOL_APPROVE contract. Master-key authenticated.
//!
//! Foundation submits this to flip a PENDING entry to ACTIVE (action=ADD) or
//! to de-list an active pool (action=REMOVE). The transaction is authenticated
//! by Contract::RequiresMasterKey() returning true for POOL_APPROVE; the
//! framework's existing HasMasterKeyInput check enforces it at the validation
//! boundary (see src/validation.cpp:175).
//!
//! The payload itself is minimal because the authority is in the tx inputs;
//! we only need to identify which CPID the action targets.
//!
class PoolApprovePayload : public IContractPayload
{
public:
    static constexpr uint32_t CURRENT_VERSION = 1;

    uint32_t m_version = CURRENT_VERSION;

    Cpid m_cpid;

    PoolApprovePayload();

    explicit PoolApprovePayload(Cpid cpid);

    GRC::ContractType ContractType() const override
    {
        return GRC::ContractType::POOL_APPROVE;
    }

    bool WellFormed(const ContractAction action) const override;

    std::string LegacyKeyString() const override;

    std::string LegacyValueString() const override
    {
        return std::string{};
    }

    CAmount RequiredBurnAmount() const override
    {
        return Contract::STANDARD_BURN_AMOUNT;
    }

    ADD_CONTRACT_PAYLOAD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(
        Stream& s,
        Operation ser_action,
        const ContractAction contract_action)
    {
        READWRITE(m_version);
        READWRITE(m_cpid);
    }
};

//!
//! \brief Handles POOL_REGISTER and POOL_APPROVE contracts.
//!
//! Persisted via the standard RegistryDB template (history-tracked, reorg-safe).
//! All on-chain pool state lives in m_pools_active / m_pools_pending / m_pool_db.
//!
//! Consumed by researcher.cpp (IsPoolCpid / IsPoolUsername) and by the Qt
//! researcher wizard pool page.
//!
class PoolRegistry : public IContractHandler
{
public:
    PoolRegistry()
        : m_pool_db(1)
    {
    }

    typedef std::map<Cpid, Pool_ptr> PoolMap;

    //!
    //! \brief Satisfies the RegistryDB pending-map template parameter. Pool
    //! entries enter the active map at PENDING status; we don't keep a
    //! separate pending sub-map because the lookup is keyed by CPID and the
    //! status field distinguishes ACTIVE vs PENDING vs DELETED. This typedef
    //! exists only because RegistryDB requires the third template parameter.
    //!
    typedef PoolMap PendingPoolMap;

    typedef std::map<uint256, Pool_ptr> HistoricalPoolMap;

    //!
    //! \brief All entries (active + pending + deleted). Caller filters by status.
    //!
    const PoolMap& Entries() const;

    //!
    //! \brief Get all pools currently in the ACTIVE state. Used by the wizard
    //! and the listpools RPC.
    //!
    std::vector<Pool> ActivePools() const;

    //!
    //! \brief Get the entry for the supplied CPID, or nullptr.
    //!
    Pool_ptr Try(const Cpid& cpid) const;

    //!
    //! \brief Cheap query for researcher.cpp::IsPoolCpid.
    //!
    bool IsActivePool(const Cpid& cpid) const;

    //!
    //! \brief Cheap query for researcher.cpp::IsPoolUsername (matches display name).
    //!
    bool IsActivePoolName(const std::string& name) const;

    void Reset() override;

    bool Validate(const Contract& contract, const CTransaction& tx, int& DoS) const override;

    bool BlockValidate(const ContractContext& ctx, int& DoS) const override;

    void Add(const ContractContext& ctx) override;

    void Delete(const ContractContext& ctx) override;

    void Revert(const ContractContext& ctx) override;

    int Initialize() override;

    int GetDBHeight() override;

    void SetDBHeight(int& height) override;

    uint64_t PassivateDB() override;

    //!
    //! \brief RegistryDB instantiation. The third template param (PendingPoolMap)
    //! is typedef'd to PoolMap because we keep PENDING entries in the main map.
    //!
    typedef RegistryDB<Pool,
                       Pool,
                       PoolStatus,
                       PoolMap,
                       PendingPoolMap,
                       std::set<Pool>,
                       HistoricalPoolMap> PoolDB;

    PoolDB& GetPoolDB();

private:
    mutable CCriticalSection cs_lock;

    PoolMap m_pool_entries;                //!< CPID -> current entry (any status).
    PendingPoolMap m_pending_pool_entries{}; //!< Not used; satisfies template.
    std::set<Pool> m_expired_pool_entries{}; //!< Not used; satisfies template.
    PoolMap m_pool_first_entries{};        //!< Not used; satisfies template.

    PoolDB m_pool_db;

    //!
    //! \brief Apply a POOL_REGISTER contract (operator-self-signed).
    //!
    //! ADD: lands as PENDING (or refreshes operator-signed re-registration).
    //! REMOVE: operator self-withdrawal (validated against existing operator key).
    //!
    void ApplyRegister(const ContractContext& ctx);

    //!
    //! \brief Apply a POOL_APPROVE contract (master-key authority).
    //!
    //! ADD: flip the named CPID's entry to ACTIVE (must already exist).
    //! REMOVE: flip to DELETED.
    //!
    void ApplyApprove(const ContractContext& ctx);
};

//!
//! \brief Global pool registry accessor.
//!
PoolRegistry& GetPoolRegistry();

} // namespace GRC

#endif // GRIDCOIN_POOL_H
