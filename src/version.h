// Copyright (c) 2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_VERSION_H
#define BITCOIN_VERSION_H

#include "config/gridcoin-config.h"

// Converts the parameter X to a string after macro replacement on X has been performed.
// Don't merge these into one macro!
#define STRINGIZE(X) DO_STRINGIZE(X)
#define DO_STRINGIZE(X) #X

#define COPYRIGHT_STR "2009-2012" STRINGIZE(COPYRIGHT_YEAR) " The Bitcoin Core Developers"

/**
 * bitcoind-res.rc includes this file, but it cannot cope with real c++ code.
 * WINDRES_PREPROC is defined to indicate that its pre-processor is running.
 * Anything other than a define should be guarded below.
 */

#if !defined(WINDRES_PREPROC)

#include <string>
#include <vector>

static const int CLIENT_VERSION =
                           1000000 * CLIENT_VERSION_MAJOR
                         +   10000 * CLIENT_VERSION_MINOR
                         +     100 * CLIENT_VERSION_REVISION
                         +       1 * CLIENT_VERSION_BUILD;

extern const std::string CLIENT_NAME;
extern const std::string CLIENT_BUILD;
extern const std::string CLIENT_DATE;


std::string FormatFullVersion();
std::string FormatSubVersion(const std::string& name, int nClientVersion, const std::vector<std::string>& comments);

#endif // WINDRES_PREPROC

///////////////////////////////////////////////////////////
// network protocol versioning                           //
//                                                       //
static const int PROTOCOL_VERSION =       180326;        //
// disconnect from peers older than this proto version   //
static const int MIN_PEER_PROTO_VERSION = 180325;        //
///////////////////////////////////////////////////////////
// initial proto version, to be increased after          //
// version/verack negotiation                            //
static const int INIT_PROTO_VERSION = 180275;            //
///////////////////////////////////////////////////////////
//
// database format versioning
//
static const int DATABASE_VERSION = 180015;

#endif
