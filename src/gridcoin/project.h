// Copyright (c) 2014-2024 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_PROJECT_H
#define GRIDCOIN_PROJECT_H

#include "amount.h"
#include "contract/contract.h"
#include "gridcoin/fwd.h"
#include "gridcoin/superblock.h"
#include "gridcoin/contract/handler.h"
#include "gridcoin/contract/payload.h"
#include "gridcoin/contract/registry_db.h"
#include "serialize.h"
#include "pubkey.h"
#include "sync.h"
#include "node/ui_interface.h"

#include <memory>
#include <vector>
#include <string>

namespace GRC
{
// See gridcoin/fwd.h for ProjectEntryStatus. It is in the forward declaration file because it is
// also needed in the superblock class, and this prevents recursive includes.

class ProjectEntry
{
public:
    //!
    //! \brief Wrapped Enumeration of scraper entry status, mainly for serialization/deserialization.
    //!
    using Status = EnumByte<ProjectEntryStatus>;

    //!
    //! \brief Project filter flag enumeration.
    //!
    //! This controls what project entries by status are in the project whitelist snapshot. Note that REG_ACTIVE
    //! is the original "ACTIVE" and represents project entries with a status of "ACTIVE" in the registry. The
    //! filter flag "ACTIVE" here includes both REG_ACTIVE and AUTO_GREYLIST_OVERRIDE project entry statuses from
    //! the registry, since both mean the project is active assuming a convergence can be formed.
    //!
    enum ProjectFilterFlag : uint8_t {
        NONE                   = 0b00000,
        DELETED                = 0b00001,
        MAN_GREYLISTED         = 0b00010,
        AUTO_GREYLISTED        = 0b00100,
        GREYLISTED             = MAN_GREYLISTED | AUTO_GREYLISTED,
        REG_ACTIVE             = 0b01000,
        AUTO_GREYLIST_OVERRIDE = 0b10000,
        ACTIVE                 = REG_ACTIVE | AUTO_GREYLIST_OVERRIDE,
        NOT_ACTIVE             = 0b00111,
        ALL_BUT_DELETED        = 0b11110,
        ALL                    = 0b11111
    };

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
    //! \param version         Project payload version.
    //! \param name            Project name from contract message key.
    //! \param url             Project URL from contract message value.
    //! \param gdpr_controls   Boolean to indicate gdpr stats export controls enforced
    //! \param status          ProjectEntryStatus to force project status.
    //!
    Project(uint32_t version, std::string name, std::string url, bool gdpr_controls, ProjectEntryStatus status);

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
    //! \brief Initialize a \c Project payload from a provided project entry
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
    //! \param filter_used The project status filter used for the project list.
    //!
    WhitelistSnapshot(ProjectListPtr projects,
                      const ProjectEntry::ProjectFilterFlag& filter_used = ProjectEntry::ProjectFilterFlag::ACTIVE);

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
    //! \brief Returns the project status filter flag used to populate the snapshot.
    //!
    //! \return ProjectFilterFlag used for whitelist snapshot.
    //!
    ProjectEntry::ProjectFilterFlag FilterUsed() const;

    //!
    //! \brief Create a snapshot copy sorted alphabetically by project name.
    //!
    //! \return A sorted copy of the snapshot.
    //!
    WhitelistSnapshot Sorted() const;

private:
    const ProjectListPtr m_projects;                //!< The vector of whitelisted projects.
    const ProjectEntry::ProjectFilterFlag m_filter; //!< The filter used to populate the readonly list.
};

//!
//! \brief The AutoGreylist class. This class is a caching, thread-safe object that is generally intended to be (and currently
//! implemented as) a singleton, global object with a cached state, where a need for an actual refresh is determined
//! by a change in the superblock hash.
//!
//! The object is initialized in anonymous namespace in project.cpp and is accessed by the GetAutoGreylistCache()
//! static method. While this object is _internally_ thread safe via the autogreylist_lock, several methods access
//! external structures for which safe access can only be obtained by holding the cs_main lock. These methods are marked by
//! EXCLUSIVE_LOCKS_REQUIRED accordingly to facilitate static lock analysis when available by the compiler.
//!
//! The object implements the current "Zero Credit Days" and "Work Availability Score" rules. The projects that qualify
//! for greylisting override the "ACTIVE" status entries in the registry. This override is NOT permanent, but instead
//! only persists while the project qualifies for greylisting using the rules, which are updated for each superblock change.
//! In turn, there is a project entry that can be made in the registry to override the auto greylist, AUTO_GREYLIST_OVERRIDE,
//! which prevents the application of the AUTO_GREYLIST status to that project. This will prevent greylisting of that
//! project, even though by the rules it would be required. This override is to provide an ability to deal with unanticipated
//! situations concerning a project that render the normal rules not applicable.
//!
class AutoGreylist
{
public:
    //!
    //! \brief This provides a class that formalizes a greylist candidate entry. If the requirements for auto
    //! greylisting are met, the candidate entry becomes an override to the corresponding project entry on the
    //! whitelist.
    //!
    class GreylistCandidateEntry
    {
    public:
        //!
        //! \brief This is the trivial, empty constructor.
        //!
        GreylistCandidateEntry()
            : m_project_name(std::string {})
            , m_zcd_20_SB_count(0)
            , m_TC_7_SB_sum(0)
            , m_TC_40_SB_sum(0)
            , m_meets_greylisting_crit(false)
            , m_TC_initial_bookmark(0)
            , m_TC_bookmark(0)
            , m_sb_from_baseline_processed(0)
            , m_update_history({})
        {}

