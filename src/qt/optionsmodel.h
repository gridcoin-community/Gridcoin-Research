#ifndef BITCOIN_QT_OPTIONSMODEL_H
#define BITCOIN_QT_OPTIONSMODEL_H

#include "sidestaketablemodel.h"
#include <QAbstractListModel>
#include <QDate>

namespace interfaces {
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
    explicit OptionsModel(interfaces::SideStakeManager& sidestake_manager, QObject* parent = nullptr);

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

    int rowCount(const QModelIndex & parent = QModelIndex()) const;
    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;
    bool setData(const QModelIndex & index, const QVariant & value, int role = Qt::EditRole);

    /* Explicit getters */
    qint64 getTransactionFee();
    qint64 getReserveBalance();
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

signals:
    void displayUnitChanged(int unit);
    void reserveBalanceChanged(qint64);
    void coinControlFeaturesChanged(bool);
    void LimitTxnDisplayChanged(bool);
    void MaskValuesChanged(bool);
    void walletStylesheetChanged(QString);
};

#endif // BITCOIN_QT_OPTIONSMODEL_H
