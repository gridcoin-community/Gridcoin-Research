// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_WINSVC_SETUP_H
#define GRIDCOIN_WINSVC_SETUP_H

#include "winsvc/scm.h"

//! gridcoin-service.exe's install, uninstall and status on Windows.
//!
//! Each prints what it does, one line at a time, and refuses with the reason instead of asking
//! anything. Each returns the tool's exit code: EXIT_OK, or EXIT_REFUSED for a refusal or a
//! failure, whose reason is printed on stderr.
namespace winsvc {

#ifdef WIN32
//! Elevated only. Refuses, before anything is changed, when the daemon next to this program is
//! missing or another account can change it or a directory above it; when the service is
//! installed and \p options.reconfigure is not set; when a directory of the layout that exists is
//! a reparse point, a file, or owned by an account other than Administrators, SYSTEM and
//! TrustedInstaller; and when the network's configuration file exists and is a reparse point, has
//! another owner or access list than the layout's, or has an includeconf line. Then it creates
//! both networks' attach groups if absent, creates the service disabled (or, with --reconfigure,
//! gives the installed one its command line and account and keeps its start type), applies
//! ConfigureService, checks that the service's account has the SID the layout names, creates or
//! re-applies every directory of the layout, writes the network's configuration file when there is
//! none (or adds the lines MissingServiceConfLines gives to one there is), and replaces the
//! network's inbound firewall rule.
int Install(const ToolOptions& options);

//! Elevated only. Refuses while the service is installed and not disabled or not stopped, and
//! while its network data directory holds wallet.dat. Then it deletes the service and its
//! firewall rule and removes the layout's empty directories, bottom-up: the network's own, and,
//! when the other network's service is not installed either, the other network's and the shared
//! ones. A directory that is not empty is kept, and so are the configuration file and the groups.
int Uninstall(const ToolOptions& options);

//! The service's state, start type, account, SID type, required privileges, preshutdown timeout,
//! failure actions and ImagePath, with any difference from what install sets; each layout
//! entry's CheckMatchesSpec result; the configuration file's check; and whose wallet the service
//! holds, from the state file. Runs without elevation too, when entries this account cannot read
//! are reported as such. EXIT_OK only when the service is installed and everything matches.
int Status(const ToolOptions& options);
#endif // WIN32

} // namespace winsvc

#endif // GRIDCOIN_WINSVC_SETUP_H
