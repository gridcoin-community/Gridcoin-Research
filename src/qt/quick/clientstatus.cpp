// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/quick/clientstatus.h>

#include <interfaces/node.h>
#include <interfaces/staking.h>
#include <qt/clientmodel.h>
#include <qt/guilog.h>
#include <qt/optionsmodel.h>
#include <qt/quick/qmlcallrunner.h>
#include <qt/quick/qmlprovider.h>
#include <util.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QJSEngine>
#include <QStringList>
#include <QtGlobal>

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace {

//! The sync state and whether it counts as in sync.
struct SyncResult {
    ClientStatus::SyncState state;
    bool in_sync;
};

//! BitcoinGUI::setNumBlocks's rule: no connections is never in sync; otherwise
//! in sync while the tip is under 90 minutes old and the height has reached
//! the peers' height.
SyncResult ComputeSync(int connections, int64_t tip_age_s, int height, int peers)
{
    if (connections == 0) return {ClientStatus::NoConnections, false};
    if (tip_age_s < 90 * 60 && height >= peers) return {ClientStatus::UpToDate, true};
    return {ClientStatus::CatchingUp, false};
}

//! The sync icon's step: the largest of 25, 50, 75 and 90 not above the
//! height's percentage of the peers' height, else 0.
int SyncProgress(int height, int peers)
{
    if (peers <= 0) return 0;
    const int64_t percent = static_cast<int64_t>(height) * 100 / peers;
    for (const int step : {90, 75, 50, 25}) {
        if (percent >= step) return step;
    }
    return 0;
}

//! BitcoinGUI::setNumBlocks's tooltip, with its texts and context.
QString SyncToolTip(int connections, int64_t tip_age_s, int count, bool up_to_date)
{
    if (connections == 0) {
        return QCoreApplication::translate("BitcoinGUI", "Sync: no connections.");
    }

    QString tooltip = QCoreApplication::translate("BitcoinGUI", "Processed %n block(s) of transaction history.", "",
                                                  count);
    const int secs = static_cast<int>(std::clamp<int64_t>(tip_age_s, std::numeric_limits<int>::min(),
                                                          std::numeric_limits<int>::max()));
    QString text;
    if (secs <= 0) {
        // Fully up to date: no age text.
    } else if (secs < 60) {
        text = QCoreApplication::translate("BitcoinGUI", "%n second(s) ago", "", secs);
    } else if (secs < 60 * 60) {
        text = QCoreApplication::translate("BitcoinGUI", "%n minute(s) ago", "", secs / 60);
    } else if (secs < 24 * 60 * 60) {
        text = QCoreApplication::translate("BitcoinGUI", "%n hour(s) ago", "", secs / (60 * 60));
    } else {
        text = QCoreApplication::translate("BitcoinGUI", "%n day(s) ago", "", secs / (60 * 60 * 24));
    }

    if (up_to_date) {
        tooltip = QCoreApplication::translate("BitcoinGUI", "Up to date") + QStringLiteral(".<br>") + tooltip;
    } else {
        tooltip = QCoreApplication::translate("BitcoinGUI", "Catching up...") + QStringLiteral("<br>") + tooltip;
    }
    if (!text.isEmpty()) {
        tooltip += QStringLiteral("<br>");
        tooltip += QCoreApplication::translate("BitcoinGUI", "Last received block was generated %1.").arg(text);
    }
    return QStringLiteral("<nobr>") + tooltip + QStringLiteral("</nobr>");
}

//! BitcoinGUI::setNumConnections's icon levels.
int ConnectionLevel(int n)
{
    switch (n) {
    case 0: return 0;
    case 1: case 2: return 1;
    case 3: case 4: case 5: return 2;
    case 6: case 7: case 8: case 9: return 3;
    default: return 4;
    }
}

//! BitcoinGUI::setNumConnections's tooltip.
QString ConnectionToolTip(int n)
{
    if (n == 0) {
        return QCoreApplication::translate("BitcoinGUI",
                                           "No active connections to the Gridcoin network. "
                                           "If this persists more than a few minutes, please check your configuration "
                                           "and your network connectivity.");
    }
    return QCoreApplication::translate("BitcoinGUI", "%n active connection(s) to the Gridcoin network", "", n);
}

//! A port of BitcoinGUI::GetEstimatedStakingFrequency: the estimated time to
//! stake is in seconds.
QString EstimatedStakingFrequency(double etts_seconds)
{
    unsigned int estimate = 0;
    if (etts_seconds >= static_cast<double>(std::numeric_limits<unsigned int>::max())) {
        estimate = std::numeric_limits<unsigned int>::max();
    } else if (etts_seconds > 0) {
        estimate = static_cast<unsigned int>(etts_seconds);
    }

    if (!estimate) return QCoreApplication::translate("BitcoinGUI", "not available");

    // Start with 1/yr
    double frequency = 3600.0 * 24.0 * 365.0 / estimate;
    QString unit = QCoreApplication::translate("BitcoinGUI", "year");
    if (frequency >= 12.0 /* times per year */) {
        frequency /= 12.0;
        unit = QCoreApplication::translate("BitcoinGUI", "month");
        if (frequency >= 30.0 /* times per month */) {
            frequency /= 30.0;
            unit = QCoreApplication::translate("BitcoinGUI", "day");
            if (frequency >= 24.0 /* times per day */) {
                frequency /= 24.0;
                unit = QCoreApplication::translate("BitcoinGUI", "hour");
            }
        }
    }
    return QCoreApplication::translate("BitcoinGUI", "%1 times per %2").arg(QString::number(frequency, 'f', 2), unit);
}

//! The names, each HTML-escaped (the tooltip is rich text), joined as
//! updateScraperIcon joins them; "none" when there are none and `none_text`.
QString JoinEscaped(const std::vector<std::string>& names, bool none_text)
{
    if (names.empty() && none_text) return QCoreApplication::translate("BitcoinGUI", "none");
    QStringList list;
    for (const std::string& name : names) list << QString::fromStdString(name).toHtmlEscaped();
    return list.join(QStringLiteral(", "));
}

} // namespace

