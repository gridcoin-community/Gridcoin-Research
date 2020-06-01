#pragma once

#include "key.h"
#include "neuralnet/cpid.h"

#include <boost/variant/get.hpp>
#include <boost/variant/variant.hpp>
#include <map>
#include <memory>
#include <string>
#include <vector>

class CWallet;
class uint256;

namespace NN {

//!
//! \brief Describes the eligibility status for earning rewards as part of the
//! research reward protocol.
//!
enum class ResearcherStatus
{
    INVESTOR,    //!< BOINC not present; ineligible for research rewards.
    ACTIVE,      //!< CPID eligible for research rewards.
    NO_PROJECTS, //!< BOINC present, but no eligible projects (investor).
    NO_BEACON,   //!< No active beacon public key advertised.
};

//!
//! \brief Represents a local BOINC project loaded from client_state.xml.
//!
struct MiningProject
{
    //!
    //! \brief Describes why a project is ineligible for Proof-of-Research
    //! rewards.
    //!
    //! Note: we're only showing one error at a time. Later, we could store
    //! these values in a bit field to track multiple errors simultaneously.
    //!
    enum class Error
    {
        NONE,            //!< Eligible project; no errors.
        INVALID_TEAM,    //!< Project not joined to a whitelisted team.
        MALFORMED_CPID,  //!< Failed to parse a valid external CPID.
        MISMATCHED_CPID, //!< External CPID failed internal CPID + email test.
    };

    //!
    //! \brief Initialize a mining project object with fields extracted from
    //! a project element in BOINC's client_state.xml file.
    //!
    //! \param name Project name from the \c <project_name> element.
    //! \param cpid External CPID parsed from the \c <external_cpid> element.
    //! \param team Associated team parsed from the \c <team_name> element.
    //!
    MiningProject(std::string name, Cpid cpid, std::string team);

    //!
    //! \brief Initialize a MiningProject instance by parsing the project XML
    //! from BOINC's client_state.xml file.
    //!
    //! \param xml Contains project data within a \c <project>...</project>
    //! element.
    //!
    static MiningProject Parse(const std::string& xml);

    std::string m_name; //!< Normalized project name.
    Cpid m_cpid;        //!< CPID of the BOINC account for the project.
    std::string m_team; //!< Name of the team joined for the project.
    Error m_error;      //!< May describe why a project is ineligible.

    //!
    //! \brief Determine whether the project is eligible for reward.
    //!
    //! \return \c true if the project represents a BOINC account with a valid
    //! CPID joined to a whitelisted team.
    //!
    bool Eligible() const;

    //!
    //! \brief Get a friendly, human-readable message that describes why the
    //! project is ineligible.
    //!
    //! \return The message as a string with any available localization applied.
    //!
    std::string ErrorMessage() const;
};

//!
//! \brief An optional type that either points to some local BOINC project or
//! does not.
//!
typedef const MiningProject* ProjectOption;

//!
//! \brief Contains a local set of BOINC projects loaded from client_state.xml.
//!
//! This is a map keyed by normalized project names. The container will ignore
//! insertion of a project with the same name as an existing project.
//!
class MiningProjectMap
{
    using ProjectStorage = std::map<std::string, MiningProject>;

public:
    typedef ProjectStorage::size_type size_type;
    typedef ProjectStorage::iterator iterator;
    typedef ProjectStorage::const_iterator const_iterator;

    //!
    //! \brief Initialize an empty BOINC project map.
    //!
    MiningProjectMap();

    //!
    //! \brief Parse the supplied collection of BOINC project XML sections into
    //! a new project map instance.
    //!
    //! \param xml Each element contains a project's XML data as parsed from
    //! BOINC's client_state.xml file.
    //!
    //! \return A new map with data for the local BOINC projects used to select
    //! an eligible CPID from. Must be filtered for team eligibility.
    //!
    static MiningProjectMap Parse(const std::vector<std::string>& xml);

    //!
    //! \brief Returns an iterator to the beginning.
    //!
    const_iterator begin() const;

    //!
    //! \brief Returns an iterator to the end.
    //!
    const_iterator end() const;

    //!
    //! \brief Get the number of projects loaded from BOINC.
    //!
    size_type size() const;

    //!
    //! \brief Determine whether the map contains any projects.
    //!
    bool empty() const;

    //!
    //! \brief Try to get the loaded BOINC project with the specified name.
    //!
    //! \param name The lowercase name of the BOINC project as it would exist
    //! in client_state.xml with underscores replaced by spaces.
    //!
    //! \return An object that contains a reference to the matching BOINC
    //! project if it exists.
    //!
    ProjectOption Try(const std::string& name) const;

    //!
    //! \brief Add the provided project to the map or replace an existing
    //! project with the same name.
    //!
    //! \param project BOINC project properties loaded from client_state.xml.
    //!
    void Set(MiningProject project);

