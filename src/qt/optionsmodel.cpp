#include "optionsmodel.h"
#include "bitcoinunits.h"

#include <QDebug>
#include <QSettings>

#include "init.h"
#include "wallet/walletdb.h"
#include "guiutil.h"

OptionsModel::OptionsModel(QObject *parent) :
    QAbstractListModel(parent)
{
    Init();
}

bool static ApplyProxySettings()
{
    QSettings settings;
    CService addrProxy(settings.value("addrProxy", "127.0.0.1:9050").toString().toStdString());
    int nSocksVersion(settings.value("nSocksVersion", 5).toInt());
    if (!settings.value("fUseProxy", false).toBool()) {
        addrProxy = CService();
        nSocksVersion = 0;
        return false;
    }
    if (nSocksVersion && !addrProxy.IsValid())
        return false;
    if (!IsLimited(NET_IPV4))
        SetProxy(NET_IPV4, addrProxy, nSocksVersion);
    if (nSocksVersion > 4) {
        if (!IsLimited(NET_IPV6))
            SetProxy(NET_IPV6, addrProxy, nSocksVersion);
        SetNameProxy(addrProxy, nSocksVersion);
    }
    return true;
}

void OptionsModel::Init()
{
    QSettings settings;

    // These are Qt-only settings:
    nDisplayUnit = settings.value("nDisplayUnit", BitcoinUnits::BTC).toInt();
    fStartAtStartup = settings.value("fStartAtStartup", false).toBool();
    fStartMin = settings.value("fStartMin", true).toBool();
    fMinimizeToTray = settings.value("fMinimizeToTray", false).toBool();
    fDisableTrxNotifications = settings.value("fDisableTrxNotifications", false).toBool();
    bDisplayAddresses = settings.value("bDisplayAddresses", false).toBool();
    fMinimizeOnClose = settings.value("fMinimizeOnClose", false).toBool();
    fCoinControlFeatures = settings.value("fCoinControlFeatures", false).toBool();
    fLimitTxnDisplay = settings.value("fLimitTxnDisplay", false).toBool();
    limitTxnDate = settings.value("limitTxnDate", QDate()).toDate();
    nReserveBalance = settings.value("nReserveBalance").toLongLong();
    language = settings.value("language", "").toString();
    walletStylesheet = settings.value("walletStylesheet", "light").toString();

    // These are shared with core Bitcoin; we want
    // command-line options to override the GUI settings:
    if (settings.contains("fUseUPnP")) {
        SoftSetBoolArg("-upnp", settings.value("fUseUPnP").toBool());
    }
    if (settings.contains("addrProxy") && settings.value("fUseProxy").toBool()) {
        SoftSetArg("-proxy", settings.value("addrProxy").toString().toStdString());
    }
    if (settings.contains("nSocksVersion") && settings.value("fUseProxy").toBool()) {
        SoftSetArg("-socks", settings.value("nSocksVersion").toString().toStdString());
    }
    if (!language.isEmpty()) {
        SoftSetArg("-lang", language.toStdString());
    }
    if (settings.contains("fDisableUpdateCheck")) {
        SoftSetBoolArg("-disableupdatecheck", settings.value("fDisableUpdateCheck").toBool());
    }
    if (settings.contains("dataDir") && dataDir != GUIUtil::getDefaultDataDirectory()) {
        SoftSetArg("-datadir", GUIUtil::qstringToBoostPath(settings.value("dataDir").toString()).string());
    }
}

int OptionsModel::rowCount(const QModelIndex & parent) const
{
    return OptionIDRowCount;
}

