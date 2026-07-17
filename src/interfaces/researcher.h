// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_INTERFACES_RESEARCHER_H
#define GRIDCOIN_INTERFACES_RESEARCHER_H

#include "amount.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class CWallet;

namespace interfaces {

class Handler;

//! The researcher's current beacon status (Phase 1d-iv). Moved here from the Qt
//! layer so the node side can classify it and the GUI just maps it to text/icon;
//! researchermodel.h aliases its `BeaconStatus` to this so existing GUI code is
//! unchanged.
enum class BeaconStatus
{
    ACTIVE,
    ERROR_INSUFFICIENT_FUNDS,
    ERROR_MISSING_KEY,
    ERROR_NOT_NEEDED,
    ERROR_TX_FAILED,
    ERROR_INVALID_PROOF_XML,
    ERROR_WALLET_LOCKED,
    NO_BEACON,
    NO_CPID,
    NO_MAGNITUDE,
    PENDING,
    RENEWAL_NEEDED,
    RENEWAL_POSSIBLE,
    ALREADY_IN_MEMPOOL,
    UNKNOWN,
};

//! The researcher's configured participation mode.
enum class ResearcherMode
{
    SOLO,
    POOL,
    NONCRUNCHER,
};

//! A pointer-free snapshot of the researcher/beacon context (Phase 1d-iv),
//! replacing ResearcherModel's direct reads of Researcher::Get(), the beacon
//! registry, magnitude/accrual, and the version gate. Raw values only — the GUI
//! keeps all the format*() presentation (cpid masking, magnitude/accrual/age
//! strings), so no core type crosses the boundary. Beacon times are Unix seconds
//! so the GUI's periodic timer can recompute countdowns locally without a
//! refetch.
struct ResearcherSnapshot
{
    // Identity
    std::string cpid;             //!< MiningId::ToString(): CPID hex, "NONCRUNCHER", or "" (invalid). has_cpid distinguishes an actual CPID.
    bool has_cpid = false;
    bool has_split_cpid = false;
    bool has_rac = false;
    std::string email;

    // Magnitude / accrual
    double magnitude = 0.0;
    std::string magnitude_text;   //!< Magnitude::ToString() ("%g"); GUI shows verbatim.
    bool has_magnitude = false;
    CAmount accrual = 0;
    std::optional<CAmount> accrual_near_limit; //!< Set when accrual is near the cap.

    // Mode / sync
    bool configured_for_noncruncher_mode = false;
    bool detected_pool_mode = false;
    bool out_of_sync = false;
    std::string mining_status;   //!< msMiningErrors verbatim; GUI shows it below sync.

    // Eligibility helpers (drive UI gating / the MRC seam)
    bool has_eligible_projects = false;
    bool has_pool_projects = false;
    bool action_needed = false;

    // Beacon status
    BeaconStatus beacon_status = BeaconStatus::UNKNOWN;
    bool beacon_present = false;         //!< A beacon record exists (may be expired); gates the age/address/expiry formatters.
    bool pending_beacon_present = false; //!< A pending beacon record exists (may be pending-expired); gates the pending formatters.
    bool has_active_beacon = false;
    bool has_pending_beacon = false;
    bool has_renewable_beacon = false;
    bool beacon_expired = false;
    bool needs_beacon_auth = false;
    std::string beacon_address;
    std::string beacon_verification_code;
    std::string beacon_error;            //!< Human-readable beacon error, if any.
    std::string cached_beacon_pubkey_hex; //!< V3 generated-but-not-advertised key.

    // Beacon times (Unix seconds; the GUI formats age/countdowns locally)
    int64_t beacon_timestamp = 0;
    int64_t beacon_age = 0;
    int64_t time_to_beacon_expiration = 0;
    int64_t time_to_pending_beacon_expiration = 0;

    // Misc
    std::string boinc_data_dir;
    bool is_v14_enabled = false;
};

//! One BOINC project row for the researcher table (Phase 1d-iv), as value data.
//! Fuses the Gridcoin whitelist, locally-detected projects, and scraper magnitude
//! for the CPID — computed node-side in one atomic pass (this is where the last
//! raw scraper read, `ConvergedScraperStatsCache`, moves off the GUI). The GUI
//! maps this to its Qt ProjectRow.
struct ResearcherProjectRow
{
    //! Whitelist state, mirroring the GUI's former ProjectRow::WhiteListStatus.
    enum class WhitelistStatus
    {
        NOT_WHITELISTED,
        EXCLUDED,
        MANUALLY_GREYLISTED,
        AUTOMATICALLY_GREYLISTED,
        WHITELISTED,
    };

    //! Non-status-derivable error label for the row. The greylisted/excluded
    //! labels are NOT here — the GUI derives those from `whitelisted` so there is
    //! one source of truth; this covers only the cases the status can't express.
    //! The GUI maps each to a tr() string (CORE_MESSAGE uses `error_message`).
    enum class ErrorKind
    {
        NONE,
        CORE_MESSAGE,          //!< A core Project::ErrorMessage() (ineligible project).
        NOT_WHITELISTED,       //!< Local project absent from the whitelist.
        NOT_ATTACHED,          //!< Whitelisted project not attached locally.
        USES_EXTERNAL_ADAPTER, //!< Whitelisted project needs an external adapter.
    };