        //!
        //! \brief This parameterized constructor is used to construct the initial (baseline) greylist candidate
        //! entry with the initial total credit value for the project.
        //!
        //! \param project_name
        //! \param TC_initial_bookmark
        //!
        GreylistCandidateEntry(std::string project_name, std::optional<uint64_t> TC_initial_bookmark)
            : m_project_name(project_name)
            , m_zcd_20_SB_count(0)
            , m_TC_7_SB_sum(0)
            , m_TC_40_SB_sum(0)
            , m_meets_greylisting_crit(false)
            , m_TC_initial_bookmark(TC_initial_bookmark)
            , m_TC_bookmark(0)
            , m_sb_from_baseline_processed(0)
            , m_update_history()
        {
            // Populate the initial historical entry from the initial baseline.
            UpdateHistoryEntry entry = UpdateHistoryEntry(0,
                                                          TC_initial_bookmark,
                                                          std::optional<uint8_t> {},
                                                          std::optional<Fraction> {},
                                                          std::optional<bool> {});

            m_update_history.push_back(entry);
        }

        //!
        //! \brief Computes the number of "Zero Credit Days" (ZCD) in the last 20 SB from the superblock baseline.
        //!
        //! The number of zero credit days is counted starting at the current (or converged if part of the miner loop)
        //! superblock backwards by superblock history. If the backwards history is limited by either the project
        //! whitelisting initial active entry, or the number of v3 superblocks, then the zcd will be computed based
        //! on the superblock history available.
        //!
        //! \return uint8_t of zcd count.
        //!
        uint8_t GetZCD()
        {
            return m_zcd_20_SB_count;
        }

        //!
        //! \brief Computes the "Work Availability Score" (WAS) in the last 40 SB from the superblock baseline.
        //!
        //! The WAS is determined by computing the change in total credit over the last seven superblocks back from the
        //! current (or converged if part of the miner loop) superblock, divided by the change in total credit over the
        //! last 40 superblocks. This is represented internally as a fraction rather than floating point to avoid
        //! consensus issues. Note that for display, the ToDouble method on the Fraction class is used to convert to
        //! a human readable double. If 40 superblocks of history are not available, either because the project
        //! was just whitelisted via an initial active project entry, or there are not enough v3 superblocks, then
        //! the WAS is computed using the history that is available. This means for newly listed projects, or where the
        //! v3 superblock history is less than 7, the WAS will be equal to 1.0 unless there are no collected statistics
        //! on two superblocks during that interval, in which case the WAS will be 0.0.
        //!
        //! \return Fraction of Work Availablity Score.
        //!
        Fraction GetWAS()
        {
            if (!m_sb_from_baseline_processed) {
                return Fraction(0);
            }

            // The Fraction class is implemented with int64_t as the underlying data type for the numerator and denominator;
            // however, the total credit is stored in the superblock as a uint64_t. To be thorough, check if either of the unsigned
            // 64 bit TC averages will overflow a signed 64 integer, and if so, then simply divide by 2 before constructing the
            // fraction. This is extremely unlikely, given that the number of SB's processed from the baseline would have to be
            // one, and then the sum overflow the int64_t max.
            uint64_t TC_7_SB_avg = m_TC_7_SB_sum / std::min<uint64_t>(m_sb_from_baseline_processed, 7);
            uint64_t TC_40_SB_avg = m_TC_40_SB_sum / std::min<uint64_t>(m_sb_from_baseline_processed, 40);

            if (TC_7_SB_avg > (uint64_t) std::numeric_limits<int64_t>::max()
                || TC_40_SB_avg > (uint64_t) std::numeric_limits<int64_t>::max()) {
                TC_7_SB_avg /= 2;
                TC_40_SB_avg /= 2;
            }

            if (TC_7_SB_avg == 0 && TC_40_SB_avg == 0) {
                return Fraction(0);
            } else {
                return Fraction(TC_7_SB_avg, TC_40_SB_avg);
            }
        }

