#include "appcache.h"
#include "boinc.h"
#include "global_objects_noui.hpp"
#include "neuralnet/researcher.h"
#include "ui_interface.h"
#include "util.h"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <openssl/md5.h>
#include <set>

using namespace NN;

// Parses the XML elements from the BOINC client_state.xml:
std::string ExtractXML(const std::string& XMLdata, const std::string& key, const std::string& key_end);

// Used to build the legacy global mining context after reloading projects:
extern std::string msMiningErrors;

namespace {
//!
//! \brief Global BOINC researcher mining context.
//!
ResearcherPtr researcher = std::make_shared<Researcher>();

//!
//! \brief Convert a project name to lowercase change any underscores to spaces.
//!
//! NOTICE: Because the application merges the loaded BOINC project details into
//! the output of the "projects" RPC method, project whitelist contracts created
//! by the master key holder must embed the project names into the contract just
//! as output by this function or the method will omit projects with mismatching
//! names from the result.
//!
//! \return A "normalized" project name.
//!
std::string LowerUnderscore(std::string data)
{
    boost::to_lower(data);
    boost::replace_all(data, "_", " ");

    return data;
}

//!
//! \brief Fetch the contents of BOINC's client_state.xml file from disk.
//!
//! \return The entire client_state.xml file as a string if readable.
//!
boost::optional<std::string> ReadClientStateXml()
{
    const fs::path path = GetBoincDataDir();
    std::string contents = GetFileContents(path / "client_state.xml");

    if (contents != "-1") {
        return boost::make_optional(std::move(contents));
    }

    LogPrintf("WARNING: Unable to obtain BOINC CPIDs.");

    if (!GetArgument("boincdatadir", "").empty()) {
        LogPrintf("Could not access configured BOINC data directory %s", path.string());
    } else {
        LogPrintf(
            "BOINC data directory is not installed in the default location.\n"
            "Please specify its current location in gridcoinresearch.conf.");
    }

    return boost::none;
}

//!
//! \brief Load a set of project XML sections from BOINC's client_state.xml
//! file to gather CPIDs from.
//!
//! \return Items containing the XML between each \c <project>...</project>
//! section present in the client_state.xml file or an empty container when
//! the file is not accessible or contains no project elements.
//!
std::vector<std::string> FetchProjectsXml()
{
    const boost::optional<std::string> client_state_xml = ReadClientStateXml();

    if (!client_state_xml) {
        return { };
    }

    std::vector<std::string> projects = split(*client_state_xml, "<project>");

    if (projects.size() < 2) {
        LogPrintf("BOINC is not attached to any projects. No CPIDs loaded.");
        return { };
    }

    // Drop the first element which never contains a project:
    //
    std::swap(projects.front(), projects.back());
    projects.pop_back();

    return projects;
}

//!
//! \brief Determine whether BOINC projects should be checked for membership in
//! a whitelisted team before enabling the associated CPID.
//!
//! \return \c true when the protocol is configured to require team membership
//! or when no protocol directive exists.
//!
bool ShouldEnforceTeamMembership()
{
    return ReadCache(Section::PROTOCOL, "REQUIRE_TEAM_WHITELIST_MEMBERSHIP").value != "false";
}

//!
//! \brief Fetch the current set of whitelisted teams.
//!
//! \return The set of whitelisted teams configured in the protocol or a set
//! with only team "gridcoin" when no protocol directive exists. Supplies an
//! empty set when the team requirement is not active.
//!
std::set<std::string> GetTeamWhitelist()
{
    if (!ShouldEnforceTeamMembership()) {
        return { };
    }

    const AppCacheEntry entry = ReadCache(Section::PROTOCOL, "TEAM_WHITELIST");

    if (entry.value.empty()) {
        return { "gridcoin" };
    }

    std::set<std::string> teams;
    std::string delimiter = "<>";

    // Handle the transition of the team whitelist delimiter from "|" to "<>":
    if (entry.value.find(delimiter) == std::string::npos) {
        delimiter = "|";
    }

    for (auto&& team_name : split(entry.value, delimiter)) {
        if (team_name.empty()) {
            continue;
        }

        boost::to_lower(team_name);

        teams.emplace(std::move(team_name));
    }

    if (teams.empty()) {
        return { "gridcoin" };
    }

    return teams;
}

//!
//! \brief Select the provided project's CPID if the project passes the rules
//! for eligibility.
//!
//! \param mining_id Updated with the project CPID if eligible.
//! \param project   Project with the CPID to determine eligibility for.
//!
void TryProjectCpid(MiningId& mining_id, const MiningProject& project)
{
    switch (project.m_error) {
        case MiningProject::Error::NONE:
            break; // Suppress warning.
        case MiningProject::Error::MALFORMED_CPID:
            LogPrintf("Invalid external CPID for project %s.", project.m_name);
            return;
        case MiningProject::Error::MISMATCHED_CPID:
            LogPrintf("CPID mismatch. Check email for %s.", project.m_name);
            return;
        case MiningProject::Error::INVALID_TEAM:
            LogPrintf("Project %s's team is not whitelisted.", project.m_name);
            return;
    }

    mining_id = project.m_cpid;

    LogPrintf(
        "Found eligible project %s with CPID %s.",
        project.m_name,
        project.m_cpid.ToString());
}

//!
//! \brief Compute an external CPID from the supplied internal CPID and the
//! configured email address.
//!
//! A bug in BOINC sometimes results in an empty external CPID element in the
//! client_state.xml file. For these cases, we'll recompute the external CPID
//! of the project from the user's internal CPID and email address. This call
//! validates that the the user's email address hash extracted from a project
//! XML node matches the email set in the Gridcoin configuration file so that
//! the wallet doesn't inadvertently generate an unowned CPID.
//!
//! \param email_hash    MD5 digest of the the email address to compare with
//! the configured email.
//! \param internal_cpid As extracted from client_state.xml. An input to the
//! hash that generates the external CPID.
//!
//! \return The computed external CPID if the hash of the configured email
//! address matches the supplied email address hash.
//!
boost::optional<Cpid> FallbackToCpidByEmail(
    const std::string& email_hash,
    const std::string& internal_cpid)
{
    if (email_hash.empty() || internal_cpid.empty()) {
        return boost::none;
    }

    const std::string email = Researcher::Email();
    std::vector<unsigned char> email_hash_bytes(16);

    MD5(reinterpret_cast<const unsigned char*>(email.data()),
        email.size(),
        email_hash_bytes.data());

    if (HexStr(email_hash_bytes) != email_hash) {
        return boost::none;
    }

    return Cpid::Hash(internal_cpid, email);
}

//!
//! \brief Try to detect a split CPID and log a warning message.
//!
//! In the future, we can extend this to display a warning in the UI.
//!
//! \param projects Map of local projects loaded from BOINC's client_state.xml
//! file.
//!
void DetectSplitCpid(const MiningProjectMap& projects)
{
    std::unordered_map<Cpid, std::string> eligible_cpids;

    for (const auto& project_pair : projects) {
        if (project_pair.second.Eligible()) {
            eligible_cpids.emplace(
                project_pair.second.m_cpid,
                project_pair.second.m_name);
        }
    }

    if (eligible_cpids.size() > 1) {
        std::string warning  = "WARNING: Detected potential CPID split.";
        warning += "Eligible CPIDs: \n";

        for (const auto& cpid_pair : eligible_cpids) {
            warning += "    " + cpid_pair.first.ToString();
            warning += " (" + cpid_pair.second + ") \n";
        }

        LogPrintf("%s", warning);
    }
}

//!
//! \brief Set the global BOINC researcher context.
//!
//! \param context Contains the CPID and local projects loaded from BOINC.
//!
void StoreResearcher(Researcher context)
{
    // TODO: this belongs in presentation layer code:
    switch (context.Status()) {
        case ResearcherStatus::ACTIVE:
            msMiningErrors = _("Eligible for Research Rewards");
            break;
        case ResearcherStatus::NO_PROJECTS:
            msMiningErrors = _("Staking Only - No Eligible Research Projects");
            break;
        default:
            msMiningErrors = _("Staking Only - Investor Mode");
            break;
    }

    std::atomic_store(
        &researcher,
        std::make_shared<Researcher>(std::move(context)));
}
} // anonymous namespace

