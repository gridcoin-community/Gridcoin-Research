#ifndef BITCOIN_QT_OPTIONSMODEL_H
#define BITCOIN_QT_OPTIONSMODEL_H

#include "sidestaketablemodel.h"
#include <QAbstractListModel>
#include <QDate>
#include <QMap>
#include <QVariant>

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace interfaces {
class Node;
class SideStakeManager;
} // namespace interfaces

/** Interface from Qt to configuration data structure for Bitcoin client.
   To Qt, the options are presented as a list with the different options
   laid out vertically.
   This can be changed to a tree once the settings become sufficiently
   complex.
 */
class OptionsModel : public QAbstractListModel
{
    Q_OBJECT

public:
    //! \p sidestake_manager backs the sidestake table model constructed here; it
    //! is owned by the process and must outlive this OptionsModel.
    //! \p node is the settings command/query surface (a separate process in the
    //! multiprocess build); the core-coupled options route reads and writes
    //! through it instead of touching gArgs / core globals directly.
    explicit OptionsModel(interfaces::Node& node, interfaces::SideStakeManager& sidestake_manager, QObject* parent = nullptr);

    enum OptionID {
        StartAtStartup,          // bool
        MinimizeToTray,          // bool
        ConfirmOnClose,          // bool
        StartMin,                // bool
        DisableTrxNotifications, // bool
        DisablePollNotifications,// bool
        MapPortUPnP,             // bool
        MinimizeOnClose,         // bool
        ProxyUse,                // bool
        ProxyIP,                 // QString
        ProxyPort,               // int
        ReserveBalance,          // qint64
        DisplayUnit,             // BitcoinUnits::Unit
        DisplayAddresses,        // bool
        Language,                // QString
        WalletStylesheet,        // QString
        CoinControlFeatures,     // bool
        LimitTxnDisplay,         // bool
        LimitTxnDate,            // QDate
        DisableUpdateCheck,      // bool
        DataDir,                 // QString
        EnableStaking,           // bool
        EnableStakeSplit,        // bool
        EnableSideStaking,       // bool
        StakingEfficiency,       // double
        MinStakeSplitValue,      // int
        PollExpireNotification,  // double
        ContractChangeToInput,   // bool
        MaskValues,              // bool
        OptionIDRowCount
    };

    void Init();

    //! Read the per-node GUI preferences into the model's members. Split out of
    //! Init() because those settings live under a per-node QSettings group keyed
    //! on the datadir (GUIUtil::nodeSettingsKey), which is only known once the
    //! datadir is finalized. Init() runs from the constructor -- before the
    //! datadir is chosen (it seeds -datadir from the stored value) -- so it cannot
    //! read them; the composition root calls this afterwards. See the ordering
    //! note in bitcoin.cpp.
    void readNodeSettings();

    //! One-time migration of reservebalance and disableupdatecheck from
    //! Gridcoin-Qt.conf into the core read-write settings file; neither needs
    //! the wallet passphrase. Marks it done with the QSettings key
    //! "coreSettingsMigrated". Must be called from the composition root AFTER
    //! the config and read-write settings files are loaded and the network is
    //! selected -- never from the constructor, which runs before those.
    void migrateCoreSettings();

    //! Runs apply() while holding a Settings elevation; false when the user gave none.
    using WithElevation = std::function<bool(const std::function<void()>& apply)>;

    //! One-time migration of the proxy and UPnP settings kept in
    //! Gridcoin-Qt.conf by an earlier version. Changing them needs the wallet
    //! passphrase, so the composition root calls this once the wallet can ask
    //! for it, through \p with_elevation. Without one, the old values are
    //! discarded. Marks it done with the QSettings key
    //! "coreElevatedSettingsMigrated", except when the node could not store
    //! them: the next start then tries again.
    void migrateElevatedCoreSettings(const WithElevation& with_elevation);

    //! Whether submitting these widget values changes an Elevate-class core setting: UPnP, proxy use,
    //! the proxy address or port while the proxy is (being) enabled, or side staking.
    bool needsSettingsElevation(const QMap<OptionID, QVariant>& submitted) const;

    //! Asks for the wallet passphrase and begins a Settings capability with it.
    //! Returns what holds that capability, which ends it when destroyed, or null
    //! when none was begun (the user cancelled, or no prompt could be shown).
    using SettingsElevationRequest = std::function<std::shared_ptr<void>()>;

    //! The wallet model sets this while it exists, and clears it (an empty
    //! function) when it goes, which also ends a capability still held. With
    //! none set, a change the node refuses for want of the passphrase fails as
    //! any other refused change does.
    void setSettingsElevationRequest(SettingsElevationRequest request);

