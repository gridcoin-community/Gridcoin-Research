// Copyright (c) 2014-2024 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_BEACON_H
#define GRIDCOIN_BEACON_H

#include "amount.h"
#include "dbwrapper.h"
#include "key.h"
#include "gridcoin/contract/handler.h"
#include "gridcoin/contract/payload.h"
#include "gridcoin/contract/registry_db.h"
#include "gridcoin/cpid.h"
#include "gridcoin/support/enumbytes.h"
#include <memory>
#include <string>
#include <vector>

class CBitcoinAddress;
class CTransaction;
class CWallet;

namespace GRC {

class Contract;

//!
//! \brief Enumeration of beacon status specific to storage. Note that expired is not a status,
//! because active beacon expiration is evaluated in real time lazily to avoid having
//! expiry triggering problems. Expired pending IS a status, because this is triggered
//! on superblock acceptance for all pending beacons that have aged beyond the limit without
//! being verified. Note that the order of the enumeration is IMPORTANT. Do not change the order
//! of existing entries, and OUT_OF_BOUND must go at the end and be retained for the EnumBytes
//! wrapper.
//!
enum class BeaconStatusForStorage
{
    UNKNOWN,
    PENDING,
    ACTIVE,
    RENEWAL,
    EXPIRED_PENDING,
    DELETED,
    OUT_OF_BOUND
};

//!
//! \brief Links an account on the BOINC platform with a participant in the
//! Gridcoin research reward protocol.
//!
//! The Gridcoin cryptocurrency technology rewards computing work executed for
//! research projects on the BOINC distributed computing platform. As a system
//! separate from BOINC, the Gridcoin network must regularly synchronize state
//! with the supported BOINC projects. The network employs several specialized
//! protocols, collectively known as the researcher reward protocol, that turn
//! recorded quantities of computational work into values of Gridcoin currency
//! by translating data collected from BOINC project servers.
//!
//! This protocol must map BOINC accounts to addresses in the Gridcoin block
//! chain to allocate and distribute rewards as GRC proportional to the work
//! completed by computers operated by the participants in the network. This
//! process depends on beacons submitted to the block chain to establish the
//! relationships between BOINC users and their wallet's cryptographic keys.
//!
//! A node in the network must advertise a beacon to initiate a process which
//! eventually demonstrates the node's eligibility to generate rewards earned
//! for BOINC computation. The action formally broadcasts a node's claim to a
//! BOINC identity by producing a transaction that includes the node's public
//! key and the external cross-project identifier (CPID) of the BOINC user to
//! associate with the key.
//!
//! This class contains the beacon data submitted to the network as a contract
//! message embedded in a transaction. A node validates a generated block that
//! claims a research reward by verifying the signature in the block using the
//! public key contained in the beacon that matches the block CPID.
//!
class Beacon
{
public:
    //!
    //! \brief Wrapped Enumeration of beacon status, mainly for serialization/deserialization.
    //!
    using Status = EnumByte<BeaconStatusForStorage>;

    //!
    //! \brief Duration in seconds to consider a beacon active.
    //!
    static constexpr int64_t MAX_AGE = 60 * 60 * 24 * 30 * 6;

    //!
    //! \brief Duration in seconds to consider a beacon eligible for renewal.
    //!
    static constexpr int64_t RENEWAL_AGE = 60 * 60 * 24 * 30 * 5;

    Cpid m_cpid;                //!< Identifies the researcher that advertised the beacon.
    CPubKey m_public_key;       //!< Verifies blocks that claim research rewards.
    int64_t m_timestamp;        //!< Time of the beacon contract transaction.

    uint256 m_hash;             //!< The hash of the transaction that advertised the beacon, or the block containing the SB.

    uint256 m_previous_hash; //!< The m_hash of the previous beacon.

    Status m_status;            //!< The status of the beacon. It is of type int instead of enum for serialization.
    //!
    //! \brief Initialize an empty, invalid beacon instance.
    //!
    Beacon();

    //!
    //! \brief Initialize a new beacon for submission in a contract.
    //!
    //! \param public_key Used to verify blocks that claim research rewards.
    //!
    Beacon(CPubKey public_key);