// -----------------------------------------------------------------------------
// Functions
// -----------------------------------------------------------------------------

std::string NN::GetPrimaryCpid()
{
    return NN::Researcher::Get()->Id().ToString();
}

// -----------------------------------------------------------------------------
// Class: MiningProject
// -----------------------------------------------------------------------------

MiningProject::MiningProject(std::string name, Cpid cpid, std::string team)
    : m_name(LowerUnderscore(std::move(name)))
    , m_cpid(std::move(cpid))
    , m_team(std::move(team))
    , m_error(Error::NONE)
{
    boost::to_lower(m_team);
}

MiningProject MiningProject::Parse(const std::string& xml)
{
    MiningProject project(
        ExtractXML(xml, "<project_name>", "</project_name>"),
        Cpid::Parse(ExtractXML(xml, "<external_cpid>", "</external_cpid>")),
        ExtractXML(xml, "<team_name>","</team_name>"));

    if (project.m_cpid.IsZero()) {
        const std::string external_cpid
            = ExtractXML(xml, "<external_cpid>", "</external_cpid>");

        // A bug in BOINC sometimes results in an empty external CPID element
        // in client_state.xml. For these cases, we'll recompute the external
        // CPID of the project from the internal CPID and email address:
        //
        if (external_cpid.empty()) {
            if (const boost::optional<Cpid> cpid = FallbackToCpidByEmail(
                ExtractXML(xml, "<email_hash>", "</email_hash>"),
                ExtractXML(xml, "<cross_project_id>", "</cross_project_id>")))
            {
                project.m_cpid = *cpid;
                return project;
            }
        }

        // For the extremely rare case that a BOINC project assigned a user a
        // CPID that contains only zeroes, double check that a CPID parsed to
        // zero is actually invalid:
        //
        if (MiningId::Parse(external_cpid).Which() != MiningId::Kind::CPID) {
            project.m_error = MiningProject::Error::MALFORMED_CPID;
            return project;
        }
    }

    // We compare the digest of the internal CPID and email address to the
    // external CPID as a smoke test to avoid running with corrupted CPIDs.
    //
    if (!project.m_cpid.Matches(
        ExtractXML(xml, "<cross_project_id>", "</cross_project_id>"),
        Researcher::Email()))
    {
        project.m_error = MiningProject::Error::MISMATCHED_CPID;
    }

    return project;
}

