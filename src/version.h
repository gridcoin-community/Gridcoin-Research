// Copyright (c) 2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_VERSION_H
#define BITCOIN_VERSION_H

///////////////////////////////////////////////////////////
// network protocol versioning                           //
//                                                       //
static const int PROTOCOL_VERSION =       180326;        //
// disconnect from peers older than this proto version   //
static const int MIN_PEER_PROTO_VERSION = 180326;        //
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
