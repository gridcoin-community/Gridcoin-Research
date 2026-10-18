// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/quick/researchersummary.h>

#include <qt/bitcoinunits.h>
#include <qt/optionsmodel.h>
#include <qt/quick/qmlprovider.h>
#include <qt/researcher/researchermodel.h>

#include <QChar>
#include <QCoreApplication>
#include <QJSEngine>

#include <chrono>

namespace {

//! ResearcherModel's mask strings: 32 '#' for the CPID, "#" for the
//! magnitude.
const QString CPID_MASK(32, QLatin1Char('#'));
const QString MAGNITUDE_MASK(QStringLiteral("#"));
//! What ResearcherModel shows for the magnitude and accrual while out of
//! sync, masked or not.
const QString OUT_OF_SYNC_TEXT(QStringLiteral("..."));

} // namespace

ResearcherSummary::ResearcherSummary(int marker)
    : QObject(nullptr), m_marker(marker), m_unit(BitcoinUnits::BTC)
{
    m_beacon_timer.setInterval(std::chrono::seconds(60));
    connect(&m_beacon_timer, &QTimer::timeout, this, &ResearcherSummary::refresh);
}

ResearcherSummary* ResearcherSummary::create(QQmlEngine*, QJSEngine*)
{
    QmlProvider* provider = QmlProvider::installed();
    if (!provider) {
        // A programming error that would hand QML a null singleton; qFatal
        // halts in every build.
        qFatal("ResearcherSummary::create: no QmlProvider is installed");
    }
    QJSEngine::setObjectOwnership(&provider->researcherSummary(), QJSEngine::CppOwnership);
    return &provider->researcherSummary();
}

void ResearcherSummary::attach(ResearcherModel& researcher, OptionsModel* options)
{
    m_researcher = &researcher;
    m_options = options;
    m_masked = options && options->getMaskValues();
    m_unit = options ? options->getDisplayUnit() : static_cast<int>(BitcoinUnits::BTC);

    m_connections.push_back(connect(&researcher, &ResearcherModel::researcherChanged, this, &ResearcherSummary::refresh));
    m_connections.push_back(connect(&researcher, &ResearcherModel::beaconChanged, this, &ResearcherSummary::refresh));
    m_connections.push_back(connect(&researcher, &ResearcherModel::magnitudeChanged, this, &ResearcherSummary::refresh));
    m_connections.push_back(connect(&researcher, &ResearcherModel::accrualChanged, this, &ResearcherSummary::refresh));
    if (options) {
        m_connections.push_back(
            connect(options, &OptionsModel::MaskValuesChanged, this, &ResearcherSummary::onMaskValuesChanged));
        m_connections.push_back(
            connect(options, &OptionsModel::displayUnitChanged, this, &ResearcherSummary::onDisplayUnitChanged));
    }
    m_beacon_timer.start();

    // The model emitted its change signals when it was built, before this
    // adapter connected, so the values are computed here once.
    refresh();
}

void ResearcherSummary::detach()
{
    for (const QMetaObject::Connection& connection : m_connections) disconnect(connection);
    m_connections.clear();
    m_beacon_timer.stop();
    m_researcher = nullptr;
    m_options = nullptr;
}

void ResearcherSummary::onMaskValuesChanged(bool masked)
{
    m_masked = masked;
    refresh();
}

void ResearcherSummary::onDisplayUnitChanged(int unit)
{
    m_unit = unit;
    refresh();
}

void ResearcherSummary::refresh()
{
    if (!m_researcher) return;
    const ResearcherModel& researcher = *m_researcher;

    // OverviewPage::updateResearcherStatus: the mode label's "CPID" has no
    // translation there.
    Mode mode = StakingOnly;
    QString mode_text;
    if (researcher.hasEligibleProjects()) {
        mode = Solo;
        mode_text = QStringLiteral("CPID");
    } else if (researcher.hasPoolProjects()) {
        mode = Pool;
        mode_text = QCoreApplication::translate("OverviewPage", "Pool");
    } else {
        mode_text = QCoreApplication::translate("OverviewPage", "Staking Only");
    }

    // Masked in ResearcherModel's order: out of sync shows "..." before the
    // mask applies; the CPID has no out-of-sync arm. The mask is applied here
    // rather than through setMaskCpidMagnitudeAccrual, whose refresh() reads
    // the node.
    const bool out_of_sync = researcher.outOfSync();
    const QString cpid = m_masked ? CPID_MASK : researcher.formatCpid();
    const QString magnitude_text = out_of_sync ? OUT_OF_SYNC_TEXT
                                   : m_masked  ? MAGNITUDE_MASK
                                               : researcher.formatMagnitude();
    bool near_limit = false;
    (void)researcher.formatAccrual(m_unit, near_limit);
    const QString accrual_text = out_of_sync ? OUT_OF_SYNC_TEXT
                                             : BitcoinUnits::formatWithPrivacy(m_unit, researcher.getAccrual(), m_masked);
    const QString status_text = researcher.formatStatus();

    // BitcoinGUI::updateBeaconIcon: hidden for a non-cruncher and in pool
    // mode. The tooltip is rich text, so every value in it is escaped.
    const bool beacon_visible = !researcher.configuredForNoncruncherMode() && !researcher.detectedPoolMode();
    const bool beacon_active = researcher.hasActiveBeacon();
    const QString escaped_cpid = cpid.toHtmlEscaped();
    const QString beacon_status = researcher.formatBeaconStatus().toHtmlEscaped();
    QString beacon_tooltip;
    if (researcher.hasPendingBeacon()) {
        beacon_tooltip = QCoreApplication::translate("BitcoinGUI", "CPID: %1\n"
                                                                   "Time left to activate: %2\n"
                                                                   "%3")
                             .arg(escaped_cpid, researcher.formatTimeToPendingBeaconExpiration().toHtmlEscaped(),
                                  beacon_status)
                             .replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    } else if (researcher.beaconExpired()) {
        beacon_tooltip = QCoreApplication::translate("BitcoinGUI", "CPID: %1\n"
                                                                   "Beacon age: %2\n"
                                                                   "Current beacon expired!\n"
                                                                   "%3")
                             .arg(escaped_cpid, researcher.formatBeaconAge().toHtmlEscaped(), beacon_status)
                             .replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    } else if (researcher.hasActiveBeacon()) {
        beacon_tooltip = QCoreApplication::translate("BitcoinGUI", "CPID: %1\n"
                                                                   "Beacon age: %2\n"
                                                                   "Expires: %3\n"
                                                                   "%4")
                             .arg(escaped_cpid, researcher.formatBeaconAge().toHtmlEscaped(),
                                  researcher.formatTimeToBeaconExpiration().toHtmlEscaped(), beacon_status)
                             .replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    } else {
        beacon_tooltip = beacon_status;
    }

    if (mode == m_mode && mode_text == m_mode_text && cpid == m_cpid && magnitude_text == m_magnitude_text &&
        status_text == m_status_text && accrual_text == m_accrual_text && near_limit == m_accrual_near_limit &&
        beacon_visible == m_beacon_visible && beacon_active == m_beacon_active &&
        beacon_tooltip == m_beacon_tooltip) {
        return;
    }
    m_mode = mode;
    m_mode_text = mode_text;
    m_cpid = cpid;
    m_magnitude_text = magnitude_text;
    m_status_text = status_text;
    m_accrual_text = accrual_text;
    m_accrual_near_limit = near_limit;
    m_beacon_visible = beacon_visible;
    m_beacon_active = beacon_active;
    m_beacon_tooltip = beacon_tooltip;
    emit summaryChanged();
}
