// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/quick/aboutinfo.h>

#include <clientversion.h>
#include <interfaces/node.h>
#include <qt/clientmodel.h>
#include <qt/quick/qmlcallrunner.h>
#include <qt/quick/qmlprovider.h>
#include <util.h>

#include <QCoreApplication>
#include <QJSEngine>

#include <memory>
#include <variant>

namespace {

//! The node setting, and this process's argument, that disable the check.
const char* const DISABLE_SETTING = "disableupdatecheck";
const char* const DISABLE_ARG = "-disableupdatecheck";

//! AboutDialog's constructor's copyright line; untranslated there.
QString CopyrightText()
{
    QString text = QStringLiteral("Copyright 2009-");
    const std::variant<int, QString> copyright_year = COPYRIGHT_YEAR;
    if (const int* year = std::get_if<int>(&copyright_year)) {
        text += QString::number(*year);
    } else if (const QString* year_text = std::get_if<QString>(&copyright_year)) {
        text += *year_text;
    } else {
        text += QStringLiteral("Present");
    }
    text += QStringLiteral(" The Bitcoin/Peercoin/Black-Coin/Gridcoin developers");
    return text;
}

//! AboutDialog::setModel's reason when a flag disables the check.
QString ConfigReason()
{
    return QCoreApplication::translate("AboutDialog", "Version information and update check has been disabled "
                                                      "by config or startup parameter.");
}

} // namespace

//! What the seed read reads. A plain value.
struct AboutInfo::HydrateResult {
    QString full_version;
    bool mainnet{false};
    bool node_disabled{false};
};

//! What a setting read reads.
struct AboutInfo::SettingResult {
    bool node_disabled{false};
};

//! What an update check reads: the result, and the node's setting when the
//! result is empty.
struct AboutInfo::CheckResult {
    interfaces::LatestVersionInfo info;
    bool node_disabled{false};
};

AboutInfo::AboutInfo(int marker)
    : QObject(nullptr), m_marker(marker), m_copyright_text(CopyrightText())
{
    updateAvailability();
}

AboutInfo* AboutInfo::create(QQmlEngine*, QJSEngine*)
{
    QmlProvider* provider = QmlProvider::installed();
    if (!provider) {
        // A programming error that would hand QML a null singleton; qFatal
        // halts in every build.
        qFatal("AboutInfo::create: no QmlProvider is installed");
    }
    QJSEngine::setObjectOwnership(&provider->aboutInfo(), QJSEngine::CppOwnership);
    return &provider->aboutInfo();
}

void AboutInfo::attach(ClientModel& client, QmlCallRunner& runner)
{
    m_node = &client.node();
    m_runner = &runner;
    // A read posted by an earlier attach never delivers.
    m_setting_in_flight = false;
    m_setting_dirty = false;

    // Under -multiprocess this process's arguments never reach the daemon,
    // and the daemon's never reach this process: either one disables the
    // check.
    m_gui_disabled = gArgs.GetBoolArg(DISABLE_ARG, false);
    updateAvailability();

    interfaces::Node& node = *m_node;
    auto result = std::make_shared<HydrateResult>();
    const uint64_t epoch = m_epoch;
    // A closed runner refuses the post; the values then stay at their defaults.
    (void)m_runner->postSerial(
        [&node, result] {
            result->full_version = QString::fromStdString(node.getClientVersion());
            result->mainnet = node.isMainNet();
            result->node_disabled = node.getSettingBool(DISABLE_SETTING, false);
        },
        [this, result, epoch] { deliverHydrate(epoch, *result); });
}

void AboutInfo::detach()
{
    ++m_epoch;
    m_node = nullptr;
    m_runner = nullptr;
}

quint64 AboutInfo::checkForUpdate()
{
    if (!m_node || !m_runner || !m_available || m_pending_request_id != 0) return 0;

    interfaces::Node& node = *m_node;
    auto result = std::make_shared<CheckResult>();
    const uint64_t epoch = m_epoch;
    const quint64 id = m_next_request_id + 1;
    // Blocking network I/O, so the pooled lane. An empty result re-reads the
    // node's setting in the same call: a setting changed at run time is then
    // reported as the configuration reason, not as a network failure.
    if (!m_runner->postPooled(
            [&node, result] {
                result->info = node.checkForLatestUpdate();
                if (result->info.version.empty()) {
                    result->node_disabled = node.getSettingBool(DISABLE_SETTING, false);
                }
            },
            [this, result, epoch, id] { deliverCheck(epoch, id, *result); })) {
        return 0;
    }
    m_next_request_id = id;
    m_pending_request_id = id;
    emit updateCheckRunningChanged();
    return id;
}

void AboutInfo::refreshUpdateCheck()
{
    if (!m_node || !m_runner) return;
    m_gui_disabled = gArgs.GetBoolArg(DISABLE_ARG, false);
    updateAvailability();
    postSettingRead();
}

void AboutInfo::deliverHydrate(uint64_t epoch, const HydrateResult& result)
{
    if (epoch != m_epoch) return;
    if (result.full_version != m_full_version) {
        m_full_version = result.full_version;
        emit fullVersionChanged();
    }
    m_mainnet = result.mainnet;
    m_node_disabled = result.node_disabled;
    updateAvailability();
}

void AboutInfo::postSettingRead()
{
    // Attached only: refreshUpdateCheck checks, and a delivery runs only in
    // the epoch of the attach that posted it.
    if (m_setting_in_flight) {
        m_setting_dirty = true;
        return;
    }

    interfaces::Node& node = *m_node;
    auto result = std::make_shared<SettingResult>();
    const uint64_t epoch = m_epoch;
    if (m_runner->postSerial([&node, result] { result->node_disabled = node.getSettingBool(DISABLE_SETTING, false); },
                             [this, result, epoch] { deliverSettingRead(epoch, *result); })) {
        m_setting_in_flight = true;
        m_setting_dirty = false;
    }
}

void AboutInfo::deliverSettingRead(uint64_t epoch, const SettingResult& result)
{
    if (epoch != m_epoch) return;
    m_setting_in_flight = false;
    m_node_disabled = result.node_disabled;
    updateAvailability();
    if (m_setting_dirty) postSettingRead();
}

void AboutInfo::deliverCheck(uint64_t epoch, quint64 id, const CheckResult& result)
{
    if (epoch != m_epoch) return;

    QString version = QString::fromStdString(result.info.version);
    QString details = QString::fromStdString(result.info.details);
    if (version.isEmpty()) {
        // AboutDialog::versionCheckFinished's text, untranslated there.
        version = result.node_disabled ? ConfigReason()
                                       : QStringLiteral("No response from GitHub - check network connectivity.");
        details.clear();
    }
    emit updateCheckFinished(id, version, details, result.info.upgrade_type);

    if (m_pending_request_id == id) {
        m_pending_request_id = 0;
        emit updateCheckRunningChanged();
    }
}

void AboutInfo::updateAvailability()
{
    // AboutDialog::setModel's gate and tooltips.
    const bool disabled = m_gui_disabled || m_node_disabled;
    const bool available = m_mainnet && !disabled;
    QString reason;
    if (!available) {
        reason = disabled ? ConfigReason()
                          : QCoreApplication::translate("AboutDialog", "Version information is only available on mainnet.");
    }
    if (available == m_available && reason == m_disabled_reason) return;
    m_available = available;
    m_disabled_reason = reason;
    emit updateCheckAvailableChanged();
}
