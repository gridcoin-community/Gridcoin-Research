// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/guifrontend.h>
#include <qt/widgetsfrontend.h>

int main(int argc, char* argv[]) { return GuiMain(argc, argv, MakeWidgetsFrontEnd); }
