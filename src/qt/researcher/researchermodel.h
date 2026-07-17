// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_RESEARCHER_RESEARCHERMODEL_H
#define GRIDCOIN_QT_RESEARCHER_RESEARCHERMODEL_H

#include "amount.h"
#include "interfaces/researcher.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <QObject>
#include <QString>

QT_BEGIN_NAMESPACE
class QIcon;
QT_END_NAMESPACE

class ResearcherWizard;
class WalletModel;

//!
//! \brief Describes the researcher's current beacon status.
//!
//! The enumeration now lives in the interfaces layer (Phase 1d-iv) so the node
//! side can classify it; this alias keeps the existing GUI references
//! (\c BeaconStatus::ACTIVE, etc.) unchanged.
//!
using BeaconStatus = interfaces::BeaconStatus;

//!
//! \brief Combined information about a BOINC project to display in a table.
//!
//! These objects incorporate BOINC project context from:
//!
//!  - The Gridcoin whitelist
//!  - Local BOINC projects detected in client_state.xml
//!  - Scraper magnitude values for a CPID
//!
//! The fusion is performed node-side by interfaces::ResearcherContext::projects();
//! ResearcherModel maps each interfaces::ResearcherProjectRow into one of these Qt
//! rows (translating the whitelist status and error label).
//!
class ProjectRow
{
public:
    enum WhiteListStatus
    {
        False,
        Excluded,
        Manually_Greylisted,
        Automatically_Greylisted,
        True
    };

    WhiteListStatus m_whitelisted;
    std::optional<bool> m_gdpr_controls;
    QString m_name;
    QString m_cpid;
    double m_magnitude = 0.0;
    double m_rac = 0.0;
    QString m_error;
};

//!
//! \brief Presents researcher context state for UI components.
//!
//! Backed by an interfaces::ResearcherContext (Phase 1d-iv): the model caches a
//! value ResearcherSnapshot refreshed on the researcher/beacon/accrual/tip
//! notifications, and every getter reads that snapshot instead of a
//! GRC::Researcher / GRC::Beacon. Beacon/mode operations and the project-table
//! fusion run node-side behind the interface.
//!
class ResearcherModel : public QObject
{
    Q_OBJECT

public:
    explicit ResearcherModel(interfaces::ResearcherContext& researcher_context);
    ~ResearcherModel();

    static QString mapBeaconStatus(const BeaconStatus status);
    QIcon mapBeaconStatusIcon(const BeaconStatus status) const;

    void showWizard(WalletModel* wallet_model);
    void setTheme(const QString& theme_name);
    void setMaskCpidMagnitudeAccrual(bool privacy);

    bool configuredForNoncruncherMode() const;
    bool outOfSync() const;
    bool detectedPoolMode() const;
    bool actionNeeded() const;
    bool hasEligibleProjects() const;
    bool hasPoolProjects() const;
    bool hasActiveBeacon() const;

    //!
    //! \brief hasPendingBeacon returns true if a pending beacon is present and also not expired while pending.
    //! \return boolean
    //!
    bool hasPendingBeacon() const;
    bool hasRenewableBeacon() const;
    bool beaconExpired() const;
    bool hasMagnitude() const;
    bool hasRAC() const;
    bool hasSplitCpid() const;
    bool needsBeaconAuth() const;

    std::optional<CAmount> accrualNearLimit() const;
    CAmount getAccrual() const;

    QString email() const;
    QString formatCpid() const;
    QString formatMagnitude() const;
    QString formatAccrual(const int display_unit, bool& near_limit) const;
    QString formatStatus() const;
    QString formatBoincPath() const;

    BeaconStatus getBeaconStatus() const;
    QIcon getBeaconStatusIcon() const;
    QString formatBeaconStatus() const;
    QString formatBeaconAge() const;
    QString formatTimeToBeaconExpiration() const;
    QString formatTimeToPendingBeaconExpiration() const;
    QString formatBeaconAddress() const;
    QString formatBeaconVerificationCode() const;

    std::vector<ProjectRow> buildProjectTable(bool extended = true) const;

    // V3 beacon ownership proof support
    bool isV14Enabled() const;
    bool hasV3CapableProjects() const;
    std::vector<std::pair<QString, QString>> buildV3ProjectList() const;
    QString generateBeaconKeyForV3();
    BeaconStatus advertiseBeaconV3(const QString& ownership_proof_xml);
    QString cachedBeaconPubKeyHex() const;

private:
    interfaces::ResearcherContext& m_researcher_context;

    //! Cached value snapshot of the researcher/beacon context. Refreshed on the
    //! interface notifications (and by the wizard commands); every getter reads it.
    interfaces::ResearcherSnapshot m_snapshot;

    bool m_wizard_open;
    bool m_privacy_enabled;
    QString m_theme_suffix;
    QString m_cached_beacon_pubkey_hex;

    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();

    //! Retained interface-notification handlers, cleared on teardown so a signal
    //! that fires after this model is destroyed cannot invoke a slot bound to
    //! freed memory (issue #3129).
    std::vector<std::unique_ptr<interfaces::Handler>> m_handlers;

    //! Map one node-side project row into a Qt ProjectRow, translating the
    //! whitelist status and resolving the (translatable) error label.
    ProjectRow mapProjectRow(const interfaces::ResearcherProjectRow& src) const;

signals:
    void researcherChanged();
    void beaconChanged();
    void magnitudeChanged();
    void accrualChanged();

public slots:
    void reload();
    void refresh();
    void onResearcherChanged();
    bool switchToSolo(const QString& email);
    bool switchToPool();
    bool switchToNoncruncher();
    void updateBeacon();
    BeaconStatus advertiseBeacon();
    void onWizardClose();
};

#endif // GRIDCOIN_QT_RESEARCHER_RESEARCHERMODEL_H