//! What the hydrate reads. A plain value: it holds no adapter, model or
//! interface pointer.
struct ClientStatus::HydrateResult {
    int connections{0};
    QDateTime last_block_date;
    QString status_bar_warnings;
    QString miner_warnings;
    double net_weight{0.0};
    int num_blocks{0};
    int num_blocks_peers{0};
    double difficulty{0.0};
    bool testnet{false};
    bool staking{false};
    double coin_weight{0.0};
    std::optional<double> etts;
};

//! What a status read reads.
struct ClientStatus::StatusResult {
    int connections{0};
    QDateTime last_block_date;
    QString status_bar_warnings;
    QString miner_warnings;
};

//! What a scraper snapshot read reads, with the event it was posted for (-1
//! and -1: the snapshot's own current event).
struct ClientStatus::ScraperResult {
    interfaces::ScraperConvergenceSnapshot snapshot;
    int event_type{-1};
    int status{-1};
};

ClientStatus::ClientStatus(int marker)
    : QObject(nullptr), m_marker(marker)
{
}

ClientStatus* ClientStatus::create(QQmlEngine*, QJSEngine*)
{
    QmlProvider* provider = QmlProvider::installed();
    if (!provider) {
        // A programming error that would hand QML a null singleton; qFatal
        // halts in every build.
        qFatal("ClientStatus::create: no QmlProvider is installed");
    }
    QJSEngine::setObjectOwnership(&provider->clientStatus(), QJSEngine::CppOwnership);
    return &provider->clientStatus();
}

