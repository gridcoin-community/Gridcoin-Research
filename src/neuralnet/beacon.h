#pragma once

#include "key.h"
#include "neuralnet/contract/handler.h"
#include "neuralnet/contract/payload.h"
#include "neuralnet/cpid.h"
#include "serialize.h"

#include <string>
#include <vector>

class CBitcoinAddress;

namespace NN {

class Contract;

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
    //! \brief Duration in seconds to consider a beacon active.
    //!
    static constexpr int64_t MAX_AGE = 60 * 60 * 24 * 30 * 6;

    //!
    //! \brief Duration in seconds to consider a beacon eligible for renewal.
    //!
    static constexpr int64_t RENEWAL_AGE = 60 * 60 * 24 * 30 * 5;

    CPubKey m_public_key;      //!< Verifies blocks that claim research rewards.
    int64_t m_timestamp;       //!< Time of the the beacon contract transaction.

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
    Beacon(CPubKey public_key, int64_t tx_timestamp);

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
    //! \brief Get the legacy string representation of a version 1 beacon
    //! contract.
    //!
    //! \return Beacon data in the legacy, semicolon-delimited string format.
    //!
    std::string ToString() const;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(m_public_key);
    }
};

//!
//! \brief A type that either points to to some beacon or does not.
//!
typedef const Beacon* BeaconOption;

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
    //! Version 1: Legacy semicolon-delimied string of fields parsed from a
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
    //! \param cpid   Identifies the researcher that advertised the beacon.
    //! \param beacon Contains the beacon public key.
    //!
    BeaconPayload(const Cpid cpid, Beacon beacon);

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
    NN::ContractType ContractType() const override
    {
        return ContractType::BEACON;
    }

    //!
    //! \brief Determine whether the instance represents a complete payload.
    //!
    //! \return \c true if the payload contains each of the required elements.
    //!
    bool WellFormed(const ContractAction action) const override
    {
        return m_version > 0
            && m_version <= CURRENT_VERSION
            && (action == ContractAction::REMOVE || m_beacon.WellFormed())
            // The DER-encoded ASN.1 ECDSA signatures typically contain 70 or
            // 71 bytes, but may hold up to 73. Sizes as low as 68 bytes seen
            // on mainnet. We only check the number of bytes here as an early
            // step:
            && (m_version == 1
                || (m_signature.size() >= 64 && m_signature.size() <= 73));
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
    int64_t RequiredBurnAmount() const override
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

        if (contract_action != ContractAction::REMOVE) {
            READWRITE(m_beacon);
        }

        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(m_signature);
        }
    }
}; // BeaconPayload

//!
//! \brief A beacon not yet verified by scraper convergence.
//!
class PendingBeacon : public Beacon
{
public:
    //!
    //! \brief Duration in seconds to retain pending beacons before erasure.
    //!
    static constexpr int64_t RETENTION_AGE = 60 * 60 * 24 * 3;

    Cpid m_cpid; // Identifies the researcher that advertised the beacon.

    //!
    //! \brief Initialize a pending beacon.
    //!
    //! \param cpid   Identifies the researcher that advertised the beacon.
    //! \param beacon Contains the beacon public key.
    //!
    PendingBeacon(const Cpid cpid, Beacon beacon);

    //!
    //! \brief Determine whether the beacon age exceeds the duration allowed
    //! for retaining a pending beacon.
    //!
    //! \param now Timestamp to consider as the current time.
    //!
    //! \return \c true if a beacon's age exceeds the maximum retention time.
    //!
    bool Expired(const int64_t now) const;
}; // PendingBeacon

//!
//! \brief Stores and manages researcher beacons.
//!
class BeaconRegistry : public IContractHandler
{
public:
    //!
    //! \brief The type that associates beacons with CPIDs in the registry.
    //!
    typedef std::unordered_map<Cpid, Beacon> BeaconMap;

    //!
    //! \brief Associates pending beacons with the hash of the beacon public
    //! keys.
    //!
    typedef std::map<CKeyID, PendingBeacon> PendingBeaconMap;

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
    //! \brief Look up the key IDs of pending beacons for the specified CPID.
    //!
    //! The wallet matches key IDs returned by this method to determine whether
    //! it contains private keys for pending beacons so that it can skip beacon
    //! advertisement if it submitted one recently.
    //!
    //! \param cpid CPID of the beacons to find results for.
    //!
    //! \return The set of RIPEMD-160 hashes of the keys for the beacons that
    //! match the supplied CPID.
    //!
    std::vector<CKeyID> FindPendingKeys(const Cpid& cpid) const;

    //!
    //! \brief Determine whether a beacon contract is valid.
    //!
    //! \param contract Contains the beacon data to validate.
    //!
    //! \return \c true if the contract contains a valid beacon.
    //!
    bool Validate(const Contract& contract) const;

    //!
    //! \brief Register a beacon from contract data.
    //!
    //! \param contract Contains information about the beacon to add.
    //!
    void Add(Contract contract) override;

    //!
    //! \brief Deregister the beacon specified by contract data.
    //!
    //! \param contract Contains information about the beacon to remove.
    //!
    void Delete(const Contract& contract) override;

    //!
    //! \brief Activate the set of pending beacons verified in a superblock.
    //!
    //! \param beacon_ids      The key IDs of the beacons to activate.
    //! \param superblock_time Timestamp of the superblock.
    //!
    void ActivatePending(
        const std::vector<uint160>& beacon_ids,
        const int64_t superblock_time);

    //!
    //! \brief Deactivate the set of beacons verified in a superblock.
    //!
    //! \param superblock_time Timestamp of the superblock.
    //!
    void Deactivate(const int64_t superblock_time);

private:
    BeaconMap m_beacons;        //!< Contains the active registered beacons.
    PendingBeaconMap m_pending; //!< Contains beacons awaiting verification.
}; // BeaconRegistry

//!
//! \brief Get the global beacon registry.
//!
//! \return Current global beacon registry instance.
//!
BeaconRegistry& GetBeaconRegistry();
}
