// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "init.h"
#include "gridcoin/appcache.h"
#include "gridcoin/backup.h"
#include "gridcoin/beacon.h"
#include "gridcoin/boinc.h"
#include "gridcoin/contract/message.h"
#include "gridcoin/magnitude.h"
#include "gridcoin/project.h"
#include "gridcoin/quorum.h"
#include "gridcoin/researcher.h"
#include "gridcoin/support/xml.h"
#include "gridcoin/tally.h"
#include "span.h"
#include "ui_interface.h"
#include "util.h"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/optional.hpp>
#include <openssl/md5.h>
#include <set>

using namespace GRC;

extern CCriticalSection cs_main;
extern std::string msMiningErrors;

namespace {
//!
//! \brief Global BOINC researcher mining context.
//!
ResearcherPtr g_researcher = std::make_shared<Researcher>();

//!
//! \brief Indicates whether the researcher context needs a refresh.
//!
std::atomic<bool> g_researcher_dirty(true);

//!
//! \brief Rewrite the configuration file to change investor mode and set the
//! email address directive.
//!
//! \param email The email address to update the directive to. If empty, set
//! the configuration to investor mode.
//!
//! \return \c false if a filesystem error occurs.
//!
bool RewriteConfigurationFileMode(const ResearcherMode mode, const std::string& email)
{
    const fs::path config_file_path = GetConfigFile();
    std::string out;

    if (mode == ResearcherMode::INVESTOR) {
        out = "investor=1\n";
    } else if (mode == ResearcherMode::SOLO) {
        out = strprintf("email=%s\n", email);
    }

    try {
        fsbridge::ifstream config_file_in(config_file_path);
        std::string line;

        LOCK(cs_main);

        while (std::getline(config_file_in, line)) {
            if (!boost::starts_with(line, "email=")
                && !boost::starts_with(line, "investor="))
            {
                out += line;
                out += "\n";
            }
        }

        config_file_in.close();
    } catch (const std::exception& e) {
        error("%s: Failed to read config file: %s", __func__, e.what());
        return false;
    }

    try {
        fsbridge::ofstream config_file_out(config_file_path);

        LOCK(cs_main);

        config_file_out << out;
        config_file_out.close();
    } catch (const std::exception& e) {
        error("%s: Failed to write config file: %s", __func__, e.what());
        return false;
    }

    return true;
}

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
//! \brief Extract the authority component (hostname:port) from a URL.
//!
//! \param URL to extract the authority component from.
//!
//! \return A view of the URL string for the authority component.
//!
Span<const char> ParseUrlHostname(const std::string& url)
{
    const auto url_end = url.end();

    auto domain_begin = url.begin();
    auto scheme_end = std::find(domain_begin, url_end, ':');

    if (std::distance(scheme_end, url_end) >= 3) {
        if (*++scheme_end == '/' && *++scheme_end == '/') {
            domain_begin = ++scheme_end;
        }
    }

    auto domain_end = std::find(domain_begin, url_end, '/');

    if (domain_end == url_end) {
        domain_end = std::find(domain_begin, url_end, '?');
    }

    return Span<const char>(&*domain_begin, &*domain_end);
}

//!
//! \brief Determine whether two URLs contain the same authority component
//! (hostname and port number).
//!
//! \param url_1 First BOINC project website URL to compare.
//! \param url_2 Second BOINC project website URL to compare.
//!
//! \return \c true if both URLs contain the same authority component.
//!
bool CompareProjectHostname(const std::string& url_1, const std::string& url_2)
{
    return ParseUrlHostname(url_1) == ParseUrlHostname(url_2);
}

//!
//! \brief Attempt to match the local project loaded from BOINC with a project
//! on the Gridcoin whitelist.
//!
//! \param project   Project loaded from BOINC to compare.
//! \param whitelist A snapshot of the current projects on the whitelist.
//!
//! \return A pointer to the whitelist project if it matches.
//!
const Project* ResolveWhitelistProject(
    const MiningProject& project,
    const WhitelistSnapshot& whitelist)
{
    for (const auto& whitelist_project : whitelist) {
        if (project.m_name == whitelist_project.m_name) {
            return &whitelist_project;
        }

        // Sometimes project whitelist contracts contain a name different from
        // the project name specified in client_state.xml. The URLs will often
        // match in this case. We just check the authority component:
        //
        if (CompareProjectHostname(project.m_url, whitelist_project.m_url)) {
            return &whitelist_project;
        }
    }

    return nullptr;
}

//!
//! \brief Represents a Gridcoin pool that stakes on behalf of its users.
//!
//! The wallet uses these entries to detect when BOINC is attached to a pool
//! account so that it can provide more useful information in the UI.
//!
class MiningPool
{
public:
    MiningPool(const Cpid cpid, std::string m_name, std::string m_url)
        : m_cpid(cpid), m_name(std::move(m_name)), m_url(std::move(m_url))
    {
    }

