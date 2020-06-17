#include "appcache.h"
#include "backup.h"
#include "boinc.h"
#include "contract/message.h"
#include "global_objects_noui.hpp"
#include "init.h"
#include "neuralnet/beacon.h"
#include "neuralnet/magnitude.h"
#include "neuralnet/quorum.h"
#include "neuralnet/researcher.h"
#include "ui_interface.h"
#include "util.h"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/optional.hpp>
#include <openssl/md5.h>
#include <set>

using namespace NN;

// Parses the XML elements from the BOINC client_state.xml:
std::string ExtractXML(const std::string& XMLdata, const std::string& key, const std::string& key_end);

extern CCriticalSection cs_main;
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
        case ResearcherStatus::NO_BEACON:
            msMiningErrors = _("Staking Only - No active beacon");
            break;
        default:
            msMiningErrors = _("Staking Only - Investor Mode");
            break;
    }

    std::atomic_store(
        &researcher,
        std::make_shared<Researcher>(std::move(context)));
}

//!
//! \brief Determine whether the wallet contains a valid private key that
//! matches the supplied beacon public key.
//!
//! \param public_key Identifies the corresponding private key.
//!
//! \return \c true if the wallet contains a matching private key.
//!
bool CheckBeaconPrivateKey(const CWallet* const wallet, const CPubKey& public_key)
{
    CKey out_key;

    if (!wallet->GetKey(public_key.GetID(), out_key)) {
        return error("%s: Key not found", __func__);
    }

    if (!out_key.IsValid()) {
        return error("%s: Invalid stored key", __func__);
    }

    if (out_key.GetPubKey() != public_key) {
        return error("%s: Public key mismatch", __func__);
    }

    return true;
}

//!
//! \brief Determine whether the wallet contains a key for a pending beacon.
//!
//! \param beacons Fetches pending beacon keys IDs.
//! \param cpid    CPID to look up pending beacons for.
//!
//! \return \c true if the a pending beacon exists for the supplied CPID.
//!
bool DetectPendingBeacon(const BeaconRegistry& beacons, const Cpid cpid)
{
    for (const auto& key_id : beacons.FindPendingKeys(cpid)) {
        if (pwalletMain->HaveKey(key_id)) {
            return true;
        }
    }

    return false;
}

//!
//! \brief Generate a new beacon key pair.
//!
//! \param cpid The participant's current primary CPID.
//!
//! \return A variant that contains the new public key if successful or a
//! description of the error that occurred.
//!
AdvertiseBeaconResult GenerateBeaconKey(const Cpid& cpid)
{
    LogPrintf("%s: Generating new keys for %s...", __func__, cpid.ToString());

    CPubKey public_key;
    if (!pwalletMain->GetKeyFromPool(public_key, false)) {
        LogPrintf("ERROR: %s: Failed to get new key from wallet", __func__);
        return BeaconError::MISSING_KEY;
    }

    // Since the network-wide rain feature outputs GRC to beacon public key
    // addresses, we describe this key as the participant's rain address to
    // help identify transactions in the GUI:
    //
    const std::string address_label = strprintf(
        "Beacon Rain Address for CPID %s (at %" PRIu64 ")",
        cpid.ToString(),
        static_cast<uint64_t>(nBestHeight));

    if (!pwalletMain->SetAddressBookName(public_key.GetID(), address_label)) {
        LogPrintf("WARNING: %s: Failed to change beacon key label", __func__);
    }

    if (!CheckBeaconPrivateKey(pwalletMain, public_key)) {
        LogPrintf("ERROR: %s: Failed to verify new beacon key", __func__);
        return BeaconError::MISSING_KEY;
    }

    if (!BackupWallet(*pwalletMain, GetBackupFilename("wallet.dat"))) {
        LogPrintf("WARNING: %s: Failed to backup wallet file", __func__);
    }

    return public_key;
}

//!
//! \brief Sign a new beacon payload with the beacon's private key.
//!
//! \param payload The beacon payload to sign.
//!
//! \return \c false if the key doesn't exist or fails to sign the payload.
//!
bool SignBeaconPayload(BeaconPayload& payload)
{
    CKey out_key;

    if (!pwalletMain->GetKey(payload.m_beacon.m_public_key.GetID(), out_key)) {
        return error("%s: Key not found", __func__);
    }

    if (!payload.Sign(out_key)) {
        return error("%s: Failed to sign payload", __func__);
    }

    return true;
}

