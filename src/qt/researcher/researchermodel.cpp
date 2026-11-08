// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/gui_state.h"
#include "interfaces/researcher.h"

#include "qt/bitcoinunits.h"
#include "qt/guistatecache.h"
#include "qt/guiutil.h"
#include "qt/researcher/researchermodel.h"
#include "qt/researcher/researcherwizard.h"

#include <QApplication>
#include <QDateTime>
#include <QIcon>
#include <QMessageBox>
#include <QtConcurrentRun>

#include <cassert>
#include <exception>

// -----------------------------------------------------------------------------
// Class: ResearcherModel
// -----------------------------------------------------------------------------

namespace {
//! Which of the four signals a change between two snapshots calls for. Each
//! group lists every field its signal's listeners read, and the sync state,
//! which they all show.
struct Changes
{
    bool researcher = false;
    bool magnitude = false;
    bool accrual = false;
    bool beacon = false;
};

Changes Compare(const interfaces::ResearcherSnapshot& a, const interfaces::ResearcherSnapshot& b)
{
    const bool sync = a.out_of_sync != b.out_of_sync;

    Changes changes;

    changes.researcher = sync
        || a.cpid != b.cpid
        || a.has_cpid != b.has_cpid
        || a.has_split_cpid != b.has_split_cpid
        || a.has_rac != b.has_rac
        || a.email != b.email
        || a.configured_for_noncruncher_mode != b.configured_for_noncruncher_mode
        || a.detected_pool_mode != b.detected_pool_mode
        || a.mining_status != b.mining_status
        || a.has_eligible_projects != b.has_eligible_projects
        || a.has_pool_projects != b.has_pool_projects
        || a.action_needed != b.action_needed
        || a.boinc_data_dir != b.boinc_data_dir
        || a.is_v14_enabled != b.is_v14_enabled;

    changes.magnitude = sync
        || a.magnitude != b.magnitude
        || a.magnitude_text != b.magnitude_text
        || a.has_magnitude != b.has_magnitude;

    changes.accrual = sync
        || a.accrual != b.accrual
        || a.accrual_near_limit != b.accrual_near_limit;

    changes.beacon = sync
        || a.beacon_status != b.beacon_status
        || a.beacon_present != b.beacon_present
        || a.pending_beacon_present != b.pending_beacon_present
        || a.has_active_beacon != b.has_active_beacon
        || a.has_pending_beacon != b.has_pending_beacon
        || a.has_renewable_beacon != b.has_renewable_beacon
        || a.beacon_expired != b.beacon_expired
        || a.needs_beacon_auth != b.needs_beacon_auth
        || a.beacon_address != b.beacon_address
        || a.beacon_verification_code != b.beacon_verification_code
        || a.beacon_error != b.beacon_error
        || a.cached_beacon_pubkey_hex != b.cached_beacon_pubkey_hex
        || a.beacon_timestamp != b.beacon_timestamp
        || a.beacon_age != b.beacon_age
        || a.time_to_beacon_expiration != b.time_to_beacon_expiration
        || a.time_to_pending_beacon_expiration != b.time_to_pending_beacon_expiration;

    return changes;
}
} // anonymous namespace

ResearcherModel::ResearcherModel(interfaces::ResearcherContext& researcher_context,
                                 interfaces::GuiState& gui_state, GuiStateCache& cache)
    : m_researcher_context(researcher_context)
    , m_gui_state(gui_state)
    , m_cache(cache)
    , m_wizard_open(false)
    , m_privacy_enabled(false)
    , m_theme_suffix("_dark")
{
    // Nothing has been computed until the core pushes it, and the node does not
    // compute this part while it syncs: a researcher never computed is shown as
    // waiting for sync, not as an investor.
    m_snapshot.out_of_sync = true;

    connect(&m_cache, &GuiStateCache::changed, this, &ResearcherModel::applyPushedState);
    connect(&m_reload_watcher, &QFutureWatcher<void>::finished, this, &ResearcherModel::finishReload);

    // A state pushed before the cache was built emits no change, so read it
    // once. Nothing listens to this model yet.
    if (m_cache.snapshot().has_researcher) {
        install(m_cache.snapshot().researcher, false);
    }
}

