// Copyright (c) 2014-2024 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_PROJECT_H
#define GRIDCOIN_PROJECT_H

#include "amount.h"
#include "contract/contract.h"
#include "gridcoin/contract/handler.h"
#include "gridcoin/contract/payload.h"
#include "gridcoin/contract/registry_db.h"
#include "serialize.h"
#include "pubkey.h"
#include "sync.h"

#include <memory>
#include <vector>
#include <string>

namespace GRC
{
//!
//! \brief Enumeration of project entry status. Unlike beacons this is for both storage
//! and memory.
//!
//! UNKNOWN status is only encountered in trivially constructed empty
//! project entries and should never be seen on the blockchain.
//!
//! DELETED status corresponds to a removed entry.
//!
//! ACTIVE corresponds to an active entry.
//!
//! OUT_OF_BOUND must go at the end and be retained for the EnumBytes wrapper.
//!
enum class ProjectEntryStatus
{
    UNKNOWN,
    DELETED,
    ACTIVE,
    OUT_OF_BOUND
};

class ProjectEntry
{
public:
    //!
    //! \brief Wrapped Enumeration of scraper entry status, mainly for serialization/deserialization.
    //!
    using Status = EnumByte<ProjectEntryStatus>;

    //!
    //! \brief Version number of the current format for a serialized project.
    //!
    //! CONSENSUS: Increment this value when introducing a breaking change and
    //! ensure that the serialization/deserialization routines also handle all
    //! of the previous versions.
    //!
    static constexpr uint32_t CURRENT_VERSION = 3;

    //!
    //! \brief Version number of the serialized project format.
    //!
    //! Defaults to the most recent version for a new project instance.
    //!
    uint32_t m_version = CURRENT_VERSION;

    std::string m_name;                   //!< As it exists in the contract key field, this is the project name.
    std::string m_url;                   //!< As it exists in the contract url field.
    int64_t m_timestamp;                 //!< Timestamp of the contract.
    uint256 m_hash;                      //!< The txid of the transaction that contains the project entry.
    uint256 m_previous_hash;             //!< The m_hash of the previous project entry with the same key.
    bool m_gdpr_controls;                //!< Boolean to indicate whether project has GDPR stats export controls.
    CPubKey m_public_key;                //!< Project public key.
    Status m_status;                     //!< The status of the project entry. (Note serialization converts to/from int.)

    //!
    //! \brief Initialize an empty, invalid project entry instance.
    //!
    ProjectEntry(uint32_t version = CURRENT_VERSION);

    //!
    //! \brief Initialize a new project entry for submission in a contract (with ACTIVE status)
    //!
    //! \param version. Project entry version. This is identical to the payload version that formed it.
    //! \param name. The key of the project entry.
    //! \param url. The url of the project entry.
    //! \param gdpr_controls. The gdpr control flag of the project entry
    //!
    ProjectEntry(uint32_t version, std::string name, std::string url);

    //!
    //! \brief Initialize a new project entry for submission in a contract (with ACTIVE status)
    //!
    //! \param version. Project entry version. This is identical to the payload version that formed it.
    //! \param name. The key of the project entry.
    //! \param url. The url of the project entry.
    //! \param gdpr_controls. The gdpr control flag of the project entry
    //!
    ProjectEntry(uint32_t version, std::string name, std::string url, bool gdpr_controls);

    //!
    //! \brief Initialize a project entry instance with data from a contract.
    //!
    //! \param version. Project entry version. This is identical to the payload version that formed it.
    //! \param name. The key of the project entry.
    //! \param url. The value of the project entry.
    //! \param gdpr_controls. The gdpr control flag of the project entry
    //! \param status. the status of the project entry.
    //! \param timestamp. The timestamp of the project entry that comes from the containing transaction
    //!
    ProjectEntry(uint32_t version, std::string name, std::string url, bool gdpr_controls, Status status, int64_t timestamp);

    //!
    //! \brief Determine whether a project entry contains each of the required elements.
    //!
    //! \return \c true if the project entry is complete.
    //!
    bool WellFormed() const;

    //!
    //! \brief This is the standardized method that returns the key value for the project entry (for
    //! the registry_db.h template.)
    //!
    //! \return std::string project name, which is the key for the project entry
    //!
    std::string Key() const;

    //!
    //! \brief Provides the project name and url (the key and principal value) as a pair.
    //! \return std::pair of strings
    //!
    std::pair<std::string, std::string> KeyValueToString() const;

    //!
    //! \brief Returns the string representation of the current project entry status
    //!
    //! \return Translated string representation of project status
    //!
    std::string StatusToString() const;

