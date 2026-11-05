// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_INIT_H
#define BITCOIN_INIT_H

#include "wallet/wallet.h"
#include <boost/thread.hpp>

#include <atomic>
#include <optional>
#include <string>
#include <utility>
#include <vector>

//! Default value for -daemon option
static constexpr bool DEFAULT_DAEMON = false;
//! Default value for -daemonwait option
static constexpr bool DEFAULT_DAEMONWAIT = false;

extern CWallet* pwalletMain;

void InitLogging();

//! \brief The private key behind the regtest genesis premine.
//!
//! The regtest genesis coinbase pays 10 UTXOs to the P2PKH of the secp256k1
//! privkey-scalar=1 compressed pubkey. Exposed so that test fixtures can sign
//! spends of the premine without restating the constant.
CKey GetRegtestPremineKey();

//! \brief Plant the regtest premine key into \p pwallet and surface its coins.
//!
//! Idempotent: does nothing if the wallet already holds the key. On a fresh
//! plant it also nudges nTimeFirstKey below the genesis timestamp and rescans
//! from pindexGenesisBlock, because genesis is loaded before the key exists and
//! the wallet-birthday optimisation would otherwise skip it.
//!
//! Callers must NOT hold cs_main or pwallet->cs_wallet; both are taken here.
void PlantRegtestPremineKey(CWallet* pwallet);

void StartShutdown();

//! Appends a generated rpcuser and rpcpassword, whichever is missing, to the existing
//! configuration file, when this account owns that file (on Windows, always). AppInit2
//! calls it when an RPC server is being started.
void AddMissingRpcCredentials();
//! Installs the SIGTERM and SIGINT handlers that request a shutdown (POSIX; nothing on
//! Windows, whose console handler AppInit2 installs). The daemon calls it at the top of
//! main(), so that a stop sent during startup is not lost; AppInit2 calls it again.
void RegisterShutdownSignalHandlers();

void Shutdown(void* parg);
bool AppInit2(ThreadHandlerPtr threads);
void ThreadAppInit2(ThreadHandlerPtr th);

void AddLoggingArgs(ArgsManager& argsman);

//! The options that belong to the GUI process only (the literal list in init.cpp).
const std::vector<std::string>& GuiOnlyArgs();

//! Removes GuiOnlyArgs() and "-ipclogtrace" from every section of the read-only config layer, so in multiprocess
//! mode the GUI takes them only from its command line or the GUI settings. Logs once per removed key. Returns the
//! removed keys, each with its leading '-' and listed once however many sections held it.
std::vector<std::string> IgnoreGuiOnlyConfigArgs(ArgsManager& args);

//! The message to show when the multiprocess GUI is started with an option it refuses: -resetblockchaindata from
//! any source (the core does that job). Otherwise nullopt.
std::optional<std::string> MultiprocessGuiArgError(const ArgsManager& args);

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

//! Whether a -paytxfee rate read at startup, from the config file, the command
//! line or gridcoinsettings.json, is high enough to warn about: above
//! MaxTxFeeRate(), the most settxfee and changesettings accept. The rate is used
//! as given either way.
bool PayTxFeeWarnsAtStartup(CAmount rate);

//! Validate, persist to gridcoinsettings.json, set in the running args, and apply
//! one or more settings given as name/value strings. Two-phase: every setting is
//! fully validated (name is a known arg; proxy/reservebalance values must parse)
//! before any is applied, so a validation failure changes nothing. An EMPTY value
//! erases the setting, from gridcoinsettings.json and from the running args, so
//! the arg falls back to the command line, then the config file, then its default.
//! It does not mean "off" or zero: a -proxy or -reservebalance set in the config
//! file applies again. On success, each name is categorized into
//! no_change_out / immediate_out / requires_restart_list_out and
//! requires_restart_out is set. On failure returns false with error_out set and
//! invalid_input_out distinguishing a caller/validation error (true; nothing was
//! changed) from an internal storage error (false).
//!
//! Each setting has a class (ArgsManager::GetSettingClass). A Free setting changes
//! freely. An Elevate setting needs HoldsElevation(): a full unlock, or the
//! passphrase capability of the request. When it is missing the call fails with
//! elevation_required_out set (and invalid_input_out false) before anything is
//! applied. A Refuse setting is refused with a value; an empty value erases it
//! from the settings file and is reported in no_change_out. On a storage failure the
//! failing setting is unchanged in the settings file, in memory and in the
//! running args. Settings processed before it (in name order, not the order
//! given) stay applied and stored. The rest are not touched. Shared core of the
//! changesettings RPC and interfaces::Node::changeSettings.
bool ChangeSettings(const std::vector<std::pair<std::string, std::string>>& settings,
                    bool& requires_restart_out,
                    std::vector<std::string>& no_change_out,
                    std::vector<std::string>& immediate_out,
                    std::vector<std::string>& requires_restart_list_out,
                    bool& invalid_input_out,
                    bool& elevation_required_out,
                    std::string& error_out);

extern bool fResetBlockchainRequest;

//! Node-lifecycle flags, defined in init.cpp (moved from main.cpp, issue
//! #3125 C9): fQtActive is set by the Qt entry point before core init so
//! shutdown paths know a GUI owns the event loop;
//! bGridcoinCoreInitComplete flips once AppInit2 finishes and gates
//! interfaces::Init::isCoreReady (#3170) plus the GUI splash handoff.
extern bool fQtActive;
extern std::atomic<bool> bGridcoinCoreInitComplete;

#endif