void ClientStatus::attach(ClientModel& client, QmlCallRunner& runner)
{
    m_client = &client;
    m_runner = &runner;
    m_options = client.getOptionsModel();
    // Reads posted by an earlier attach never deliver, so none is in flight.
    m_status_in_flight = false;
    m_status_dirty = false;
    m_scraper_in_flight = false;
    m_scraper_dirty = false;

    m_connections.push_back(connect(&client, &ClientModel::numBlocksChanged, this, &ClientStatus::onNumBlocksChanged));
    m_connections.push_back(connect(&client, &ClientModel::difficultyChanged, this, &ClientStatus::onDifficultyChanged));
    m_connections.push_back(
        connect(&client, &ClientModel::numConnectionsChanged, this, &ClientStatus::onNumConnectionsChanged));
    m_connections.push_back(connect(&client, &ClientModel::minerStatusChanged, this, &ClientStatus::onMinerStatusChanged));
    m_connections.push_back(connect(&client, &ClientModel::updateScraperStatus, this, &ClientStatus::onScraperStatus));
    if (m_options) {
        m_connections.push_back(
            connect(m_options, &OptionsModel::MaskValuesChanged, this, &ClientStatus::onMaskValuesChanged));
        onMaskValuesChanged(m_options->getMaskValues());
    }

    postHydrate();
    postScraperRead(-1, -1);
}

void ClientStatus::detach()
{
    for (const QMetaObject::Connection& connection : m_connections) disconnect(connection);
    m_connections.clear();
    ++m_epoch;
    m_client = nullptr;
    m_runner = nullptr;
    m_options = nullptr;
}

void ClientStatus::onNumBlocksChanged(int count, int peers)
{
    m_signal_blocks = true;
    setBlocks(count, peers);
    postStatusRead();
}

void ClientStatus::onDifficultyChanged(double difficulty)
{
    m_signal_difficulty = true;
    setWeights(difficulty, m_network_weight, m_coin_weight);
}

void ClientStatus::onNumConnectionsChanged(int count)
{
    m_signal_connections = true;
    setConnections(count);
    postStatusRead();
}

void ClientStatus::onMinerStatusChanged(bool staking, double net_weight, double coin_weight, double etts)
{
    m_signal_miner = true;
    m_staking = staking;
    m_etts = etts;
    setWeights(m_difficulty, net_weight, coin_weight);
    // Formatted from the stored miner warnings; the status read posted below
    // formats it again with the warnings it reads.
    updateStaking();
    postStatusRead();
}

void ClientStatus::onScraperStatus(int event_type, int status)
{
    postScraperRead(event_type, status);
}

void ClientStatus::onMaskValuesChanged(bool masked)
{
    if (masked == m_masked) return;
    m_masked = masked;
    setWeights(m_difficulty, m_network_weight, m_coin_weight);
    // The staking tooltip names the coin weight and the estimated staking
    // frequency, so it follows the mask too.
    updateStaking();
}

void ClientStatus::postHydrate()
{
    if (!m_client || !m_runner) return;

    ClientModel& client = *m_client;
    interfaces::Node& node = client.node();
    interfaces::StakingStatus& staking = client.stakingStatus();
    auto result = std::make_shared<HydrateResult>();
    const uint64_t epoch = m_epoch;

    m_signal_blocks = false;
    m_signal_difficulty = false;
    m_signal_connections = false;
    m_signal_miner = false;
    // A closed runner refuses the post; the values then stay at their defaults.
    (void)m_runner->postSerial(
        [&client, &node, &staking, result] {
            result->connections = client.getNumConnections();
            result->last_block_date = client.getLastBlockDate();
            result->status_bar_warnings = client.getStatusBarWarnings();
            result->miner_warnings = client.getMinerWarnings();
            result->net_weight = client.getNetWeight();
            result->num_blocks = node.getNumBlocks();
            result->num_blocks_peers = node.getNumBlocksOfPeers();
            result->difficulty = node.getDifficulty();
            result->testnet = node.isTestNet();
            result->staking = staking.isStaking();
            result->coin_weight = staking.getCoinWeight();
            if (result->staking) result->etts = staking.tryGetEstimatedTimeToStake();
        },
        [this, result, epoch] { deliverHydrate(epoch, *result); });
}