    //!
    //! \brief Returns the translated or untranslated string of the input project entry status
    //!
    //! \param status. ProjectEntryStatus
    //! \param translated. True for translated, false for not translated. Defaults to true.
    //!
    //! \return Project entry status string.
    //!
    std::string StatusToString(const ProjectEntryStatus& status, const bool& translated = true) const;

    //!
    //! \brief Get a user-friendly display name created from the project key.
    //!
    std::string DisplayName() const;

    //!
    //! \brief Get the root URL of the project's BOINC server website.
    //!
    std::string BaseUrl() const;

    //!
    //! \brief Get a user-accessible URL to the project's BOINC website.
    //!
    std::string DisplayUrl() const;

    //!
    //! \brief Get the URL to the project's statistics export files.
    //!
    //! \param type If empty, return a URL to the project's stats directory.
    //!             Otherwise, return a URL to the specified export archive.
    //!
    std::string StatsUrl(const std::string& type = "") const;

    //!
    //! \brief Returns true if project has project stats GDPR export controls
    //!
    std::optional<bool> HasGDPRControls() const;

    //!
    //! \brief Comparison operator overload used in the unit test harness.
    //!
    //! \param b The right hand side project entry to compare for equality.
    //!
    //! \return Equal or not.
    //!
    bool operator==(ProjectEntry b);

    //!
    //! \brief Comparison operator overload used in the unit test harness.
    //!
    //! \param b The right hand side project entry to compare for equality.
    //!
    //! \return Equal or not.
    //!
    bool operator!=(ProjectEntry b);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(m_version);
        READWRITE(m_name);
        READWRITE(m_url);
        READWRITE(m_timestamp);
        READWRITE(m_hash);
        READWRITE(m_gdpr_controls);
        READWRITE(m_previous_hash);
        READWRITE(m_status);
    }
};

//!
//! \brief The type that defines a shared pointer to a ProjectEntry
//!
typedef std::shared_ptr<ProjectEntry> ProjectEntry_ptr;

//!
//! \brief A type that either points to some ProjectEntry or does not.
//!
typedef const ProjectEntry_ptr ProjectEntryOption;

//!
//! \brief Represents a BOINC project in the Gridcoin whitelist.
//!
class Project : public IContractPayload, public ProjectEntry
{
public:
    //!
    //! \brief The maximum number of characters allowed for a serialized project
    //! name field.
    //!
    static constexpr size_t MAX_NAME_SIZE = 100;

    //!
    //! \brief The maximum number of characters allowed for a serialized project
    //! URL field.
    //!
    //! This is probably far more than we'd ever need because the project URLs
    //! contain shallow paths. The limit just exists to discourage spam.
    //!
    static constexpr size_t MAX_URL_SIZE = 500;

    //!
    //! \brief Initialize an empty, invalid project object.
    //!
    Project(uint32_t version = CURRENT_VERSION);

    //!
    //! \brief Initialize a \c Project using data from the contract.
    //!
    //! \param name          Project name from contract message key.
    //! \param url           Project URL from contract message value.
    //!
    Project(std::string name, std::string url);

    //!
    //! \brief Initialize a \c Project using data from the contract.
    //!
    //! \param version       Project payload version.
    //! \param name          Project name from contract message key.
    //! \param url           Project URL from contract message value.
    //!
    Project(uint32_t version, std::string name, std::string url);

    //!
    //! \brief Initialize a \c Project using data from the contract.
    //!
    //! \param name          Project name from contract message key.
    //! \param url           Project URL from contract message value.
    //! \param timestamp     Timestamp
    //! \param version       Project payload version.
    //!
    Project(std::string name, std::string url, int64_t timestamp, uint32_t version = CURRENT_VERSION);

    //!
    //! \brief Initialize a \c Project using data from the contract.
    //!
    //! \param version       Project payload version.
    //! \param name          Project name from contract message key.
    //! \param url           Project URL from contract message value.
    //! \param gdpr_controls Boolean to indicate gdpr stats export controls enforced
    //!
    Project(uint32_t version, std::string name, std::string url, bool gdpr_controls);

    //!
    //! \brief Initialize a \c Project using data from the contract.
    //!
    //! \param version       Project payload version.
    //! \param name          Project name from contract message key.
    //! \param url           Project URL from contract message value.
    //! \param gdpr_controls Boolean to indicate gdpr stats export controls enforced
    //! \param timestamp     Timestamp
    //!
    Project(uint32_t version, std::string name, std::string url, bool gdpr_controls, int64_t timestamp);

    //!
    //! \brief Initialize a \c Project using data from the contract.
    //!
    //! \param name          Project name from contract message key.
    //! \param url           Project URL from contract message value.
    //! \param timestamp     Timestamp
    //! \param version       Project payload version.
    //! \param gdpr_controls Boolean to indicate gdpr stats export controls enforced
    //!
    Project(std::string name, std::string url, int64_t timestamp, uint32_t version, bool gdpr_controls);

