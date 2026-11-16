// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_CLIENTSTATUS_H
#define BITCOIN_QT_QUICK_CLIENTSTATUS_H

#include <QtQml/qqmlregistration.h>

#include <QMetaObject>
#include <QObject>
#include <QString>

#include <cstdint>
#include <type_traits>
#include <vector>

class ClientModel;
class OptionsModel;
class QmlCallRunner;
class QQmlEngine;
class QJSEngine;

//! The QML singleton `ClientStatus`: the block height, the sync state, the
//! connections, the staking state, the warnings and the scraper state, as the
//! Widgets status bar and overview show them. It is owned by QmlProvider, which
//! hands QML this instance through create(). The live values arrive as the
//! ClientModel signals' arguments; every node and staking read runs on the call
//! runner (the status and seed reads on the serial lane, the scraper snapshot
//! on the pooled lane), never on the GUI thread. On the GUI thread the adapter
//! only takes the model's options model and its node and staking interface
//! references, and reads the mask state from the options model.
class ClientStatus : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ClientStatus)
    QML_SINGLETON
    Q_PROPERTY(int marker READ marker CONSTANT)
    Q_PROPERTY(int numBlocks READ numBlocks NOTIFY blocksChanged)
    Q_PROPERTY(int numBlocksPeers READ numBlocksPeers NOTIFY blocksChanged)
    Q_PROPERTY(QString blocksText READ blocksText NOTIFY blocksChanged)
    Q_PROPERTY(bool inSync READ inSync NOTIFY syncChanged)
    Q_PROPERTY(SyncState syncState READ syncState NOTIFY syncChanged)
    Q_PROPERTY(int syncProgress READ syncProgress NOTIFY syncChanged)
    Q_PROPERTY(QString syncToolTip READ syncToolTip NOTIFY syncChanged)
    Q_PROPERTY(int numConnections READ numConnections NOTIFY connectionsChanged)
    Q_PROPERTY(int connectionLevel READ connectionLevel NOTIFY connectionsChanged)
    Q_PROPERTY(QString connectionToolTip READ connectionToolTip NOTIFY connectionsChanged)
    Q_PROPERTY(double difficulty READ difficulty NOTIFY weightsChanged)
    Q_PROPERTY(double networkWeight READ networkWeight NOTIFY weightsChanged)
    Q_PROPERTY(double coinWeight READ coinWeight NOTIFY weightsChanged)
    Q_PROPERTY(QString coinWeightText READ coinWeightText NOTIFY weightsChanged)
    Q_PROPERTY(StakingState stakingState READ stakingState NOTIFY stakingChanged)
    Q_PROPERTY(QString stakingToolTip READ stakingToolTip NOTIFY stakingChanged)
    Q_PROPERTY(QString statusBarWarnings READ statusBarWarnings NOTIFY warningsChanged)
    Q_PROPERTY(QString minerWarnings READ minerWarnings NOTIFY warningsChanged)
    Q_PROPERTY(bool isTestNet READ isTestNet NOTIFY networkChanged)
    Q_PROPERTY(ScraperState scraperState READ scraperState NOTIFY scraperChanged)
    Q_PROPERTY(QString scraperToolTip READ scraperToolTip NOTIFY scraperChanged)

public:
    //! The sync state, by BitcoinGUI::setNumBlocks's rule.
    enum SyncState { NoConnections, CatchingUp, UpToDate };
    Q_ENUM(SyncState)
    //! The three arms of BitcoinGUI::updateStakingIcon.
    enum StakingState { Staking, NotStaking, UnableToStake };
    Q_ENUM(StakingState)
    //! The arms of BitcoinGUI::updateScraperIcon.
    enum ScraperState { Waiting, Inactive, Downloading, Converged, NoConvergence };
    Q_ENUM(ScraperState)

    //! `marker` identifies the provider that owns this instance.
    explicit ClientStatus(int marker);

    //! The singleton factory: hands QML the installed provider's instance with
    //! C++ ownership. Halts the process when no provider is installed.
    static ClientStatus* create(QQmlEngine* qml_engine, QJSEngine* js_engine);

    //! Follows the model's signals and posts the seed read (the hydrate) and
    //! the first scraper snapshot. The model and the runner must outlive the
    //! matching detach().
    void attach(ClientModel& client, QmlCallRunner& runner);
    //! Disconnects from the model; a read still in flight delivers nothing. The
    //! values are kept. A detach() before attach() does nothing.
    void detach();

    int marker() const { return m_marker; }
    int numBlocks() const { return m_num_blocks; }
    int numBlocksPeers() const { return m_num_blocks_peers; }
    QString blocksText() const { return m_blocks_text; }
    bool inSync() const { return m_in_sync; }
    SyncState syncState() const { return m_sync_state; }
    int syncProgress() const { return m_sync_progress; }
    QString syncToolTip() const { return m_sync_tooltip; }
    int numConnections() const { return m_num_connections; }
    int connectionLevel() const { return m_connection_level; }
    QString connectionToolTip() const { return m_connection_tooltip; }
    double difficulty() const { return m_difficulty; }
    double networkWeight() const { return m_network_weight; }
    double coinWeight() const { return m_coin_weight; }
    QString coinWeightText() const { return m_coin_weight_text; }
    StakingState stakingState() const { return m_staking_state; }
    QString stakingToolTip() const { return m_staking_tooltip; }
    QString statusBarWarnings() const { return m_status_bar_warnings; }
    QString minerWarnings() const { return m_miner_warnings; }
    bool isTestNet() const { return m_is_testnet; }
    ScraperState scraperState() const { return m_scraper_state; }
    QString scraperToolTip() const { return m_scraper_tooltip; }