void ClientStatus::deliverHydrate(uint64_t epoch, const HydrateResult& result)
{
    if (epoch != m_epoch) return;

    if (!m_signal_blocks) setBlocks(result.num_blocks, result.num_blocks_peers);
    if (!m_signal_connections) setConnections(result.connections);
    if (!m_signal_miner) {
        m_staking = result.staking;
        if (result.etts) m_etts = *result.etts;
    }
    setWeights(m_signal_difficulty ? m_difficulty : result.difficulty,
               m_signal_miner ? m_network_weight : result.net_weight,
               m_signal_miner ? m_coin_weight : result.coin_weight);
    setTestNet(result.testnet);
    setWarnings(result.status_bar_warnings, result.miner_warnings);
    updateSync(result.connections, result.last_block_date.secsTo(QDateTime::currentDateTime()));
    updateStaking();
}

void ClientStatus::postStatusRead()
{
    if (!m_client || !m_runner) return;
    if (m_status_in_flight) {
        m_status_dirty = true;
        return;
    }

    ClientModel& client = *m_client;
    auto result = std::make_shared<StatusResult>();
    const uint64_t epoch = m_epoch;
    if (m_runner->postSerial(
            [&client, result] {
                result->connections = client.getNumConnections();
                result->last_block_date = client.getLastBlockDate();
                result->status_bar_warnings = client.getStatusBarWarnings();
                result->miner_warnings = client.getMinerWarnings();
            },
            [this, result, epoch] { deliverStatusRead(epoch, *result); })) {
        m_status_in_flight = true;
        m_status_dirty = false;
    }
}

void ClientStatus::deliverStatusRead(uint64_t epoch, const StatusResult& result)
{
    if (epoch != m_epoch) return;
    m_status_in_flight = false;

    setConnections(result.connections);
    updateSync(result.connections, result.last_block_date.secsTo(QDateTime::currentDateTime()));
    // The warnings follow every read, whatever the sync state: the footer
    // shows the current ones while catching up too.
    setWarnings(result.status_bar_warnings, result.miner_warnings);
    updateStaking();

    if (m_status_dirty) postStatusRead();
}

void ClientStatus::postScraperRead(int event_type, int status)
{
    if (!m_client || !m_runner) return;
    if (m_scraper_in_flight) {
        m_scraper_dirty = true;
        m_scraper_dirty_type = event_type;
        m_scraper_dirty_status = status;
        return;
    }

    interfaces::Node& node = m_client->node();
    auto result = std::make_shared<ScraperResult>();
    result->event_type = event_type;
    result->status = status;
    const uint64_t epoch = m_epoch;
    if (m_runner->postPooled([&node, result] { result->snapshot = node.getScraperConvergenceSnapshot(); },
                             [this, result, epoch] { deliverScraperRead(epoch, *result); })) {
        m_scraper_in_flight = true;
        m_scraper_dirty = false;
    }
}

