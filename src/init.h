// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_INIT_H
#define BITCOIN_INIT_H

#include "wallet/wallet.h"
#include <boost/thread.hpp>

extern CWallet* pwalletMain;

void InitLogging();

void StartShutdown();

bool ShutdownRequested();

void Shutdown(void* parg);
bool AppInit2(ThreadHandlerPtr threads);
void ThreadAppInit2(ThreadHandlerPtr th);

std::string HelpMessage();
std::string VersionMessage();
std::string LogSomething();

extern bool fSnapshotRequest;
#endif