    //!
    //! \brief Initialize a beacon instance with data from a contract.
    //!
    //! \param public_key   Used to verify blocks that claim research rewards.
    //! \param tx_timestamp Time of the transaction with the beacon contract.
    //!
    Beacon(CPubKey public_key, int64_t tx_timestamp, uint256 hash);

    //!
    //! \brief Initialize a beacon instance from a contract that contains beacon
    //! data in the legacy, semicolon-delimited string format.
    //!
    //! \param contract Contains the beacon data in a legacy, serialized format.
    //!
    //! \return A beacon matching the data in the contract, or an invalid beacon
    //! instance if the contract is malformed.
    //!
    static Beacon Parse(const std::string& value);

    //!
    //! \brief Determine whether a beacon contains each of the required elements.
    //!
    //! The result of this method call does NOT guarantee that the beacon is
    //! valid--additional context is needed to check for renewable, expired,
    //! or existing beacons. The return value of \c true only indicates that
    //! the object contains the data needed for a well-formed beacon.
    //!
    //! \return \c true if the beacon is complete.
    //!
    bool WellFormed() const;

    //!
    //! \brief Provides the beacon CPID (key) and status (value) as a pair of strings.
    //! \return std::pair of strings
    //!
    std::pair<std::string, std::string> KeyValueToString() const;

    //!
    //! \brief Returns the string representation of the current beacon status
    //!
    //! \return Translated string representation of beacon status
    //!
    std::string StatusToString() const;

    //!
    //! \brief Returns the translated or untranslated string of the input beacon status
    //!
    //! \param status. BeaconStatusForStorage status
    //! \param translated. True for translated, false for not translated. Defaults to true.
    //!
    //! \return Beacon status string.
    //!
    std::string StatusToString(const BeaconStatusForStorage& status, const bool& translated = true) const;

    //!
    //! \brief Get the elapsed time since advertisement.
    //!
    //! \param now Timestamp to consider as the current time.
    //!
    //! \return Beacon age in seconds.
    //!
    int64_t Age(const int64_t now) const;

    //!
    //! \brief Determine whether the beacon age exceeds the duration of a beacon
    //! that a participant can use to claim rewards.
    //!
    //! \param now Timestamp to consider as the current time.
    //!
    //! \return \c true if a beacon's age exceeds the maximum duration.
    //!
    bool Expired(const int64_t now) const;

    //!
    //! \brief Determine whether the beacon is eligible for renewal.
    //!
    //! \param now Timestamp to consider as the current time.
    //!
    //! \return \c true if a beacon's age exceeds the minimum duration for
    //! renewal.
    //!
    bool Renewable(const int64_t now) const;

    //!
    //! \brief Determine whether the beacon was renewed.
    //!
    //! \return \c true if the beacon is a renewal rather than new advertisement
    //!
    bool Renewed() const;

    //!
    //! \brief Get the hash of the beacon public key.
    //!
    //! \return RIPEMD-160 hash produced from the public key.
    //!
    CKeyID GetId() const;

    //!
    //! \brief Get the beacon's destination wallet address.
    //!
    //! This address provides a recipient for network-wide rain transactions.
    //!
    //! \return Address produced from the beacon's public key.
    //!
    CBitcoinAddress GetAddress() const;

    //!
    //! \brief Get the code that the scrapers use to verify the beacon.
    //!
    //! World Community Grid allows no more than 30 characters for an account's
    //! username field so we format the key ID using base58 encoding to produce
    //! a shorter verification code.
    //!
    //! \return Base58 representation of the RIPEMD-160 hash of the public key.
    //!
    std::string GetVerificationCode() const;

    //!
    //! \brief Determine whether the given wallet contains a private key for
    //! this beacon's public key. Because this function is intended to work
    //! even if the wallet is locked, it does not check whether the key pair is
    //! actually valid.
    //!
    //! \return \c true if the wallet contains a matching private key.
    //!
    bool WalletHasPrivateKey(const CWallet* const wallet) const;