//!
//! \brief Send a transaction that contains a beacon contract.
//!
//! \param cpid   CPID to send a beacon for.
//! \param beacon Contains the CPID's beacon public key.
//! \param action Determines whether to add or remove a beacon.
//!
//! \return A variant that contains the new public key if successful or a
//! description of the error that occurred.
//!
AdvertiseBeaconResult SendBeaconContract(
    const Cpid& cpid,
    Beacon beacon,
    ContractAction action = ContractAction::ADD)
{
    if (pwalletMain->IsLocked()) {
        LogPrintf("WARNING: %s: Wallet locked.", __func__);
        return BeaconError::WALLET_LOCKED;
    }

    // Ensure that the wallet contains enough coins to send a transaction for
    // the contract.
    //
    // TODO: refactor wallet so we can determine this dynamically. For now, we
    // require 1 GRC:
    //
    if (pwalletMain->GetBalance() < COIN) {
        LogPrintf("WARNING: %s: Insufficient funds.", __func__);
        return BeaconError::INSUFFICIENT_FUNDS;
    }

    BeaconPayload payload(cpid, beacon);

    if (!SignBeaconPayload(payload)) {
        return BeaconError::MISSING_KEY;
    }

    const auto result_pair = SendContract(
        NN::MakeContract<BeaconPayload>(action, std::move(payload)));

    if (!result_pair.second.empty()) {
        return BeaconError::TX_FAILED;
    }

    return AdvertiseBeaconResult(std::move(beacon.m_public_key));
}

//!
//! \brief Generate keys for and send a new beacon contract.
//!
//! \param cpid The CPID to create the beacon for.
//!
//! \return A variant that contains the new public key if successful or a
//! description of the error that occurred.
//!
AdvertiseBeaconResult SendNewBeacon(const Cpid& cpid)
{
    // First, determine whether we can successfully send a beacon contract. The
    // wallet must be unlocked and hold a balance great enough to send a beacon
    // transaction. Otherwise, we may create a bogus beacon key that lingers in
    // the wallet:
    //
    if (pwalletMain->IsLocked()) {
        LogPrintf("WARNING: %s: Wallet locked.", __func__);
        return BeaconError::WALLET_LOCKED;
    }

    // Ensure that the wallet contains enough coins to send a transaction for
    // the contract.
    //
    // TODO: refactor wallet so we can determine this dynamically. For now, we
    // require 1 GRC:
    //
    if (pwalletMain->GetBalance() < COIN) {
        LogPrintf("WARNING: %s: Insufficient funds.", __func__);
        return BeaconError::INSUFFICIENT_FUNDS;
    }

    AdvertiseBeaconResult result = GenerateBeaconKey(cpid);

    if (auto key_option = result.TryPublicKey()) {
        result = SendBeaconContract(cpid, std::move(*key_option));
    }

    return result;
}

//!
//! \brief Send a contract that renews an existing beacon.
//!
//! \param cpid   The CPID to create the beacon for.
//! \param beacon Contains the public key to renew.
//!
//! \return A variant that contains the public key if successful or a
//! description of the error that occurred.
//!
AdvertiseBeaconResult RenewBeacon(const Cpid& cpid, const Beacon& beacon)
{
    if (!beacon.Renewable(GetAdjustedTime())) {
        LogPrintf("%s: Beacon renewal not needed", __func__);
        return BeaconError::NOT_NEEDED;
    }

    LogPrintf("%s: Renewing beacon for %s", __func__, cpid.ToString());

    // A participant may run the wallet on two computers, but only one computer
    // holds the beacon private key. If BOINC also exists on both computers for
    // the same CPID, both nodes will attempt to renew the beacon. Only allow a
    // node with the private key to send the contract:
    //
    if (!CheckBeaconPrivateKey(pwalletMain, beacon.m_public_key)) {
        LogPrintf("WARNING: %s: Missing or invalid private key", __func__);
        return BeaconError::MISSING_KEY;
    }

    return SendBeaconContract(cpid, beacon);
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
        return nullptr;
    }

    return &iter->second;
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
// Class: AdvertiseBeaconResult
// -----------------------------------------------------------------------------

AdvertiseBeaconResult::AdvertiseBeaconResult(CPubKey public_key)
    : m_result(std::move(public_key))
{
}

AdvertiseBeaconResult::AdvertiseBeaconResult(const BeaconError error)
    : m_result(error)
{
}

CPubKey* AdvertiseBeaconResult::TryPublicKey()
{
    if (m_result.which() == 0) {
        return &boost::get<CPubKey>(m_result);
    }

    return nullptr;
}

const CPubKey* AdvertiseBeaconResult::TryPublicKey() const
{
    if (m_result.which() == 0) {
        return &boost::get<CPubKey>(m_result);
    }

    return nullptr;
}

