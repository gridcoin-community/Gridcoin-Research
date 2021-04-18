// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <QtGlobal>

QT_BEGIN_NAMESPACE
class QWidget;
QT_END_NAMESPACE

namespace GRC {
//!
//! \brief Set a widget's scaled font size in integer points.
//!
//! This accomodates macOS which scales a point unit with a different base DPI
//! than other operating systems.
//!
void ScaleFontPointSize(QWidget* widget, int point_size);

//!
//! \brief Set a widget's scaled font size in floating-point points.
//!
//! This accomodates macOS which scales a point unit with a different base DPI
//! than other operating systems.
//!
void ScaleFontPointSizeF(QWidget* widget, double point_size);
} // namespace GRC