    //!
    //! \brief Get the legacy string representation of a version 1 beacon
    //! contract.
    //!
    //! \return Beacon data in the legacy, semicolon-delimited string format.
    //!
    std::string ToString() const;

    //!
    //! \brief Comparison operator overload used in the unit test harness.
    //!
    //! \param b The right hand side beacon to compare for equality.
    //!
    //! \return Equal or not.
    //!
    bool operator==(Beacon b);

    //!
    //! \brief Comparison operator overload used in the unit test harness.
    //!
    //! \param b The right hand side beacon to compare for equality.
    //!
    //! \return Equal or not.
    //!
    bool operator!=(Beacon b);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(m_public_key);
    }
};

//!
//! \brief The type that defines a shared pointer to a Beacon
//!
typedef std::shared_ptr<Beacon> Beacon_ptr;

//!
//! \brief A type that either points to some beacon or does not.
//!
typedef const Beacon_ptr BeaconOption;

//!
//! \brief The body of a beacon contract advertised in a transaction.
//!
class BeaconPayload : public IContractPayload
{
public:
    //!
    //! \brief Version number of the current format for a serialized beacon.
    //!
    //! CONSENSUS: Increment this value when introducing a breaking change and
    //! ensure that the serialization/deserialization routines also handle all
    //! of the previous versions.
    //!
    static constexpr uint32_t CURRENT_VERSION = 2;

    //!
    //! \brief Version number of the serialized beacon format.
    //!
    //! Defaults to the most recent version for a new beacon instance.
    //!
    //! Version 1: Legacy semicolon-delimited string of fields parsed from a
    //! contract value with the following format:
    //!
    //!    BASE64(HEX(CPIDV2);HEX(RANDOM_HASH);BASE58(ADDRESS);HEX(PUBLIC_KEY))
    //!
    //! Version 2: Beacon data serializable in binary format. Stored in a
    //! contract's value field as bytes.
    //!
    uint32_t m_version = CURRENT_VERSION;

    Cpid m_cpid;     //!< Identifies the researcher that advertised the beacon.
    Beacon m_beacon; //!< Contains the beacon public key.

    //!
    //! \brief Beacon data signed by the beacon's private key.
    //!
    //! By checking beacon signatures, we can allow for self-service deletion
    //! of existing beacons and deny spoofed beacons for the same public key.
    //!
    std::vector<uint8_t> m_signature;

    //!
    //! \brief Initialize an empty, invalid beacon payload.
    //!
    BeaconPayload();

    //!
    //! \brief Initialize a beacon payload for the specified CPID.
    //!
    //! \param version Version of the serialized beacon format.
    //! \param cpid    Identifies the researcher that advertised the beacon.
    //! \param beacon  Contains the beacon public key.
    //!
    BeaconPayload(const uint32_t version, const Cpid& cpid, Beacon beacon);

    //!
    //! \brief Initialize a beacon payload for the specified CPID.
    //!
    //! \param cpid   Identifies the researcher that advertised the beacon.
    //! \param beacon Contains the beacon public key.
    //!
    BeaconPayload(const Cpid& cpid, Beacon beacon);

    //!
    //! \brief Initialize a beacon payload instance from beacon data in the
    //! legacy, semicolon-delimited string format.
    //!
    //! \param key   CPID represented as a hex string.
    //! \param value Beacon data in the legacy, semicolon-delimited format.
    //!
    //! \return A beacon matching the data in the contract, or an invalid beacon
    //! payload instance if the data is malformed.
    //!
    static BeaconPayload Parse(const std::string& key, const std::string& value);

    //!
    //! \brief Get the type of contract that this payload contains data for.
    //!
    GRC::ContractType ContractType() const override
    {
        return GRC::ContractType::BEACON;
    }