    WhitelistStatus whitelisted = WhitelistStatus::NOT_WHITELISTED;
    ErrorKind error_kind = ErrorKind::NONE;
    std::optional<bool> gdpr_controls;
    std::string name;
    std::string cpid;
    double magnitude = 0.0;
    double rac = 0.0;
    std::string error_message; //!< Core Project::ErrorMessage() when error_kind == CORE_MESSAGE.
};

//! Outcome of a beacon-advertise command: the resulting BeaconStatus the GUI maps
//! to translated text (mirrors the former return of ResearcherModel::advertise*).
struct BeaconAdvertiseResult
{
    BeaconStatus status = BeaconStatus::UNKNOWN;
};

//! One whitelisted project (name + url) — value rows for the poll-wizard project
//! pickers. Decision A (1d-iv): VotingModel's getActiveProjectNames/getActive
//! ProjectUrls migrate onto this too, closing the last 1d-iii ratchet entry.
struct WhitelistProject
{
    std::string name;
    std::string url;
};

//! Called when the researcher context changes (uiInterface.ResearcherChanged).
//! Payload-free (decision B): the consumer refetches snapshot() on its own thread,
//! which drops the former cross-thread marshal of a GRC::ResearcherPtr.
using ResearcherChangedFn = std::function<void()>;

//! Called when accrual changes from a stake or MRC
//! (uiInterface.AccrualChangedFromStakeOrMRC).
using AccrualChangedFn = std::function<void()>;

//! Called when the beacon changes (uiInterface.BeaconChanged).
using BeaconChangedFn = std::function<void()>;

//! Called when the block tip changes (uiInterface.NotifyBlocksChanged) — the
//! researcher panel refreshes sync-dependent state. Payload mirrors the core
//! signal (new best height/time and whether a header-only update).
using BlocksChangedFn = std::function<void(bool, int, int64_t, uint32_t)>;

//! The researcher/beacon/scraper boundary (Phase 1d-iv). Hands the GUI a value
//! snapshot, the fused project table, and command-style beacon/mode operations,
//! so ResearcherModel holds no GRC::Researcher / GRC::Beacon and calls no beacon
//! registry, quorum, whitelist, or scraper-cache global directly. Over the node's
//! single wallet (needed for the beacon-key / advertise / V3 paths).
class ResearcherContext
{
public:
    virtual ~ResearcherContext() = default;

    //! The current researcher/beacon snapshot (one node-side read under cs_main).
    //! Blocks on cs_main; use for the initial fetch and researcher-changed resets.
    virtual ResearcherSnapshot snapshot() = 0;

    //! Like snapshot() but with TRY_LOCK(cs_main): returns nullopt if cs_main is
    //! contended rather than blocking. Preserves the former ResearcherModel::
    //! refresh() behaviour of bowing out cleanly during heavy chain catch-up so
    //! the per-block refresh never stalls the GUI thread.
    virtual std::optional<ResearcherSnapshot> trySnapshot() = 0;

    //! Cheap, lock-free out-of-sync-by-age check (wraps OutOfSyncByAge()). The
    //! former refresh() updated this flag even when it could not take cs_main, so
    //! the GUI can show "Waiting for sync..." during catch-up without blocking.
    virtual bool outOfSync() = 0;

    //! Whether any BOINC project supports beacon ownership-proof (V3) advertisement
    //! (wraps GetProjectsWithOwnershipProofSupport()). Gates the wizard's V3 UI.
    virtual bool hasV3CapableProjects() = 0;

    //! The researcher project table (whitelist + local projects + scraper
    //! magnitude/exclusions, fused node-side). \p extended controls whether
    //! greylist/GDPR detail is included, matching the former buildProjectTable.
    virtual std::vector<ResearcherProjectRow> projects(bool extended) = 0;

    //! The whitelisted projects (name + url) for the poll-wizard pickers.
    virtual std::vector<WhitelistProject> whitelistProjects() = 0;

    //! Maximum on-chain project name / URL lengths (GRC::Project::MAX_NAME_SIZE /
    //! MAX_URL_SIZE). The poll wizard's project-entry fields cap their Qt input at
    //! these, so the limit crosses the boundary rather than gridcoin/project.h.
    virtual int maxProjectNameLength() = 0;
    virtual int maxProjectUrlLength() = 0;

    //! V3-ownership-proof-capable projects (name + url) for the wizard.
    virtual std::vector<WhitelistProject> v3CapableProjects() = 0;

    //! Switch participation mode. SOLO takes the researcher email; POOL /
    //! NONCRUNCHER ignore it. Returns whether the change was applied.
    virtual bool switchMode(ResearcherMode mode, const std::string& email) = 0;

    //! Advertise a (V2) beacon; returns the resulting status.
    virtual BeaconAdvertiseResult advertiseBeacon() = 0;

    //! Generate (and cache) a V3 beacon key, returning its public-key hex ("" on
    //! failure). Runs the cs_main + cs_wallet key generation node-side.
    virtual std::string generateBeaconKeyForV3() = 0;

    //! Advertise a V3 beacon with the supplied ownership-proof XML.
    virtual BeaconAdvertiseResult advertiseBeaconV3(const std::string& ownership_proof_xml) = 0;

    //! Reload the researcher context from configuration.
    virtual void reload() = 0;

    //! Subscribe to the researcher/beacon/accrual/tip notifications.
    virtual std::unique_ptr<Handler> handleResearcherChanged(ResearcherChangedFn fn) = 0;
    virtual std::unique_ptr<Handler> handleBeaconChanged(BeaconChangedFn fn) = 0;
    virtual std::unique_ptr<Handler> handleAccrualChanged(AccrualChangedFn fn) = 0;
    virtual std::unique_ptr<Handler> handleBlocksChanged(BlocksChangedFn fn) = 0;
};

//! Return an in-process ResearcherContext over the global researcher/beacon
//! registries and the node's single wallet. \p wallet must outlive the object.
std::unique_ptr<ResearcherContext> MakeResearcherContext(CWallet* wallet);

} // namespace interfaces

#endif // GRIDCOIN_INTERFACES_RESEARCHER_H
