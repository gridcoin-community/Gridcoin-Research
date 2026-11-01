// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_NETBASE_H
#define BITCOIN_NETBASE_H

#include <string>
#include <vector>

#include "compat.h"
#include "netaddress.h"
#include "serialize.h"

// Network base config, extracted from the former externs to keep this header
// stateless (the state lives in netbase.cpp). Set once from init.
int GetConnectTimeout();
void SetConnectTimeout(int timeout);
bool GetNameLookup();
void SetNameLookup(bool enable);

#ifdef WIN32
// In MSVC, this is defined as a macro, undefine it to prevent a compile and link error
#undef SetPort
#endif

typedef CService proxyType;

enum Network ParseNetwork(std::string net);
bool SetProxy(enum Network net, const CService& addrProxy);
bool GetProxy(enum Network net, proxyType& proxyInfoOut);
bool IsProxy(const CNetAddr& addr);
bool SetNameProxy(const CService& addrProxy);
bool HaveNameProxy();
bool LookupHost(const char *pszName, std::vector<CNetAddr>& vIP, unsigned int nMaxSolutions, bool fAllowLookup);
bool LookupHost(const char *pszName, CNetAddr& addr, bool fAllowLookup);
bool Lookup(const char *pszName, CService& addr, int portDefault, bool fAllowLookup);
bool Lookup(const char *pszName, std::vector<CService>& vAddr, int portDefault, bool fAllowLookup, unsigned int nMaxSolutions);
CService LookupNumeric(const char *pszName, int portDefault = 0);
bool LookupSubNet(const char *pszName, CSubNet& subnet);
bool ConnectSocket(const CService &addr, SOCKET& hSocketRet, int nTimeout = GetConnectTimeout());
bool ConnectSocketByName(CService &addr, SOCKET& hSocketRet, const char *pszDest, int portDefault = 0, int nTimeout = GetConnectTimeout());

#endif