    //!
    //! \brief Determine whether the instance represents a complete payload.
    //!
    //! \return \c true if the payload contains each of the required elements.
    //!
    bool WellFormed(const ContractAction action) const override
    {
        if (m_version <= 0 || m_version > CURRENT_VERSION) {
            return false;
        }

        if (m_version == 1) {
            return m_beacon.WellFormed() || action == ContractAction::REMOVE;
        }

        return m_beacon.WellFormed()
            // The DER-encoded ASN.1 ECDSA signatures typically contain 70 or
            // 71 bytes, but may hold up to 73. Sizes as low as 68 bytes seen
            // on mainnet. We only check the number of bytes here as an early
            // step:
            && (m_signature.size() >= 64 && m_signature.size() <= 73);
    }

    //!
    //! \brief Get a string for the key used to construct a legacy contract.
    //!
    std::string LegacyKeyString() const override
    {
        return m_cpid.ToString();
    }

    //!
    //! \brief Get a string for the value used to construct a legacy contract.
    //!
    std::string LegacyValueString() const override
    {
        return m_beacon.ToString();
    }

    //!
    //! \brief Get the burn fee amount required to send a particular contract.
    //!
    //! \return Burn fee in units of 1/100000000 GRC.
    //!
    CAmount RequiredBurnAmount() const override
    {
        return 0.5 * COIN;
    }

    //!
    //! \brief Sign the beacon with its private key.
    //!
    //! \param private_key Corresponds to the beacon's public key.
    //!
    //! \return \c false if signing fails.
    //!
    bool Sign(CKey& private_key);

    //!
    //! \brief Validate the authenticity of a beacon by verifying the digital
    //! signature.
    //!
    //! \return \c true if the beacon's signature matches the its public key.
    //!
    bool VerifySignature() const;

    ADD_CONTRACT_PAYLOAD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(
        Stream& s,
        Operation ser_action,
        const ContractAction contract_action)
    {
        READWRITE(m_version);
        READWRITE(m_cpid);
        READWRITE(m_beacon);

        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(m_signature);
        }
    }
}; // BeaconPayload

//!
//! \brief A beacon not yet verified by scraper convergence. TODO: Get rid of this given the changes for
//! LevelDB storage. The only difference at this point is a different Expired function, which can
//! actually be put in the Beacon class Expire as a conditional evaluation.
//!
class PendingBeacon : public Beacon
{
public:
    PendingBeacon() : Beacon()
    {
    };
    //!
    //! \brief Duration in seconds to retain pending beacons before erasure.
    //!
    static constexpr int64_t RETENTION_AGE = 60 * 60 * 24 * 3;

    explicit PendingBeacon(Beacon beacon) : Beacon(beacon)
    {
    };
    //!
    //! \brief Initialize a pending beacon.
    //!
    //! \param cpid   Identifies the researcher that advertised the beacon.
    //! \param beacon Contains the beacon public key.
    //!
    PendingBeacon(const Cpid& cpid, Beacon beacon);

    //!
    //! \brief Determine whether the beacon age exceeds the duration allowed
    //! for retaining a pending beacon.
    //!
    //! \param now Timestamp to consider as the current time.
    //!
    //! \return \c true if a beacon's age exceeds the maximum retention time.
    //!
    bool PendingExpired(const int64_t now) const;
}; // PendingBeacon

//!
//! \brief A beacon that uses different serialization for storage to disk via LevelDB.
//! TODO: Change this to inherit from Beacon, although it doesn't matter really,
//! because the only difference at this point is the Expired function, and that is not
//! used for StorageBeacons.
//!
class StorageBeacon : public PendingBeacon
{
public:
    StorageBeacon() : PendingBeacon()
    {
    };

    explicit StorageBeacon(PendingBeacon beacon) : PendingBeacon(beacon)
    {
    };

    explicit StorageBeacon(Beacon beacon) : PendingBeacon(beacon)
    {
    };

    StorageBeacon(const Cpid& cpid, Beacon beacon) : PendingBeacon(cpid, beacon)
    {
    };

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(m_public_key);
        READWRITE(m_timestamp);
        READWRITE(m_hash);
        READWRITE(m_previous_hash);
        READWRITE(m_cpid);
        READWRITE(m_status);
    }
};

