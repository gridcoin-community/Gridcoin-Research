// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_RESEARCHERSUMMARY_H
#define BITCOIN_QT_QUICK_RESEARCHERSUMMARY_H

#include <QtQml/qqmlregistration.h>

#include <QMetaObject>
#include <QObject>
#include <QString>
#include <QTimer>

#include <type_traits>
#include <vector>

class OptionsModel;
class QQmlEngine;
class QJSEngine;
class ResearcherModel;

//! The QML singleton `ResearcherSummary`: the researcher mode, CPID,
//! magnitude, status, pending accrual and beacon state, as the Widgets
//! overview and status bar show them. It is owned by QmlProvider, which hands
//! QML this instance through create(). Every value comes from
//! ResearcherModel's cached snapshot, so it reads no node and posts nothing;
//! the mask state and the display unit come from the options model, because
//! ResearcherModel holds none.
class ResearcherSummary : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ResearcherSummary)
    QML_SINGLETON
    Q_PROPERTY(int marker READ marker CONSTANT)
    Q_PROPERTY(Mode mode READ mode NOTIFY summaryChanged)
    Q_PROPERTY(QString modeText READ modeText NOTIFY summaryChanged)
    Q_PROPERTY(QString cpid READ cpid NOTIFY summaryChanged)
    Q_PROPERTY(bool showMagnitude READ showMagnitude NOTIFY summaryChanged)
    Q_PROPERTY(QString magnitudeText READ magnitudeText NOTIFY summaryChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY summaryChanged)
    Q_PROPERTY(QString accrualText READ accrualText NOTIFY summaryChanged)
    Q_PROPERTY(bool accrualNearLimit READ accrualNearLimit NOTIFY summaryChanged)
    Q_PROPERTY(bool beaconVisible READ beaconVisible NOTIFY summaryChanged)
    Q_PROPERTY(bool beaconActive READ beaconActive NOTIFY summaryChanged)
    Q_PROPERTY(QString beaconToolTip READ beaconToolTip NOTIFY summaryChanged)

public:
    //! OverviewPage::updateResearcherStatus's three modes.
    enum Mode { StakingOnly, Pool, Solo };
    Q_ENUM(Mode)

    //! `marker` identifies the provider that owns this instance.
    explicit ResearcherSummary(int marker);

    //! The singleton factory: hands QML the installed provider's instance with
    //! C++ ownership. Halts the process when no provider is installed.
    static ResearcherSummary* create(QQmlEngine* qml_engine, QJSEngine* js_engine);

    //! Computes every value from the model's snapshot and follows its signals,
    //! and the options model's mask and unit; a null `options` means unmasked
    //! in GRC. The models must outlive the matching detach().
    void attach(ResearcherModel& researcher, OptionsModel* options);
    //! Disconnects from both models and stops the beacon-age timer; the values
    //! are kept. A detach() before attach() does nothing.
    void detach();

    int marker() const { return m_marker; }
    Mode mode() const { return m_mode; }
    QString modeText() const { return m_mode_text; }
    QString cpid() const { return m_cpid; }
    bool showMagnitude() const { return m_mode == Solo; }
    QString magnitudeText() const { return m_magnitude_text; }
    QString statusText() const { return m_status_text; }
    QString accrualText() const { return m_accrual_text; }
    bool accrualNearLimit() const { return m_accrual_near_limit; }
    bool beaconVisible() const { return m_beacon_visible; }
    bool beaconActive() const { return m_beacon_active; }
    QString beaconToolTip() const { return m_beacon_tooltip; }

signals:
    void summaryChanged();

private:
    void onMaskValuesChanged(bool masked);
    void onDisplayUnitChanged(int unit);
    //! Recomputes every value; emits summaryChanged once if any changed.
    void refresh();

    int m_marker;
    ResearcherModel* m_researcher{nullptr};
    OptionsModel* m_options{nullptr};
    std::vector<QMetaObject::Connection> m_connections;
    //! Refreshes the beacon tooltip's age, as BitcoinGUI's 60 s timer does.
    QTimer m_beacon_timer;
    bool m_masked{false};
    //! A BitcoinUnits::Unit value.
    int m_unit{0};

    Mode m_mode{StakingOnly};
    QString m_mode_text;
    QString m_cpid;
    QString m_magnitude_text;
    QString m_status_text;
    QString m_accrual_text;
    bool m_accrual_near_limit{false};
    bool m_beacon_visible{false};
    bool m_beacon_active{false};
    QString m_beacon_tooltip;
};

static_assert(!std::is_default_constructible_v<ResearcherSummary>,
              "Qt 6.4 prefers new T over create() for a default-constructible singleton");

#endif // BITCOIN_QT_QUICK_RESEARCHERSUMMARY_H
