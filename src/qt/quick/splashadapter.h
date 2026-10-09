// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_SPLASHADAPTER_H
#define BITCOIN_QT_QUICK_SPLASHADAPTER_H

#include <QtQml/qqmlregistration.h>

#include <QObject>
#include <QString>

#include <type_traits>

class QQmlEngine;
class QJSEngine;

//! The QML singleton `Splash`: start-up progress. It is owned by QmlProvider,
//! which hands QML this instance through create(), so it outlives every
//! engine.
class SplashAdapter : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Splash)
    QML_SINGLETON
    Q_PROPERTY(int marker READ marker CONSTANT)
    Q_PROPERTY(int loaded READ loaded NOTIFY progressChanged)
    Q_PROPERTY(int total READ total NOTIFY progressChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(bool finished READ finished NOTIFY finishedChanged)

public:
    //! `marker` identifies the provider that owns this instance.
    explicit SplashAdapter(int marker);

    //! The singleton factory: hands QML the installed provider's Splash with C++
    //! ownership. Halts the process when no provider is installed.
    static SplashAdapter* create(QQmlEngine* qml_engine, QJSEngine* js_engine);

    int marker() const { return m_marker; }
    int loaded() const { return m_loaded; }
    int total() const { return m_total; }
    QString message() const { return m_message; }
    bool active() const { return m_active; }
    bool finished() const { return m_finished; }

    //! Each setter assigns and emits its NOTIFY signal only when the value
    //! changes.
    void setActive(bool active);
    void setFinished(bool finished);

public Q_SLOTS:
    //! The slot bitcoin.cpp's InitMessage bridge invokes by name. It reads the
    //! progress out of the core's two start-up formats, "<loaded>/<highest>
    //! Blocks Loaded (<n>%)" and "<depth>/<check depth> Blocks Verified" (both
    //! from CTxDB::LoadBlockIndex's InitMessage calls); any other text has no
    //! progress and sets both numbers to 0. `alignment` is ignored.
    void showMessage(const QString& message, int alignment);

signals:
    void progressChanged();
    void messageChanged();
    void activeChanged();
    void finishedChanged();

private:
    int m_marker;
    int m_loaded{0};
    int m_total{0};
    QString m_message;
    bool m_active{false};
    bool m_finished{false};
};

static_assert(!std::is_default_constructible_v<SplashAdapter>,
              "Qt 6.4 prefers new T over create() for a default-constructible singleton");

#endif // BITCOIN_QT_QUICK_SPLASHADAPTER_H