        //!
        //! \brief UpdateGreylistCandidateEntry
        //!
        //! \param total_credit
        //! \param sb_from_baseline
        //!
        void UpdateGreylistCandidateEntry(std::optional<uint64_t> total_credit, uint8_t sb_from_baseline)
        {
            uint8_t sbs_from_baseline_no_update = sb_from_baseline - m_sb_from_baseline_processed - 1;

            m_sb_from_baseline_processed = sb_from_baseline;

            // ZCD part. Remember we are going backwards, so if total_credit is greater than or equal to
            // the bookmark, then we have zero or even negative project total credit between superblocks, so
            // this qualifies as a ZCD.
            if (m_sb_from_baseline_processed <= 20) {
                // Skipped updates count as ZCDs. We stop counting at 20 superblocks (back) from
                // the initial SB baseline. The skipped updates may actually not be used in practice, because we are
                // iterating over the entire whitelist for each SB and inserting std::nullopt TC updates for each project.
                m_zcd_20_SB_count += sbs_from_baseline_no_update;

                // If total credit is greater than the bookmark, this means that (going forward in time) credit actually
                // declined, so in addition to the zero credit days recorded for the skipped updates, we must add an
                // additional day for the credit decline (going forward in time). If total credit is std::nullopt, then
                // this means no statistics available, which also is a ZCD.
                if ((total_credit && total_credit >= m_TC_bookmark) || !total_credit){
                    ++m_zcd_20_SB_count;
                }
            }

            // WAS part. Here we deal with two numbers, the 40 SB from baseline, and the 7 SB from baseline. We use
            // the initial bookmark, and compute the difference, which is the same as adding up the deltas between
            // the TC's in each superblock. For updates with no total credit entry (i.e. std::optional is nullopt),
            // do not change the sums.
            //
            // If the initial bookmark TC is not available (i.e. the current SB did not have stats for this project),
            // but this update does, then update the initial bookmark to this total credit.
            if (!m_TC_initial_bookmark && total_credit) {
                m_TC_initial_bookmark = total_credit;
            }

            if (total_credit && m_TC_initial_bookmark && m_TC_initial_bookmark > total_credit) {
                if (m_sb_from_baseline_processed <= 7) {
                    m_TC_7_SB_sum = *m_TC_initial_bookmark - *total_credit;
                }

                if (m_sb_from_baseline_processed <= 40) {
                    m_TC_40_SB_sum = *m_TC_initial_bookmark - *total_credit;
                }
            }

            // Update bookmark.
            if (total_credit) {
                m_TC_bookmark = *total_credit;
            }

            uint8_t zcd = GetZCD();
            Fraction was = GetWAS();

            // Apply rules and determine if greylisting criteria is met.
            m_meets_greylisting_crit = (sb_from_baseline >= 2 && (zcd > 7 || was < Fraction(1, 10)));

            // Insert historical entry.
            UpdateHistoryEntry entry(sb_from_baseline, total_credit, zcd, was, m_meets_greylisting_crit);
            m_update_history.push_back(entry);
        }

        //!
        //! \brief This is used to store an update entry for historical purposes.
        //!
        struct UpdateHistoryEntry
        {
            //!
            //! \brief Specific constructor for UpdateHistoryEntry.
            //!
            //! \param sb_from_baseline_processed
            //! \param total_credit
            //! \param meets_greylisting_crit
            //!
            UpdateHistoryEntry(uint8_t sb_from_baseline_processed,
                               std::optional<uint64_t>total_credit,
                               std::optional<uint8_t> zcd,
                               std::optional<Fraction> was,
                               std::optional<bool> meets_greylisting_crit)
                : m_sb_from_baseline_processed(sb_from_baseline_processed)
                , m_total_credit(total_credit)
                , m_zcd(zcd)
                , m_was(was)
                , m_meets_greylisting_crit(meets_greylisting_crit)
            {}

            uint8_t m_sb_from_baseline_processed;
            std::optional<uint64_t> m_total_credit;
            std::optional<uint8_t> m_zcd;
            std::optional<Fraction> m_was;
            std::optional<bool> m_meets_greylisting_crit;
        };

        //!
        //! \brief This provides the update history vector for the greylist candidate entry.
        //! \return
        //!
        const std::vector<UpdateHistoryEntry> GetUpdateHistory() const
        {
            return m_update_history;
        }

        const std::string m_project_name;

        uint8_t m_zcd_20_SB_count;
        uint64_t m_TC_7_SB_sum;
        uint64_t m_TC_40_SB_sum;
        bool m_meets_greylisting_crit;

