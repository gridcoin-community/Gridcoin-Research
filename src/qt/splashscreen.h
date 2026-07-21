// Copyright (c) 2011-2024 The Bitcoin Core developers
// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_SPLASHSCREEN_H
#define BITCOIN_QT_SPLASHSCREEN_H

#include <QColor>
#include <QPixmap>
#include <QString>
#include <QWidget>

/** Startup splash screen showing the block-loading progress.
 *
 * Intentionally NOT a QSplashScreen. A QSplashScreen has the Qt::SplashScreen
 * window type, which KWin (and other compositors) mishandle for Xwayland
 * clients -- the window is dropped behind the other windows and given a taskbar
 * entry, hiding the block-index progress. A plain top-level QWidget is treated
 * as a normal window (the app's first window gets initial focus and is placed
 * on top), which behaves correctly under Xwayland while still working natively.
 * It keeps the frameless, always-on-top look of the old splash.
 */
class SplashScreen : public QWidget
{
    Q_OBJECT

public:
    explicit SplashScreen(QWidget* parent = nullptr);

    //! Close the splash once the main window is ready.
    void finish();

protected:
    void paintEvent(QPaintEvent* event) override;

public Q_SLOTS:
    //! Show a status message on the splash. Arg types (QString, int) match the
    //! queued uiInterface InitMessage bridge in bitcoin.cpp.
    void showMessage(const QString& message, int alignment);

private:
    QPixmap m_pixmap;
    QString m_message;
    int m_alignment{0};
    QColor m_color{Qt::black};
};

#endif // BITCOIN_QT_SPLASHSCREEN_H
