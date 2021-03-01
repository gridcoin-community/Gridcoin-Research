// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "base58.h"
#include "main.h"
#include "gridcoin/beacon.h"
#include "gridcoin/boinc.h"
#include "gridcoin/magnitude.h"
#include "gridcoin/project.h"
#include "gridcoin/quorum.h"
#include "gridcoin/researcher.h"
#include "ui_interface.h"

#include "qt/bitcoinunits.h"
#include "qt/guiutil.h"
#include "qt/researcher/researchermodel.h"
#include "qt/researcher/researcherwizard.h"

#include <QIcon>
#include <QMessageBox>
#include <QTimer>

extern CWallet* pwalletMain;

using namespace GRC;
using LogFlags = BCLog::LogFlags;

namespace {
constexpr double SECONDS_IN_DAY = 24.0 * 60.0 * 60.0;
constexpr int64_t BEACON_RENEWAL_WARNING_THRESHOLD = 15 * SECONDS_IN_DAY;

//!
//! \brief Model callback bound to the \c ResearcherChanged core signal.
//!
void ResearcherChanged(ResearcherModel* model)
{
    LogPrint(LogFlags::QT, "GUI: received ResearcherChanged() core signal");

    QMetaObject::invokeMethod(
        model,
        "resetResearcher",
        Qt::QueuedConnection,
        Q_ARG(GRC::ResearcherPtr, Researcher::Get()));
}

//!
//! \brief Model callback bound to the \c BeaconChanged core signal.
//!
void BeaconChanged(ResearcherModel* model)
{
    LogPrint(LogFlags::QT, "GUI: received BeaconChanged() core signal");

    QMetaObject::invokeMethod(model, "updateBeacon", Qt::QueuedConnection);
}

//!
//! \brief Convert a beacon advertisement error to a beacon status.
//!
//! \param error Beacon advertisement error provided the researcher API.
//!
//! \return Describes the advertisement error as a beacon status.
//!
BeaconStatus MapAdvertiseBeaconError(const BeaconError error)
{
    switch (error) {
        case BeaconError::NONE:               return BeaconStatus::ACTIVE;
        case BeaconError::INSUFFICIENT_FUNDS: return BeaconStatus::ERROR_INSUFFICIENT_FUNDS;
        case BeaconError::MISSING_KEY:        return BeaconStatus::ERROR_MISSING_KEY;
        case BeaconError::NO_CPID:            return BeaconStatus::NO_CPID;
        case BeaconError::NOT_NEEDED:         return BeaconStatus::ERROR_NOT_NEEDED;
        case BeaconError::PENDING:            return BeaconStatus::PENDING;
        case BeaconError::TX_FAILED:          return BeaconStatus::ERROR_TX_FAILED;
        case BeaconError::WALLET_LOCKED:      return BeaconStatus::ERROR_WALLET_LOCKED;
    }

    assert(false); // Suppress warning
}
} // anonymous namespace

// -----------------------------------------------------------------------------
// Class: ResearcherModel
// -----------------------------------------------------------------------------

ResearcherModel::ResearcherModel()
    : m_beacon_status(BeaconStatus::UNKNOWN)
    , m_configured_for_investor_mode(false)
    , m_wizard_open(false)
{
    qRegisterMetaType<ResearcherPtr>("GRC::ResearcherPtr");

    resetResearcher(Researcher::Get());
    subscribeToCoreSignals();

    if (GRC::Researcher::ConfiguredForInvestorMode()) {
        m_configured_for_investor_mode = true;
    }

    QTimer *refresh_timer = new QTimer(this);
    connect(refresh_timer, SIGNAL(timeout()), this, SLOT(refresh()));
    refresh_timer->start(30 * 1000);
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
        case BeaconStatus::UNKNOWN:
            return tr("Waiting for data.");
    }

    assert(false); // Suppress warning
}