void ClientStatus::deliverScraperRead(uint64_t epoch, const ScraperResult& result)
{
    if (epoch != m_epoch) return;
    m_scraper_in_flight = false;

    const bool attach_read = result.event_type < 0 || result.status < 0;
    int event_type = result.event_type;
    int status = result.status;
    if (event_type < 0 || status < 0) {
        event_type = result.snapshot.current_event_type;
        status = result.snapshot.current_event_status;
    }
    if (event_type < 0 || status < 0 ||
        (attach_read && event_type == static_cast<int>(scrapereventtypes::OutOfSync))) {
        // No scraper event since the node started: the event
        // BitcoinGUI::setClientModel starts the scraper icon with. An
        // out-of-sync event found at attach starts the same way.
        event_type = static_cast<int>(scrapereventtypes::OutOfSync);
        status = CT_UPDATING;
    }

    const interfaces::ScraperConvergenceSnapshot& snapshot = result.snapshot;
    std::optional<ScraperState> state;
    QString tooltip;
    const bool convergence = event_type == static_cast<int>(scrapereventtypes::Convergence) ||
                             event_type == static_cast<int>(scrapereventtypes::SBContract);
    if (event_type == static_cast<int>(scrapereventtypes::OutOfSync) && status == CT_UPDATING) {
        state = Waiting;
        tooltip = QCoreApplication::translate("BitcoinGUI", "Scraper: waiting on wallet to sync.");
    } else if (event_type == static_cast<int>(scrapereventtypes::Sleep) && status == CT_NEW) {
        state = Inactive;
        tooltip = QCoreApplication::translate("BitcoinGUI", "Scraper: superblock not needed - inactive.");
    } else if (event_type == static_cast<int>(scrapereventtypes::Stats) &&
               (status == CT_NEW || status == CT_UPDATED || status == CT_UPDATING)) {
        state = Downloading;
        tooltip = QCoreApplication::translate("BitcoinGUI", "Scraper: downloading and processing stats.");
    } else if (convergence && (status == CT_NEW || status == CT_UPDATED) && snapshot.time) {
        state = Converged;
        const QString date = QString::fromStdString(DateTimeStrFormat("%x %H:%M:%S", snapshot.time));
        const QString excluded_projects = JoinEscaped(snapshot.excluded_projects, true);
        // As in updateScraperIcon, the scraper lists only when the scraper log
        // category is on.
        if (GUILog::WillLogCategory(GUILogCategory::SCRAPER)) {
            tooltip = QCoreApplication::translate("BitcoinGUI",
                                                  "Scraper: Convergence achieved, date/time %1 UTC. \n"
                                                  "Project(s) excluded: %2. \n"
                                                  "Scrapers included: %3. \n"
                                                  "Scraper(s) excluded: %4. \n"
                                                  "Scraper(s) not publishing: %5.")
                          .arg(date, excluded_projects, JoinEscaped(snapshot.included_scrapers, false),
                               JoinEscaped(snapshot.excluded_scrapers, true),
                               JoinEscaped(snapshot.scrapers_not_publishing, true))
                          .replace(QLatin1Char('\n'), QStringLiteral("<br>"));
        } else {
            tooltip = QCoreApplication::translate("BitcoinGUI",
                                                  "Scraper: Convergence achieved, date/time %1 UTC. \n"
                                                  " Project(s) excluded: %2.")
                          .arg(date, excluded_projects)
                          .replace(QLatin1Char('\n'), QStringLiteral("<br>"));
        }
    } else if (convergence && status == CT_DELETED) {
        state = NoConvergence;
        tooltip = QCoreApplication::translate("BitcoinGUI",
                                              "Scraper: No convergence able to be achieved. Will retry in a few minutes.");
    }

    if (state && (*state != m_scraper_state || tooltip != m_scraper_tooltip)) {
        m_scraper_state = *state;
        m_scraper_tooltip = tooltip;
        emit scraperChanged();
    }

    if (m_scraper_dirty) postScraperRead(m_scraper_dirty_type, m_scraper_dirty_status);
}

void ClientStatus::setBlocks(int count, int peers)
{
    if (count == m_num_blocks && peers == m_num_blocks_peers) return;
    m_num_blocks = count;
    m_num_blocks_peers = peers;
    emit blocksChanged();
    updateBlocksText();
}

void ClientStatus::updateBlocksText()
{
    // OverviewPage::setHeight's rule; that text has no translation there.
    QString text = QString::number(m_num_blocks);
    if (!m_in_sync && m_num_blocks_peers > 0 && m_num_blocks < m_num_blocks_peers) {
        const int64_t percent = static_cast<int64_t>(m_num_blocks) * 100 / m_num_blocks_peers;
        text += QStringLiteral(" of %1 (%2%)").arg(QString::number(m_num_blocks_peers), QString::number(percent));
    }
    if (text == m_blocks_text) return;
    m_blocks_text = text;
    emit blocksChanged();
}

