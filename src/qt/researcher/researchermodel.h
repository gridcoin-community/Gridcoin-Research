// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_RESEARCHER_RESEARCHERMODEL_H
#define GRIDCOIN_QT_RESEARCHER_RESEARCHERMODEL_H

#include "amount.h"
#include "interfaces/researcher.h"

#include <optional>
#include <utility>
#include <vector>

#include <QFutureWatcher>
#include <QObject>
#include <QString>

QT_BEGIN_NAMESPACE
class QIcon;
QT_END_NAMESPACE

class GuiStateCache;
class ResearcherWizard;
class WalletModel;

namespace interfaces {
class GuiState;
} // namespace interfaces

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
//! Backed by the researcher part of the state the core pushes (GuiStateCache):
//! the model keeps a value ResearcherSnapshot, replaced when a push changes it,
//! and every getter reads that snapshot instead of a GRC::Researcher /
//! GRC::Beacon. It takes no core lock to refresh it. While the part is absent
//! (the node computes it only when synced) the model keeps its last snapshot
//! marked out of sync, so a researcher whose state has never been computed is
//! shown as waiting for sync and never as an investor. Beacon/mode operations
//! and the project-table fusion run node-side behind the interface.
//!
class ResearcherModel : public QObject
{
    Q_OBJECT

public:
    //! Reads \p cache once, since a state already pushed when the cache was
    //! built emits no change, then follows it. \p gui_state is only asked for
    //! an update after a user action.
    ResearcherModel(interfaces::ResearcherContext& researcher_context, interfaces::GuiState& gui_state,
                    GuiStateCache& cache);
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

    //! The pools a researcher can join, as (name, website URL) pairs, one per
    //! operator site. Backs the wizard pool page's table, which used to carry
    //! the same two rows hardcoded in its .ui.
    std::vector<std::pair<QString, QString>> buildPoolList() const;
    QString generateBeaconKeyForV3();
    BeaconStatus advertiseBeaconV3(const QString& ownership_proof_xml);
    QString cachedBeaconPubKeyHex() const;

private:
    interfaces::ResearcherContext& m_researcher_context;
    interfaces::GuiState& m_gui_state;
    GuiStateCache& m_cache;

    //! The researcher part of the last pushed state, or the last such part
    //! marked out of sync. Every getter reads it.
    interfaces::ResearcherSnapshot m_snapshot;

    bool m_wizard_open;
    bool m_privacy_enabled;
    QString m_theme_suffix;
    QString m_cached_beacon_pubkey_hex;

    //! The core's reload, which takes cs_main, runs on a worker so the GUI
    //! thread never waits on it.
    QFutureWatcher<void> m_reload_watcher;

    //! A reload was asked for while one was running; one more follows it.
    bool m_reload_again = false;

    //! Install \p next as the snapshot. With \p notify, report the signals of
    //! the groups of fields that differ.
    void install(interfaces::ResearcherSnapshot next, bool notify);

    //! Map one node-side project row into a Qt ProjectRow, translating the
    //! whitelist status and resolving the (translatable) error label.
    ProjectRow mapProjectRow(const interfaces::ResearcherProjectRow& src) const;

signals:
    void researcherChanged();
    void beaconChanged();
    void magnitudeChanged();
    void accrualChanged();

    //! The core finished a reload() and an update was asked for. The project
    //! table reads the core's projects, which are current now; the fields the
    //! snapshot carries follow with the pushed state.
    void reloaded();

public slots:
    //! Reload the core's researcher state (a user action), off the GUI thread.
    void reload();
    bool switchToSolo(const QString& email);
    bool switchToPool();
    bool switchToNoncruncher();
    BeaconStatus advertiseBeacon();
    void onWizardClose();

private slots:
    //! Follow the cache: install its researcher part, or the last snapshot out
    //! of sync when it has none.
    void applyPushedState();
    void finishReload();
};

#endif // GRIDCOIN_QT_RESEARCHER_RESEARCHERMODEL_H