QIcon ResearcherModel::mapBeaconStatusIcon(const BeaconStatus status)
{
    constexpr char success[] = ":/icons/beacon_green";
    constexpr char warning[] = ":/icons/beacon_yellow";
    constexpr char danger[] = ":/icons/beacon_red";
    constexpr char inactive[] = ":/icons/beacon_grey";

    switch (status) {
        case BeaconStatus::ACTIVE:                   return QIcon(success);
        case BeaconStatus::ERROR_INSUFFICIENT_FUNDS: return QIcon(danger);
        case BeaconStatus::ERROR_MISSING_KEY:        return QIcon(danger);
        case BeaconStatus::ERROR_NOT_NEEDED:         return QIcon(success);
        case BeaconStatus::ERROR_TX_FAILED:          return QIcon(danger);
        case BeaconStatus::ERROR_WALLET_LOCKED:      return QIcon(danger);
        case BeaconStatus::NO_BEACON:                return QIcon(inactive);
        case BeaconStatus::NO_CPID:                  return QIcon(inactive);
        case BeaconStatus::NO_MAGNITUDE:             return QIcon(warning);
        case BeaconStatus::PENDING:                  return QIcon(warning);
        case BeaconStatus::RENEWAL_NEEDED:           return QIcon(danger);
        case BeaconStatus::RENEWAL_POSSIBLE:         return QIcon(warning);
        case BeaconStatus::UNKNOWN:                  return QIcon(inactive);
    }

    assert(false); // Suppress warning
}

void ResearcherModel::showWizard(WalletModel* wallet_model)
{
    if (m_wizard_open) {
        return;
    }

    m_wizard_open = true;

    ResearcherWizard *wizard = new ResearcherWizard(nullptr, this, wallet_model);

    if (configuredForInvestorMode()) {
        wizard->setStartId(ResearcherWizard::PageInvestor);
    } else if (detectedPoolMode()) {
        wizard->setStartId(ResearcherWizard::PagePoolSummary);
    } else if (hasRenewableBeacon()) {
        wizard->setStartId(ResearcherWizard::PageBeacon);
    } else if (!actionNeeded()) {
        wizard->setStartId(ResearcherWizard::PageSummary);
    }

    wizard->show();
}

bool ResearcherModel::configuredForInvestorMode() const
{
    return m_configured_for_investor_mode;
}

bool ResearcherModel::detectedPoolMode() const
{
    return !hasEligibleProjects() && hasPoolProjects();
}

bool ResearcherModel::actionNeeded() const
{
    if (configuredForInvestorMode()) {
        return false;
    }

    if (hasEligibleProjects()) {
        return !hasActiveBeacon() && !hasPendingBeacon();
    }

    return !hasPoolProjects();
}

bool ResearcherModel::hasEligibleProjects() const
{
    return m_researcher->Id().Which() == MiningId::Kind::CPID;
}

bool ResearcherModel::hasPoolProjects() const
{
    return m_researcher->Projects().ContainsPool();
}

bool ResearcherModel::hasActiveBeacon() const
{
    return m_beacon && !m_beacon->Expired(GetAdjustedTime());
}

bool ResearcherModel::hasPendingBeacon() const
{
    return m_pending_beacon.operator bool();
}

bool ResearcherModel::hasRenewableBeacon() const
{
    return m_beacon && m_beacon->Renewable(GetAdjustedTime());
}

bool ResearcherModel::hasMagnitude() const
{
    return m_researcher->Magnitude() != 0;
}

bool ResearcherModel::hasRAC() const
{
    return m_researcher->HasRAC();
}

bool ResearcherModel::needsBeaconAuth() const
{
    if (!hasPendingBeacon()) {
        return false;
    }

    if (!hasActiveBeacon()) {
        return true;
    }

    return m_beacon->m_public_key != m_pending_beacon->m_public_key;
}

QString ResearcherModel::email() const
{
    return QString::fromStdString(Researcher::Email());
}

QString ResearcherModel::formatCpid() const
{
    return QString::fromStdString(m_researcher->Id().ToString());
}

QString ResearcherModel::formatMagnitude() const
{
    return QString::fromStdString(m_researcher->Magnitude().ToString());
}

QString ResearcherModel::formatAccrual(const int display_unit) const
{
    return BitcoinUnits::formatWithUnit(display_unit, m_researcher->Accrual());
}

QString ResearcherModel::formatStatus() const
{
    // TODO: The getmininginfo RPC shares this global. Refactor to remove it:
    return QString::fromStdString(msMiningErrors);
}

QString ResearcherModel::formatBoincPath() const
{
    return QString::fromStdString(GetBoincDataDir().string());
}