    //!
    //! \brief Initialize a \c Project payloard from a provided project entry
    //! \param version. Project Payload version.
    //! \param entry. Project entry to place in the payload.
    //!
    Project(ProjectEntry entry);

    //!
    //! \brief Get the type of contract that this payload contains data for.
    //!
    GRC::ContractType ContractType() const override
    {
        return GRC::ContractType::PROJECT;
    }

    //!
    //! \brief Determine whether the object contains a well-formed payload.
    //!
    //! \param action The action declared for the contract that contains the
    //! payload. It may determine how to validate the payload.
    //!
    //! \return \c true if the payload is complete.
    //!
    bool WellFormed(const ContractAction action) const override
    {
        if (m_name.empty()) {
            return false;
        }

        if (action != ContractAction::REMOVE && m_url.empty()) {
            return false;
        }

        // m_gdpr_controls is not serialized unless the contract version is v2+
        if (m_version < 2 && m_gdpr_controls) {
            return false;
        }

        return true;
    }

    //!
    //! \brief Get a string for the key used to construct a legacy contract.
    //!
    std::string LegacyKeyString() const override
    {
        return m_name;
    }

    //!
    //! \brief Get a string for the value used to construct a legacy contract.
    //!
    std::string LegacyValueString() const override
    {
        return m_url;
    }

    //!
    //! \brief Get the burn fee amount required to send a particular contract.
    //!
    //! \return Burn fee in units of 1/100000000 GRC.
    //!
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
        READWRITE(LIMITED_STRING(m_name, MAX_NAME_SIZE));

        if (contract_action != ContractAction::REMOVE) {
            READWRITE(LIMITED_STRING(m_url, MAX_URL_SIZE));

            if (m_version >= 2) {
                READWRITE(m_gdpr_controls);
                READWRITE(m_public_key);
            }
        }

        if (m_version >= 3) {
            READWRITE(m_hash);
            READWRITE(m_previous_hash);
            READWRITE(m_status);
        }
    }
}; // Project (entry payload)

//!
//! \brief A collection of projects in the Gridcoin whitelist.
//!
typedef std::map<std::string, ProjectEntry> ProjectEntryMap;
typedef std::vector<ProjectEntry> ProjectList;

//!
//! \brief Smart pointer around a collection of projects.
//!
typedef std::shared_ptr<ProjectList> ProjectListPtr;

//!
//! \brief Read-only view of the Gridcoin project whitelist at a point in time.
//!
class WhitelistSnapshot
{
public:
    typedef ProjectList::size_type size_type;
    typedef ProjectList::iterator iterator;
    typedef ProjectList::const_iterator const_iterator;

    //!
    //! \brief Initialize a snapshot for the provided project list from the project entry map.
    //!
    //! \param projects A copy of the smart pointer to the project list.
    //!
    WhitelistSnapshot(ProjectListPtr projects);

    //!
    //! \brief Returns an iterator to the beginning.
    //!
    const_iterator begin() const;

    //!
    //! \brief Returns an iterator to the end.
    //!
    const_iterator end() const;

    //!
    //! \brief Get the number of projects in the whitelist.
    //!
    size_type size() const;

    //!
    //! \brief Determine whether the whitelist contains any projects.
    //!
    //! This does not guarantee that the whitelist is up-to-date. The caller is
    //! responsible for verifying the block height.
    //!
    //! \return \c true if the whitelist contains at least one project.
    //!
    bool Populated() const;

    //!
    //! \brief Determine whether the specified project exists in the whitelist.
    //!
    //! \param name Project name matching the contract key.
    //!
    //! \return \c true if the whitelist contains a project with matching name.
    //!
    bool Contains(const std::string& name) const;

    //!
    //! \brief Create a snapshot copy sorted alphabetically by project name.
    //!
    //! \return A sorted copy of the snapshot.
    //!
    WhitelistSnapshot Sorted() const;

private:
    const ProjectListPtr m_projects;  //!< The vector of whitelisted projects.
};

//!
//! \brief Registry that manages the collection of BOINC projects in the Gridcoin whitelist.
//!
class Whitelist : public IContractHandler
{
public:
    //!
    //! \brief Initializes the project whitelist manager. The version must be incremented when
    //! introducing a breaking change in the storage format (serialization) of the project entry.
    //!
    //! Version 0: <= 5.4.2.0 where there was no backing db.
    //! Version 1: TBD.
    //!
    Whitelist()
        :m_project_db(1)
    {
    };

    //!
    //! \brief The type that keys project entries by their key strings. Note that the entries
    //! in this map are actually smart shared pointer wrappers, so that the same actual object
    //! can be held by both this map and the historical map without object duplication.
    //!
    typedef std::map<std::string, ProjectEntry_ptr> ProjectEntryMap;