ResearcherModel::~ResearcherModel()
{
    // Join the worker: it uses the researcher context. It contains its own
    // exceptions, so this does not throw out of the destructor.
    m_reload_watcher.waitForFinished();
}

void ResearcherModel::install(interfaces::ResearcherSnapshot next, bool notify)
{
    const Changes changes = Compare(m_snapshot, next);

    // The new snapshot is in place before any signal goes out: the listeners,
    // such as the wizard's beacon and authentication pages, read it, and the
    // one replaced may be an out-of-sync one.
    m_snapshot = std::move(next);

    if (!notify) return;

    if (changes.researcher) emit researcherChanged();
    if (changes.magnitude) emit magnitudeChanged();
    if (changes.accrual) emit accrualChanged();
    if (changes.beacon) emit beaconChanged();
}

void ResearcherModel::applyPushedState()
{
    const interfaces::GuiStateSnapshot& pushed = m_cache.snapshot();

    if (pushed.has_researcher) {
        install(pushed.researcher, true);
        return;
    }

    // The part is absent while the node is out of sync. Keep the last snapshot,
    // out of sync, so nothing computed earlier is shown as current.
    interfaces::ResearcherSnapshot kept = m_snapshot;
    kept.out_of_sync = true;
    install(std::move(kept), true);
}

QString ResearcherModel::mapBeaconStatus(const BeaconStatus status)
{
    switch (status) {
        case BeaconStatus::ACTIVE:
            return tr("Beacon is active.");
        case BeaconStatus::ERROR_INSUFFICIENT_FUNDS:
            return tr("Balance too low to send a beacon contract.");
        case BeaconStatus::ERROR_MISSING_KEY:
            return tr("Beacon private key missing or invalid.");
        case BeaconStatus::ERROR_NOT_NEEDED:
            return tr("Current beacon is not renewable yet.");
        case BeaconStatus::ERROR_TX_FAILED:
            return tr("Unable to send beacon transaction. See debug.log");
        case BeaconStatus::ERROR_INVALID_PROOF_XML:
            return tr("Invalid ownership proof XML. Verify the pasted content.");
        case BeaconStatus::ERROR_WALLET_LOCKED:
            return tr("Unlock wallet fully to send a beacon transaction.");
        case BeaconStatus::NO_BEACON:
            return tr("No active beacon.");
        case BeaconStatus::NO_CPID:
            return tr("No CPID detected.");
        case BeaconStatus::NO_MAGNITUDE:
            return tr("Zero magnitude in the last superblock.");
        case BeaconStatus::PENDING:
            return tr("Pending beacon is awaiting network confirmation.");
        case BeaconStatus::RENEWAL_NEEDED:
            return tr("Beacon expires soon. Renew immediately.");
        case BeaconStatus::RENEWAL_POSSIBLE:
            return tr("Beacon eligible for renewal.");
        case BeaconStatus::ALREADY_IN_MEMPOOL:
            return tr("Beacon advertisement transaction already in mempool. If it is this wallet's "
                      "and is still unconfirmed an hour after sending, sending again replaces it.");
        case BeaconStatus::UNKNOWN:
            return tr("Waiting for sync...");
    }

    assert(false); // Suppress warning
}