BeaconStatus ResearcherModel::getBeaconStatus() const
{
    return m_beacon_status;
}

QString ResearcherModel::formatBeaconStatus() const
{
    return mapBeaconStatus(m_beacon_status);
}

QIcon ResearcherModel::getBeaconStatusIcon() const
{
    return mapBeaconStatusIcon(m_beacon_status);
}

QString ResearcherModel::formatBeaconAge() const
{
    if (!m_beacon) {
        return QString();
    }

    return GUIUtil::formatDurationStr(m_beacon->Age(GetAdjustedTime()));
}

QString ResearcherModel::formatTimeToBeaconExpiration() const
{
    if (!m_beacon) {
        return QString();
    }

    return GUIUtil::formatDurationStr(Beacon::MAX_AGE - m_beacon->Age(GetAdjustedTime()));
}

QString ResearcherModel::formatBeaconAddress() const
{
    if (!m_beacon) {
        return QString();
    }

    return QString::fromStdString(m_beacon->GetAddress().ToString());
}

QString ResearcherModel::formatBeaconVerificationCode() const
{
    if (!m_pending_beacon) {
        return QString();
    }

    return QString::fromStdString(m_pending_beacon->GetVerificationCode());
}

std::vector<ProjectRow> ResearcherModel::buildProjectTable(bool extended) const
{
    // We do a funny dance here to link-up three loosly-related record types:
    //
    //   - Local BOINC projects detected from client_state.xml
    //   - Projects on the Gridcoin whitelist
    //   - Project magnitude statistics produced by the scrapers
    //
    // ...into an overview of all three that shows how a participant's attached
    // projects behave in the network.
    //

    const WhitelistSnapshot whitelist = GetWhitelist().Snapshot();
    std::vector<ExplainMagnitudeProject> explain_mag;
    std::map<std::string, ProjectRow> rows;

    if (extended) {
        if (const CpidOption cpid = m_researcher->Id().TryCpid()) {
            explain_mag = GRC::Quorum::ExplainMagnitude(*cpid);
        }
    }

    for (const auto& project_pair : m_researcher->Projects()) {
        const MiningProject& project = project_pair.second;

        ProjectRow row;

        if (!project.m_cpid.IsZero()) {
            row.m_cpid = QString::fromStdString(project.m_cpid.ToString());
        }

        if (!project.Eligible()) {
            row.m_error = QString::fromStdString(project.ErrorMessage());
        }

        // Project whitelist contracts may not contain names that match the
        // project names in BOINC's client_state.xml file. We use a routine
        // that also compares the project URL to establish the relationship
        // between local projects and whitelisted projects:
        //
        if (const Project* whitelist_project = project.TryWhitelist(whitelist)) {
            row.m_whitelisted = true;
            row.m_name = QString::fromStdString(whitelist_project->DisplayName()).toLower();

            for (const auto& explain_mag_project : explain_mag) {
                if (explain_mag_project.m_name == whitelist_project->m_name) {
                    row.m_magnitude = explain_mag_project.m_magnitude;
                    row.m_rac = explain_mag_project.m_rac;
                    break;
                }
            }

            rows.emplace(whitelist_project->m_name, std::move(row));
        } else {
            row.m_whitelisted = false;
            row.m_name = QString::fromStdString(project.m_name).toLower();
            row.m_rac = project.m_rac;

            if (project.Eligible()) {
                row.m_error = tr("Not whitelisted");
            }

            rows.emplace(project.m_name, std::move(row));
        }
    }

    // Add any whitelisted projects not detected from the local BOINC client:
    //
    for (const auto& project : GetWhitelist().Snapshot()) {
        if (rows.find(project.m_name) != rows.end()) {
            continue;
        }

        ProjectRow row;
        row.m_whitelisted = true;
        row.m_name = QString::fromStdString(project.DisplayName()).toLower();
        row.m_magnitude = 0.0;
        row.m_error = tr("Not attached");

        for (const auto& explain_mag_project : explain_mag) {
            if (explain_mag_project.m_name == project.m_name) {
                row.m_magnitude = explain_mag_project.m_magnitude;
                row.m_rac = explain_mag_project.m_rac;
                break;
            }
        }

        rows.emplace(project.m_name, std::move(row));
    }

    std::vector<ProjectRow> rows_out;
    rows_out.reserve(rows.size());

    for (auto& row_pair : rows) {
        rows_out.emplace_back(std::move(row_pair.second));
    }

    return rows_out;
}

