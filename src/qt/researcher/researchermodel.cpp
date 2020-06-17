#include "base58.h"
#include "bitcoinunits.h"
#include "guiutil.h"
#include "neuralnet/beacon.h"
#include "neuralnet/magnitude.h"
#include "neuralnet/researcher.h"
#include "researchermodel.h"
#include "ui_interface.h"

#include <QIcon>
#include <QTimer>

using namespace NN;

extern CCriticalSection cs_main;
extern std::string msMiningErrors;

namespace {
constexpr double SECONDS_IN_DAY = 24.0 * 60.0 * 60.0;
constexpr int64_t BEACON_RENEWAL_WARNING_AGE = Beacon::MAX_AGE - (15 * SECONDS_IN_DAY);

//!
//! \brief Model callback bound to the \c ResearcherChanged core signal.
//!
void ResearcherChanged(ResearcherModel* model)
{
    LogPrintf("ResearcherChanged()");
    QMetaObject::invokeMethod(
        model,
        "resetResearcher",
        Qt::QueuedConnection,
        Q_ARG(NN::ResearcherPtr, Researcher::Get()));
}

//!
//! \brief Model callback bound to the \c BeaconChanged core signal.
//!
void BeaconChanged(ResearcherModel* model)
{
    LogPrintf("BeaconChanged()");
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
        case BeaconError::NOT_NEEDED:         return BeaconStatus::ACTIVE;
        case BeaconError::PENDING:            return BeaconStatus::PENDING;
        case BeaconError::TX_FAILED:          return BeaconStatus::ERROR_TX_FAILED;
        case BeaconError::WALLET_LOCKED:      return BeaconStatus::ERROR_WALLET_LOCKED;
    }

    return BeaconStatus::UNKNOWN;
};
} // anonymous namespace

// -----------------------------------------------------------------------------
// Class: ResearcherModel
// -----------------------------------------------------------------------------

ResearcherModel::ResearcherModel()
{
    qRegisterMetaType<ResearcherPtr>("NN::ResearcherPtr");

    resetResearcher(Researcher::Get());

    if (NN::Researcher::ConfiguredForInvestorMode()) {
        m_configured_for_investor_mode = true;
        return;
    }

    m_configured_for_investor_mode = false;

    subscribeToCoreSignals();

    QTimer *refresh_timer = new QTimer(this);
    connect(refresh_timer, SIGNAL(timeout()), this, SLOT(refresh()));
    refresh_timer->start(30 * 1000);
}

ResearcherModel::~ResearcherModel()
{
    unsubscribeFromCoreSignals();
}

bool ResearcherModel::configuredForInvestorMode() const
{
    return m_configured_for_investor_mode;
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
    if (configuredForInvestorMode()) {
        return tr("Researcher mode disabled by configuration");
    }

    // TODO: The getmininginfo RPC shares this global. Refactor to remove it:
    return QString::fromStdString(msMiningErrors);
}

BeaconStatus ResearcherModel::getBeaconStatus() const
{
    return m_beacon_status;
}

QString ResearcherModel::formatBeaconStatus() const
{
    switch (m_beacon_status) {
        case BeaconStatus::ACTIVE:
            return tr("Beacon is active.");
        case BeaconStatus::ERROR_INSUFFICIENT_FUNDS:
            return tr("Balance too low to send a beacon contract.");
        case BeaconStatus::ERROR_MISSING_KEY:
            return tr("Beacon private key missing or invalid.");
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

    return tr("Waiting for data.");
}

QIcon ResearcherModel::getBeaconStatusIcon() const
{
    constexpr char success[] = ":/icons/beacon_green";
    constexpr char warning[] = ":/icons/beacon_yellow";
    constexpr char danger[] = ":/icons/beacon_red";
    constexpr char inactive[] = ":/icons/beacon_grey";

    switch (m_beacon_status) {
        case BeaconStatus::ACTIVE:                   return QIcon(success);
        case BeaconStatus::ERROR_INSUFFICIENT_FUNDS: return QIcon(danger);
        case BeaconStatus::ERROR_MISSING_KEY:        return QIcon(danger);
        case BeaconStatus::ERROR_TX_FAILED:          return QIcon(danger);
        case BeaconStatus::ERROR_WALLET_LOCKED:      return QIcon(danger);
        case BeaconStatus::NO_BEACON:                return QIcon(danger);
        case BeaconStatus::NO_CPID:                  return QIcon(inactive);
        case BeaconStatus::NO_MAGNITUDE:             return QIcon(warning);
        case BeaconStatus::PENDING:                  return QIcon(warning);
        case BeaconStatus::RENEWAL_NEEDED:           return QIcon(danger);
        case BeaconStatus::RENEWAL_POSSIBLE:         return QIcon(warning);
        case BeaconStatus::UNKNOWN:                  return QIcon(inactive);
    }

    return QIcon(inactive);
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

void ResearcherModel::refresh()
{
    updateBeacon();

    emit accrualChanged();
}

void ResearcherModel::resetResearcher(ResearcherPtr researcher)
{
    m_researcher = std::move(researcher);

    emit researcherChanged();

    updateBeacon();
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

    {
        LOCK(cs_main);
        const BeaconOption beacon = GetBeaconRegistry().Try(*cpid);

        if (!beacon) {
            m_beacon.reset(nullptr);
            m_beacon_status = BeaconStatus::NO_BEACON;

            emit beaconChanged();

            return;
        }

        m_beacon.reset(new Beacon(*beacon));
        m_beacon_status = MapAdvertiseBeaconError(m_researcher->BeaconError());
    }

    if (m_beacon_status == BeaconStatus::ACTIVE) {
        const int64_t now = GetAdjustedTime();

        if (m_beacon->Age(now) >= BEACON_RENEWAL_WARNING_AGE) {
            m_beacon_status = BeaconStatus::RENEWAL_NEEDED;
        } else if (m_beacon->Renewable(now)) {
            m_beacon_status = BeaconStatus::RENEWAL_POSSIBLE;
        } else if (m_researcher->Magnitude() == 0) {
            m_beacon_status = BeaconStatus::NO_MAGNITUDE;
        } else {
            m_beacon_status = BeaconStatus::ACTIVE;
        }
    }

    emit beaconChanged();
}

void ResearcherModel::subscribeToCoreSignals()
{
    // Connect signals to client
    uiInterface.ResearcherChanged.connect(boost::bind(ResearcherChanged, this));
    uiInterface.ResearcherChanged.connect(boost::bind(BeaconChanged, this));
}

void ResearcherModel::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client
    uiInterface.ResearcherChanged.disconnect(boost::bind(ResearcherChanged, this));
    uiInterface.ResearcherChanged.disconnect(boost::bind(BeaconChanged, this));
}
