// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_RECENTTRANSACTIONS_H
#define BITCOIN_QT_QUICK_RECENTTRANSACTIONS_H

#include <QtQml/qqmlregistration.h>

#include <QByteArray>
#include <QHash>
#include <QIdentityProxyModel>
#include <QMetaObject>
#include <QString>
#include <QVariant>

#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>

class OptionsModel;
class OverviewTxModel;
class QmlCallRunner;
class QQmlEngine;
class QJSEngine;
class WalletModel;

//! The QML singleton `RecentTransactions`: the overview's recent transaction
//! list, a proxy over an OverviewTxModel (the VIEW_OVERVIEW cursor) that the
//! adapter builds at attach and destroys at detach. It is owned by
//! QmlProvider, which hands QML this instance through create(). It serves
//! exactly seven roles, so QML never reaches a role that looks up an address
//! label in the wallet.
//!
//! The OverviewTxModel calls the transaction source on the GUI thread: its
//! constructor registers the view and reads the first rows, a newer Reset
//! reads them again, and its destructor unregisters the view. The served-row
//! limit this adapter sets runs on the call runner's serial lane.
class RecentTransactions : public QIdentityProxyModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RecentTransactions)
    QML_SINGLETON
    Q_PROPERTY(int marker READ marker CONSTANT)
    Q_PROPERTY(bool masked READ masked NOTIFY maskedChanged)

public:
    enum Role {
        IconSourceRole = Qt::UserRole + 1,
        AmountRole,
        AmountTextRole,
        NegativeRole,
        AddressRole,
        DateRole,
        ConfirmedRole,
    };

    //! `marker` identifies the provider that owns this instance.
    explicit RecentTransactions(int marker);
    ~RecentTransactions() override;

    //! The singleton factory: hands QML the installed provider's instance with
    //! C++ ownership. Halts the process when no provider is installed.
    static RecentTransactions* create(QQmlEngine* qml_engine, QJSEngine* js_engine);

    //! Builds the OverviewTxModel over `wallet`'s transaction source and serves
    //! it; follows the options model's mask state and unit (a null `options`
    //! means unmasked in GRC). The models and the runner must outlive the
    //! matching detach().
    void attach(WalletModel& wallet, OptionsModel* options, QmlCallRunner& runner);
    //! Stops serving and destroys the OverviewTxModel, so its destructor
    //! unregisters the view while the transaction source is alive. A detach()
    //! before attach() does nothing.
    void detach();

    //! The list shows `n` rows. Applies OverviewPage::updateTransactions'
    //! stairstep to this adapter's own served-row limit and posts a changed
    //! limit on the serial lane; unattached, it only records `n`.
    Q_INVOKABLE void setVisibleRows(int n);

    int marker() const { return m_marker; }
    bool masked() const { return m_masked; }

    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void maskedChanged();

private:
    void onMaskValuesChanged(bool masked);
    void onDisplayUnitChanged(int unit);
    void postLimit();
    void deliverLimit(uint64_t epoch);

    int m_marker;
    WalletModel* m_wallet{nullptr};
    OptionsModel* m_options{nullptr};
    QmlCallRunner* m_runner{nullptr};
    std::unique_ptr<OverviewTxModel> m_source;
    std::vector<QMetaObject::Connection> m_connections;
    //! Moved by every detach(); a delivery of an earlier epoch does nothing.
    uint64_t m_epoch{0};

    bool m_masked{false};
    //! A BitcoinUnits::Unit value.
    int m_unit{0};
    //! The last row count setVisibleRows() was given.
    int m_visible_rows{0};
    //! This adapter's served-row limit; OverviewTxModel's own stays at the
    //! attach value.
    int m_limit{0};
    //! The limit the last setViewLimit post carried.
    int m_posted_limit{0};
    bool m_limit_in_flight{false};
    bool m_limit_dirty{false};
};

static_assert(!std::is_default_constructible_v<RecentTransactions>,
              "Qt 6.4 prefers new T over create() for a default-constructible singleton");

#endif // BITCOIN_QT_QUICK_RECENTTRANSACTIONS_H
