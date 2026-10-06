// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_QMLFONTS_H
#define BITCOIN_QT_QUICK_QMLFONTS_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

//! The UI font family of the QML front end. On Apple platforms it is
//! system_family, the platform's own system font, which the OS displays and
//! the application never ships; everywhere else it is Inter, which the
//! application bundles.
QString ChooseUiFontFamily(bool apple_platform, const QString& system_family);

//! The files LoadQmlFonts() registers, sorted: every file under the :/fonts/
//! resource directory except the font licence texts (file names starting
//! "ofl-", in any case).
QStringList QmlFontPaths();

//! Registers every file under the :/fonts/ resource directory with
//! QFontDatabase, except the font licence texts (file names starting "ofl-",
//! in any case), and returns how many it registered; a file it cannot register
//! is logged. Call it once the application object exists and before the QML
//! root loads.
int LoadQmlFonts();

//! The Fonts singleton of Gridcoin.Qml. uiFamily is ChooseUiFontFamily() for
//! the platform this build targets, with QFontDatabase's general system font.
//! A constant with no state of its own, so each engine creates its own.
class FontsAdapter : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Fonts)
    QML_SINGLETON
    Q_PROPERTY(QString uiFamily READ uiFamily CONSTANT)

public:
    explicit FontsAdapter(QObject* parent = nullptr);
    QString uiFamily() const;

private:
    QString m_ui_family;
};

#endif // BITCOIN_QT_QUICK_QMLFONTS_H
