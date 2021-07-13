// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/decoration.h>

#include <QFont>
#include <QIcon>
#include <QPaintDevice>
#include <QPixmap>
#include <QSize>
#include <QWidget>

using namespace GRC;

namespace {
//!
//! \brief The pixels-per-inch value that the user interface is designed for.
//!
constexpr int REFERENCE_DPI = 96;

//!
//! \brief The virtual pixels-per-inch of the operating system.
//!
//! MacOS typically uses a different base DPI than Windows or Linux. This
//! causes rendering calculated from a reference DPI to appear smaller on
//! Macs which creates legibility and layout issues.
//!
#ifdef Q_OS_MAC
constexpr int OS_BASE_DPI = 72;
#else
constexpr int OS_BASE_DPI = 96;
#endif

//!
//! \brief The base width and height in pixels for status bar icons.
//!
constexpr int STATUSBAR_ICONSIZE = 16;
} // Anonymous namespace

// -----------------------------------------------------------------------------
// Functions
// -----------------------------------------------------------------------------

void GRC::ScaleFontPointSize(QWidget* widget, int point_size)
{
    if (!widget) {
        return;
    }

    QFont font = widget->font();
    font.setPointSize(point_size * REFERENCE_DPI / OS_BASE_DPI);
    widget->setFont(font);
}

void GRC::ScaleFontPointSizeF(QWidget* widget, double point_size)
{
    if (!widget) {
        return;
    }

    QFont font = widget->font();
    font.setPointSizeF(point_size * REFERENCE_DPI / OS_BASE_DPI);
    widget->setFont(font);
}

int GRC::ScalePx(QPaintDevice* painter, int px)
{
    if (!painter) {
        return px;
    }

    return painter->logicalDpiX() * px / OS_BASE_DPI;
}

QSize GRC::ScaleSize(QPaintDevice* painter, int width, int height)
{
    return QSize(ScalePx(painter, width), ScalePx(painter, height));
}

QSize GRC::ScaleSize(QPaintDevice* painter, int size)
{
    return ScaleSize(painter, size, size);
}

QPixmap GRC::ScaleIcon(QPaintDevice* painter, const QIcon& icon, int size)
{
    const int scaled_size = ScalePx(painter, size);

    return icon.pixmap(scaled_size, scaled_size);
}

QPixmap GRC::ScaleIcon(QPaintDevice* painter, const QString& icon_path, int size)
{
    return ScaleIcon(painter, QIcon(icon_path), size);
}

QPixmap GRC::ScaleStatusIcon(QPaintDevice* painter, const QIcon& icon)
{
    return ScaleIcon(painter, icon, STATUSBAR_ICONSIZE);
}

QPixmap GRC::ScaleStatusIcon(QPaintDevice* painter, const QString& icon_path)
{
    return ScaleIcon(painter, icon_path, STATUSBAR_ICONSIZE);
}
