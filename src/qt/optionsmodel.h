#ifndef OPTIONSMODEL_H
#define OPTIONSMODEL_H

#include <QAbstractListModel>
#include <QDate>

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
    explicit OptionsModel(QObject *parent = 0);

    enum OptionID {
        StartAtStartup,          // bool
        MinimizeToTray,          // bool
        StartMin,                // bool
        DisableTrxNotifications, // bool
        MapPortUPnP,             // bool
        MinimizeOnClose,         // bool
        ProxyUse,                // bool
        ProxyIP,                 // QString
        ProxyPort,               // int
        ProxySocksVersion,       // int
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
        OptionIDRowCount,
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
    bool getDisableTrxNotifications();
    bool getMinimizeOnClose();
    int getDisplayUnit();
	bool getDisplayAddresses();
    bool getCoinControlFeatures();
    bool getLimitTxnDisplay();
    QDate getLimitTxnDate();
    int64_t getLimitTxnDateTime();
    QString getLanguage() { return language; }
    QString getCurrentStyle();
    QString getDataDir();

private:
    int nDisplayUnit;
    bool fMinimizeToTray;
    bool fStartAtStartup;
    bool fStartMin;
    bool fDisableTrxNotifications;
	bool bDisplayAddresses;
    bool fMinimizeOnClose;
    bool fCoinControlFeatures;
    bool fLimitTxnDisplay;
    QDate limitTxnDate;
    QString language;
    QString walletStylesheet;
    QString dataDir;

signals:
    void displayUnitChanged(int unit);
    void reserveBalanceChanged(qint64);
    void coinControlFeaturesChanged(bool);
    void LimitTxnDisplayChanged(bool);
    void walletStylesheetChanged(QString);
};

#endif // OPTIONSMODEL_H