    private:
        std::optional<uint64_t> m_TC_initial_bookmark; //!< This is a "reverse" bookmark - we are going backwards in SB's.
        uint64_t m_TC_bookmark;
        uint8_t m_sb_from_baseline_processed;

        std::vector<UpdateHistoryEntry> m_update_history;
    };

    typedef std::map<std::string, GreylistCandidateEntry> Greylist;

    //!
    //! \brief Smart pointer around a collection of projects.
    //!
    typedef std::shared_ptr<Greylist> GreylistPtr;

    typedef Greylist::size_type size_type;
    typedef Greylist::iterator iterator;
    typedef Greylist::const_iterator const_iterator;

    //!
    //! \brief The trivial constructor for the AutoGreylist class.
    //!
    AutoGreylist();

    //!
    //! \brief Returns an iterator to the beginning.
    //!
    const_iterator begin() const;

    //!
    //! \brief Returns an iterator to the end.
    //!
    const_iterator end() const;

    //!
    //! \brief Get the number of projects in the auto greylist. This should be equal to the number of whitelisted projects,
    //! since the auto greylist tracks "candidate" entries, and they are marked as to whether they qualify for greylisting.
    //!
    size_type size() const;

    //!
    //! \brief Determine whether the specified project exists in the auto greylist.
    //!
    //! \param name Project name matching the contract key.
    //!
    //! \param only_auto_greylisted A boolean that specifies whether the search is against all projects or only those
    //! that meet auto greylisting criteria.
    //!
    //! \return \c true if the auto greylist contains a project with matching name.
    //!
    bool Contains(const std::string& name, const bool& only_auto_greylisted = true) const;

    //!
    //! \brief This refreshes the AutoGreylist object from the last superblock in the chain.
    //!
    void Refresh();

    //!
    //! \brief This refreshes the AutoGreylist object from an input Superblock pointer.
    //!
    //! Note that the AutoGreylist object refreshed this way will also be used to update the referenced superblock
    //! object
    //!
    //! \param superblock_ptr The superblock pointer with which to refresh the automatic greylist. This can be a candidate superblock
    //! from a scraper convergence, or in the instance of this being called from Refresh(), could be the current
    //! superblock on the chain.
    //!
    void RefreshWithSuperblock(SuperblockPtr superblock_ptr_in);

    //!
    //! \brief This refreshes the AutoGreylist object from an input Superblock that is going to be associated
    //! with the current head of the chain. This mode is used in the scraper during the construction of the superblock contract.
    //!
    //! Note that the AutoGreylist object refreshed this way will also be used to update the referenced superblock
    //! object
    //!
    //! \param superblock The superblock with which to refresh the automatic greylist. This will generally be a candidate superblock
    //! from a scraper convergence, and is used in the call chain from the miner loop. The superblock object project status
    //! will be updated.
    //!
    void RefreshWithSuperblock(Superblock& superblock);

    //!
    //! \brief Resets the AutoGreylist object. This is called by the Whitelist Reset().
    //!
    void Reset();

    //!
    //! \brief Static method to provide the shared pointer to the global auto greylist cache object. The actual
    //! global is in an anonymous namespace to ensure the access is only through this method and also prevents having
    //! to have extern statements all over.
    //!
    //! \return shared pointer to the auto greylist global cache object.
    //!
    static std::shared_ptr<AutoGreylist> GetAutoGreylistCache();

private:
    mutable CCriticalSection autogreylist_lock;

    GreylistPtr m_greylist_ptr;
    QuorumHash m_superblock_hash;
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
    //! Version 0: <= 5.4.5.0 where there was no backing db.
    //! Version 1: >= 5.4.6.0.
    //!
    Whitelist()
        : m_project_db(1)
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
    //! \brief Get a read-only view of the projects in the whitelist. The default filter is ACTIVE, which
    //! provides the original ACTIVE project only view. The refresh_greylist filter is used to refresh
    //! the AutoGreylist as part of taking the snapshot.
    //!
    WhitelistSnapshot Snapshot(const ProjectEntry::ProjectFilterFlag& filter = ProjectEntry::ProjectFilterFlag::ACTIVE,
                               const bool& refresh_greylist = true, const bool &include_override = true) const;

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
    //! \brief This returns m_project_first_actives.
    //! \return
    //!
    const ProjectEntryMap GetProjectsFirstActive() const;

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

    //!
    //! \brief Core signal to indicate that a project status has changed.
    //!
    boost::signals2::signal<void (const ProjectEntry_ptr project, ChangeType status)> NotifyProjectChanged;

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

    ProjectEntryMap m_project_first_actives;             //!< Tracks when projects were first activated for auto greylisting purposes.

    ProjectEntryDB m_project_db;                         //!< The project db member
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
