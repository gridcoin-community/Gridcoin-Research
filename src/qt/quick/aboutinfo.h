// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_ABOUTINFO_H
#define BITCOIN_QT_QUICK_ABOUTINFO_H

#include <QtQml/qqmlregistration.h>

#include <QObject>
#include <QString>
#include <QtGlobal>

#include <cstdint>
#include <type_traits>

class ClientModel;
class QmlCallRunner;
class QQmlEngine;
class QJSEngine;

namespace interfaces {
class Node;
} // namespace interfaces

//! The QML singleton `AboutInfo`: the About window's version, copyright and
//! version-information check. It is owned by QmlProvider, which hands QML this
//! instance through create(). The node reads run on the call runner: the
//! version and settings on the serial lane, the update check, which can block
//! on the network, on the pooled lane.
class AboutInfo : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(AboutInfo)
    QML_SINGLETON
    Q_PROPERTY(int marker READ marker CONSTANT)
    Q_PROPERTY(QString fullVersion READ fullVersion NOTIFY fullVersionChanged)
    Q_PROPERTY(QString copyrightText READ copyrightText CONSTANT)
    Q_PROPERTY(bool updateCheckAvailable READ updateCheckAvailable NOTIFY updateCheckAvailableChanged)
    Q_PROPERTY(QString updateCheckDisabledReason READ updateCheckDisabledReason NOTIFY updateCheckAvailableChanged)
    Q_PROPERTY(bool updateCheckRunning READ updateCheckRunning NOTIFY updateCheckRunningChanged)
    Q_PROPERTY(quint64 pendingRequestId READ pendingRequestId NOTIFY updateCheckRunningChanged)

public:
    //! UpdateDialog::UpdateType, 0 to 3.
    enum UpgradeType { Unknown, Leisure, Mandatory, Unsupported };
    Q_ENUM(UpgradeType)

    //! `marker` identifies the provider that owns this instance.
    explicit AboutInfo(int marker);

    //! The singleton factory: hands QML the installed provider's instance with
    //! C++ ownership. Halts the process when no provider is installed.
    static AboutInfo* create(QQmlEngine* qml_engine, QJSEngine* js_engine);

    //! Reads this process's -disableupdatecheck and posts the serial read of
    //! the version, the network and the node's setting. The model and the
    //! runner must outlive the matching detach().
    void attach(ClientModel& client, QmlCallRunner& runner);
    //! A read still in flight delivers nothing; the values are kept. A
    //! detach() before attach() does nothing.
    void detach();

    //! Starts a version-information check on the pooled lane and returns its
    //! request id (from 1), or 0 when unattached, unavailable, already running
    //! or refused by a closed runner. updateCheckFinished carries the id.
    Q_INVOKABLE quint64 checkForUpdate();
    //! Re-reads this process's -disableupdatecheck at once and the node's on
    //! the serial lane; the About window calls it each time it opens.
    //! Unattached, it does nothing.
    Q_INVOKABLE void refreshUpdateCheck();

    int marker() const { return m_marker; }
    QString fullVersion() const { return m_full_version; }
    QString copyrightText() const { return m_copyright_text; }
    bool updateCheckAvailable() const { return m_available; }
    QString updateCheckDisabledReason() const { return m_disabled_reason; }
    bool updateCheckRunning() const { return m_pending_request_id != 0; }
    quint64 pendingRequestId() const { return m_pending_request_id; }

signals:
    void fullVersionChanged();
    void updateCheckAvailableChanged();
    void updateCheckRunningChanged();
    //! A check finished. An empty result reads as the configuration reason
    //! when the node's setting disables the check, else as no response.
    void updateCheckFinished(quint64 id, const QString& version, const QString& details, int upgradeType);

private:
    struct HydrateResult;
    struct SettingResult;
    struct CheckResult;

    void deliverHydrate(uint64_t epoch, const HydrateResult& result);
    void postSettingRead();
    void deliverSettingRead(uint64_t epoch, const SettingResult& result);
    void deliverCheck(uint64_t epoch, quint64 id, const CheckResult& result);
    //! Recomputes the availability and the reason; emits on a change.
    void updateAvailability();

    int m_marker;
    interfaces::Node* m_node{nullptr};
    QmlCallRunner* m_runner{nullptr};
    //! Moved by every detach(); a delivery of an earlier epoch does nothing.
    uint64_t m_epoch{0};

    QString m_full_version;
    QString m_copyright_text;
    bool m_mainnet{false};
    //! This process's -disableupdatecheck.
    bool m_gui_disabled{false};
    //! The node's disableupdatecheck setting.
    bool m_node_disabled{false};
    bool m_available{false};
    QString m_disabled_reason;
    quint64 m_next_request_id{0};
    quint64 m_pending_request_id{0};
    bool m_setting_in_flight{false};
    bool m_setting_dirty{false};
};

static_assert(!std::is_default_constructible_v<AboutInfo>,
              "Qt 6.4 prefers new T over create() for a default-constructible singleton");

#endif // BITCOIN_QT_QUICK_ABOUTINFO_H