signals:
    void blocksChanged();
    void syncChanged();
    void connectionsChanged();
    void weightsChanged();
    void stakingChanged();
    void warningsChanged();
    void networkChanged();
    void scraperChanged();

private:
    struct HydrateResult;
    struct StatusResult;
    struct ScraperResult;

    void onNumBlocksChanged(int count, int peers);
    void onDifficultyChanged(double difficulty);
    void onNumConnectionsChanged(int count);
    void onMinerStatusChanged(bool staking, double net_weight, double coin_weight, double etts);
    void onScraperStatus(int event_type, int status);
    void onMaskValuesChanged(bool masked);

    void postHydrate();
    void deliverHydrate(uint64_t epoch, const HydrateResult& result);
    void postStatusRead();
    void deliverStatusRead(uint64_t epoch, const StatusResult& result);
    void postScraperRead(int event_type, int status);
    void deliverScraperRead(uint64_t epoch, const ScraperResult& result);

    //! Each setter assigns and emits its NOTIFY signal only on a change.
    void setBlocks(int count, int peers);
    void setConnections(int count);
    void setWeights(double difficulty, double net_weight, double coin_weight);
    void setWarnings(const QString& status_bar_warnings, const QString& miner_warnings);
    void setTestNet(bool testnet);
    //! Recomputes the sync state from the connection count and the tip age.
    void updateSync(int connections, int64_t tip_age);
    //! Formats the block text from the height, the peers and the sync state.
    void updateBlocksText();
    //! Formats the staking state and tooltip from the stored values.
    void updateStaking();

    int m_marker;
    ClientModel* m_client{nullptr};
    QmlCallRunner* m_runner{nullptr};
    OptionsModel* m_options{nullptr};
    std::vector<QMetaObject::Connection> m_connections;
    //! Moved by every detach(); a delivery of an earlier epoch does nothing.
    uint64_t m_epoch{0};

    // Values.
    int m_num_blocks{0};
    int m_num_blocks_peers{0};
    QString m_blocks_text{QStringLiteral("0")};
    bool m_in_sync{false};
    SyncState m_sync_state{NoConnections};
    int m_sync_progress{0};
    QString m_sync_tooltip;
    int m_num_connections{0};
    int m_connection_level{0};
    QString m_connection_tooltip;
    double m_difficulty{0.0};
    double m_network_weight{0.0};
    double m_coin_weight{0.0};
    QString m_coin_weight_text{QStringLiteral("0")};
    bool m_staking{false};
    double m_etts{0.0};
    StakingState m_staking_state{UnableToStake};
    QString m_staking_tooltip;
    QString m_status_bar_warnings;
    QString m_miner_warnings;
    bool m_is_testnet{false};
    ScraperState m_scraper_state{Waiting};
    QString m_scraper_tooltip;
    bool m_masked{false};

    // Reads in flight, and triggers that arrived meanwhile.
    bool m_status_in_flight{false};
    bool m_status_dirty{false};
    bool m_scraper_in_flight{false};
    bool m_scraper_dirty{false};
    int m_scraper_dirty_type{-1};
    int m_scraper_dirty_status{-1};

    // The hydrate's fields that a signal set since the hydrate was posted; the
    // hydrate does not overwrite them.
    bool m_signal_blocks{false};
    bool m_signal_difficulty{false};
    bool m_signal_connections{false};
    bool m_signal_miner{false};
};

static_assert(!std::is_default_constructible_v<ClientStatus>,
              "Qt 6.4 prefers new T over create() for a default-constructible singleton");

#endif // BITCOIN_QT_QUICK_CLIENTSTATUS_H