//!
//! \brief Stores and manages researcher beacons.
//!
class BeaconRegistry : public IContractHandler
{
public:
    //!
    //! \brief BeaconRegistry constructor. The parameter is the version number of the underlying
    //! beacon entry db. This must be incremented when implementing format changes to the beacon
    //! entries to force a reinit.
    //!
    //! Version 0: <= 5.2.0.0
    //! Version 1: = 5.2.1.0
    //! Version 2: 5.2.1.0 with hotfix and > 5.2.1.0
    //! Version 3: 5.4.5.5+
    //!
    BeaconRegistry()
        : m_beacon_db(3)
    {
    };

    //!
    //! \brief The type that associates beacons with CPIDs in the registry. This
    //! is done via smart pointers to save memory.
    //!
    typedef std::unordered_map<Cpid, Beacon_ptr> BeaconMap;

    //!
    //! \brief Associates pending beacons with the hash of the beacon public
    //! keys. This is done via smart pointers to save memory.
    typedef std::map<CKeyID, Beacon_ptr> PendingBeaconMap;

    //!
    //! \brief The type that associates historical beacons with a SHA256 hash. This is done
    //! with smart pointers to save memory. Note that most of the time this is the hash of
    //! the beacon contract, but in the case of beacon activations in the superblock, this
    //! is a hash of the superblock hash and the pending beacon that is being activated's hash
    //! to make the records unique.
    //!
    typedef std::map<uint256, Beacon_ptr> HistoricalBeaconMap;

    //!
    //! \brief Get the collection of registered beacons.
    //!
    //! \return A reference to the beacons stored in the registry.
    //!
    const BeaconMap& Beacons() const;

    //!
    //! \brief Get the collection of beacons awaiting verification.
    //!
    //! \return A reference to the pending beacon map stored in the registry.
    //!
    const PendingBeaconMap& PendingBeacons() const;

    //!
    //! \brief Get the set of beacons that have expired while pending (awaiting verification)
    //! \return A reference to the expired pending beacon set.
    //!
    const std::set<Beacon_ptr>& ExpiredBeacons() const;

    //!
    //! \brief Get the beacon for the specified CPID.
    //!
    //! \param cpid The researcher's CPID to look up a beacon for.
    //!
    //! \return An object that either contains a reference to some beacon if
    //! it exists for the CPID or does not.
    //!
    BeaconOption Try(const Cpid& cpid) const;

    //!
    //! \brief Get the beacon for the specified CPID if it did not expire.
    //!
    //! \param cpid The researcher's CPID to look up a beacon for.
    //! \param now  The time in seconds to check from for beacon expiration.
    //!
    //! \return An object that either contains a reference to some beacon if
    //! it is active for the CPID or does not.
    //!
    BeaconOption TryActive(const Cpid& cpid, const int64_t now) const;

    //!
    //! \brief Get the set of pending beacons for the specified CPID.
    //!
    //! \param cpid CPID to find pending beacons for.
    //!
    //! \return A set of pending beacons advertised for the supplied CPID.
    //!
    std::vector<Beacon_ptr> FindPending(const Cpid& cpid) const;

    //!
    //! \brief Find a historical beacon entry from the beacon (txid) hash;
    //! \param txid hash
    //! \return An object that either contains a reference to a historical
    //! beacon entry if found or does not.
    //!
    const BeaconOption FindHistorical(const uint256& hash);

    //!
    //! \brief Determine whether a beacon is active for the specified CPID.
    //!
    //! \param cpid The CPID to check for an active beacon.
    //! \param now  The time in seconds to check from for beacon expiration.
    //!
    //! \return \c true if the registry contains an active beacon for the
    //! specified CPID.
    //!
    bool ContainsActive(const Cpid& cpid, const int64_t now) const;

    //!
    //! \brief Determine whether a beacon is active for the specified CPID at
    //! this moment.
    //!
    //! \param cpid The CPID to check for an active beacon.
    //!
    //! \return \c true if the registry contains an active beacon for the
    //! specified CPID.
    //!
    bool ContainsActive(const Cpid& cpid) const;