bool MiningProject::Eligible() const
{
    return m_error == Error::NONE;
}

std::string MiningProject::ErrorMessage() const
{
    switch (m_error) {
        case Error::NONE:            return "";
        case Error::INVALID_TEAM:    return _("Invalid team");
        case Error::MALFORMED_CPID:  return _("Malformed CPID");
        case Error::MISMATCHED_CPID: return _("Project email mismatch");
        default:                     return _("Unknown error");
    }
}

// -----------------------------------------------------------------------------
// Class: MiningProjectMap
// -----------------------------------------------------------------------------

MiningProjectMap::MiningProjectMap()
{
}

MiningProjectMap MiningProjectMap::Parse(const std::vector<std::string>& xml)
{
    MiningProjectMap projects;

    for (const auto& project_xml : xml) {
        MiningProject project = MiningProject::Parse(project_xml);

        if (project.m_name.empty()) {
            LogPrintf("Skipping invalid BOINC project with empty name.");
            continue;
        }

        projects.Set(std::move(project));
    }

    return projects;
}

MiningProjectMap::const_iterator MiningProjectMap::begin() const
{
    return m_projects.begin();
}

MiningProjectMap::const_iterator MiningProjectMap::end() const
{
    return m_projects.end();
}

MiningProjectMap::size_type MiningProjectMap::size() const
{
    return m_projects.size();
}

bool MiningProjectMap::empty() const
{
    return m_projects.empty();
}

ProjectOption MiningProjectMap::Try(const std::string& name) const
{
    const auto iter = m_projects.find(name);

    if (iter == m_projects.end()) {
        return boost::none;
    }

    return iter->second;
}

