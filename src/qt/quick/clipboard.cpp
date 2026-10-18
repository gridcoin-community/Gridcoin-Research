// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/quick/clipboard.h>

#include <qt/quick/qmlprovider.h>

#include <QChar>
#include <QClipboard>
#include <QGuiApplication>
#include <QJSEngine>
#include <QtGlobal>

Clipboard::Clipboard(int marker)
    : QObject(nullptr), m_marker(marker)
{
}

Clipboard* Clipboard::create(QQmlEngine*, QJSEngine*)
{
    QmlProvider* provider = QmlProvider::installed();
    if (!provider) {
        // A programming error that would hand QML a null singleton; qFatal
        // halts in every build.
        qFatal("Clipboard::create: no QmlProvider is installed");
    }
    QJSEngine::setObjectOwnership(&provider->clipboard(), QJSEngine::CppOwnership);
    return &provider->clipboard();
}

void Clipboard::setText(const QString& text)
{
    QString stripped = text;
    stripped.remove(QChar(0x2009));
    m_last_text = stripped;
    if (QClipboard* clipboard = QGuiApplication::clipboard()) clipboard->setText(stripped);
}

QString Clipboard::text() const
{
    QClipboard* clipboard = QGuiApplication::clipboard();
    return clipboard ? clipboard->text() : QString();
}