    //!
    //! \brief Destroy the contract handler state in case of an error in loading
    //! the beacon registry state from LevelDB to prepare for reload from contract
    //! replay. This is not used for Beacons, unless -clearbeaconhistory is specified
    //! as a startup argument, because contract replay storage and full reversion has
    //! been implemented for beacons.
    //!
    void Reset() override;

    //!
    //! \brief Determine whether a beacon contract is valid.
    //!
    //! \param contract Contains the beacon data to validate.
    //! \param tx       Transaction that contains the contract.
    //! \param DoS      Misbehavior out.
    //!
    //! \return \c true if the contract contains a valid beacon.
    //!
    bool Validate(const Contract& contract, const CTransaction& tx, int &DoS) const override;

    //!
    //! \brief Determine whether a beacon contract is valid including block context. This is used
    //! in ConnectBlock.
    //!
    //! \param ctx ContractContext containing the beacon data to validate.
    //! \param DoS Misbehavior score out.
    //!
    //! \return  \c false If the contract fails validation.
    //!
    bool BlockValidate(const ContractContext& ctx, int& DoS) const override;

    //!
    //! \brief Register a beacon from contract data.
    //!
    //! \param ctx References the beacon contract and associated context.
    //!
    void Add(const ContractContext& ctx) override;

    //!
    //! \brief Deregister the beacon specified by contract data. Note this is the
    //! REMOVE contract action and is different than Revert below.
    //!
    //! \param ctx References the beacon contract and associated context.
    //!
    void Delete(const ContractContext& ctx) override;

    //!
    //! \brief Revert the registry state for the cpid to the state prior
    //! to this ContractContext application. This is typically an issue
    //! during reorganizations, where blocks are disconnected.
    //!
    //! \param ctx References the beacon contract and associated context.
    //!
    void Revert(const ContractContext& ctx) override;

    //!
    //! \brief Activate the set of pending beacons verified in a superblock. This is called
    //! when the superblock is committed and connected.
    //!
    //! \param beacon_ids      The key IDs of the beacons to activate.
    //! \param superblock_time Timestamp of the superblock.
    //! \param height          Height of the superblock
    //!
    void ActivatePending(const std::vector<uint160>& beacon_ids,
        const int64_t superblock_time, const uint256& block_hash, const int &height);

    //!
    //! \brief Deactivate the set of beacons verified in a superblock. This is called in BlockDisconnectBatch
    //! when a block as part of the batch to be disconnected in a reorg is a superblock, and undoes the
    //! ActivatePending that was applied for that superblock, i.e. it is the inverse of ActivatePending.
    //!
    //! \param hash of the superblock.
    //!
    void Deactivate(const uint256 superblock_hash);

    //!
    //! \brief Initialize the BeaconRegistry, which now includes restoring the state of the BeaconRegistry from
    //! LevelDB on wallet start.
    //!
    //! \return Block height of the database restored from LevelDB. Zero if no LevelDB beacon data is found or
    //! there is some issue in LevelDB beacon retrieval. (This will cause the contract replay to change scope
    //! and initialize the BeaconRegistry from contract replay and store in LevelDB.)
    //!
    int Initialize() override;

    //!
    //! \brief Gets the block height through which is stored in the beacon registry database.
    //!
    //! \return block height.
    //!
    int GetDBHeight() override;

    //!
    //! \brief Function normally only used after a series of reverts during block disconnects, because
    //! block disconnects are done in groups back to a common ancestor, and will include a series of reverts.
    //! This is essentially atomic, and therefore the final (common) height only needs to be set once. TODO:
    //! reversion should be done with a vector argument of the contract contexts, along with a final height to
    //! clean this up and move the logic to here from the calling function.
    //!
    //! \param height to set the storage DB bookmark.
    //!
    void SetDBHeight(int& height) override;

    //!
    //! \brief Resets the maps in the BeaconRegistry but does not disturb the underlying LevelDB
    //! storage. This is only used during testing in the testing harness.
    //!
    void ResetInMemoryOnly();

    //!
    //! \brief Passivates the elements in the beacon db, which means remove from memory elements in the
    //! historical map that are not referenced by either the m_beacons or m_pending. The backing store of
    //! the element removed from memory is retained and will be transparently restored if find()
    //! is called on the hash key for the element.
    //!
    //! \return The number of elements passivated.
    //!
    uint64_t PassivateDB() override;

