// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_INIT_H
#define BITCOIN_INIT_H

#include "wallet/wallet.h"
#include <boost/thread.hpp>

#include <string>
#include <utility>
#include <vector>

//! Default value for -daemon option
static constexpr bool DEFAULT_DAEMON = false;
//! Default value for -daemonwait option
static constexpr bool DEFAULT_DAEMONWAIT = false;

extern CWallet* pwalletMain;

void InitLogging();

void StartShutdown();

void Shutdown(void* parg);
bool AppInit2(ThreadHandlerPtr threads);
void ThreadAppInit2(ThreadHandlerPtr th);

void AddLoggingArgs(ArgsManager& argsman);

/**
 * Register all arguments with the ArgsManager
 */
void SetupServerArgs();

std::string VersionMessage();
std::string LogSomething();

//! Apply the immediate side effect of a changed read-write setting for the
//! "push-model" knobs whose consumers do not re-read gArgs live (currently
//! -proxy and -reservebalance apply here; -upnp restarts the port-map thread).
//! No-op for pull-model settings (the staking cluster etc., which re-read gArgs
//! on use). Shared by the changesettings RPC and interfaces::Node::changeSettings
//! so a live edit takes effect without a restart in both paths.
void ApplyRwSettingSideEffect(const std::string& name);

//! Validate, persist to gridcoinsettings.json, force-set into the running args,
//! and apply one or more settings given as name/value strings. Two-phase: every
//! setting is fully validated (name is a known arg; proxy/reservebalance values
//! must parse) before any is applied, so a validation failure changes nothing.
//! An EMPTY value erases the setting (unset → default), which is how "off"/default
//! is expressed for knobs like -proxy/-reservebalance and avoids persisting a
//! value that would fail on restart. On success, each name is categorized into
//! no_change_out / immediate_out / requires_restart_list_out and
//! requires_restart_out is set. On failure returns false with error_out set and
//! invalid_input_out distinguishing a caller/validation error (true; nothing was
//! changed) from an internal storage error (false; the settings file write
//! failed and some earlier settings in the batch may already be applied). Shared
//! core of the changesettings RPC and interfaces::Node::changeSettings.
bool ChangeSettings(const std::vector<std::pair<std::string, std::string>>& settings,
                    bool& requires_restart_out,
                    std::vector<std::string>& no_change_out,
                    std::vector<std::string>& immediate_out,
                    std::vector<std::string>& requires_restart_list_out,
                    bool& invalid_input_out,
                    std::string& error_out);

extern bool fResetBlockchainRequest;
#endif