QVariant OptionsModel::data(const QModelIndex & index, int role) const
{
    if(role == Qt::EditRole)
    {
        QSettings settings;
        switch(index.row())
        {
        case StartAtStartup:
            return QVariant(fStartAtStartup);
        case StartMin:
            return QVariant(fStartMin);
        case MinimizeToTray:
            return QVariant(fMinimizeToTray);
        case DisableTrxNotifications:
            return QVariant(fDisableTrxNotifications);
        case MapPortUPnP:
            return settings.value("fUseUPnP", GetBoolArg("-upnp", true));
        case MinimizeOnClose:
            return QVariant(fMinimizeOnClose);
        case ProxyUse:
            return settings.value("fUseProxy", false);
        case ProxyIP: {
            proxyType proxy;
            if (GetProxy(NET_IPV4, proxy))
                return QVariant(QString::fromStdString(proxy.first.ToStringIP()));
            else
                return QVariant(QString::fromStdString("127.0.0.1"));
        }
        case ProxyPort: {
            proxyType proxy;
            if (GetProxy(NET_IPV4, proxy))
                return QVariant(proxy.first.GetPort());
            else
                return QVariant(9050);
        }
        case ProxySocksVersion:
            return settings.value("nSocksVersion", 5);
        case ReserveBalance:
            return QVariant((qint64) nReserveBalance);
        case DisplayUnit:
            return QVariant(nDisplayUnit);
		case DisplayAddresses:		
            return QVariant(bDisplayAddresses);
        case Language:
            return settings.value("language", "");
        case WalletStylesheet:
            return settings.value("walletStylesheet", "light");
        case CoinControlFeatures:
            return QVariant(fCoinControlFeatures);
        case LimitTxnDisplay:
            return QVariant(fLimitTxnDisplay);
        case LimitTxnDate:
            return QVariant(limitTxnDate);
        case DisableUpdateCheck:
            return QVariant(GetBoolArg("-disableupdatecheck", false));
        case DataDir:
            return settings.value("dataDir", QString::fromStdString(GetArg("-datadir", GetDataDir().string())));
        default:
            return QVariant();
        }
    }
    return QVariant();
}

