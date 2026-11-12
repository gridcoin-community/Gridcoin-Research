// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

// Windows backend of the peer-check hooks declared in ipc/peercred.h. AF_UNIX on
// Windows exposes no peer credentials, and kj hands the server no socket fd, so
// no peer identity is read here: the DACLs on node.sock and ipc.cookie
// (ipc/win_endpoint.h) decide who may connect, and the identity check on the
// connection (ipc/win_attest.h) identifies the peer. The attach group is resolved
// here, for those DACLs, the check's pipe and ServerPeerPolicy. The build lists
// this file on every platform; elsewhere it compiles to nothing.

#include "ipc/peercred.h"

#ifdef WIN32

#include "ipc/win_endpoint.h"
#include "logging.h"
#include "sync.h"
#include "tinyformat.h"
#include "util/strencodings.h"
#include "util/win_acl.h"
#include "util/syserror.h"

#include <exception>
#include <string>
#include <string_view>
#include <utility>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace ipc {
namespace {

//! The attach group ConfigureAttachGroup resolved. A leaf lock: nothing is
//! called while it is held.
Mutex g_attach_group_mutex;
std::optional<util::winacl::Sid> g_attach_sid GUARDED_BY(g_attach_group_mutex);

//! The local group a name was resolved to, or the operator-facing reason it
//! could not be.
struct ResolvedGroup
{
    std::optional<util::winacl::Sid> sid;
    std::string problem;
};

//! Look \p name up on this computer only (<COMPUTERNAME>\<name>), and accept it only
//! as a local group (SidTypeAlias) found in the computer's own domain. A
//! domain-local group is an alias too, which is why the name is qualified by the
//! computer and may not name a domain itself, and why the domain it was found in is
//! checked.
ResolvedGroup ResolveAttachGroup(const std::string& name)
{
    const std::string shown = SanitizeString(name);

    wchar_t computer[MAX_COMPUTERNAME_LENGTH + 1] = {};
    DWORD length = MAX_COMPUTERNAME_LENGTH + 1;
    if (!::GetComputerNameW(computer, &length)) {
        return {std::nullopt, strprintf("The IPC attach group '%s' could not be looked up: this computer's name "
                                        "cannot be read (Windows error %u).",
                                        shown, static_cast<unsigned>(::GetLastError()))};
    }

    try {
        const std::string computer_name = WideToUtf8(std::wstring_view(computer, length));
        const std::optional<std::string> lookup = win::LocalGroupLookupName(computer_name, name);
        if (!lookup) {
            return {std::nullopt, strprintf("'%s' is not a local group name -ipcattachgroup can take. Give the name of "
                                            "a group on this computer, with no domain or computer before it.",
                                            shown)};
        }

        const std::optional<util::winacl::AccountLookup> account = util::winacl::LookupAccount(Utf8ToWide(*lookup));
        if (!account) {
            return {std::nullopt, strprintf("The IPC attach group '%s' does not exist on this computer.", shown)};
        }

        // Only a local group found among this computer's own accounts: a builtin
        // alias found under the computer's name is not one.
        if (std::optional<std::string> reason = win::AttachGroupAccountRefusal(*account, computer_name)) {
            return {std::nullopt, strprintf("'%s' cannot be the IPC attach group: %s.", shown, *reason)};
        }

        return {account->sid, {}};
    } catch (const std::exception& e) {
        return {std::nullopt, strprintf("The IPC attach group '%s' could not be looked up: %s", shown, AsciiWhat(e))};
    }
}

} // namespace

std::optional<PeerIdentity> ReadPeerIdentity(int fd)
{
    // Nothing can be read from the socket, so there is no identity to return, and
    // the callers' refusal on nullopt holds here too.
    (void)fd;
    return std::nullopt;
}

bool ConfigureAttachGroup(const std::string& name)
{
    std::optional<util::winacl::Sid> sid;
    if (!name.empty()) {
        ResolvedGroup resolved = ResolveAttachGroup(name);
        if (!resolved.sid) {
            LogPrintf("IPC: %s", resolved.problem);
            return false;
        }
        sid = std::move(resolved.sid);
    }

    {
        LOCK(g_attach_group_mutex);
        g_attach_sid = sid;
    }
    if (sid) {
        LogPrintf("IPC: -ipcattachgroup: the local group '%s' (%s) is given the right to connect to the IPC socket "
                  "and to read the cookie\n",
                  SanitizeString(name), util::winacl::FormatSid(*sid));
    }
    return true;
}

std::optional<std::string> AttachGroupProblem(const std::string& name)
{
    if (name.empty()) return std::nullopt;

    ResolvedGroup resolved = ResolveAttachGroup(name);
    if (resolved.sid) return std::nullopt;

    return resolved.problem;
}

namespace win {
std::optional<std::string> AttachGroupSid()
{
    LOCK(g_attach_group_mutex);
    if (!g_attach_sid) return std::nullopt;

    return util::winacl::FormatSid(*g_attach_sid);
}
} // namespace win

} // namespace ipc

#endif // WIN32
