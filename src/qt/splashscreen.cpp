// Copyright (c) 2011-2024 The Bitcoin Core developers
// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/splashscreen.h"

#include <QGuiApplication>
#include <QPainter>
#include <QPaintEvent>
#include <QRect>
#include <QScreen>

SplashScreen::SplashScreen(QWidget* parent)
    : QWidget(parent)
{
    m_pixmap = QPixmap(":/images/splash");

    // Normal top-level window (NOT Qt::SplashScreen), frameless and above the
    // other windows -- see the class comment for the Xwayland rationale.
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);

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
