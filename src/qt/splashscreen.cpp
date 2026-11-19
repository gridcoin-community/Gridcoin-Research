// Copyright (c) 2011-2024 The Bitcoin Core developers
// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/splashscreen.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QRect>
#include <QScreen>

SplashScreen::SplashScreen(QWidget* parent)
    : QWidget(parent)
{
    m_pixmap = QPixmap(":/images/splash");

    // Normal top-level window (NOT Qt::SplashScreen) without a frame, kept above
    // the other windows until the user clicks it -- see the class comment. The
    // minimise hint is there because a frameless window has no minimise button
    // of its own for a window manager to go by.
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowMinimizeButtonHint);

    // A title and an icon for the taskbar entry. The main window sets the
    // application's icon, and it does not exist yet. The application name
    // carries the network, so a testnet splash can be told from a mainnet one.
    setWindowTitle(QCoreApplication::applicationName());
    setWindowIcon(QIcon(QPixmap(":/images/gridcoin")));
    setToolTip(tr("Click to minimize this window. The wallet keeps loading."));

    if (!m_pixmap.isNull()) {
        setFixedSize(m_pixmap.size());
        if (QScreen* screen = QGuiApplication::primaryScreen()) {
            const QRect r(QPoint(), m_pixmap.size());
            move(screen->geometry().center() - r.center());
        }
    }
}

void SplashScreen::finish()
{
    hide();
}

void SplashScreen::mousePressEvent(QMouseEvent* event)
{
    Q_UNUSED(event);

    // Changing the flags of a window hides it, so the flag goes first and the
    // window is then shown minimised.
    if (windowFlags().testFlag(Qt::WindowStaysOnTopHint)) {
        setWindowFlag(Qt::WindowStaysOnTopHint, false);
    }

    showMinimized();
}

void SplashScreen::showMessage(const QString& message, int alignment)
{
    m_message = message;
    m_alignment = alignment;
    update();
}

void SplashScreen::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.drawPixmap(0, 0, m_pixmap);
    if (!m_message.isEmpty()) {
        // Inset a little from the edges so the text is not flush against them,
        // matching the old QSplashScreen bottom-centered status line.
        const QRect r = rect().adjusted(5, 5, -5, -5);
        painter.setPen(m_color);
        painter.drawText(r, m_alignment, m_message);
    }
}