QIcon ResearcherModel::mapBeaconStatusIcon(const BeaconStatus status) const
{
    constexpr char success[] = ":/icons/status_beacon_green";
    constexpr char warning[] = ":/icons/status_beacon_yellow";
    constexpr char danger[] = ":/icons/status_beacon_red";
    constexpr char inactive[] = ":/icons/status_beacon_gray";

    const auto make_icon = [this](const char* const icon) {
        return QIcon(icon + m_theme_suffix);
    };

    switch (status) {
        case BeaconStatus::ACTIVE:                   return make_icon(success);
        case BeaconStatus::ERROR_INSUFFICIENT_FUNDS: return make_icon(danger);
        case BeaconStatus::ERROR_MISSING_KEY:        return make_icon(danger);
        case BeaconStatus::ERROR_NOT_NEEDED:         return make_icon(success);
        case BeaconStatus::ERROR_TX_FAILED:          return make_icon(danger);
        case BeaconStatus::ERROR_INVALID_PROOF_XML: return make_icon(danger);
        case BeaconStatus::ERROR_WALLET_LOCKED:      return make_icon(danger);
        case BeaconStatus::NO_BEACON:                return make_icon(inactive);
        case BeaconStatus::NO_CPID:                  return make_icon(inactive);
        case BeaconStatus::NO_MAGNITUDE:             return make_icon(warning);
        case BeaconStatus::PENDING:                  return make_icon(warning);
        case BeaconStatus::RENEWAL_NEEDED:           return make_icon(danger);
        case BeaconStatus::RENEWAL_POSSIBLE:         return make_icon(warning);
        case BeaconStatus::ALREADY_IN_MEMPOOL:       return make_icon(warning);
        case BeaconStatus::UNKNOWN:                  return make_icon(inactive);
    }

    assert(false); // Suppress warning
}

void ResearcherModel::showWizard(WalletModel* wallet_model)
{
    if (m_wizard_open) {
        return;
    }

    if (outOfSync()) {
        QMessageBox::warning(QApplication::activeWindow(),
            QObject::tr("Wallet Not In Sync"),
            QObject::tr("The wallet must be in sync to manage beacons. Please wait "
               "for synchronization to complete before using the researcher "
               "and beacon configuration wizard."));
        return;
    }

    m_wizard_open = true;

    ResearcherWizard *wizard = new ResearcherWizard(nullptr, this, wallet_model);

    if (configuredForNoncruncherMode()) {
        wizard->setStartId(ResearcherWizard::PageNoncruncher);
    } else if (detectedPoolMode()) {
        wizard->setStartId(ResearcherWizard::PagePoolSummary);
    } else if (hasSplitCpid()) {
        // If there is a split CPID situation, then the actionNeeded is also set, but
        // in the case of a split CPID we want to go to the PageSummary screen, where they
        // will see the warning for the split CPID. This is more important than renewing the beacon
        wizard->setStartId(ResearcherWizard::PageSummary);
    } else if (hasRenewableBeacon()) {
        wizard->setStartId(ResearcherWizard::PageBeacon);
    } else if (!actionNeeded()) {
        wizard->setStartId(ResearcherWizard::PageSummary);
    }

    wizard->show();
}

void ResearcherModel::setTheme(const QString& theme_name)
{
    m_theme_suffix = "_" + theme_name;

    emit beaconChanged();
}

void ResearcherModel::setMaskCpidMagnitudeAccrual(bool privacy)
{
    m_privacy_enabled = privacy;

    // The masking only changes how the held snapshot is formatted, so the
    // listeners repaint from it, with no call to the core.
    emit researcherChanged();
    emit magnitudeChanged();
    emit accrualChanged();
    emit beaconChanged();
}

bool ResearcherModel::configuredForNoncruncherMode() const
{
    return m_snapshot.configured_for_noncruncher_mode;
}

bool ResearcherModel::outOfSync() const
{
    return m_snapshot.out_of_sync;
}

bool ResearcherModel::detectedPoolMode() const
{
    return m_snapshot.detected_pool_mode;
}

bool ResearcherModel::actionNeeded() const
{
    return m_snapshot.action_needed;
}

bool ResearcherModel::hasEligibleProjects() const
{
    return m_snapshot.has_eligible_projects;
}