    MiningPool(const std::string& cpid, std::string m_name, std::string m_url)
        : MiningPool(Cpid::Parse(cpid), std::move(m_name), std::move(m_url))
    {
    }

    Cpid m_cpid;        //!< The pool's external CPID.
    std::string m_name; //!< The name of the pool.
    std::string m_url;  //!< The pool's website URL.
};

//!
//! \brief The set of known Gridcoin pools.
//!
//! TODO: In the future, we may add a contract type that allows pool operators
//! to register a pool via the blockchain. The static list gets us by for now.
//!
const MiningPool g_pools[] = {
    { "7d0d73fe026d66fd4ab8d5d8da32a611", "grcpool.com", "https://grcpool.com/" },
    { "a914eba952be5dfcf73d926b508fd5fa", "grcpool.com-2", "https://grcpool.com/" },
    { "163f049997e8a2dee054d69a7720bf05", "grcpool.com-3", "https://grcpool.com/" },
    { "326bb50c0dd0ba9d46e15fae3484af35", "Arikado", "https://gridcoinpool.ru/" },
};

//!
//! \brief Determine whether the provided CPID belongs to a Gridcoin pool.
//!
//! \param cpid An external CPID for a project loaded from BOINC.
//!
//! \return \c true if the CPID matches a known Gridcoin pool's CPID.
//!
bool IsPoolCpid(const Cpid cpid)
{
    for (const auto& pool : g_pools) {
        if (pool.m_cpid == cpid) {
            return true;
        }
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
        case MiningProject::Error::POOL:
            LogPrintf("Project %s is attached to a pool.", project.m_name);
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
        std::string warning  = "WARNING: Detected potential CPID split. ";
        warning += "Eligible CPIDs: \n";

        for (const auto& cpid_pair : eligible_cpids) {
            warning += "    " + cpid_pair.first.ToString();
            warning += " (" + cpid_pair.second + ")\n";
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
        case ResearcherStatus::POOL:
            msMiningErrors = _("Staking Only - Pool Detected");
            break;
        case ResearcherStatus::NO_PROJECTS:
            msMiningErrors = _("Staking Only - No Eligible Research Projects");
            break;
        case ResearcherStatus::NO_BEACON:
            msMiningErrors = _("Staking Only - No active beacon");
            break;
        case ResearcherStatus::INVESTOR:
            msMiningErrors = _("Staking Only - Investor Mode");
            break;
    }

    std::atomic_store(
        &g_researcher,
        std::make_shared<Researcher>(std::move(context)));

    g_researcher_dirty = false;

    uiInterface.ResearcherChanged();
}

//!
//! \brief A piece of helper state that keeps track of recently-advertised
//! pending beacons.
//!
class RecentBeacons
{
public:
    //!
    //! \brief Load the set of pending beacons that match the node's private
    //! keys.
    //!
    //! THREAD SAFETY: Lock cs_main and pwalletMain->cs_wallet before calling
    //! this method.
    //!
    //! \param beacons Contains the set of pending beacons to import from.
    //!
    void ImportRegistry(const BeaconRegistry& beacons)
    {
        AssertLockHeld(cs_main);
        AssertLockHeld(pwalletMain->cs_wallet);

        for (const auto& pending_pair : beacons.PendingBeacons()) {
            const CKeyID& key_id = pending_pair.first;
            const PendingBeacon beacon = static_cast<PendingBeacon>(*pending_pair.second);

            if (pwalletMain->HaveKey(key_id)) {
                auto iter_pair = m_pending.emplace(beacon.m_cpid, beacon);
                iter_pair.first->second.m_timestamp = beacon.m_timestamp;
            }
        }
    }

    //!
    //! \brief Fetch the pending beacon for the specified CPID if it exists.
    //!
    //! \param cpid CPID that the beacon was advertised for.
    //!
    //! \return A pointer to the pending beacon if one exists for the CPID.
    //!
    const PendingBeacon* Try(const Cpid cpid)
    {
        AssertLockHeld(cs_main);

        const auto iter = m_pending.find(cpid);

        if (iter == m_pending.end()) {
            return nullptr;
        }

        if (iter->second.Expired(GetAdjustedTime())) {
            m_pending.erase(iter);
            return nullptr;
        }

        // If the pending beacon activated, don't report it as pending still:
        //
        if (const BeaconOption beacon = GetBeaconRegistry().Try(cpid)) {
            if (beacon->m_public_key == iter->second.m_public_key) {
                m_pending.erase(iter);
                return nullptr;
            }
        }

        return &iter->second;
    }

    //!
    //! \brief Stash a beacon that the node just advertised to the network.
    //!
    //! \param cpid   CPID that the beacon was advertised for.
    //! \param result Contains the public key if the transaction succeeded.
    //!
    void Remember(const Cpid cpid, const AdvertiseBeaconResult& result)
    {
        AssertLockHeld(cs_main);

        if (const CPubKey* key = result.TryPublicKey()) {
            auto iter_pair = m_pending.emplace(cpid, PendingBeacon(cpid, *key));
            iter_pair.first->second.m_timestamp = GetAdjustedTime();
        }
    }

private:
    std::map<Cpid, PendingBeacon> m_pending; //!< Known set of pending beacons.
}; // RecentBeacons

//!
//! \brief A cache of recently-advertised pending beacons.
//!
//! THREAD SAFETY: Lock cs_main before calling methods on this object.
//!
RecentBeacons g_recent_beacons;

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
        MakeContract<BeaconPayload>(action, std::move(payload)));

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
// Class: MiningProject
// -----------------------------------------------------------------------------

MiningProject::MiningProject(std::string name,
    Cpid cpid,
    std::string team,
    std::string url,
    double rac)
    : m_name(LowerUnderscore(std::move(name)))
    , m_cpid(std::move(cpid))
    , m_team(std::move(team))
    , m_url(std::move(url))
    , m_rac(std::move(rac))
    , m_error(Error::NONE)
{
    boost::to_lower(m_team);
}

MiningProject MiningProject::Parse(const std::string& xml)
{
    MiningProject project(
        ExtractXML(xml, "<project_name>", "</project_name>"),
        Cpid::Parse(ExtractXML(xml, "<external_cpid>", "</external_cpid>")),
        ExtractXML(xml, "<team_name>", "</team_name>"),
        ExtractXML(xml, "<master_url>", "</master_url>"),
        std::strtold(ExtractXML(xml, "<user_expavg_credit>",
                                "</user_expavg_credit>").c_str(), nullptr));

    if (IsPoolCpid(project.m_cpid) && !GetBoolArg("-pooloperator", false)) {
        project.m_error = MiningProject::Error::POOL;
        return project;
    }

    if (project.m_cpid.IsZero()) {
        const std::string external_cpid
            = ExtractXML(xml, "<external_cpid>", "</external_cpid>");

        // Old BOINC server versions may not provide an external CPID element
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

const Project* MiningProject::TryWhitelist(const WhitelistSnapshot& whitelist) const
{
    return ResolveWhitelistProject(*this, whitelist);
}

bool MiningProject::Whitelisted(const WhitelistSnapshot& whitelist) const
{
    return TryWhitelist(whitelist) != nullptr;
}

std::string MiningProject::ErrorMessage() const
{
    switch (m_error) {
        case Error::NONE:            return "";
        case Error::INVALID_TEAM:    return _("Invalid team");
        case Error::MALFORMED_CPID:  return _("Malformed CPID");
        case Error::MISMATCHED_CPID: return _("Project email mismatch");
        case Error::POOL:            return _("Pool");
    }

    return _("Unknown error");
}

// -----------------------------------------------------------------------------
// Class: MiningProjectMap
// -----------------------------------------------------------------------------

MiningProjectMap::MiningProjectMap() : m_has_pool_project(false)
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

bool MiningProjectMap::ContainsPool() const
{
    return m_has_pool_project;
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
    m_has_pool_project |= project.m_error == MiningProject::Error::POOL;
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
    , m_beacon_error(GRC::BeaconError::NONE)
{
}

Researcher::Researcher(
    MiningId mining_id,
    MiningProjectMap projects,
    const GRC::BeaconError beacon_error)
    : m_mining_id(std::move(mining_id))
    , m_projects(std::move(projects))
    , m_beacon_error(beacon_error)
{
}

void Researcher::Initialize()
{
    {
        LOCK2(cs_main, pwalletMain->cs_wallet);
        g_recent_beacons.ImportRegistry(GetBeaconRegistry());
    }

    Reload();
}

void Researcher::RunRenewBeaconJob()
{
    if (OutOfSyncByAge()) {
        return;
    }

    const ResearcherPtr researcher = Get();

    if (!researcher->Eligible()) {
        return;
    }

    TRY_LOCK(cs_main, locked_main);

    if (!locked_main) {
        return;
    }

    // Do not perform an automated renewal for participants with existing
    // beacons before a superblock is due. This avoids overwriting beacon
    // timestamps in the beacon registry in a way that causes the renewed
    // beacon to appear ahead of the scraper beacon consensus window.
    //
    if (!Quorum::SuperblockNeeded(pindexBest->nTime)) {
        TRY_LOCK(pwalletMain->cs_wallet, locked_wallet);

        if (!locked_wallet) {
            return;
        }

        researcher->AdvertiseBeacon();
    }
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

    return false;
}

ResearcherPtr Researcher::Get()
{
    return std::atomic_load(&g_researcher);
}

void Researcher::MarkDirty()
{
    g_researcher_dirty = true;
}

void Researcher::Reload()
{
    if (ConfiguredForInvestorMode(true)) {
        StoreResearcher(Researcher()); // Investor
        return;
    }

    // Don't force an empty email to investor mode for pool detection:
    if (Researcher::Email().empty()) {
        LogPrintf(
            "WARNING: Please set 'email=<your BOINC account email>' in "
            "gridcoinresearch.conf or 'investor=1' to decline research "
            "rewards");
    }

    LogPrintf("Loading BOINC CPIDs...");

    if (!GetArgument("boinckey", "").empty()) {
        // TODO: implement a safer way to export researcher context that does
        // not risk accidental exposure of an internal CPID and email address.
        LogPrintf("WARNING: boinckey is no longer supported.");
    }

    Reload(MiningProjectMap::Parse(FetchProjectsXml()));
}

void Researcher::Reload(MiningProjectMap projects, GRC::BeaconError beacon_error)
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

    // Enable a user to override CPIDs detected from BOINC's client_state.xml
    // file. This provides some flexibility for a user that needs to manage a
    // split CPID situation or for people that run a wallet on computers that
    // do not have BOINC installed:
    //
    if (mapArgs.count("-forcecpid")) {
        mining_id = MiningId::Parse(GetArg("-forcecpid", ""));

        if (mining_id.Which() == MiningId::Kind::CPID) {
            LogPrintf("Configuration forces CPID: %s", mining_id.ToString());
        } else {
            LogPrintf("ERROR: invalid CPID in -forcecpid");
            mining_id = MiningId::ForInvestor();
        }
    }

    if (mining_id.Which() != MiningId::Kind::CPID) {
        for (const auto& project_pair : projects) {
            TryProjectCpid(mining_id, project_pair.second);
        }
    }

    if (const CpidOption cpid = mining_id.TryCpid()) {
        DetectSplitCpid(projects);
        LogPrintf("Selected primary CPID: %s", cpid->ToString());
    } else if (!projects.empty()) {
        LogPrintf("WARNING: no projects eligible for research rewards.");
    }

    StoreResearcher(
        Researcher(std::move(mining_id), std::move(projects), beacon_error));
}

void Researcher::Refresh()
{
    if (!g_researcher_dirty) {
        return;
    }

    const ResearcherPtr researcher = Get();

    Reload(researcher->m_projects, researcher->m_beacon_error);
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
        LOCK(cs_main);
        return GetBeaconRegistry().ContainsActive(*cpid);
    }

    return false;
}

bool Researcher::IsInvestor() const
{
    return m_mining_id.Which() == MiningId::Kind::INVESTOR;
}

GRC::Magnitude Researcher::Magnitude() const
{
    if (const auto cpid_option = m_mining_id.TryCpid()) {
        LOCK(cs_main);
        return Quorum::GetMagnitude(*cpid_option);
    }

    return GRC::Magnitude::Zero();
}

bool Researcher::HasRAC() const
{
    for (const auto& iter : m_projects)
    {
        // Only one whitelisted project with positive RAC
        // is required to return true.
        if (iter.second.Eligible() && iter.second.m_rac > 0.0)
        {
            return true;
        }
    }

    return false;
}

CAmount Researcher::Accrual() const
{
    const CpidOption cpid = m_mining_id.TryCpid();

    if (!cpid || !pindexBest) {
        return 0;
    }

    const int64_t now = OutOfSyncByAge() ? pindexBest->nTime : GetAdjustedTime();

    LOCK(cs_main);

    return Tally::GetAccrual(*cpid, now, pindexBest);
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
        if (m_projects.ContainsPool()) {
            return ResearcherStatus::POOL;
        }

        return ResearcherStatus::NO_PROJECTS;
    }

    return ResearcherStatus::INVESTOR;
}

boost::optional<Beacon> Researcher::TryBeacon() const
{
    const CpidOption cpid = m_mining_id.TryCpid();

    if (!cpid) {
        return boost::none;
    }

    LOCK(cs_main);

    const BeaconOption beacon = GetBeaconRegistry().Try(*cpid);

    if (!beacon) {
        return boost::none;
    }

    return *beacon;
}

boost::optional<Beacon> Researcher::TryPendingBeacon() const
{
    const CpidOption cpid = m_mining_id.TryCpid();

    if (!cpid) {
        return boost::none;
    }

    LOCK(cs_main);

    const PendingBeacon* pending_beacon = g_recent_beacons.Try(*cpid);

    if (!pending_beacon) {
        return boost::none;
    }

    return *pending_beacon;
}

GRC::BeaconError Researcher::BeaconError() const
{
    return m_beacon_error;
}

bool Researcher::ChangeMode(const ResearcherMode mode, std::string email)
{
    boost::to_lower(email);

    if (mode == ResearcherMode::INVESTOR && ConfiguredForInvestorMode()) {
        return true;
    } else if (mode == ResearcherMode::SOLO && email == Email()) {
        return true;
    }

    if (!RewriteConfigurationFileMode(mode, email)) {
        return false;
    }

    {
        LOCK(cs_main);
        ForceSetArg("-email", email);
        ForceSetArg("-investor", mode == ResearcherMode::INVESTOR ? "1" : "0");
    }

    Reload();

    return true;
}

AdvertiseBeaconResult Researcher::AdvertiseBeacon(const bool force)
{
    const CpidOption cpid = m_mining_id.TryCpid();

    if (!cpid) {
        return GRC::BeaconError::NO_CPID;
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);

    const BeaconRegistry& beacons = GetBeaconRegistry();
    const BeaconOption current_beacon = beacons.TryActive(*cpid, GetAdjustedTime());

    AdvertiseBeaconResult result(GRC::BeaconError::NONE);

    if (force) {
        result = SendNewBeacon(*cpid);
    } else if (g_recent_beacons.Try(*cpid)) {
        LogPrintf("%s: Beacon awaiting confirmation already", __func__);
        return GRC::BeaconError::PENDING;
    } else if (!current_beacon) {
        result = SendNewBeacon(*cpid);
    } else {
        result = RenewBeacon(*cpid, *current_beacon);
    }

    if (result.Error() == GRC::BeaconError::NONE) {
        g_recent_beacons.Remember(*cpid, result);
    }

    if (result.Error() != GRC::BeaconError::NOT_NEEDED) {
        m_beacon_error = result.Error();
    }

    uiInterface.BeaconChanged();

    return result;
}

AdvertiseBeaconResult Researcher::RevokeBeacon(const Cpid cpid)
{
    LOCK2(cs_main, pwalletMain->cs_wallet);

    const BeaconOption beacon = GetBeaconRegistry().Try(cpid);

    if (!beacon) {
        LogPrintf("ERROR: %s: No active beacon for %s", __func__, cpid.ToString());
        return GRC::BeaconError::NO_CPID;
    }

    return SendBeaconContract(cpid, *beacon, ContractAction::REMOVE);
}
