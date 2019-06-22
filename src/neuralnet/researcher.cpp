#include "appcache.h"
#include "boinc.h"
#include "global_objects_noui.hpp"
#include "neuralnet/researcher.h"
#include "ui_interface.h"
#include "util.h"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/replace.hpp>

using namespace NN;

// Parses the XML elements from the BOINC client_state.xml:
std::string ExtractXML(const std::string& XMLdata, const std::string& key, const std::string& key_end);

// Used to build the legacy global mining context after reloading projects:
MiningCPID GetMiningCPID();
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
//! \brief Determine whether the wallet must run in investor mode before trying
//! to load BOINC CPIDs.
//!
//! \return \c true if the user explicitly configured investor mode or failed
//! to input a valid email address.
//!
bool ConfiguredForInvestorMode()
{
    if (GetBoolArg("-investor", false)) {
        LogPrintf("Investor mode configured. Skipping CPID import.");
        return true;
    }

    if (Researcher::Email().empty()) {
        LogPrintf(
            "WARNING: Please set 'email=<your BOINC account email>' in "
            "gridcoinresearch.conf. Continuing in investor mode.");

        return true;
    }

    return false;
}

//!
//! \brief Fetch the contents of BOINC's client_state.xml file from disk.
//!
//! \return The entire client_state.xml file as a string if readable.
//!
boost::optional<std::string> ReadClientStateXml()
{
    const std::string path = GetBoincDataDir();
    const std::string contents = GetFileContents(path + "client_state.xml");

    if (contents != "-1") {
        return contents;
    }

    LogPrintf("WARNING: Unable to obtain BOINC CPIDs.");

    if (!GetArgument("boincdatadir", "").empty()) {
        LogPrintf("Could not access configured BOINC data directory %s", path);
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
    // We could swap-n-pop the element to avoid shifting the whole sequence.
    // However, BOINC sorts the projects in client_state.xml by name, and we
    // select the last valid CPID present in this file. To avoid a surprise,
    // we'll just erase the first item. This routine doesn't run very often.
    //
    projects.erase(projects.begin());

    return projects;
}

//!
//! \brief Determine whether BOINC projects should be checked for membership in
//! team Gridcoin before enabling the associated CPID.
//!
//! \return \c true when the protocol is configured to require team membership
//! or when no protocol directive exists.
//!
bool ShouldEnforceTeamMembership()
{
    return ReadCache(Section::PROTOCOL, "REQUIRE_TEAM_WHITELIST_MEMBERSHIP").value != "false";
}

//!
//! \brief Process the provided project XML from BOINC's client_state.xml file
//! and load it into the supplied researcher context if valid.
//!
//! \param mining_id   Updated with the project CPID if eligible.
//! \param projects    Projects map to store the loaded project in.
//! \param project_xml As extracted from BOINC's client_state.xml file.
//!
void LoadProject(
    MiningId& mining_id,
    MiningProjectMap& projects,
    const std::string& project_xml)
{
    MiningProject project = MiningProject::Parse(project_xml);

    if (project.m_name.empty()) {
        LogPrintf("Skipping invalid BOINC project with empty name.");
        return;
    }

    // TODO: maybe we should support the TEAM_WHITELIST protocol directive:
    if (ShouldEnforceTeamMembership() && project.m_team != "gridcoin") {
        LogPrintf("Project %s is not joined to team Gridcoin.", project.m_name);
        project.m_error = MiningProject::Error::INVALID_TEAM;
        projects.Set(std::move(project));

        return;
    }

    if (project.m_cpid.IsZero()) {
        const std::string external_cpid
            = ExtractXML(project_xml, "<external_cpid>", "</external_cpid>");

        // For the extremely rare case that a BOINC project assigned a user a
        // CPID that contains only zeroes, double check that a CPID parsed to
        // zero is actually invalid:
        //
        if (MiningId::Parse(external_cpid).Which() != MiningId::Kind::CPID) {
            LogPrintf("Invalid external CPID for project %s.", project.m_name);
            project.m_error = MiningProject::Error::MALFORMED_CPID;
            projects.Set(std::move(project));

            return;
        }
    }

    // We compare the digest of the internal CPID and email address to the
    // external CPID as a smoke test to avoid running with corrupted CPIDs.
    //
    if (!project.m_cpid.Matches(
        ExtractXML(project_xml, "<cross_project_id>", "</cross_project_id>"),
        Researcher::Email()))
    {
        LogPrintf("CPID mismatch. Check email for %s.", project.m_name);
        project.m_error = MiningProject::Error::MISMATCHED_CPID;
        projects.Set(std::move(project));

        return;
    }

    mining_id = project.m_cpid;

    LogPrintf(
        "Found eligible project %s with CPID %s.",
        project.m_name,
        project.m_cpid.ToString());

    projects.Set(std::move(project));
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
//! \brief Set up the legacy global mining context variables after reloading
//! researcher context.
//!
//! \param researcher Contains the context to export.
//!
void SetLegacyResearcherContext(const Researcher& researcher)
{
    MiningCPID mc = GetMiningCPID();

    mc.initialized = true;
    mc.cpid = researcher.Id().ToString();
    mc.Magnitude = 0;
    mc.clientversion = "";
    mc.RSAWeight = 0;
    mc.LastPaymentTime = 0;
    mc.lastblockhash = "0";
    // Reuse for debugging
    mc.Organization = GetArg("-org", "");

    GlobalCPUMiningCPID = std::move(mc);

    // TODO: this belongs in presentation layer code:
    switch (researcher.Status()) {
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
}

//!
//! \brief Set the global BOINC researcher context.
//!
//! \param context Contains the CPID and local projects loaded from BOINC.
//!
void StoreResearcher(Researcher context)
{
    SetLegacyResearcherContext(context);

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
    return MiningProject(
        ExtractXML(xml, "<project_name>", "</project_name>"),
        Cpid::Parse(ExtractXML(xml, "<external_cpid>", "</external_cpid>")),
        ExtractXML(xml, "<team_name>","</team_name>"));
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

ResearcherPtr Researcher::Get()
{
    return std::atomic_load(&researcher);
}

void Researcher::Reload()
{
    if (ConfiguredForInvestorMode()) {
        StoreResearcher(Researcher()); // Investor
        return;
    }

    LogPrintf("Loading BOINC CPIDs...");

    if (!GetArgument("boinckey", "").empty()) {
        // TODO: implement a safer way to export researcher context that does
        // not risk accidental exposure of an internal CPID and email address.
        LogPrintf("WARNING: boinckey is no longer supported.");
    }

    Reload(FetchProjectsXml());
}

void Researcher::Reload(const std::vector<std::string>& projects_xml)
{
    Researcher researcher;

    for (const auto& project_xml : projects_xml) {
        LoadProject(researcher.m_mining_id, researcher.m_projects, project_xml);
    }

    if (const CpidOption cpid = researcher.m_mining_id.TryCpid()) {
        DetectSplitCpid(researcher.m_projects);
        LogPrintf("Selected primary CPID: %s", cpid->ToString());
    } else if (!researcher.m_projects.empty()) {
        LogPrintf("WARNING: no projects eligible for research rewards.");
    }

    StoreResearcher(std::move(researcher));
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