bool ResearcherModel::hasPoolProjects() const
{
    return m_snapshot.has_pool_projects;
}

bool ResearcherModel::hasActiveBeacon() const
{
    return m_snapshot.has_active_beacon;
}

bool ResearcherModel::hasPendingBeacon() const
{
    return m_snapshot.has_pending_beacon;
}

bool ResearcherModel::hasRenewableBeacon() const
{
    return m_snapshot.has_renewable_beacon;
}

bool ResearcherModel::beaconExpired() const
{
    return m_snapshot.beacon_expired;
}

bool ResearcherModel::hasMagnitude() const
{
    return m_snapshot.has_magnitude;
}

bool ResearcherModel::hasRAC() const
{
    return m_snapshot.has_rac;
}

bool ResearcherModel::hasSplitCpid() const
{
    return m_snapshot.has_split_cpid;
}

bool ResearcherModel::needsBeaconAuth() const
{
    return m_snapshot.needs_beacon_auth;
}

std::optional<CAmount> ResearcherModel::accrualNearLimit() const
{
    return m_snapshot.accrual_near_limit;
}

CAmount ResearcherModel::getAccrual() const
{
    return m_snapshot.accrual;
}

QString ResearcherModel::email() const
{
    return QString::fromStdString(m_snapshot.email);
}

QString ResearcherModel::formatCpid() const
{
    if (m_privacy_enabled) {
        return "################################";
    }

    return QString::fromStdString(m_snapshot.cpid);
}

QString ResearcherModel::formatMagnitude() const
{
    if (outOfSync()) {
        return "...";
    }

    if (m_privacy_enabled) {
        return "#";
    }

    return QString::fromStdString(m_snapshot.magnitude_text);
}

QString ResearcherModel::formatAccrual(const int display_unit, bool& near_limit) const
{
    const CAmount accrual = m_snapshot.accrual;
    const std::optional<CAmount> near_limit_accrual = m_snapshot.accrual_near_limit;

    // Defence in depth: the node should already send nullopt when there is no
    // real accrual cap (see Researcher::AccrualNearLimit), but guard against a
    // non-positive threshold here too so a 0 threshold never reads as "at the
    // limit" for a 0 accrual.
    near_limit = near_limit_accrual && *near_limit_accrual > 0 && accrual >= *near_limit_accrual;

    if (outOfSync()) {
        return "...";
    }

    return BitcoinUnits::formatWithPrivacy(display_unit, accrual, m_privacy_enabled);
}

QString ResearcherModel::formatStatus() const
{
    if (outOfSync()) {
        return tr("Waiting for sync...");
    }

    return QString::fromStdString(m_snapshot.mining_status);
}

QString ResearcherModel::formatBoincPath() const
{
    return QString::fromStdString(m_snapshot.boinc_data_dir);
}

BeaconStatus ResearcherModel::getBeaconStatus() const
{
    return m_snapshot.beacon_status;
}

QString ResearcherModel::formatBeaconStatus() const
{
    return mapBeaconStatus(m_snapshot.beacon_status);
}

QIcon ResearcherModel::getBeaconStatusIcon() const
{
    return mapBeaconStatusIcon(m_snapshot.beacon_status);
}

QString ResearcherModel::formatBeaconAge() const
{
    if (!m_snapshot.beacon_present) {
        return QString();
    }

    // Compute the age live from the beacon timestamp so the 60s beacon-tooltip
    // timer ticks between snapshot refreshes (the former model recomputed
    // Beacon::Age() on each call). Local wall-clock time is used rather than the
    // node's network-adjusted time; the difference is at most a few seconds and
    // invisible at formatDurationStr's granularity.
    const int64_t age = QDateTime::currentSecsSinceEpoch() - m_snapshot.beacon_timestamp;
    return GUIUtil::formatDurationStr(age);
}

