#include "optionsmodel.h"

#include <QDebug>
#include <QSettings>

#include "bitcoinunits.h"
#include "guiutil.h"
#include "interfaces/node.h"
#include "util/strencodings.h" // For SplitHostPort (IPv6-aware host:port parsing).

#include "miner.h"

#include <utility>

namespace {
//! Format a satoshi amount as a plain money string (no localization/thousands
//! separators) for the -reservebalance setting, matching what the core's
//! ParseMoney reads. BitcoinUnits::format inserts thin-space separators, so it
//! cannot be used here.
std::string FormatReserveBalanceSetting(qint64 sat)
{
    const bool negative = sat < 0;
    const qint64 abs_sat = negative ? -sat : sat;
    const qint64 coin = BitcoinUnits::factor(BitcoinUnits::BTC);
    const int decimals = BitcoinUnits::decimals(BitcoinUnits::BTC);
    QString s = (negative ? "-" : "") + QString::number(abs_sat / coin) + "."
                + QString::number(abs_sat % coin).rightJustified(decimals, '0');
    return s.toStdString();
}

//! Push the effective proxy to the core -proxy setting: the GUI keeps its three
//! proxy fields (enable + address) as local editing state in QSettings, and the
//! effective value (the address when enabled, empty when disabled) is mirrored to
//! gridcoinsettings.json through the node. Empty disables/erases -proxy; netbase
//! cannot clear a live proxy, so disabling is effective on restart (unchanged
//! from the previous behavior).
bool PushEffectiveProxy(interfaces::Node& node, QSettings& settings)
{
    const bool use_proxy = settings.value("fUseProxy", false).toBool();
    const std::string addr =
        use_proxy ? settings.value("addrProxy", "127.0.0.1:9050").toString().toStdString() : std::string();
    return node.changeSettings({{"proxy", addr}}).ok;
}

//! Split a stored "host:port" proxy string into host and port, IPv6-aware
//! (SplitHostPort strips the [] brackets around an IPv6 host and only treats a
//! colon as the port separator when it is unambiguous). Port defaults to 9050.
std::pair<QString, int> SplitProxy(const QString& addr)
{
    int port = 9050;
    std::string host;
    SplitHostPort(addr.toStdString(), port, host);
    return {QString::fromStdString(host), port};
}

//! Format host + port back into the stored proxy string, bracketing an IPv6
//! host (one containing a colon) so it round-trips through SplitProxy and is
//! accepted by the node's proxy validation -- matching the old
//! CService::ToStringIPPort() form.
QString FormatProxy(const QString& host, int port)
{
    const QString h = host.contains(':') ? "[" + host + "]" : host;
    return h + ":" + QString::number(port);
}
} // namespace

OptionsModel::OptionsModel(interfaces::Node& node, interfaces::SideStakeManager& sidestake_manager, QObject *parent) :
    QAbstractListModel(parent),
    m_node(node),
    m_sidestake_manager(sidestake_manager)
{
    Init();
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
    fDisablePollNotifications = settings.value("fDisablePollNotifications", false).toBool();
    bDisplayAddresses = settings.value("bDisplayAddresses", false).toBool();
    fMinimizeOnClose = settings.value("fMinimizeOnClose", false).toBool();
    fConfirmOnClose = settings.value("fConfirmOnClose", false).toBool();
    fCoinControlFeatures = settings.value("fCoinControlFeatures", false).toBool();
    fLimitTxnDisplay = settings.value("fLimitTxnDisplay", false).toBool();
    fMaskValues = settings.value("fMaskValues", false).toBool();
    limitTxnDate = settings.value("limitTxnDate", QDate()).toDate();
    pollExpireNotification = settings.value("pollExpireNotification", 8.0).toDouble();
    language = settings.value("language", "").toString();
    walletStylesheet = settings.value("walletStylesheet", "dark").toString();

    // Language stays GUI-local (the core does not read -lang): apply it into this
    // process's own args for the Qt translator.
    if (!language.isEmpty()) {
        gArgs.SoftSetArg("-lang", language.toStdString());
    }
    // DataDir is restart-only and remains a next-launch copy here; the Phase-2
    // spawn handshake will own the authoritative datadir in the split build.
    if (settings.contains("dataDir") && dataDir != GUIUtil::getDefaultDataDirectory()) {
        // Use ShortPathString to get an 8.3 short path on Windows when the path contains characters
        // outside the system code page. gArgs is a narrow-string store, and fs::path::string() would
        // corrupt non-codepage characters via code page narrowing.
        std::string datadir_narrow = fsbridge::ShortPathString(GUIUtil::qstringToBoostPath(settings.value("dataDir").toString()));
        if (!datadir_narrow.empty()) {
            gArgs.SoftSetArg("-datadir", datadir_narrow);
        }
    }

    m_sidestake_model = new SideStakeTableModel(m_sidestake_manager, this);
}

