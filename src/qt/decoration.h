// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_DECORATION_H
#define BITCOIN_QT_DECORATION_H

#include <QtGlobal>

QT_BEGIN_NAMESPACE
class QIcon;
class QPaintDevice;
class QPixmap;
class QSize;
class QString;
class QWidget;
QT_END_NAMESPACE

namespace GRC {
//!
//! \brief Set a widget's scaled font size in integer points.
//!
//! This accommodates macOS which scales a point unit with a different base DPI
//! than other operating systems.
//!
void ScaleFontPointSize(QWidget* widget, int point_size);

//!
//! \brief Set a widget's scaled font size in floating-point points.
//!
//! This accommodates macOS which scales a point unit with a different base DPI
//! than other operating systems.
//!
void ScaleFontPointSizeF(QWidget* widget, double point_size);

//!
//! \brief Scale a pixel value according to the OS base DPI and any DPI scaling.
//!
int ScalePx(QPaintDevice* painter, int px);

//!
//! \brief Scale pixel values according to the OS base DPI and any DPI scaling.
//!
QSize ScaleSize(QPaintDevice* painter, int width, int height);

//!
//! \brief Scale pixel values according to the OS base DPI and any DPI scaling.
//!
QSize ScaleSize(QPaintDevice* painter, int size);

//!
//! \brief Create an image for an icon according to the OS base DPI and any
//! DPI scaling.
//!
QPixmap ScaleIcon(QPaintDevice* painter, const QIcon& icon, int size);

//!
//! \brief Create an image for an icon according to the OS base DPI and any
//! DPI scaling.
//!
QPixmap ScaleIcon(QPaintDevice* painter, const QString& icon_path, int size);

//!
//! \brief Create an image for a status bar icon according to the OS base DPI
//! and any DPI scaling.
//!
QPixmap ScaleStatusIcon(QPaintDevice* painter, const QIcon& icon);

//!
//! \brief Create an image for a status bar icon according to the OS base DPI
//! and any DPI scaling.
//!
QPixmap ScaleStatusIcon(QPaintDevice* painter, const QString& icon_path);
} // namespace GRC

#endif // BITCOIN_QT_DECORATION_H