    //!
    //! \brief PendingProjectEntryMap. Not actually used here, but required for the template.
    //!
    typedef ProjectEntryMap PendingProjectEntryMap;

    //!
    //! \brief The type that keys historical project entries by the contract hash (txid).
    //! Note that the entries in this map are actually smart shared pointer wrappers, so that
    //! the same actual object can be held by both this map and the (current) project entry map
    //! without object duplication.
    //!
    typedef std::map<uint256, ProjectEntry_ptr> HistoricalProjectEntryMap;

    //!
    //! \brief Get a read-only view of the projects in the whitelist.
    //!
    WhitelistSnapshot Snapshot() const;

    //!
    //! \brief Destroy the contract handler state to prepare for historical
    //! contract replay.
    //!
    void Reset() override;

    //!
    //! \brief Perform contextual validation for the provided contract.
    //!
    //! \param contract Contract to validate.
    //! \param tx       Transaction that contains the contract.
    //! \param DoS      Misbehavior score out
    //!
    //! \return \c false If the contract fails validation.
    //!
    bool Validate(const Contract& contract, const CTransaction& tx, int& DoS) const override;

    //!
    //! \brief Perform contextual validation for the provided contract including block context. This is used
    //! in ConnectBlock.
    //!
    //! \param ctx ContractContext to validate.
    //! \param DoS Misbehavior score out.
    //!
    //! \return \c false If the contract fails validation.
    //!
    bool BlockValidate(const ContractContext& ctx, int& DoS) const override;

    //!
    //! \brief Add a project to the whitelist from contract data.
    //!
    //! \param ctx References the project contract and associated context.
    //!
    void Add(const ContractContext& ctx) override;

    //!
    //! \brief Remove the specified project from the whitelist.
    //!
    //! \param ctx References the project contract and associated context.
    //!
    void Delete(const ContractContext& ctx) override;

    //!
    //! \brief Revert the registry state for the project entry to the state prior
    //! to this ContractContext application. This is typically an issue
    //! during reorganizations, where blocks are disconnected.
    //!
    //! \param ctx References the project entry contract and associated context.
    //!
    void Revert(const ContractContext& ctx) override;

    //!
    //! \brief Initialize the Project Whitelist (registry), which now includes restoring the state of the whitelist from
    //! LevelDB on wallet start.
    //!
    //! \return Block height of the database restored from LevelDB. Zero if no LevelDB project entry data is found or
    //! there is some issue in LevelDB project entry retrieval. (This will cause the contract replay to change scope
    //! and initialize the Whitelist from contract replay and store in LevelDB.)
    //!
    int Initialize() override;

    //!
    //! \brief Gets the block height through which is stored in the project entry registry database.
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
    //! \brief Resets the maps in the Whitelist but does not disturb the underlying LevelDB
    //! storage. This is only used during testing in the testing harness.
    //!
    void ResetInMemoryOnly();

    //!
    //! \brief Passivates the elements in the project db, which means remove from memory elements in the
    //! historical map that are not referenced by m_projects. The backing store of the element removed
    //! from memory is retained and will be transparently restored if find() is called on the hash key
    //! for the element.
    //!
    //! \return The number of elements passivated.
    //!
    uint64_t PassivateDB() override;

    //!
    //! \brief Specializes the template RegistryDB for the ScraperEntry class. Note that std::set<ProjectEntry> is not
    //! actually used.
    //!
    typedef RegistryDB<ProjectEntry,
                       ProjectEntry,
                       ProjectEntryStatus,
                       ProjectEntryMap,
                       PendingProjectEntryMap,
                       std::set<ProjectEntry>,
                       HistoricalProjectEntryMap> ProjectEntryDB;

private:
    //!
    //! \brief Protects the registry with multithreaded access. This is implemented INTERNAL to the registry class.
    //!
    mutable CCriticalSection cs_lock;

    //!
    //! \brief Private helper method for the Add and Delete methods above. They both use identical code (with
    //! different input statuses).
    //!
    //! \param ctx The contract context for the add or delete.
    //!
    void AddDelete(const ContractContext& ctx);

    ProjectEntryMap m_project_entries;                   //!< The set of whitelisted projects.
    PendingProjectEntryMap m_pending_project_entries {}; //!< Not actually used. Only to satisfy the template.

    std::set<ProjectEntry> m_expired_project_entries {}; //!< Not actually used. Only to satisfy the template.

    ProjectEntryDB m_project_db; //!< The project db member
public:

    ProjectEntryDB& GetProjectDB();
}; // Whitelist (Project Registry)

//!
//! \brief Get the global project whitelist registry.
//!
//! \return Current global whitelist registry instance.
//!
Whitelist& GetWhitelist();
} // namespace GRC

#endif // GRIDCOIN_PROJECT_H