void MiningProjectMap::Set(MiningProject project)
{
    m_projects.emplace(project.m_name, std::move(project));
}

void MiningProjectMap::ApplyTeamWhitelist(const std::set<std::string>& teams)
{
    for (auto& project_pair : m_projects) {
        MiningProject& project = project_pair.second;

        switch (project.m_error) {
            case MiningProject::Error::NONE:
                if (!teams.empty() && !teams.count(project.m_team)) {
                    project.m_error = MiningProject::Error::INVALID_TEAM;
                }
                break;
            case MiningProject::Error::INVALID_TEAM:
                if (teams.empty() || teams.count(project.m_team)) {
                    project.m_error = MiningProject::Error::NONE;
                }
                break;
            default:
                continue;
        }
    }
}

// -----------------------------------------------------------------------------
// Class: Researcher
// -----------------------------------------------------------------------------

Researcher::Researcher()
    : m_mining_id(MiningId::ForInvestor())
{
}

Researcher::Researcher(MiningId mining_id, MiningProjectMap projects)
    : m_mining_id(std::move(mining_id))
    , m_projects(std::move(projects))
{
}

std::string Researcher::Email()
{
    std::string email = GetArgument("email", "");
    boost::to_lower(email);

    return email;
}

bool Researcher::ConfiguredForInvestorMode(bool log)
{
    if (GetBoolArg("-investor", false)) {
        if (log) LogPrintf("Investor mode configured. Skipping CPID import.");
        return true;
    }

    if (Researcher::Email().empty()) {
        if (log) LogPrintf(
            "WARNING: Please set 'email=<your BOINC account email>' in "
            "gridcoinresearch.conf. Continuing in investor mode.");

        return true;
    }

    return false;
}

ResearcherPtr Researcher::Get()
{
    return std::atomic_load(&researcher);
}

void Researcher::Reload()
{
    if (ConfiguredForInvestorMode(true)) {
        StoreResearcher(Researcher()); // Investor
        return;
    }

    LogPrintf("Loading BOINC CPIDs...");

    if (!GetArgument("boinckey", "").empty()) {
        // TODO: implement a safer way to export researcher context that does
        // not risk accidental exposure of an internal CPID and email address.
        LogPrintf("WARNING: boinckey is no longer supported.");
    }

    Reload(MiningProjectMap::Parse(FetchProjectsXml()));
}

void Researcher::Reload(MiningProjectMap projects)
{
    const std::set<std::string> team_whitelist = GetTeamWhitelist();

    if (team_whitelist.empty()) {
        LogPrintf("BOINC team requirement inactive at last known block.");
    } else {
        LogPrintf(
            "BOINC team requirement active at last known block. Whitelist: %s",
            boost::algorithm::join(team_whitelist, ", "));
    }

    projects.ApplyTeamWhitelist(team_whitelist);

    MiningId mining_id = MiningId::ForInvestor();

    for (const auto& project_pair : projects) {
        TryProjectCpid(mining_id, project_pair.second);
    }

    if (const CpidOption cpid = mining_id.TryCpid()) {
        DetectSplitCpid(projects);
        LogPrintf("Selected primary CPID: %s", cpid->ToString());
    } else if (!projects.empty()) {
        LogPrintf("WARNING: no projects eligible for research rewards.");
    }

    StoreResearcher(Researcher(std::move(mining_id), std::move(projects)));
}

void Researcher::Refresh()
{
    Reload(Get()->m_projects);
}

const MiningId& Researcher::Id() const
{
    return m_mining_id;
}

const MiningProjectMap& Researcher::Projects() const
{
    return m_projects;
}

ProjectOption Researcher::Project(const std::string& name) const
{
    return m_projects.Try(name);
}

bool Researcher::Eligible() const
{
    return m_mining_id.Which() == MiningId::Kind::CPID;
}

bool Researcher::IsInvestor() const
{
    return !Eligible();
}

ResearcherStatus Researcher::Status() const
{
    if (Eligible()) {
        return ResearcherStatus::ACTIVE;
    }

    if (!m_projects.empty()) {
        return ResearcherStatus::NO_PROJECTS;
    }

    return ResearcherStatus::INVESTOR;
}