    //! After the node refused a change for want of the wallet passphrase, ask
    //! for it and return whether a Settings capability is now held, so that the
    //! caller sends the same change once more. Asks at most once until control
    //! returns to the event loop and holds the capability until then, so that
    //! one submit of several fields asks once.
    bool elevateSettingsAfterRefusal();

    //! Before a change that needsSettingsElevation() expects to need the wallet
    //! passphrase: ask for it, and return whether a Settings capability is now
    //! held. This is the one ask until control returns to the event loop, as in
    //! elevateSettingsAfterRefusal(), and the capability is held until then, so
    //! a change the node refuses during it is not asked for again. Already asked
    //! in that time, the earlier answer stands. Without a request set, nothing
    //! is asked and the answer is false. The wallet model's request asks nothing
    //! of an unencrypted or fully unlocked wallet, and answers true.
    bool elevateSettingsBeforeChange();

    int rowCount(const QModelIndex & parent = QModelIndex()) const;
    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;
    bool setData(const QModelIndex & index, const QVariant & value, int role = Qt::EditRole);

    /* Explicit getters */
    qint64 getReserveBalance();
    //! Whether the node is on testnet, read through the node interface
    //! (interfaces::Node::isTestNet). Lets dialogs that hold an OptionsModel gate
    //! testnet-only UI without calling chainparams (OnTestnet()) from GUI code.
    bool isTestNet();
    //! Whether the node is on mainnet, read through the node interface
    //! (interfaces::Node::isMainNet). Distinct from !isTestNet(), which is also
    //! true on regtest -- use this to gate UI for anything that only happens on
    //! the live network.
    bool isMainNet();
    bool getStartAtStartup();
    bool getStartMin();
    bool getMinimizeToTray();
    bool getConfirmOnClose();
    bool getDisableTrxNotifications();
    bool getDisablePollNotifications();
    bool getMinimizeOnClose();
    int getDisplayUnit();
	bool getDisplayAddresses();
    bool getCoinControlFeatures();
    bool getLimitTxnDisplay();
    bool getSuppressNetworkGraph();
    bool getMaskValues();
    //! Whether stale (orphaned) coinstake txns are shown in the transaction
    //! list (-showorphans). Read through the node settings interface so the tx
    //! models need not include util/system.h to reach gArgs.
    bool getShowOrphans();
    QDate getLimitTxnDate();
    int64_t getLimitTxnDateTime();
    double getPollExpireNotification();
    QString getLanguage() { return language; }
    QString getCurrentStyle();
    QString getDataDir();

    SideStakeTableModel* getSideStakeTableModel();

    /* Explicit setters */
    void setCurrentStyle(QString theme);
    void setMaskValues(bool privacy_mode);
    void toggleCoinControlFeatures();

private:
    //! Change core settings through the node. A change refused for want of the
    //! wallet passphrase is sent once more when elevateSettingsAfterRefusal()
    //! gets a Settings capability. Returns whether the node stored the change.
    bool changeCoreSettings(const std::vector<std::pair<std::string, std::string>>& settings);

    //! Ask through m_settings_elevation_request, unless that is unset or has
    //! asked since control last returned to the event loop, and hold what it
    //! began until control returns there. Returns whether a capability is held.
    bool askForSettingsElevation();

    //! Settings command/query surface; owned by the process, outlives this model.
    interfaces::Node& m_node;
    int nDisplayUnit;
    bool fMinimizeToTray;
    bool fStartAtStartup;
    bool fStartMin;
    bool fDisableTrxNotifications;
    bool fDisablePollNotifications;
    bool bDisplayAddresses;
    bool fMinimizeOnClose;
    bool fConfirmOnClose;
    bool fCoinControlFeatures;
    bool fLimitTxnDisplay;
    bool fMaskValues;
    QDate limitTxnDate;
    double pollExpireNotification;
    QString language;
    QString walletStylesheet;
    QString dataDir;

    //! The sidestake registry interface backing m_sidestake_model. Owned by the
    //! process (created before this OptionsModel) and outlives it; held by
    //! reference so Init() can construct the table model with it.
    interfaces::SideStakeManager& m_sidestake_manager;

    SideStakeTableModel* m_sidestake_model;

    //! Set by the wallet model; see setSettingsElevationRequest().
    SettingsElevationRequest m_settings_elevation_request;

    //! The capability elevateSettingsAfterRefusal() began, held until control
    //! returns to the event loop.
    std::shared_ptr<void> m_settings_elevation;

    //! elevateSettingsAfterRefusal() has asked since control last returned to
    //! the event loop.
    bool m_settings_elevation_asked{false};

signals:
    void displayUnitChanged(int unit);
    void reserveBalanceChanged(qint64);
    void coinControlFeaturesChanged(bool);
    void LimitTxnDisplayChanged(bool);
    void MaskValuesChanged(bool);
    void walletStylesheetChanged(QString);
};

#endif // BITCOIN_QT_OPTIONSMODEL_H