QString ResearcherModel::formatTimeToBeaconExpiration() const
{
    if (!m_snapshot.beacon_present) {
        return QString();
    }

    // Recompute the remaining time live for the same reason. The beacon's max age
    // is constant, recovered here as (snapshot age + snapshot remaining) so no
    // core constant has to cross the interface boundary.
    const int64_t max_age = m_snapshot.beacon_age + m_snapshot.time_to_beacon_expiration;
    const int64_t age = QDateTime::currentSecsSinceEpoch() - m_snapshot.beacon_timestamp;
    return GUIUtil::formatDurationStr(max_age - age);
}

QString ResearcherModel::formatTimeToPendingBeaconExpiration() const
{
    if (!m_snapshot.pending_beacon_present) {
        return QString();
    }

    return GUIUtil::formatDurationStr(m_snapshot.time_to_pending_beacon_expiration);
}

QString ResearcherModel::formatBeaconAddress() const
{
    if (!m_snapshot.beacon_present) {
        return QString();
    }

    return QString::fromStdString(m_snapshot.beacon_address);
}

QString ResearcherModel::formatBeaconVerificationCode() const
{
    if (!m_snapshot.pending_beacon_present) {
        return QString();
    }

    return QString::fromStdString(m_snapshot.beacon_verification_code);
}

ProjectRow ResearcherModel::mapProjectRow(const interfaces::ResearcherProjectRow& src) const
{
    using WS = interfaces::ResearcherProjectRow::WhitelistStatus;
    using EK = interfaces::ResearcherProjectRow::ErrorKind;

    ProjectRow row;
    // The node sends the display name in original case; lower it here with
    // QString::toLower() (Unicode-aware), matching the former buildProjectTable.
    row.m_name = QString::fromStdString(src.name).toLower();
    row.m_cpid = QString::fromStdString(src.cpid);
    row.m_magnitude = src.magnitude;
    row.m_rac = src.rac;
    row.m_gdpr_controls = src.gdpr_controls;

    switch (src.whitelisted) {
        case WS::NOT_WHITELISTED:          row.m_whitelisted = ProjectRow::False; break;
        case WS::EXCLUDED:                 row.m_whitelisted = ProjectRow::Excluded; break;
        case WS::MANUALLY_GREYLISTED:      row.m_whitelisted = ProjectRow::Manually_Greylisted; break;
        case WS::AUTOMATICALLY_GREYLISTED: row.m_whitelisted = ProjectRow::Automatically_Greylisted; break;
        case WS::WHITELISTED:              row.m_whitelisted = ProjectRow::True; break;
    }

    // The greylisted/excluded labels are derived from the status (single source
    // of truth); every other label comes from the node-side error kind.
    switch (src.whitelisted) {
        case WS::MANUALLY_GREYLISTED:
            row.m_error = tr("Manually Greylisted");
            break;
        case WS::AUTOMATICALLY_GREYLISTED:
            row.m_error = tr("Automatically Greylisted");
            break;
        case WS::EXCLUDED:
            row.m_error = tr("Excluded");
            break;
        default:
            switch (src.error_kind) {
                case EK::NONE:
                    break;
                case EK::CORE_MESSAGE:
                    row.m_error = QString::fromStdString(src.error_message);
                    break;
                case EK::NOT_WHITELISTED:
                    row.m_error = tr("Not whitelisted");
                    break;
                case EK::NOT_ATTACHED:
                    row.m_error = tr("Not attached");
                    break;
                case EK::USES_EXTERNAL_ADAPTER:
                    row.m_error = tr("Uses external adapter");
                    break;
            }
            break;
    }

    return row;
}

std::vector<ProjectRow> ResearcherModel::buildProjectTable(bool extended) const
{
    std::vector<ProjectRow> rows;

    for (const auto& src : m_researcher_context.projects(extended)) {
        rows.push_back(mapProjectRow(src));
    }

    return rows;
}

