// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RESEARCHERMODEL_H
#define RESEARCHERMODEL_H

#include <memory>
#include <QObject>

QT_BEGIN_NAMESPACE
class QIcon;
QT_END_NAMESPACE

class ResearcherWizard;
class WalletModel;

namespace GRC {
class Beacon;
class Researcher;

//!
//! \brief A smart pointer around the global BOINC researcher context.
//!
typedef std::shared_ptr<Researcher> ResearcherPtr;
}

//!
//! \brief Describes the researcher's current beacon status.
//!
enum class BeaconStatus
{
    ACTIVE,
    ERROR_INSUFFICIENT_FUNDS,
    ERROR_MISSING_KEY,
    ERROR_NOT_NEEDED,
    ERROR_TX_FAILED,
    ERROR_WALLET_LOCKED,
    NO_BEACON,
    NO_CPID,
    NO_MAGNITUDE,
    PENDING,
    RENEWAL_NEEDED,
    RENEWAL_POSSIBLE,
    UNKNOWN,
};

//!
//! \brief Combined information about a BOINC project to display in a table.
//!
//! These objects incorporate BOINC project context from:
//!
//!  - The Gridcoin whitelist
//!  - Local BOINC projects detected in client_state.xml
//!  - Scraper magnitude values for a CPID
//!
class ProjectRow
{
public:
    bool m_whitelisted;
    QString m_name;
    QString m_cpid;
    double m_magnitude = 0.0;
    double m_rac = 0.0;
    QString m_error;
};

//!
//! \brief Presents researcher context state for UI components.
//!
class ResearcherModel : public QObject
{
    Q_OBJECT

public:
    ResearcherModel();
    ~ResearcherModel();

    static QString mapBeaconStatus(const BeaconStatus status);
    static QIcon mapBeaconStatusIcon(const BeaconStatus status);

    void showWizard(WalletModel* wallet_model);

    bool configuredForInvestorMode() const;
    bool detectedPoolMode() const;
    bool actionNeeded() const;
    bool hasEligibleProjects() const;
    bool hasPoolProjects() const;
    bool hasActiveBeacon() const;
    bool hasPendingBeacon() const;
    bool hasRenewableBeacon() const;
    bool hasMagnitude() const;
    bool hasRAC() const;
    bool needsBeaconAuth() const;

    QString email() const;
    QString formatCpid() const;
    QString formatMagnitude() const;
    QString formatAccrual(const int display_unit) const;
    QString formatStatus() const;
    QString formatBoincPath() const;

    BeaconStatus getBeaconStatus() const;
    QIcon getBeaconStatusIcon() const;
    QString formatBeaconStatus() const;
    QString formatBeaconAge() const;
    QString formatTimeToBeaconExpiration() const;
    QString formatBeaconAddress() const;
    QString formatBeaconVerificationCode() const;

    std::vector<ProjectRow> buildProjectTable(bool extended = true) const;

private:
    GRC::ResearcherPtr m_researcher;
    std::unique_ptr<GRC::Beacon> m_beacon;
    std::unique_ptr<GRC::Beacon> m_pending_beacon;
    BeaconStatus m_beacon_status;
    bool m_configured_for_investor_mode;
    bool m_wizard_open;

    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();

signals:
    void researcherChanged();
    void beaconChanged();
    void magnitudeChanged();
    void accrualChanged();

public slots:
    void reload();
    void refresh();
    void resetResearcher(GRC::ResearcherPtr researcher);
    bool switchToSolo(const QString& email);
    bool switchToPool();
    bool switchToInvestor();
    void updateBeacon();
    BeaconStatus advertiseBeacon();
    void onWizardClose();
};

#endif // RESEARCHERMODEL_H