bool OptionsModel::setData(const QModelIndex & index, const QVariant & value, int role)
{
    bool successful = true; /* set to false on parse error */
    if(role == Qt::EditRole)
    {
        QSettings settings;
        switch(index.row())
        {
        case StartAtStartup:
            if (fStartAtStartup != value.toBool())
            {
                fStartAtStartup = value.toBool();
                settings.setValue("fStartAtStartup", fStartAtStartup);
                successful = GUIUtil::SetStartOnSystemStartup(fStartAtStartup, fStartMin);
            }
            break;
        case StartMin:
            if (fStartMin != value.toBool())
            {
                fStartMin = value.toBool();
                settings.setValue("fStartMin", fStartMin);
                successful = GUIUtil::SetStartOnSystemStartup(fStartAtStartup, fStartMin);
            }
            break;
        case MinimizeToTray:
            fMinimizeToTray = value.toBool();
            settings.setValue("fMinimizeToTray", fMinimizeToTray);
            break;
        case DisableTrxNotifications:
            fDisableTrxNotifications = value.toBool();
            settings.setValue("fDisableTrxNotifications", fDisableTrxNotifications);
            break;
        case MapPortUPnP:
            fUseUPnP = value.toBool();
            settings.setValue("fUseUPnP", fUseUPnP);
            MapPort();
            break;
        case MinimizeOnClose:
            fMinimizeOnClose = value.toBool();
            settings.setValue("fMinimizeOnClose", fMinimizeOnClose);
            break;
        case ProxyUse:
            settings.setValue("fUseProxy", value.toBool());
            ApplyProxySettings();
            break;
        case ProxyIP: {
            proxyType proxy;
            proxy.first = CService("127.0.0.1", 9050);
            GetProxy(NET_IPV4, proxy);

            CNetAddr addr(value.toString().toStdString());
            proxy.first.SetIP(addr);
            settings.setValue("addrProxy", proxy.first.ToStringIPPort().c_str());
            successful = ApplyProxySettings();
        }
        break;
        case ProxyPort: {
            proxyType proxy;
            proxy.first = CService("127.0.0.1", 9050);
            GetProxy(NET_IPV4, proxy);

            proxy.first.SetPort(value.toInt());
            settings.setValue("addrProxy", proxy.first.ToStringIPPort().c_str());
            successful = ApplyProxySettings();
        }
        break;
        case ProxySocksVersion: {
            proxyType proxy;
            proxy.second = 5;
            GetProxy(NET_IPV4, proxy);

            proxy.second = value.toInt();
            settings.setValue("nSocksVersion", proxy.second);
            successful = ApplyProxySettings();
        }
        break;
        case ReserveBalance:
            nReserveBalance = value.toLongLong();
            settings.setValue("nReserveBalance", (qint64) nReserveBalance);
            emit reserveBalanceChanged(nReserveBalance);
            break;
        case DisplayUnit:
            nDisplayUnit = value.toInt();
            settings.setValue("nDisplayUnit", nDisplayUnit);
            emit displayUnitChanged(nDisplayUnit);
            break;
		case DisplayAddresses:		
             bDisplayAddresses = value.toBool();		
             settings.setValue("bDisplayAddresses", bDisplayAddresses);		
             break;
        case Language:
            settings.setValue("language", value);
            break;
        case WalletStylesheet:
            walletStylesheet = value.toString();
            settings.setValue("walletStylesheet", walletStylesheet);
            emit walletStylesheetChanged(walletStylesheet);
            break;
        case CoinControlFeatures: {
            fCoinControlFeatures = value.toBool();
            settings.setValue("fCoinControlFeatures", fCoinControlFeatures);
            emit coinControlFeaturesChanged(fCoinControlFeatures);
            }
            break;
        case LimitTxnDisplay:
            fLimitTxnDisplay = value.toBool();
            settings.setValue("fLimitTxnDisplay", fLimitTxnDisplay);
            emit LimitTxnDisplayChanged(fLimitTxnDisplay);
            break;
        case LimitTxnDate:
            limitTxnDate = value.toDate();
            settings.setValue("limitTxnDate", limitTxnDate);
            break;
        case DisableUpdateCheck:
            SetArgument("disableupdatecheck", value.toBool() ? "1" : "0");
            settings.setValue("fDisableUpdateCheck", value.toBool());
            break;
        case DataDir:
            // There is no SetArgument here, because the core data directory cannot
            // be changed while the wallet is running.
            dataDir = value.toString();
            settings.setValue("dataDir", dataDir);
        default:
            break;
        }
    }
    emit dataChanged(index, index);

    return successful;
}

qint64 OptionsModel::getTransactionFee()
{
    return nTransactionFee;
}

qint64 OptionsModel::getReserveBalance()
{
    return nReserveBalance;
}

bool OptionsModel::getCoinControlFeatures()
{
    return fCoinControlFeatures;
}

bool OptionsModel::getLimitTxnDisplay()
{
    return fLimitTxnDisplay;
}

QDate OptionsModel::getLimitTxnDate()
{
    return limitTxnDate;
}

int64_t OptionsModel::getLimitTxnDateTime()
{
    QDateTime limitTxnDateTime(limitTxnDate);

    return limitTxnDateTime.toMSecsSinceEpoch() / 1000;
}

bool OptionsModel::getStartAtStartup()
{
    return fStartAtStartup;
}

bool OptionsModel::getStartMin()
{
    return fStartMin;
}

bool OptionsModel::getMinimizeToTray()
{
    return fMinimizeToTray;
}

bool OptionsModel::getDisableTrxNotifications()
{
    return fDisableTrxNotifications;
}

bool OptionsModel::getMinimizeOnClose()
{
    return fMinimizeOnClose;
}

int OptionsModel::getDisplayUnit()
{
    return nDisplayUnit;
}

bool OptionsModel::getDisplayAddresses()		
{		
    return bDisplayAddresses;		
}

QString OptionsModel::getCurrentStyle()
{
    return walletStylesheet;
}

QString OptionsModel::getDataDir()
{
    return dataDir;
}