void ResearcherModel::reload()
{
    Researcher::Reload();
    resetResearcher(Researcher::Get());
}

void ResearcherModel::refresh()
{
    updateBeacon();

    emit magnitudeChanged();
    emit accrualChanged();
}

void ResearcherModel::resetResearcher(ResearcherPtr researcher)
{
    m_researcher = std::move(researcher);

    emit researcherChanged();

    updateBeacon();
}

bool ResearcherModel::switchToSolo(const QString& email)
{
    m_configured_for_investor_mode = false;

    return m_researcher->ChangeMode(ResearcherMode::SOLO, email.toStdString());
}

bool ResearcherModel::switchToPool()
{
    m_configured_for_investor_mode = false;

    return m_researcher->ChangeMode(ResearcherMode::POOL, std::string());
}

bool ResearcherModel::switchToInvestor()
{
    m_configured_for_investor_mode = true;

    return m_researcher->ChangeMode(ResearcherMode::INVESTOR, std::string());
}

void ResearcherModel::updateBeacon()
{
    const CpidOption cpid = m_researcher->Id().TryCpid();

    if (!cpid) {
        m_beacon.reset(nullptr);
        m_beacon_status = BeaconStatus::NO_CPID;

        emit beaconChanged();

        return;
    }

    bool beacon_key_present = false;

    if (auto beacon_option = m_researcher->TryBeacon()) {
        m_beacon.reset(new Beacon(std::move(*beacon_option)));
        beacon_key_present = m_beacon->WalletHasPrivateKey(pwalletMain);
    } else {
        m_beacon.reset(nullptr);
    }

    if (auto beacon_option = m_researcher->TryPendingBeacon()) {
        m_pending_beacon.reset(new Beacon(std::move(*beacon_option)));
        beacon_key_present = m_pending_beacon->WalletHasPrivateKey(pwalletMain);
    } else {
        m_pending_beacon.reset(nullptr);
    }

    if (beacon_key_present) {
        m_beacon_status = MapAdvertiseBeaconError(m_researcher->BeaconError());
    } else {
        m_beacon_status = BeaconStatus::ERROR_MISSING_KEY;
    }

    // If automatic advertisement/renewal encountered a problem, raise this
    // error first:
    //
    if (m_beacon_status != BeaconStatus::ACTIVE) {
        emit beaconChanged();
        return;
    }

    if (m_pending_beacon) {
        m_beacon_status = BeaconStatus::PENDING;
    } else if (m_beacon) {
        const int64_t now = GetAdjustedTime();

        if (m_beacon->Expired(now + BEACON_RENEWAL_WARNING_THRESHOLD)) {
            m_beacon_status = BeaconStatus::RENEWAL_NEEDED;
        } else if (m_beacon->Renewable(now)) {
            m_beacon_status = BeaconStatus::RENEWAL_POSSIBLE;
        } else if (m_researcher->Magnitude() == 0) {
            m_beacon_status = BeaconStatus::NO_MAGNITUDE;
        } else {
            m_beacon_status = BeaconStatus::ACTIVE;
        }
    } else {
        m_beacon_status = BeaconStatus::NO_BEACON;
    }

    emit beaconChanged();
}

BeaconStatus ResearcherModel::advertiseBeacon()
{
    const AdvertiseBeaconResult result = m_researcher->AdvertiseBeacon();

    return MapAdvertiseBeaconError(result.Error());
}

void ResearcherModel::onWizardClose()
{
    m_wizard_open = false;
}

void ResearcherModel::subscribeToCoreSignals()
{
    // Connect signals to client
    uiInterface.ResearcherChanged.connect(boost::bind(ResearcherChanged, this));
    uiInterface.BeaconChanged.connect(boost::bind(BeaconChanged, this));
}

void ResearcherModel::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client
    uiInterface.ResearcherChanged.disconnect(boost::bind(ResearcherChanged, this));
    uiInterface.ResearcherChanged.disconnect(boost::bind(BeaconChanged, this));
}