    //!
    //! \brief Set the eligibility status of the projects in the map based
    //! on their association with the provided set of teams.
    //!
    //! \param teams The set of whitelisted teams to validate the provided
    //!              project's team against or an empty container when the
    //!              protocol team requirement is disabled.
    //!
    void ApplyTeamWhitelist(const std::set<std::string>& teams);

private:
    //!
    //! \brief Stores the local BOINC projects loaded from client_state.xml.
    //!
    ProjectStorage m_projects;
}; // MiningProjectMap

class Researcher; // forward for ResearcherPtr

//!
//! \brief A smart pointer around the global BOINC researcher context.
//!
typedef std::shared_ptr<Researcher> ResearcherPtr;

//!
//! \brief Describes errors that might occur during beacon advertisement.
//!
enum class BeaconError
{
    NONE,               //!< Beacon advertised successfully.
    INSUFFICIENT_FUNDS, //!< Balance too low to send a contract transaction.
    MISSING_KEY,        //!< Beacon private key missing or invalid.
    NO_CPID,            //!< No valid CPID detected (investor mode).
    NOT_NEEDED,         //!< Beacon exists for the CPID. No renewal needed.
    TOO_SOON,           //!< Not enough time elapsed for pending advertisement.
    TX_FAILED,          //!< Beacon contract transacton failed to send.
    WALLET_LOCKED,      //!< Wallet not fully unlocked.
};

//!
//! \brief Describes the result of a beacon advertisement.
//!
class AdvertiseBeaconResult
{
public:
    //!
    //! \brief Initialize a successful result.
    //!
    //! \param public_key The advertised beacon public key.
    //!
    AdvertiseBeaconResult(CPubKey public_key);

    //!
    //! \brief Initialize a failed result.
    //!
    //! \param error Describes the error that occurred.
    //!
    AdvertiseBeaconResult(const BeaconError error);

    //!
    //! \brief Get the beacon public key if advertisement succeeded.
    //!
    //! \return An object that points to the beacon public key if advertisement
    //! succeeded or does not.
    //!
    CPubKey* TryPublicKey();

    //!
    //! \brief Get the beacon public key if advertisement succeeded.
    //!
    //! \return An object that points to the beacon public key if advertisement
    //! succeeded or does not.
    //!
    const CPubKey* TryPublicKey() const;

    //!
    //! \brief Get a description of the error that occurred, if any.
    //!
    //! \return Describes the error result.
    //!
    BeaconError Error() const;

private:
    //!
    //! \brief Contains the beacon public key if advertisement succeeded or
    //! the error result if it did not.
    //!
    boost::variant<CPubKey, BeaconError> m_result;
};

//!
//! \brief Manages the global BOINC researcher context.
//!
//! This class governs a singleton that contains the global BOINC context set
//! for the application to participate in the researcher reward protocol. The
//! class creates the context by reading BOINC's client_state.xml file on the
//! local computer to extract a CPID used to associate the wallet to accounts
//! on the BOINC platform.
//!
//! Thread safety: the API currently requires a lock on \c cs_main to reload
//! the BOINC context because it sets some legacy global variables. After it
//! finishes, the API is immutable. The reload operation stores a pointer to
//! the singleton object atomically.
//!
class Researcher
{
public:
    //!
    //! \brief Initialize the researcher context to an investor.
    //!
    Researcher();

    //!
    //! \brief Initialize a researcher context with a mining ID and a set of
    //! local projects.
    //!
    //! \param mining_id Represents a CPID or an investor.
    //! \param projects  A set of local projects loaded from BOINC.
    //!
    Researcher(MiningId mining_id, MiningProjectMap projects);

    //!
    //! \brief Get the configured BOINC account email address.
    //!
    //! \return Lowercase BOINC email address as set in the configuration file.
    //!
    static std::string Email();

    //!
    //! \brief Determine whether the wallet must run in investor mode before trying
    //! to load BOINC CPIDs.
    //!
    //! \return \c true if the user explicitly configured investor mode or failed
    //! to input a valid email address.
    //!
    static bool ConfiguredForInvestorMode(bool log = false);

    //!
    //! \brief Get the current global researcher context.
    //!
    //! \return A smart pointer around the current \c Researcher object.
    //!
    static ResearcherPtr Get();

    //!
    //! \brief Reload the wallet's researcher mining context from BOINC.
    //!
    //! This method attempts to read BOINC's client_state.xml file to gather
    //! CPIDs used to establish the wallet's eligibility to generate rewards
    //! in the Proof-of-Research protocol. If BOINC is authenticated with at
    //! least one eligible project, the call will set the BOINC context used
    //! to mint blocks that claim Proof-of-Research rewards. Otherwise, this
    //! method resets the wallet's mining context to investor mode.
    //!
    static void Reload();

