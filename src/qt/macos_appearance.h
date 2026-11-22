// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_MACOS_APPEARANCE_H
#define BITCOIN_QT_MACOS_APPEARANCE_H

//! Draw all of the application's windows, title bars and native panels included, in the light or dark
//! appearance. The application then no longer follows the appearance set in System Settings. Qt rebuilds
//! its palette to match.
void SetMacAppearance(bool dark);

#endif // BITCOIN_QT_MACOS_APPEARANCE_H
