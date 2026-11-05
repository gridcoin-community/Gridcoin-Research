// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_WIN_ATTEST_H
#define GRIDCOIN_IPC_WIN_ATTEST_H

#include "util/win_acl.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace interfaces {
class Init;
} // namespace interfaces

//! Identifying the peer of a Windows IPC connection through a one-shot named pipe.
//!
//! The server (the core) creates a named pipe with a random name, which only its
//! own account and, when one is configured, the attach group may open, and sends
//! the name's suffix and a random nonce over the IPC connection. The client (the
//! GUI) opens the pipe, checks that its owner is the server account it expects,
//! writes the nonce, and only then asks the server to finish. The server reads the
//! nonce, waiting at most ATTEST_FINISH_WAIT for it. Once the server has read the
//! nonce, the pipe holds the security context of the client that wrote it. The
//! server impersonates that client just long enough to open its token, then reads
//! from the token the facts ClassifyTokenFacts decides on: the user, attach-group
//! membership, integrity level, impersonation level and whether it is an
//! AppContainer.
//!
//! The client opens the pipe at the identification level, so the token the server
//! gets can be read but cannot be used to act as the client.
//!
//! The classifier, the access list and the name checks are pure, so they build and
//! are tested on every platform. The pipe itself needs Windows.
//!
//! Binding to the IPC connection. The check proves who wrote the nonce into the pipe
//! and, to the client, that the pipe belongs to the expected server account. It does
//! not prove that the client's IPC connection is the one the challenge came over.
//! That rests on who can reach the real server at all: a challenge from the real
//! server reaches a client over any other connection only through a process that is
//! itself connected to the real server. In the same-account mode the socket is
//! owner-only, so no other account can connect to it. Wherever other accounts can
//! write the socket, the directory rule in win_endpoint.h keeps it from being
//! replaced between the client's check and its connect, so the client's connection
//! is to the real server. The pipe's name is not secret (pipe names can be listed),
//! and nothing here relies on it being unknown: the access list decides who may open
//! the pipe, and a name already taken makes Begin fail.
//!
//! The handshake that uses the check keeps it to its own connection:
//! - the server makes one AttestServer for each IPC connection, owned by that
//!   connection's serving object, and sends its Begin's suffix and nonce over that
//!   connection alone;
//! - the server accepts that connection's authentication only after that same
//!   AttestServer's Finish returned facts that ClassifyTokenFacts does not refuse;
//!   no other connection's result counts for it;
//! - the client takes the suffix only from the connection it opened, writes the
//!   nonce only after AttestClientPipe::Open's owner check passed, asks the server
//!   to finish only after the nonce is written, keeps the pipe open until the
//!   server has answered, and sends its cookie only when that answer, over the same
//!   connection, is a success.
namespace ipc::win {

using util::winacl::Sid;

//! A Finish called later than this after its Begin is refused. Nothing waits this
//! long. It lies inside IPC_AUTH_DEADLINE, which bounds the whole handshake (checked
//! in win_attest.cpp).
constexpr std::chrono::seconds ATTEST_DEADLINE{10};

//! How long Finish waits for the nonce's bytes. The client has written them before it
//! asks for Finish, so this only covers bytes still on their way.
constexpr std::chrono::milliseconds ATTEST_FINISH_WAIT{1000};

//! Whether a Finish at \p now, for a Begin at \p begun, is in time: no later than
//! \p deadline after it.
bool FinishInTime(std::chrono::steady_clock::time_point begun, std::chrono::steady_clock::time_point now,
                  std::chrono::milliseconds deadline);

//! The nonce's length in bytes. It travels over the IPC connection as twice as many
//! lower-case hex digits, and through the pipe as the bytes themselves.
constexpr size_t ATTEST_NONCE_BYTES = 32;

//! How many random bytes name the pipe. The suffix is twice as many lower-case hex
//! digits.
constexpr size_t ATTEST_SUFFIX_BYTES = 16;

//! The path of every attestation pipe up to its suffix, in this computer's pipe
//! namespace.
inline constexpr wchar_t ATTEST_PIPE_PREFIX[] = LR"(\\.\pipe\gridcoin-attest-)";

namespace detail {
// The single rights by their bare names, which are <windows.h>'s macros of the
// same values in a file that included it first (see util/win_acl.h).
using namespace util::winacl::mask;
constexpr uint32_t ATTEST_PEER = FILE_READ_DATA | FILE_WRITE_DATA | FILE_READ_ATTRIBUTES | READ_CONTROL | SYNCHRONIZE;
} // namespace detail

//! The attach group's rights on the pipe: read and write it, and read its attributes
//! and its owner and DACL. Each right is named on its own, never FILE_GENERIC_WRITE:
//! on a pipe FILE_APPEND_DATA is FILE_CREATE_PIPE_INSTANCE, the right to create an
//! instance of the pipe's name, and FILE_GENERIC_WRITE includes it.
constexpr uint32_t ATTEST_PEER_MASK = detail::ATTEST_PEER;

//! SECURITY_MANDATORY_MEDIUM_RID, the integrity level of an ordinary desktop
//! process, and the only one ClassifyTokenFacts accepts (checked against the SDK in
//! win_attest.cpp).
constexpr uint32_t INTEGRITY_MEDIUM_RID = 0x2000;

//! Whether a process whose token is \p token_elevated (TokenElevation) and runs at
//! \p integrity_rid counts as elevated to the multiprocess GUI, which then refuses
//! to start: its token is elevated, or its integrity level is above Medium, which
//! ClassifyTokenFacts refuses whatever TokenElevation says. A level below Medium
//! does not count: ClassifyTokenFacts refuses that one for being sandboxed.
bool AboveMediumOrElevated(bool token_elevated, uint32_t integrity_rid);

//! SecurityIdentification, the impersonation level at which the server can read a
//! token but not act with it (checked against the SDK in win_attest.cpp).
constexpr uint32_t IMPERSONATION_IDENTIFICATION = 1;

//! What the server read from the token of the client that wrote the nonce.
struct TokenFacts
{
    Sid user;
    //! CheckTokenMembership's verdict for the attach group; false when none is
    //! configured.
    bool member_of_attach_group{false};
    //! The last sub-authority of the token's integrity label, a
    //! SECURITY_MANDATORY_*_RID.
    uint32_t integrity_rid{0};
    //! The token's SECURITY_IMPERSONATION_LEVEL.
    uint32_t impersonation_level{0};
    bool is_appcontainer{false};
};

//! What a peer is to the server.
enum class PeerClass : uint8_t
{
    Owner,       //!< the server's own account
    AttachGroup, //!< a member of the configured attach group
    Refused,
};

//! A peer's class, and the reason for it, worded to be logged.
struct Classification
{
    PeerClass cls{PeerClass::Refused};
    std::string reason;
};

//! The class of the peer whose token gave \p facts, to a server running as \p self.
//! Refused when its user is not a well-formed SID or is ANONYMOUS LOGON (S-1-5-7),
//! when it is an AppContainer, when its impersonation level is below
//! Identification, or when its integrity level is not Medium: below it with a
//! reason that says low integrity (sandboxed), above it with one that says above
//! Medium integrity (elevated or uiAccess). Otherwise Owner when its user is
//! \p self, AttachGroup when \p attach_group_configured and it is a member, and
//! Refused for anyone else.
Classification ClassifyTokenFacts(const TokenFacts& facts, const Sid& self, bool attach_group_configured);

//! Whether \p s is exactly 2 * ATTEST_SUFFIX_BYTES (32) lower-case hex digits.
bool IsValidPipeSuffix(std::string_view s);

//! Whether \p s is exactly 2 * ATTEST_NONCE_BYTES (64) lower-case hex digits.
bool IsValidNonceHex(std::string_view s);

//! ATTEST_PIPE_PREFIX followed by \p suffix, or nullopt unless IsValidPipeSuffix.
//! Both ends build the pipe's path here and only here, so it always names a pipe on
//! this computer, never a path the peer chose.
std::optional<std::wstring> AttestPipePath(std::string_view suffix);

//! The pipe's owner and protected DACL: owner \p self; \p self with FILE_ALL_ACCESS;
//! plus \p attach_group, when set, with ATTEST_PEER_MASK. No entry is inheritable.
util::winacl::AclSpec AttestPipeAclSpec(const Sid& self, const std::optional<Sid>& attach_group);

#ifdef WIN32
//! The server's end of one attestation. Single use: Begin, then Finish, once each.
//! It may be called from more than one thread; a leaf mutex orders the calls.
class AttestServer
{
public:
    //! A server running as \p self, the account that will own the pipe, which must
    //! be this process's token user. \p attach_group, when set, may open the pipe
    //! too. A Finish more than \p deadline after Begin is refused.
    AttestServer(Sid self, std::optional<Sid> attach_group, std::chrono::milliseconds deadline = ATTEST_DEADLINE);
    //! Closes the pipe if Finish has not, which takes its name away. No operation on
    //! the pipe is pending by then: Finish starts and ends each one itself. It must
    //! not run while a Begin or Finish of this object is still running on another
    //! thread; the owner destroys it only after its calls have returned.
    ~AttestServer();

