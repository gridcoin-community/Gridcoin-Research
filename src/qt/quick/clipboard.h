// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_CLIPBOARD_H
#define BITCOIN_QT_QUICK_CLIPBOARD_H

#include <QtQml/qqmlregistration.h>

#include <QObject>
#include <QString>

#include <type_traits>

class QQmlEngine;
class QJSEngine;

//! The QML singleton `Clipboard`: the system clipboard for QML, which has no
//! clipboard of its own. It is owned by QmlProvider, which hands QML this
//! instance through create(). It reads no model, so it has no attach and is
//! always usable.
class Clipboard : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Clipboard)
    QML_SINGLETON
    Q_PROPERTY(int marker READ marker CONSTANT)

public:
    //! `marker` identifies the provider that owns this instance.
    explicit Clipboard(int marker);

    //! The singleton factory: hands QML the installed provider's instance with
    //! C++ ownership. Halts the process when no provider is installed.
    static Clipboard* create(QQmlEngine* qml_engine, QJSEngine* js_engine);

    //! Writes `text` to the clipboard without its thin spaces (U+2009), the
    //! digit-group separators BitcoinUnits::format inserts.
    Q_INVOKABLE void setText(const QString& text);
    //! The clipboard's text.
    Q_INVOKABLE QString text() const;

    int marker() const { return m_marker; }
    //! The last text setText() wrote (C++ only).
    QString lastText() const { return m_last_text; }

private:
    int m_marker;
    QString m_last_text;
};

static_assert(!std::is_default_constructible_v<Clipboard>,
              "Qt 6.4 prefers new T over create() for a default-constructible singleton");

#endif // BITCOIN_QT_QUICK_CLIPBOARD_H