void ClientStatus::setConnections(int count)
{
    const int level = ConnectionLevel(count);
    const QString tooltip = ConnectionToolTip(count);
    if (count == m_num_connections && level == m_connection_level && tooltip == m_connection_tooltip) return;
    m_num_connections = count;
    m_connection_level = level;
    m_connection_tooltip = tooltip;
    emit connectionsChanged();
}

void ClientStatus::setWeights(double difficulty, double net_weight, double coin_weight)
{
    const QString text = m_masked ? QStringLiteral("#.##") : QString::number(coin_weight, 'f', 0);
    if (difficulty == m_difficulty && net_weight == m_network_weight && coin_weight == m_coin_weight &&
        text == m_coin_weight_text) {
        return;
    }
    m_difficulty = difficulty;
    m_network_weight = net_weight;
    m_coin_weight = coin_weight;
    m_coin_weight_text = text;
    emit weightsChanged();
}

void ClientStatus::setWarnings(const QString& status_bar_warnings, const QString& miner_warnings)
{
    if (status_bar_warnings == m_status_bar_warnings && miner_warnings == m_miner_warnings) return;
    m_status_bar_warnings = status_bar_warnings;
    m_miner_warnings = miner_warnings;
    emit warningsChanged();
}

void ClientStatus::setTestNet(bool testnet)
{
    if (testnet == m_is_testnet) return;
    m_is_testnet = testnet;
    emit networkChanged();
}

void ClientStatus::updateSync(int connections, int64_t tip_age)
{
    const SyncResult sync = ComputeSync(connections, tip_age, m_num_blocks, m_num_blocks_peers);
    const int progress = SyncProgress(m_num_blocks, m_num_blocks_peers);
    const QString tooltip = SyncToolTip(connections, tip_age, m_num_blocks, sync.state == UpToDate);
    if (sync.state != m_sync_state || sync.in_sync != m_in_sync || progress != m_sync_progress ||
        tooltip != m_sync_tooltip) {
        m_sync_state = sync.state;
        m_in_sync = sync.in_sync;
        m_sync_progress = progress;
        m_sync_tooltip = tooltip;
        emit syncChanged();
    }
    updateBlocksText();
}

void ClientStatus::updateStaking()
{
    // BitcoinGUI::updateStakingIcon's three arms. The miner warnings are node
    // text in a rich-text tooltip, so they are escaped. While values are masked
    // the coin weight reads as coinWeightText reads, and so does the estimated
    // staking frequency, which the node computes from the wallet's own coins.
    StakingState state;
    QString tooltip;
    const QString warnings = m_miner_warnings.toHtmlEscaped();
    const QString weight = m_masked ? QStringLiteral("#.##") : QString::number(m_coin_weight, 'f', 0);
    const QString frequency = m_masked ? QStringLiteral("#.##") : EstimatedStakingFrequency(m_etts);
    if (m_staking) {
        state = Staking;
        tooltip = QCoreApplication::translate("BitcoinGUI", "Staking.<br>Your weight is %1<br>Network weight is %2<br>"
                                                            "<b>Estimated</b> staking frequency is %3.")
                      .arg(weight, QString::number(m_network_weight, 'f', 0), frequency);
    } else if (m_coin_weight == 0.0) {
        state = UnableToStake;
        tooltip = QCoreApplication::translate("BitcoinGUI", "Unable to stake: %1").arg(warnings);
    } else {
        state = NotStaking;
        tooltip = QCoreApplication::translate("BitcoinGUI",
                                              "Not staking currently: %1, <b>Estimated</b> staking frequency is %2.")
                      .arg(warnings, frequency);
    }
    if (state == m_staking_state && tooltip == m_staking_tooltip) return;
    m_staking_state = state;
    m_staking_tooltip = tooltip;
    emit stakingChanged();
}