    //!
    //! \brief Reload the wallet's researcher mining context from the supplied
    //! collection of BOINC project data.
    //!
    //! \param projects Data for one or more projects as loaded from BOINC's
    //! client_state.xml file.
    //!
    static void Reload(MiningProjectMap projects);

    //!
    //! \brief Rescan the set of in-memory projects for eligible CPIDs without
    //! reloading the projects from disk.
    //!
    //! This method provides a hook for dynamic protocol configuration messages
    //! that update eligibility of local CPIDs. For example, when the receiving
    //! a protocol directive that enables or disables the team requirement, the
    //! application calls this method to update the researcher context with any
    //! newly eligible or ineligible CPIDs.
    //!
    static void Refresh();

    //!
    //! \brief Get the primary mining ID that identifies the owner of the wallet.
    //!
    //! \return Contains a CPID or represents an investor.
    //!
    const MiningId& Id() const;

    //!
    //! \brief Get the local BOINC projects loaded from client_state.xml.
    //!
    //! \return A map of local BOINC projects keyed by name.
    //!
    const MiningProjectMap& Projects() const;

    //!
    //! \brief Try to get the loaded BOINC project with the specified name.
    //!
    //! \param name The lowercase name of the BOINC project as it would exist
    //! in client_state.xml with underscores replaced by spaces.
    //!
    //! \return An object that contains a reference to the matching BOINC
    //! project if it exists.
    //!
    ProjectOption Project(const std::string& name) const;

    //!
    //! \brief Determine whether the wallet loaded BOINC projects eligible for
    //! Proof-of-Research rewards.
    //!
    //! \return \c true if the wallet loaded at least one project with a valid
    //! CPID and team membership (if team requirement active).
    //!
    bool Eligible() const;

    //!
    //! \brief Determine whether the wallet will attempt to stake for rewards
    //! in investor mode only.
    //!
    //! \return \c true if the wallet loaded no eligible BOINC projects or it
    //! is configured to start in investor mode.
    //!
    bool IsInvestor() const;

    //!
    //! \brief Get a value that indicates how the wallet participates in the
    //! Proof-of-Research protocol.
    //!
    //! \return The status depends on whether the wallet successfully loaded
    //! eligible CPIDs from BOINC.
    //!
    ResearcherStatus Status() const;

    //!
    //! \brief Submit a beacon contract to the network for the current CPID.
    //!
    //! This method sends a transaction to advertise a new beacon key when all
    //! of the following are true:
    //!
    //!  - The node obtained a valid CPID from BOINC (not investor)
    //!  - The node did not send a beacon transaction recently
    //!  - No beacon exists for the CPID or it expired, or...
    //!  - A beacon for the CPID exists and elapsed the renewal threshold
    //!  - The wallet generated a new beacon key successfully if needed
    //!  - The wallet signed the new beacon payload with its private key
    //!  - The wallet is fully unlocked
    //!  - The wallet contains a balance sufficient to send a transaction
    //!
    //! \return A variant that contains the new public key if successful or a
    //! description of the error that occurred.
    //!
    AdvertiseBeaconResult AdvertiseBeacon();

    //!
    //! \brief Submit a contract to the network to revoke an existing beacon.
    //!
    //! This process only works for the beacons that a node owns the private
    //! keys for.
    //!
    //! \param cpid CPID associated with the beacon to delete.
    //!
    //! \return A variant that contains the public key of the deleted beacon if
    //! successful or a description of the error that occurred.
    //!
    AdvertiseBeaconResult RevokeBeacon(const Cpid cpid);

    //!
    //! \brief Load legacy beacon private keys from the configuration file into
    //! the wallet.
    //!
    //! Old versions of Gridcoin wrote beacon keys to the configuration file
    //! instead of storing them in the wallet. This routine imports the keys
    //! from that file if needed.
    //!
    //! \param pwallet A pointer to the wallet object to import the keys into.
    //!
    //! \return \c true if the import finished without an error.
    //!
    bool ImportBeaconKeysFromConfig(CWallet* const pwallet) const;

private:
    MiningId m_mining_id;        //!< CPID or INVESTOR variant.
    MiningProjectMap m_projects; //!< Local projects loaded from BOINC.
    BeaconError m_beacon_error;  //!< Last beacon error that occurred, if any.
}; // Researcher

//!
//! \brief Get the primary CPID selected from BOINC's client_state.xml file.
//!
//! This is a temporary, transitional function for areas of code that expect
//! a CPID as a string and replaces the global msPrimaryCPID variable.
//!
//! \return String representation of the MiningId stored in the researcher
//! context ("INVESTOR" or an external CPID).
//!
std::string GetPrimaryCpid();
}