void OptionsModel::migrateCoreSettings()
{
    // proxy / UPnP / reservebalance / update-check are now core read-write
    // settings (gridcoinsettings.json), reached through interfaces::Node. On the
    // first run after upgrade, move any values the user had in Gridcoin-Qt.conf
    // into the core so they are not silently dropped, then stop touching them
    // from QSettings. This runs from the composition root AFTER the config file
    // and the read-write settings file are loaded and the network is selected --
    // OptionsModel itself is constructed earlier (before those), so it must not
    // write settings from its constructor.
    QSettings settings;
    if (settings.value("coreSettingsMigrated", false).toBool()) {
        return;
    }

    // Only migrate a setting the user has NOT already set in gridcoinresearch.conf
    // (or on the command line). The old Init() used SoftSet, so a config value
    // always won over the remembered GUI value; skipping already-set settings
    // preserves that precedence instead of force-overriding the config file.
    std::vector<std::pair<std::string, std::string>> migrate;
    if (!m_node.isSettingSet("proxy")
            && settings.value("fUseProxy", false).toBool() && settings.contains("addrProxy")) {
        migrate.emplace_back("proxy", settings.value("addrProxy").toString().toStdString());
    }
    if (!m_node.isSettingSet("upnp") && settings.contains("fUseUPnP")) {
        migrate.emplace_back("upnp", settings.value("fUseUPnP").toBool() ? "1" : "0");
    }
    if (!m_node.isSettingSet("reservebalance")
            && settings.contains("nReserveBalance") && settings.value("nReserveBalance").toLongLong() > 0) {
        migrate.emplace_back("reservebalance",
                             FormatReserveBalanceSetting(settings.value("nReserveBalance").toLongLong()));
    }
    if (!m_node.isSettingSet("disableupdatecheck") && settings.contains("fDisableUpdateCheck")) {
        migrate.emplace_back("disableupdatecheck", settings.value("fDisableUpdateCheck").toBool() ? "1" : "0");
    }
    // Only mark the migration done once it has actually been persisted. If the
    // node write fails, leave the flag unset so the next launch retries rather
    // than permanently dropping the user's old Gridcoin-Qt.conf values.
    if (migrate.empty() || m_node.changeSettings(migrate).ok) {
        settings.setValue("coreSettingsMigrated", true);
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
        case ConfirmOnClose:
            return QVariant(fConfirmOnClose);
        case DisableTrxNotifications:
            return QVariant(fDisableTrxNotifications);
        case DisablePollNotifications:
            return QVariant(fDisablePollNotifications);
        case MapPortUPnP:
            return QVariant(m_node.getSettingBool("upnp", true));
        case MinimizeOnClose:
            return QVariant(fMinimizeOnClose);
        case ProxyUse:
            // GUI editing state: whether the user enabled the proxy. The effective
            // proxy address is mirrored to the core -proxy setting on change.
            return settings.value("fUseProxy", false);
        case ProxyIP:
            return QVariant(SplitProxy(settings.value("addrProxy", "127.0.0.1:9050").toString()).first);
        case ProxyPort:
            return QVariant(SplitProxy(settings.value("addrProxy", "127.0.0.1:9050").toString()).second);
        case ReserveBalance: {
            qint64 sat = 0;
            BitcoinUnits::parse(BitcoinUnits::BTC,
                                QString::fromStdString(m_node.getSettingStr("reservebalance", "")), &sat);
            return QVariant(sat);
        }
        case DisplayUnit:
            return QVariant(nDisplayUnit);
		case DisplayAddresses:
            return QVariant(bDisplayAddresses);
        case Language:
            return settings.value("language", "");
        case WalletStylesheet:
            return settings.value("walletStylesheet", "dark");
        case CoinControlFeatures:
            return QVariant(fCoinControlFeatures);
        case LimitTxnDisplay:
            return QVariant(fLimitTxnDisplay);
        case MaskValues:
            return QVariant(fMaskValues);
        case LimitTxnDate:
            return QVariant(limitTxnDate);
        case PollExpireNotification:
            return QVariant(pollExpireNotification);
        case DisableUpdateCheck:
            return QVariant(m_node.getSettingBool("disableupdatecheck", false));
        case DataDir:
            return settings.value("dataDir", GUIUtil::boostPathToQString(GetDataDir()));
        case EnableStaking:
            // These come from the core and are read-write settings, read through
            // the node so the GUI never touches gArgs directly.
            return QVariant(m_node.getSettingBool("staking", true));
        case EnableStakeSplit:
            return QVariant(m_node.getSettingBool("enablestakesplit", false));
        case EnableSideStaking:
            return QVariant(m_node.getSettingBool("enablesidestaking", false));
        case StakingEfficiency:
            return QVariant((double) m_node.getSettingInt("stakingefficiency", 90));
        case MinStakeSplitValue:
            return QVariant((qint64) m_node.getSettingInt("minstakesplitvalue", MIN_STAKE_SPLIT_VALUE_GRC));
        case ContractChangeToInput:
            return QVariant(m_node.getSettingBool("contractchangetoinputaddress", false));
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
        case ConfirmOnClose:
            fConfirmOnClose = value.toBool();
            settings.setValue("fConfirmOnClose", fConfirmOnClose);
            break;
        case DisableTrxNotifications:
            fDisableTrxNotifications = value.toBool();
            settings.setValue("fDisableTrxNotifications", fDisableTrxNotifications);
            break;
        case DisablePollNotifications:
            fDisablePollNotifications = value.toBool();
            settings.setValue("fDisablePollNotifications", fDisablePollNotifications);
            break;
        case MapPortUPnP:
            // Core setting: the node applies it (SetUseUPnP + MapPort start/stop
            // the port-map thread) via the immediate-effect hook.
            successful = m_node.changeSettings({{"upnp", value.toBool() ? "1" : "0"}}).ok;
            break;
        case MinimizeOnClose:
            fMinimizeOnClose = value.toBool();
            settings.setValue("fMinimizeOnClose", fMinimizeOnClose);
            break;
        case ProxyUse:
            settings.setValue("fUseProxy", value.toBool());
            successful = PushEffectiveProxy(m_node, settings);
            break;
        case ProxyIP: {
            // Replace the host part of the stored address; port is kept. IPv6-aware
            // via SplitProxy/FormatProxy (brackets round-trip correctly).
            const int port = SplitProxy(settings.value("addrProxy", "127.0.0.1:9050").toString()).second;
            settings.setValue("addrProxy", FormatProxy(value.toString(), port));
            successful = PushEffectiveProxy(m_node, settings);
        }
        break;
        case ProxyPort: {
            // Replace the port part of the stored address; host is kept.
            const QString host = SplitProxy(settings.value("addrProxy", "127.0.0.1:9050").toString()).first;
            settings.setValue("addrProxy", FormatProxy(host, value.toInt()));
            successful = PushEffectiveProxy(m_node, settings);
        }
        break;
        case ReserveBalance: {
            // Clamp negatives: a reserve is never negative, and core ParseMoney
            // would reject a "-..." string, leaving the value stale.
            const qint64 sat = value.toLongLong() > 0 ? value.toLongLong() : 0;
            // Store as a plain money string (empty when zero => erase/no reserve).
            successful = m_node.changeSettings(
                {{"reservebalance", sat > 0 ? FormatReserveBalanceSetting(sat) : std::string()}}).ok;
            if (successful) {
                emit reserveBalanceChanged(sat);
            }
        }
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
        case MaskValues:
            fMaskValues = value.toBool();
            settings.setValue("fMaskValues", fMaskValues);
            emit MaskValuesChanged(fMaskValues);
            break;
        case LimitTxnDate:
            limitTxnDate = value.toDate();
            settings.setValue("limitTxnDate", limitTxnDate);
            break;
        case PollExpireNotification:
            pollExpireNotification = value.toDouble();
            settings.setValue("pollExpireNotification", pollExpireNotification);
            break;
        case DisableUpdateCheck:
            // Core read-write setting (the core scheduler and the GUI both read
            // it); route through the node so it persists to gridcoinsettings.json.
            successful = m_node.changeSettings({{"disableupdatecheck", value.toBool() ? "1" : "0"}}).ok;
            break;
        case DataDir:
            // There is no SetArgument here, because the core data directory cannot
            // be changed while the wallet is running.
            dataDir = value.toString();
            settings.setValue("dataDir", dataDir);
            break;
        // The following are core read-write settings stored in
        // gridcoinsettings.json; a stored value overrides the read-only config
        // file. The node validates, persists, and force-sets them (the miner
        // re-reads them live on each stake).
        case EnableStaking:
            successful = m_node.changeSettings({{"staking", value.toBool() ? "1" : "0"}}).ok;
            break;
        case EnableStakeSplit:
            successful = m_node.changeSettings({{"enablestakesplit", value.toBool() ? "1" : "0"}}).ok;
            break;
        case EnableSideStaking:
            successful = m_node.changeSettings({{"enablesidestaking", value.toBool() ? "1" : "0"}}).ok;
            break;
        case StakingEfficiency:
            successful = m_node.changeSettings({{"stakingefficiency", value.toString().toStdString()}}).ok;
            break;
        case MinStakeSplitValue:
            successful = m_node.changeSettings({{"minstakesplitvalue", value.toString().toStdString()}}).ok;
            break;
        case ContractChangeToInput:
            successful = m_node.changeSettings({{"contractchangetoinputaddress", value.toBool() ? "1" : "0"}}).ok;
            break;
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
    qint64 sat = 0;
    BitcoinUnits::parse(BitcoinUnits::BTC,
                        QString::fromStdString(m_node.getSettingStr("reservebalance", "")), &sat);
    return sat;
}

bool OptionsModel::isTestNet()
{
    return m_node.isTestNet();
}

bool OptionsModel::getCoinControlFeatures()
{
    return fCoinControlFeatures;
}

void OptionsModel::toggleCoinControlFeatures()
{
    setData(QAbstractItemModel::createIndex(CoinControlFeatures, 0), !fCoinControlFeatures, Qt::EditRole);
}

bool OptionsModel::getLimitTxnDisplay()
{
    return fLimitTxnDisplay;
}

bool OptionsModel::getSuppressNetworkGraph()
{
    return gArgs.GetArg("-suppressnetworkgraph", "false") == "true";
}

bool OptionsModel::getMaskValues()
{
    return fMaskValues;
}

QDate OptionsModel::getLimitTxnDate()
{
    return limitTxnDate;
}

int64_t OptionsModel::getLimitTxnDateTime()
{
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    QDateTime limitTxnDateTime(limitTxnDate);
#else
    QDateTime limitTxnDateTime = limitTxnDate.startOfDay();
#endif

    return limitTxnDateTime.toMSecsSinceEpoch() / 1000;
}

double OptionsModel::getPollExpireNotification()
{
    return pollExpireNotification;
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

bool OptionsModel::getConfirmOnClose()
{
    return fConfirmOnClose;
}

bool OptionsModel::getDisableTrxNotifications()
{
    return fDisableTrxNotifications;
}

bool OptionsModel::getDisablePollNotifications()
{
    return fDisablePollNotifications;
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
    // Native stylesheet removed for now:
    if (walletStylesheet == "native") {
        return "dark";
    }

    return walletStylesheet;
}

void OptionsModel::setCurrentStyle(QString theme)
{
    setData(QAbstractItemModel::createIndex(WalletStylesheet, 0), theme, Qt::EditRole);
}

void OptionsModel::setMaskValues(bool privacy_mode)
{
    setData(QAbstractItemModel::createIndex(MaskValues, 0), privacy_mode, Qt::EditRole);
}

QString OptionsModel::getDataDir()
{
    return dataDir;
}

SideStakeTableModel* OptionsModel::getSideStakeTableModel()
{
    return m_sidestake_model;
}