    AttestServer(const AttestServer&) = delete;
    AttestServer& operator=(const AttestServer&) = delete;

    //! Create the pipe, AttestPipePath of a fresh random suffix with
    //! AttestPipeAclSpec(self, attach_group) and one instance, refusing a name that
    //! already exists and remote clients. The client can open it from then on;
    //! nothing is waited for or started on it. Returns {suffix, nonce_hex}, or
    //! nullopt (logged) on any failure or a second call.
    std::optional<std::pair<std::string, std::string>> Begin();

    //! For a client that has already opened the pipe and written the nonce: connect,
    //! read exactly the nonce's bytes, waiting at most ATTEST_FINISH_WAIT for them,
    //! compare them in constant time, then impersonate the client to open its token,
    //! and read the facts from it. Every operation on the pipe is started and ended
    //! on the calling thread, and the pipe is closed before it returns. nullopt
    //! (logged) when no client has opened the pipe, on any failure, when called more
    //! than the deadline after Begin, without a successful Begin, or on a second
    //! call. A failure to stop impersonating aborts the process. It blocks for up
    //! to ATTEST_FINISH_WAIT and impersonates the client on the calling thread, so
    //! it must run on a worker thread, never the IPC event loop.
    std::optional<TokenFacts> Finish();

private:
    struct State;
    std::unique_ptr<State> m_state;
};

//! The client's end of one attestation. Move-only; it closes the pipe when
//! destroyed. Write the nonce before asking the server to finish, and keep it until
//! the server has answered: the server reads the client's identity through this
//! connection.
class AttestClientPipe
{
public:
    //! Open AttestPipePath(\p suffix) to read and write it at the identification
    //! level (SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION), then read the pipe's
    //! owner through the handle opened, and refuse unless it is \p expected_server.
    //! Nothing is written to the pipe here. nullopt with \p error set, naming the
    //! owner when it is another, on any failure or an invalid suffix.
    static std::optional<AttestClientPipe> Open(const std::string& suffix, const Sid& expected_server,
                                                std::string& error);