    //!
    //! \brief This function walks the linked beacon entries back (using the m_previous_hash member) from a provided
    //! beacon to find the initial advertisement. Note that this does NOT traverse non-continuous beacon ownership,
    //! which occurs when a beacon is allowed to expire and must be reverified under a new key.
    //!
    //! \param beacon smart shared pointer to beacon entry to begin walking back
    //! \param beacon_chain_out shared pointer to UniValue beacon chain out report array
    //! \return root (advertisement) beacon entry smart shared pointer
    //!
    Beacon_ptr GetBeaconChainletRoot(Beacon_ptr beacon,
                                     std::shared_ptr<std::vector<std::pair<uint256, int64_t>>> beacon_chain_out = nullptr);

    //!
    //! \brief Returns whether IsContract correction is needed in ReplayContracts during initialization
    //! \return
    //!
    bool NeedsIsContractCorrection();

    //!
    //! \brief Sets the state of the IsContract needs correction flag in memory and LevelDB
    //! \param flag The state to set
    //! \return
    //!
    bool SetNeedsIsContractCorrection(bool flag);

    //!
    //! \brief Specializes the template RegistryDB for the ScraperEntry class
    //!
    typedef RegistryDB<Beacon,
                       StorageBeacon,
                       BeaconStatusForStorage,
                       BeaconMap,
                       PendingBeaconMap,
                       std::set<Beacon_ptr>,
                       HistoricalBeaconMap> BeaconDB;

private:
    //!
    //! \brief Protects the registry with multithreaded access. This is implemented INTERNAL to the registry class.
    //!
    mutable CCriticalSection cs_lock;

    BeaconMap m_beacons;        //!< Contains the active registered beacons.
    PendingBeaconMap m_pending; //!< Contains beacons awaiting verification.

    //!
    //! \brief Contains pending beacons that have expired.
    //!
    //! Contains pending beacons that have expired but need to be retained until the next SB (activation) to ensure a
    //! reorganization will successfully resurrect expired pending beacons back into pending ones up to the depth equal to one SB to
    //! the next, which is about 960 blocks. The reason this is necessary is two fold: 1) it makes the lookup for expired
    //! pending beacons in the deactivate method much simpler in the case of a reorg across a SB boundary, and 2) it holds
    //! a reference to the pending beacon shared pointer object in the history map, which prevents it from being passivated.
    //! Otherwise, a passivation event, which would remove the pending deleted beacons, followed by a reorganization across
    //! SB boundary could have a small possibility of removing a pending beacon that could be verified in the alternative SB
    //! eventually staked.
    //!
    //! This set is cleared and repopulated at each SB accepted by the node with the current expired pending beacons.
    //!
    std::set<Beacon_ptr> m_expired_pending;

    //!
    //! \brief The member variable that is the instance of the beacon database. This is private to the
    //! beacon registry and is only accessible by beacon registry functions.
    //!
    BeaconDB m_beacon_db;

    //!
    //! \brief The function that is used in Add() to try to renew a beacon by matching the incoming beacon's
    //! public key.
    //!
    //! \param current_beacon_ptr A smart shared pointer to the current beacon that is a candidate for renewal
    //! by the incoming payload.
    //! \param height The height of the transaction (context) that represents the renewal attempt.
    //! \param payload The incoming payload of the transaction context to be validated for renewal.
    //!
    //! \return Success or failure of renewal attempt.
    //!
    bool TryRenewal(Beacon_ptr& current_beacon_ptr, int& height, const BeaconPayload& payload);

public:
    //!
    //! \brief Gets a reference to beacon database.
    //!
    //! \return Current beacon database instance.
    //!
    BeaconDB& GetBeaconDB();

}; // BeaconRegistry

//!
//! \brief Get the global beacon registry.
//!
//! \return Current global beacon registry instance.
//!
BeaconRegistry& GetBeaconRegistry();
}

#endif // GRIDCOIN_BEACON_H