BeaconError AdvertiseBeaconResult::Error() const
{
    if (m_result.which() > 0) {
        return boost::get<BeaconError>(m_result);
    }

    return BeaconError::NONE;
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
    if (GetBoolArg("-investor", false) || Researcher::Email() == "investor") {
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
    if (const CpidOption cpid = m_mining_id.TryCpid()) {
        return GetBeaconRegistry().ContainsActive(*cpid);
    }

    return false;
}

bool Researcher::IsInvestor() const
{
    return m_mining_id.Which() == MiningId::Kind::INVESTOR;
}

NN::Magnitude Researcher::Magnitude() const
{
    if (const auto cpid_option = m_mining_id.TryCpid()) {
        LOCK(cs_main);
        return Quorum::GetMagnitude(*cpid_option);
    }

    return NN::Magnitude::Zero();
}

ResearcherStatus Researcher::Status() const
{
    if (Eligible()) {
        return ResearcherStatus::ACTIVE;
    }

    if (m_mining_id.Which() == MiningId::Kind::CPID) {
        return ResearcherStatus::NO_BEACON;
    }

    if (!m_projects.empty()) {
        return ResearcherStatus::NO_PROJECTS;
    }

    return ResearcherStatus::INVESTOR;
}

AdvertiseBeaconResult Researcher::AdvertiseBeacon()
{
    AssertLockHeld(cs_main);
    AssertLockHeld(pwalletMain->cs_wallet);

    const CpidOption cpid = m_mining_id.TryCpid();

    if (!cpid) {
        return BeaconError::NO_CPID;
    }

    static int64_t last_advertised_height = 0;

    // Disallow users from attempting to advertise a beacon repeatedly before
    // the network confirms the previous contract in the chain. This prevents
    // unintentional spam caused by users who mistakenly try to advertise new
    // beacons manually in quick succession when setting up the wallet.
    //
    if (last_advertised_height >= (nBestHeight - 5)) {
        LogPrintf("ERROR: %s: Beacon awaiting confirmation already", __func__);
        return BeaconError::PENDING;
    }

    const BeaconRegistry& beacons = GetBeaconRegistry();
    const BeaconOption current_beacon = beacons.Try(*cpid);

    AdvertiseBeaconResult result(BeaconError::NONE);

    if (!current_beacon) {
        if (DetectPendingBeacon(beacons, *cpid)) {
            LogPrintf("%s: Beacon awaiting verification already", __func__);
            return BeaconError::PENDING;
        }

        result = SendNewBeacon(*cpid);
    } else {
        result = RenewBeacon(*cpid, *current_beacon);
    }

    m_beacon_error = result.Error();

    if (m_beacon_error == BeaconError::NONE) {
        last_advertised_height = nBestHeight;
    }

    return result;
}

AdvertiseBeaconResult Researcher::RevokeBeacon(const Cpid cpid)
{
    AssertLockHeld(cs_main);
    AssertLockHeld(pwalletMain->cs_wallet);

    const BeaconOption beacon = GetBeaconRegistry().Try(cpid);

    if (!beacon) {
        LogPrintf("ERROR: %s: No active beacon for %s", __func__, cpid.ToString());
        return BeaconError::NO_CPID;
    }

    return SendBeaconContract(cpid, *beacon, ContractAction::REMOVE);
}

bool Researcher::ImportBeaconKeysFromConfig(CWallet* const pwallet) const
{
    AssertLockHeld(pwallet->cs_wallet);

    const CpidOption cpid = m_mining_id.TryCpid();

    if (!cpid) {
        return true; // Cannot import beacon key for investor.
    }

    const std::string key_config_directive = strprintf(
        "privatekey%s%s",
        cpid->ToString(),
        fTestNet ? "testnet" : "");

    const std::string secret = GetArgument(key_config_directive, "");

    if (secret.empty()) {
        return true; // No beacon key exists in the configuration file.
    }

    const auto vecsecret = ParseHex(secret);
    CKey key;

    if (!key.SetPrivKey(CPrivKey(vecsecret.begin(), vecsecret.end()))) {
        return error("%s: Invalid private key", __func__);
    }

    const CPubKey public_key = key.GetPubKey();
    const CKeyID address = public_key.GetID();

    if (pwallet->HaveKey(address)) {
        return true;
    }

    if (pwallet->IsLocked()) {
        return error("%s: Wallet locked!", __func__);
    }

    pwallet->MarkDirty();
    pwallet->mapKeyMetadata[address].nCreateTime = 0;

    if (!pwallet->AddKey(key)) {
        return error("%s: Failed to add key to wallet", __func__);
    }

    // Since the network-wide rain feature outputs GRC to beacon public key
    // addresses, we describe this key as the participant's rain address to
    // help identify transactions in the GUI:
    //
    const std::string address_label = strprintf(
        "Beacon Rain Address for CPID %s (imported)",
        cpid->ToString());

    if (!pwallet->SetAddressBookName(address, address_label)) {
        LogPrintf("WARNING: %s: Failed to change beacon key label", __func__);
    }

    if (!CheckBeaconPrivateKey(pwallet, public_key)) {
        return error("%s: Failed to verify imported beacon key", __func__);
    }

    if (!BackupWallet(*pwalletMain, GetBackupFilename("wallet.dat"))) {
        LogPrintf("WARNING: %s: Failed to backup wallet file", __func__);
    }

    return true;
}
