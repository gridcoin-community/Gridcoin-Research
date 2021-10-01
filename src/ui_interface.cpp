// Copyright (c) 2010-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <ui_interface.h>


bool InitError(const std::string &str)
{
    uiInterface.ThreadSafeMessageBox(str, "Gridcoin", CClientUIInterface::MSG_ERROR);
    return false;
}

void InitWarning(const std::string &str)
{
    uiInterface.ThreadSafeMessageBox(str, "Gridcoin", CClientUIInterface::MSG_WARNING);
}
