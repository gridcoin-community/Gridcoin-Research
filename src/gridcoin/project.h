// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "amount.h"
#include "gridcoin/contract/handler.h"
#include "gridcoin/contract/payload.h"
#include "serialize.h"

#include <memory>
#include <vector>
#include <string>

namespace GRC
{
//!
//! \brief Represents a BOINC project in the Gridcoin whitelist.
//!
class Project : public IContractPayload
{
public:
    //!
    //! \brief Version number of the current format for a serialized project.
    //!
    //! CONSENSUS: Increment this value when introducing a breaking change and
    //! ensure that the serialization/deserialization routines also handle all
    //! of the previous versions.
    //!
    static constexpr uint32_t CURRENT_VERSION = 1;

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
    //! \brief Version number of the serialized project format.
    //!
    //! Defaults to the most recent version for a new project instance.
    //!
    uint32_t m_version = CURRENT_VERSION;

    std::string m_name;   //!< As it exists in the contract key field.
    std::string m_url;    //!< As it exists in the contract value field.
    int64_t m_timestamp;  //!< Timestamp of the contract.

    //!
    //! \brief Initialize an empty, invalid project object.
    //!
    Project();

    //!
    //! \brief Initialize a new project for submission in a transaction.
    //!
    //! \param name      Project name from contract message key.
    //! \param url       Project URL from contract message value.
    //!
    Project(std::string name, std::string url);

    //!
    //! \brief Initialize a \c Project using data from the contract.
    //!
    //! \param name      Project name from contract message key.
    //! \param url       Project URL from contract message value.
    //! \param timestamp Contract timestamp.
    //!
    Project(std::string name, std::string url, int64_t timestamp);

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
        return !m_name.empty()
            && (action == ContractAction::REMOVE || !m_url.empty());
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
        }
    }
};

//!
//! \brief A collection of projects in the Gridcoin whitelist.
//!
typedef std::vector<Project> ProjectList;

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
    //! \brief Initialize a snapshot for the provided project list.
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
    //! responsible for verifiying the block height.
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
    const ProjectListPtr m_projects;  //!< The set of whitelisted projects.
};

//!
//! \brief Manages the collection of BOINC projects in the Gridcoin whitelist.
//!
//! An object of this class stores in memory the set of data that represents
//! the BOINC projects in the Gridcoin whitelist. The application collects this
//! data from network policy messages published to the network as contracts in
//! a transaction.
//!
//! A \c Whitelist instance provides no direct access to the data it stores.
//! Consumers call the \c Snapshot() method to obtain a view of the data. These
//! \c WhitelistSnapshot instances enable read-only access to the project data
//! as it existed at the time of their construction.
//!
//! This class implements copy-on-write semantics to facilitate thread-safety
//! and to reduce overhead. It uses smart pointers to share ownership of project
//! data with snapshots until the application mutates the data by adding or
//! removing a project. During mutation, a \c Whitelist object copies its data
//! and stores that behind a new smart pointer as an atomic operation, so any
//! existing \c WhitelistSnapshot instances become obsolete.
//!
//! For this reason, consumers of this data shall not hold long-lived snapshot
//! objects. Instead, they poll for the current whitelist data and retain it
//! briefly as needed for each local routine. Because whitelisted projects
//! change so infrequently, this design enables efficient, lock-free access to
//! the data.
//!
//! Although this class protects against undefined behavior that results from
//! access to its data by multiple threads, it does not guard against a data
//! race that occurs when two threads mutate the project data at the same time.
//! Only the result of one thread's operation will persist because of the time
//! needed to copy the project data before the atomic update. The application
//! only modifies this data from one thread now. The implementation needs more
//! coarse locking if it will support multiple writers in the future.
//!
class Whitelist : public IContractHandler
{
public:
    //!
    //! \brief Initializes the project whitelist manager.
    //!
    Whitelist();

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
    //!
    //! \return \c false If the contract fails validation.
    //!
    bool Validate(const Contract& contract, const CTransaction& tx) const override
    {
        return true; // No contextual validation needed yet
    }

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

private:
    // With C++20, use std::atomic<std::shared_ptr<T>> instead:
    ProjectListPtr m_projects;  //!< The set of whitelisted projects.

    //!
    //! \brief Create a copy of the current whitelist that excludes the project
    //! with the specified name if it exists.
    //!
    //! \param name Project name to exclude from the copy.
    //!
    //! \return A smart pointer to the copy of the whitelist.
    //!
    ProjectListPtr CopyFilteredWhitelist(const std::string& name) const;
};

//!
//! \brief Get the global project whitelist manager.
//!
//! \return Current global whitelist manager instance.
//!
Whitelist& GetWhitelist();
}