void ResearcherModel::reload()
{
    // A user action that keeps its core call, made on a worker: the call takes
    // cs_main. A request made while one runs is joined by one more run after it.
    if (m_reload_watcher.isRunning()) {
        m_reload_again = true;
        return;
    }

    m_reload_watcher.setFuture(QtConcurrent::run([this]() {
        try {
            m_researcher_context.reload();
        } catch (const std::exception&) {
            // Nothing to report: the next push shows whatever state the core is in.
        } catch (...) {
        }
    }));
}

void ResearcherModel::finishReload()
{
    // The result reaches the model with the next push; ask for it now rather
    // than wait for the core's cadence.
    m_gui_state.requestUpdate();

    emit reloaded();

    if (m_reload_again) {
        m_reload_again = false;
        reload();
    }
}

bool ResearcherModel::switchToSolo(const QString& email)
{
    // Optimistically clear the noncruncher flag so the wizard's immediately
    // following page selection sees the new mode (the former model set the
    // member directly before calling ChangeMode).
    m_snapshot.configured_for_noncruncher_mode = false;

    const bool result = m_researcher_context.switchMode(interfaces::ResearcherMode::SOLO, email.toStdString());
    m_gui_state.requestUpdate();
    return result;
}

bool ResearcherModel::switchToPool()
{
    m_snapshot.configured_for_noncruncher_mode = false;

    const bool result = m_researcher_context.switchMode(interfaces::ResearcherMode::POOL, std::string());
    m_gui_state.requestUpdate();
    return result;
}

bool ResearcherModel::switchToNoncruncher()
{
    m_snapshot.configured_for_noncruncher_mode = true;

    const bool result = m_researcher_context.switchMode(interfaces::ResearcherMode::NONCRUNCHER, std::string());
    m_gui_state.requestUpdate();
    return result;
}

BeaconStatus ResearcherModel::advertiseBeacon()
{
    const BeaconStatus status = m_researcher_context.advertiseBeacon().status;
    m_gui_state.requestUpdate();
    return status;
}

bool ResearcherModel::isV14Enabled() const
{
    return m_snapshot.is_v14_enabled;
}

bool ResearcherModel::hasV3CapableProjects() const
{
    return m_researcher_context.hasV3CapableProjects();
}

std::vector<std::pair<QString, QString>> ResearcherModel::buildV3ProjectList() const
{
    std::vector<std::pair<QString, QString>> result;

    for (const auto& project : m_researcher_context.v3CapableProjects()) {
        result.emplace_back(QString::fromStdString(project.name),
                            QString::fromStdString(project.url));
    }

    return result;
}

std::vector<std::pair<QString, QString>> ResearcherModel::buildPoolList() const
{
    std::vector<std::pair<QString, QString>> result;

    for (const auto& pool : m_researcher_context.activePools()) {
        result.emplace_back(QString::fromStdString(pool.name),
                            QString::fromStdString(pool.url));
    }

    return result;
}

QString ResearcherModel::generateBeaconKeyForV3()
{
    const std::string public_key_hex = m_researcher_context.generateBeaconKeyForV3();

    // Only cache on success (non-empty), matching the former model: a failed
    // regeneration must not clear a previously generated key.
    if (!public_key_hex.empty()) {
        m_cached_beacon_pubkey_hex = QString::fromStdString(public_key_hex);
        return m_cached_beacon_pubkey_hex;
    }

    return QString();
}

BeaconStatus ResearcherModel::advertiseBeaconV3(const QString& ownership_proof_xml)
{
    const BeaconStatus status = m_researcher_context.advertiseBeaconV3(ownership_proof_xml.toStdString()).status;
    m_gui_state.requestUpdate();
    return status;
}

QString ResearcherModel::cachedBeaconPubKeyHex() const
{
    return m_cached_beacon_pubkey_hex;
}

void ResearcherModel::onWizardClose()
{
    m_cached_beacon_pubkey_hex.clear();
    m_wizard_open = false;
}
