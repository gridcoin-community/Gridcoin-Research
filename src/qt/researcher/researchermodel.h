#ifndef RESEARCHERMODEL_H
#define RESEARCHERMODEL_H

#include <memory>
#include <QObject>

class QIcon;

namespace NN {
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
//! \brief Presents researcher context state for UI components.
//!
class ResearcherModel : public QObject
{
    Q_OBJECT

public:
    ResearcherModel();
    ~ResearcherModel();

    bool configuredForInvestorMode() const;
    QString formatCpid() const;
    QString formatMagnitude() const;
    QString formatAccrual(const int display_unit) const;
    QString formatStatus() const;

    BeaconStatus getBeaconStatus() const;
    QIcon getBeaconStatusIcon() const;
    QString formatBeaconStatus() const;
    QString formatBeaconAge() const;
    QString formatTimeToBeaconExpiration() const;
    QString formatBeaconAddress() const;

private:
    NN::ResearcherPtr m_researcher;
    std::unique_ptr<NN::Beacon> m_beacon;
    BeaconStatus m_beacon_status;
    bool m_configured_for_investor_mode;

    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();

signals:
    void researcherChanged();
    void beaconChanged();
    void accrualChanged();

public slots:
    void refresh();
    void resetResearcher(NN::ResearcherPtr researcher);
    void updateBeacon();
};

#endif // RESEARCHERMODEL_H