    //! Write the ATTEST_NONCE_BYTES bytes \p nonce_hex holds, which must pass
    //! IsValidNonceHex. False with \p error set on any failure.
    bool WriteNonce(const std::string& nonce_hex, std::string& error);

    ~AttestClientPipe() = default;
    AttestClientPipe(AttestClientPipe&&) noexcept = default;
    AttestClientPipe& operator=(AttestClientPipe&&) noexcept = default;
    AttestClientPipe(const AttestClientPipe&) = delete;
    AttestClientPipe& operator=(const AttestClientPipe&) = delete;

private:
    explicit AttestClientPipe(util::winacl::UniqueHandle handle) : m_handle(std::move(handle)) {}

    util::winacl::UniqueHandle m_handle;
};

//! The client's half of the check, on the IPC connection whose Init is \p init,
//! before the client sends its cookie: beginAttest(); the pipe it names opened
//! with AttestClientPipe::Open against \p expected_server, which the GUI takes from
//! ExpectedServerSid(); the nonce written; then finishAttest(), with the pipe held
//! open until it has returned. Throws std::runtime_error with the text to show:
//! - "The node did not complete the Windows identity check; use matching builds
//!   of gridcoinresearch and gridcoinresearchd." when beginAttest() throws, as an
//!   older node's does, or answers served=false;
//! - "The node refused this GUI's identity (wrong account, not in Gridcoin GUI
//!   Users, or running elevated). See the node's debug.log." when finishAttest()
//!   returns false;
//! - a text naming the cause when the node named no valid pipe, the pipe could
//!   not be opened or written or has another owner, or finishAttest() throws.
//! Nothing is written to the pipe unless its owner is \p expected_server.
void RunClientAttestation(interfaces::Init& init, const Sid& expected_server);
#endif // WIN32

} // namespace ipc::win

#endif // GRIDCOIN_IPC_WIN_ATTEST_H
