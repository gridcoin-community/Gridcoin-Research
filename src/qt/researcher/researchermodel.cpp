// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/handler.h"
#include "interfaces/researcher.h"

#include "qt/bitcoinunits.h"
#include "qt/guiutil.h"
#include "qt/researcher/researchermodel.h"
#include "qt/researcher/researcherwizard.h"

#include <QApplication>
#include <QDateTime>
#include <QIcon>
#include <QMessageBox>

#include <cassert>

// -----------------------------------------------------------------------------
// Class: ResearcherModel
// -----------------------------------------------------------------------------

ResearcherModel::ResearcherModel(interfaces::ResearcherContext& researcher_context)
    : m_researcher_context(researcher_context)
    , m_wizard_open(false)
    , m_privacy_enabled(false)
    , m_theme_suffix("_dark")
{
    // Prime the cached snapshot before any getter can run, then subscribe.
    m_snapshot = m_researcher_context.snapshot();
    subscribeToCoreSignals();

    // The 30-second polling refresh timer that used to live here is gone in
    // favour of an event-driven refresh on the interface's block-tip
    // notification (subscribed below). Per-block accrual updates propagate
    // within one chain-tip-advance event. The researcher/beacon/accrual
    // notifications drive their own targeted refreshes as before.
}

ResearcherModel::~ResearcherModel()
{
    unsubscribeFromCoreSignals();
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
            return tr("Beacon advertisement transaction already in mempool.");
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

    refresh();
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

    near_limit = near_limit_accrual && accrual >= *near_limit_accrual;

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
    m_researcher_context.reload();
    onResearcherChanged();
}

void ResearcherModel::refresh()
{
    // Cheap, lock-free sync-state check first, mirroring the former refresh():
    // the "Waiting for sync..." state must update even when cs_main is contended.
    const bool out_of_sync = m_researcher_context.outOfSync();

    if (out_of_sync != m_snapshot.out_of_sync) {
        m_snapshot.out_of_sync = out_of_sync;
        emit researcherChanged();
    }

    // Full refresh via TRY_LOCK: bow out cleanly under cs_main contention rather
    // than stalling the GUI thread during heavy chain catch-up.
    if (auto snap = m_researcher_context.trySnapshot()) {
        m_snapshot = std::move(*snap);
        emit magnitudeChanged();
        emit accrualChanged();
        emit beaconChanged();
    }
}

void ResearcherModel::onResearcherChanged()
{
    // A researcher-context change is significant and infrequent; take the full
    // (blocking) snapshot and repaint everything, matching the former
    // resetResearcher() -> emit researcherChanged() + updateBeacon() sequence.
    m_snapshot = m_researcher_context.snapshot();

    emit researcherChanged();
    emit beaconChanged();
    emit magnitudeChanged();
    emit accrualChanged();
}

bool ResearcherModel::switchToSolo(const QString& email)
{
    // Optimistically clear the noncruncher flag so the wizard's immediately
    // following page selection sees the new mode (the former model set the
    // member directly before calling ChangeMode).
    m_snapshot.configured_for_noncruncher_mode = false;

    return m_researcher_context.switchMode(interfaces::ResearcherMode::SOLO, email.toStdString());
}

bool ResearcherModel::switchToPool()
{
    m_snapshot.configured_for_noncruncher_mode = false;

    return m_researcher_context.switchMode(interfaces::ResearcherMode::POOL, std::string());
}

bool ResearcherModel::switchToNoncruncher()
{
    m_snapshot.configured_for_noncruncher_mode = true;

    return m_researcher_context.switchMode(interfaces::ResearcherMode::NONCRUNCHER, std::string());
}

void ResearcherModel::updateBeacon()
{
    // Beacon state lives in the snapshot; refetch (blocking — beacon changes are
    // infrequent) and repaint the beacon display.
    m_snapshot = m_researcher_context.snapshot();

    emit beaconChanged();
    emit researcherChanged();
}

BeaconStatus ResearcherModel::advertiseBeacon()
{
    return m_researcher_context.advertiseBeacon().status;
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
    return m_researcher_context.advertiseBeaconV3(ownership_proof_xml.toStdString()).status;
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

void ResearcherModel::subscribeToCoreSignals()
{
    // Subscribe to the interface notifications, retaining each handler so it is
    // severed in unsubscribeFromCoreSignals() (from ~ResearcherModel). The
    // callbacks fire on a core thread, so each marshals to the GUI thread via a
    // queued invocation of the matching slot; a signal firing after this object
    // is gone would otherwise touch freed memory (issue #3129).
    m_handlers.emplace_back(m_researcher_context.handleResearcherChanged(
        [this]() { QMetaObject::invokeMethod(this, "onResearcherChanged", Qt::QueuedConnection); }));

    m_handlers.emplace_back(m_researcher_context.handleBeaconChanged(
        [this]() { QMetaObject::invokeMethod(this, "updateBeacon", Qt::QueuedConnection); }));

    m_handlers.emplace_back(m_researcher_context.handleAccrualChanged(
        [this]() { QMetaObject::invokeMethod(this, "refresh", Qt::QueuedConnection); }));

    m_handlers.emplace_back(m_researcher_context.handleBlocksChanged(
        [this](bool, int, int64_t, uint32_t) {
            QMetaObject::invokeMethod(this, "refresh", Qt::QueuedConnection);
        }));
}

void ResearcherModel::unsubscribeFromCoreSignals()
{
    // Clearing the retained handlers runs each Handler's destructor, which
    // disconnects it (issue #3129).
    m_handlers.clear();
}
