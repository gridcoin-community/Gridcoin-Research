// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "ipc/win_attest.h"

#include "ipc/serve_init.h"
#include "tinyformat.h"

#include <algorithm>

#ifdef WIN32
#include "ipc/handshake.h"
#include "logging.h"
#include "random.h"
#include "sync.h"
#include "util/strencodings.h"

#include <array>
#include <cstdlib>
#include <exception>
#include <system_error>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif // WIN32

static_assert(ipc::win::ATTEST_DEADLINE < ipc::IPC_AUTH_DEADLINE,
              "the attestation must finish inside the handshake's own deadline");

#ifdef WIN32
static_assert(ipc::win::INTEGRITY_MEDIUM_RID == static_cast<uint32_t>(SECURITY_MANDATORY_MEDIUM_RID));
static_assert(ipc::win::IMPERSONATION_IDENTIFICATION == static_cast<uint32_t>(SecurityIdentification));
// The right a peer must never hold on the pipe is the one FILE_APPEND_DATA names on a
// file, and FILE_GENERIC_WRITE includes it.
static_assert(FILE_CREATE_PIPE_INSTANCE == FILE_APPEND_DATA);
static_assert((ipc::win::ATTEST_PEER_MASK & FILE_CREATE_PIPE_INSTANCE) == 0);
static_assert((FILE_GENERIC_WRITE & FILE_CREATE_PIPE_INSTANCE) != 0);
#endif

namespace ipc::win {

// The single rights by their bare names: <windows.h>'s macros where it is included,
// util::winacl::mask's constants elsewhere, with the same values.
using namespace util::winacl::mask;

using util::winacl::AceSpec;
using util::winacl::AclSpec;
using util::winacl::FormatSid;

namespace {

//! Whether \p s is exactly \p length lower-case hex digits.
bool IsLowerHex(std::string_view s, size_t length)
{
    if (s.size() != length) return false;

    return std::all_of(s.begin(), s.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

} // namespace

Classification ClassifyTokenFacts(const TokenFacts& facts, const Sid& self, bool attach_group_configured)
{
    const std::string user = FormatSid(facts.user);

    // Every refusal comes before either class, so neither the server's own account
    // nor the attach group admits a token that one of them refuses.
    if (user.empty()) {
        return {PeerClass::Refused, "the peer's token user is not a well-formed SID"};
    }
    if (facts.user == util::winacl::wellknown::AnonymousLogon()) {
        return {PeerClass::Refused, "the peer is ANONYMOUS LOGON (S-1-5-7)"};
    }
    if (facts.is_appcontainer) {
        return {PeerClass::Refused, strprintf("the peer %s runs in an AppContainer", user)};
    }
    if (facts.impersonation_level < IMPERSONATION_IDENTIFICATION) {
        return {PeerClass::Refused, strprintf("the peer %s connected at the anonymous impersonation level, which "
                                              "does not identify it",
                                              user)};
    }
    if (facts.integrity_rid > INTEGRITY_MEDIUM_RID) {
        return {PeerClass::Refused,
                strprintf("the peer %s runs elevated (integrity level 0x%x, above Medium); the wallet must not run as "
                          "administrator in multiprocess mode",
                          user, facts.integrity_rid)};
    }

    if (facts.user == self) {
        return {PeerClass::Owner, strprintf("the peer %s is this account", user)};
    }
    if (attach_group_configured && facts.member_of_attach_group) {
        return {PeerClass::AttachGroup, strprintf("the peer %s is a member of the attach group", user)};
    }

    return {PeerClass::Refused,
            attach_group_configured
                ? strprintf("the peer %s is neither this account (%s) nor a member of the attach group", user,
                            FormatSid(self))
                : strprintf("the peer %s is not this account (%s), and no attach group is configured", user,
                            FormatSid(self))};
}

bool IsValidPipeSuffix(std::string_view s)
{
    return IsLowerHex(s, 2 * ATTEST_SUFFIX_BYTES);
}

bool IsValidNonceHex(std::string_view s)
{
    return IsLowerHex(s, 2 * ATTEST_NONCE_BYTES);
}

std::optional<std::wstring> AttestPipePath(std::string_view suffix)
{
    if (!IsValidPipeSuffix(suffix)) return std::nullopt;

    // The suffix is ASCII, so each character widens to itself.
    std::wstring path(ATTEST_PIPE_PREFIX);
    path.append(suffix.begin(), suffix.end());

    return path;
}

AclSpec AttestPipeAclSpec(const Sid& self, const std::optional<Sid>& attach_group)
{
    AclSpec spec;
    spec.owner = self;
    spec.aces.push_back(AceSpec{self, FILE_ALL_ACCESS});
    if (attach_group) spec.aces.push_back(AceSpec{*attach_group, ATTEST_PEER_MASK});

    return spec;
}

#ifdef WIN32
namespace {

//! Closes a handle on every exit.
struct HandleCloser
{
    void operator()(HANDLE handle) const
    {
        if (handle != nullptr && handle != INVALID_HANDLE_VALUE) ::CloseHandle(handle);
    }
};

using HandleGuard = std::unique_ptr<void, HandleCloser>;

//! Frees a descriptor from MakeSecurityDescriptor on every exit.
struct DescriptorFreer
{
    void operator()(void* sd) const { util::winacl::FreeSecurityDescriptor(sd); }
};

using DescriptorGuard = std::unique_ptr<void, DescriptorFreer>;

//! The rights the client opens the pipe with: read and write it, and read its owner.
//! CreateFileW adds FILE_READ_ATTRIBUTES, which ATTEST_PEER_MASK grants.
constexpr DWORD CLIENT_ACCESS = FILE_READ_DATA | FILE_WRITE_DATA | READ_CONTROL | SYNCHRONIZE;
static_assert((CLIENT_ACCESS & ~ATTEST_PEER_MASK) == 0, "the client asks only for rights a peer is given");

//! \p call and the Win32 \p error as a reason.
std::string Win32Failure(const char* call, DWORD error)
{
    return strprintf("%s failed: %s (%u)", call, std::system_category().message(static_cast<int>(error)), error);
}

//! The milliseconds left until \p expires: zero once it has passed, and never
//! INFINITE.
DWORD MillisecondsUntil(std::chrono::steady_clock::time_point expires)
{
    const auto now = std::chrono::steady_clock::now();
    if (now >= expires) return 0;

    const auto left = std::chrono::ceil<std::chrono::milliseconds>(expires - now).count();
    return static_cast<DWORD>(std::min<decltype(left)>(left, INFINITE - 1));
}

//! Wait until \p expires for the overlapped operation \p ov started on \p pipe, and
//! read how many bytes it moved into \p transferred. When it has not finished by
//! then it is cancelled, and in every case it has completed when this returns, so
//! \p ov and the operation's buffer may be reused or freed. False with \p error set
//! when it failed or ran out of time.
bool CompleteIo(HANDLE pipe, OVERLAPPED& ov, std::chrono::steady_clock::time_point expires, DWORD& transferred,
                std::string& error)
{
    transferred = 0;
    const DWORD waited = ::WaitForSingleObject(ov.hEvent, MillisecondsUntil(expires));
    if (waited != WAIT_OBJECT_0) {
        const DWORD wait_error = ::GetLastError();
        // CancelIoEx fails with ERROR_NOT_FOUND when the operation has just finished;
        // either way the wait below then returns once it has completed.
        ::CancelIoEx(pipe, &ov);
        ::GetOverlappedResult(pipe, &ov, &transferred, TRUE);
        error = waited == WAIT_TIMEOUT ? std::string("the deadline passed")
                                       : Win32Failure("WaitForSingleObject", wait_error);
        return false;
    }
    if (!::GetOverlappedResult(pipe, &ov, &transferred, FALSE)) {
        error = Win32Failure("GetOverlappedResult", ::GetLastError());
        return false;
    }

    return true;
}

//! A copy of the SID \p psid points to, or nullopt when it is not valid.
std::optional<Sid> SidAt(PSID psid)
{
    if (psid == nullptr || !::IsValidSid(psid)) return std::nullopt;

    const auto* const bytes = static_cast<const unsigned char*>(psid);
    return Sid(bytes, bytes + ::GetLengthSid(psid));
}

//! Read \p what of \p token into \p buffer, sized to fit. False with \p error set.
bool QueryToken(HANDLE token, TOKEN_INFORMATION_CLASS what, const char* name, std::vector<unsigned char>& buffer,
                std::string& error)
{
    // The first call sizes the buffer; it fails with ERROR_INSUFFICIENT_BUFFER by design.
    DWORD length = 0;
    ::GetTokenInformation(token, what, nullptr, 0, &length);
    buffer.assign(length ? length : 1, 0);

    if (!::GetTokenInformation(token, what, buffer.data(), static_cast<DWORD>(buffer.size()), &length)) {
        error = Win32Failure(name, ::GetLastError());
        return false;
    }

    return true;
}

//! Read the DWORD-sized \p what of \p token into \p value. False with \p error set.
bool QueryTokenDword(HANDLE token, TOKEN_INFORMATION_CLASS what, const char* name, DWORD& value, std::string& error)
{
    DWORD length = 0;
    if (!::GetTokenInformation(token, what, &value, sizeof(value), &length)) {
        error = Win32Failure(name, ::GetLastError());
        return false;
    }

    return true;
}

//! The facts ClassifyTokenFacts decides on, read from \p token, a client's
//! impersonation token opened for TOKEN_QUERY. nullopt with \p error set.
std::optional<TokenFacts> ReadTokenFacts(HANDLE token, const std::optional<Sid>& attach_group, std::string& error)
{
    TokenFacts facts;
    std::vector<unsigned char> buffer;

    if (!QueryToken(token, TokenUser, "GetTokenInformation(TokenUser)", buffer, error)) return std::nullopt;
    std::optional<Sid> user = SidAt(reinterpret_cast<const TOKEN_USER*>(buffer.data())->User.Sid);
    if (!user) {
        error = "the token's user is not a valid SID";
        return std::nullopt;
    }
    facts.user = std::move(*user);

    DWORD level = 0;
    if (!QueryTokenDword(token, TokenImpersonationLevel, "GetTokenInformation(TokenImpersonationLevel)", level,
                         error)) {
        return std::nullopt;
    }
    facts.impersonation_level = level;

    // The integrity level is the last sub-authority of the token's mandatory label.
    if (!QueryToken(token, TokenIntegrityLevel, "GetTokenInformation(TokenIntegrityLevel)", buffer, error)) {
        return std::nullopt;
    }
    const PSID label = reinterpret_cast<const TOKEN_MANDATORY_LABEL*>(buffer.data())->Label.Sid;
    if (label == nullptr || !::IsValidSid(label) || *::GetSidSubAuthorityCount(label) == 0) {
        error = "the token's integrity label is not a valid SID";
        return std::nullopt;
    }
    facts.integrity_rid = *::GetSidSubAuthority(label, *::GetSidSubAuthorityCount(label) - 1u);

    DWORD appcontainer = 0;
    if (!QueryTokenDword(token, TokenIsAppContainer, "GetTokenInformation(TokenIsAppContainer)", appcontainer,
                         error)) {
        return std::nullopt;
    }
    facts.is_appcontainer = appcontainer != 0;

    if (attach_group) {
        // CheckTokenMembership takes a non-const PSID and only reads it. It counts a
        // deny-only group as no membership.
        Sid group = *attach_group;
        BOOL member = FALSE;
        if (!::CheckTokenMembership(token, group.data(), &member)) {
            error = Win32Failure("CheckTokenMembership", ::GetLastError());
            return std::nullopt;
        }
        facts.member_of_attach_group = member != FALSE;
    }

    return facts;
}

//! Ends the thread's impersonation of the pipe's client when it goes out of scope,
//! on every way out. Made right after ImpersonateNamedPipeClient succeeded.
class RevertOnExit
{
public:
    RevertOnExit() = default;
    RevertOnExit(const RevertOnExit&) = delete;
    RevertOnExit& operator=(const RevertOnExit&) = delete;

    ~RevertOnExit()
    {
        // A thread that cannot stop acting as the client must not run on as the
        // client: whatever it did next would be done in the client's name. This is
        // the one failure in the attestation that stops the process instead of
        // refusing the peer.
        if (!::RevertToSelf()) {
            const DWORD error = ::GetLastError();
            LogPrintf("IPC: RevertToSelf failed after the Windows identity check (error %u); aborting\n", error);
            std::abort();
        }
    }
};

} // namespace

struct AttestServer::State
{
    State(Sid self_in, std::optional<Sid> attach_group_in, std::chrono::milliseconds deadline_in)
        : self(std::move(self_in)), attach_group(std::move(attach_group_in)), deadline(deadline_in)
    {
    }

    enum class Phase : uint8_t
    {
        Idle,  //!< Begin not called yet
        Begun, //!< the pipe is waiting for the client
        Done,  //!< used: Finish was called, or Begin failed
    };

    const Sid self;
    const std::optional<Sid> attach_group;
    const std::chrono::milliseconds deadline;

    //! A leaf: nothing else is locked while it is held.
    Mutex mutex;
    Phase phase GUARDED_BY(mutex){Phase::Idle};
    HandleGuard pipe GUARDED_BY(mutex);
    //! The manual-reset event each overlapped operation on the pipe signals.
    HandleGuard event GUARDED_BY(mutex);
    //! The connect started in Begin. The system writes to it until the connect
    //! completes, so it lives as long as the pipe.
    OVERLAPPED connect_ov GUARDED_BY(mutex){};
    //! Whether that connect is still to be waited out.
    bool connect_pending GUARDED_BY(mutex){false};
    //! The nonce's bytes, and when the client's time runs out.
    std::string nonce GUARDED_BY(mutex);
    std::chrono::steady_clock::time_point expires GUARDED_BY(mutex);

    //! Create the pipe and start the connect. False with \p error set.
    bool Open(const std::wstring& path, std::string& error) EXCLUSIVE_LOCKS_REQUIRED(mutex);

    //! The client's facts once it has connected and written the nonce. nullopt with
    //! \p error set.
    std::optional<TokenFacts> Attest(std::string& error) EXCLUSIVE_LOCKS_REQUIRED(mutex);

    //! Cancel and wait out a pending connect, then close the pipe and the event.
    void Close() EXCLUSIVE_LOCKS_REQUIRED(mutex);
};

bool AttestServer::State::Open(const std::wstring& path, std::string& error)
{
    const DescriptorGuard sd(util::winacl::MakeSecurityDescriptor(AttestPipeAclSpec(self, attach_group)));
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = sd.get();
    sa.bInheritHandle = FALSE;

    // One instance, and only if no pipe of the name exists yet; local clients only.
    const DWORD open_mode = PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED;
    const DWORD pipe_mode = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS;
    const HANDLE handle = ::CreateNamedPipeW(path.c_str(), open_mode, pipe_mode, 1, 64, 64, 0, &sa);
    if (handle == INVALID_HANDLE_VALUE) {
        error = Win32Failure("CreateNamedPipeW", ::GetLastError());
        return false;
    }
    pipe.reset(handle);

    event.reset(::CreateEventW(nullptr, /*bManualReset=*/TRUE, /*bInitialState=*/FALSE, nullptr));
    if (!event) {
        error = Win32Failure("CreateEventW", ::GetLastError());
        return false;
    }

    // Usually the connect is left pending for Finish to wait out. ERROR_PIPE_CONNECTED
    // is a client that connected between the create and the connect, and a success
    // is a connect done at once; either way the pipe is connected, with nothing
    // pending.
    connect_ov = OVERLAPPED{};
    connect_ov.hEvent = event.get();
    if (!::ConnectNamedPipe(pipe.get(), &connect_ov)) {
        const DWORD rc = ::GetLastError();
        if (rc == ERROR_IO_PENDING) {
            connect_pending = true;
        } else if (rc != ERROR_PIPE_CONNECTED) {
            error = Win32Failure("ConnectNamedPipe", rc);
            return false;
        }
    }

    return true;
}

std::optional<TokenFacts> AttestServer::State::Attest(std::string& error)
{
    const HANDLE handle = pipe.get();

    // The connect started in Begin.
    DWORD transferred = 0;
    if (connect_pending) {
        const bool connected = CompleteIo(handle, connect_ov, expires, transferred, error);
        connect_pending = false;
        if (!connected) {
            error = "the client did not connect: " + error;
            return std::nullopt;
        }
    }

    // Exactly the nonce's bytes, which a byte-mode pipe may deliver in parts.
    std::array<unsigned char, ATTEST_NONCE_BYTES> got{};
    size_t have = 0;
    while (have < got.size()) {
        OVERLAPPED ov{};
        ov.hEvent = event.get();
        if (!::ResetEvent(ov.hEvent)) {
            error = Win32Failure("ResetEvent", ::GetLastError());
            return std::nullopt;
        }
        if (!::ReadFile(handle, got.data() + have, static_cast<DWORD>(got.size() - have), nullptr, &ov)) {
            const DWORD rc = ::GetLastError();
            if (rc != ERROR_IO_PENDING) {
                error = "the client did not write the nonce: " + Win32Failure("ReadFile", rc);
                return std::nullopt;
            }
        }
        if (!CompleteIo(handle, ov, expires, transferred, error)) {
            error = "the client did not write the nonce: " + error;
            return std::nullopt;
        }
        if (transferred == 0) {
            error = "the client wrote nothing";
            return std::nullopt;
        }
        have += transferred;
    }

    DWORD more = 0;
    if (!::PeekNamedPipe(handle, nullptr, 0, nullptr, &more, nullptr)) {
        error = Win32Failure("PeekNamedPipe", ::GetLastError());
        return std::nullopt;
    }
    if (more != 0) {
        error = "the client wrote more than the nonce";
        return std::nullopt;
    }
    if (!ConstantTimeEqual(std::string(got.begin(), got.end()), nonce)) {
        error = "the client wrote another nonce";
        return std::nullopt;
    }

    // The pipe now holds the security context of the client that wrote the nonce.
    // Impersonate it only to open its token, and nothing else, then stop.
    if (!::ImpersonateNamedPipeClient(handle)) {
        error = Win32Failure("ImpersonateNamedPipeClient", ::GetLastError());
        return std::nullopt;
    }
    HANDLE raw_token = nullptr;
    DWORD open_error = ERROR_SUCCESS;
    {
        const RevertOnExit revert;
        // The token is opened as this process (OpenAsSelf): at the identification
        // level the client's own context could not open it.
        if (!::OpenThreadToken(::GetCurrentThread(), TOKEN_QUERY, /*OpenAsSelf=*/TRUE, &raw_token)) {
            open_error = ::GetLastError();
        }
    }
    if (open_error != ERROR_SUCCESS) {
        error = Win32Failure("OpenThreadToken", open_error);
        return std::nullopt;
    }
    const HandleGuard token(raw_token);

    return ReadTokenFacts(token.get(), attach_group, error);
}

void AttestServer::State::Close()
{
    if (pipe && connect_pending) {
        ::CancelIoEx(pipe.get(), &connect_ov);
        DWORD transferred = 0;
        ::GetOverlappedResult(pipe.get(), &connect_ov, &transferred, TRUE);
        connect_pending = false;
    }
    pipe.reset();
    event.reset();
}

AttestServer::AttestServer(Sid self, std::optional<Sid> attach_group, std::chrono::milliseconds deadline)
    : m_state(std::make_unique<State>(std::move(self), std::move(attach_group), deadline))
{
}

AttestServer::~AttestServer()
{
    LOCK(m_state->mutex);
    m_state->Close();
}

std::optional<std::pair<std::string, std::string>> AttestServer::Begin()
{
    // Drawn before the lock is taken, which keeps the mutex a leaf.
    std::array<unsigned char, ATTEST_SUFFIX_BYTES> suffix_bytes{};
    std::array<unsigned char, ATTEST_NONCE_BYTES> nonce_bytes{};
    GetStrongRandBytes(suffix_bytes);
    GetStrongRandBytes(nonce_bytes);

    State& state = *m_state;
    LOCK(state.mutex);
    if (state.phase != State::Phase::Idle) {
        LogPrintf("IPC: the Windows identity check was begun a second time; refused\n");
        return std::nullopt;
    }
    // Used from here on, whether or not the pipe can be made.
    state.phase = State::Phase::Done;

    std::string error;
    try {
        std::string suffix = HexStr(suffix_bytes);
        const std::optional<std::wstring> path = AttestPipePath(suffix);
        if (!path) {
            error = "the pipe's name is not valid";
        } else if (state.Open(*path, error)) {
            std::pair<std::string, std::string> challenge{std::move(suffix), HexStr(nonce_bytes)};
            state.nonce.assign(nonce_bytes.begin(), nonce_bytes.end());
            state.expires = std::chrono::steady_clock::now() + state.deadline;
            state.phase = State::Phase::Begun;
            return challenge;
        }
    } catch (const std::exception& e) {
        error = e.what();
    }

    state.Close();
    LogPrintf("IPC: could not begin the Windows identity check: %s\n", error);
    return std::nullopt;
}

std::optional<TokenFacts> AttestServer::Finish()
{
    State& state = *m_state;
    LOCK(state.mutex);
    if (state.phase != State::Phase::Begun) {
        LogPrintf("IPC: the Windows identity check was finished without being begun, or a second time; refused\n");
        return std::nullopt;
    }
    state.phase = State::Phase::Done;

    std::string error;
    std::optional<TokenFacts> facts;
    try {
        facts = state.Attest(error);
    } catch (const std::exception& e) {
        error = e.what();
    }
    state.Close();

    if (!facts) LogPrintf("IPC: the Windows identity check failed: %s\n", error);
    return facts;
}

std::optional<AttestClientPipe> AttestClientPipe::Open(const std::string& suffix, const Sid& expected_server,
                                                       std::string& error)
{
    const std::optional<std::wstring> path = AttestPipePath(suffix);
    if (!path) {
        error = "the node named an identity-check pipe that is not 32 lower-case hex digits";
        return std::nullopt;
    }

    // At the identification level the server can read this process's token but not
    // act with it.
    const HANDLE handle = ::CreateFileW(path->c_str(), CLIENT_ACCESS, 0, nullptr, OPEN_EXISTING,
                                        SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        error = "could not open the node's identity-check pipe: " + Win32Failure("CreateFileW", ::GetLastError());
        return std::nullopt;
    }
    HandleGuard guard(handle);

    // The owner is read through the handle just opened, so it is the owner of the
    // pipe this process is connected to, and it is checked before anything is
    // written: the server can identify this process only after it has read from the
    // pipe.
    std::optional<Sid> owner;
    try {
        owner = util::winacl::ReadAcl(handle, /*kernel_object=*/true).owner;
    } catch (const std::exception& e) {
        error = strprintf("could not read the owner of the node's identity-check pipe: %s", e.what());
        return std::nullopt;
    }
    if (!owner) {
        error = "the node's identity-check pipe has no owner that could be read";
        return std::nullopt;
    }
    if (*owner != expected_server) {
        error = strprintf("the node's identity-check pipe is owned by %s, not by the expected node account %s",
                          FormatSid(*owner), FormatSid(expected_server));
        return std::nullopt;
    }

    return AttestClientPipe(guard.release());
}

bool AttestClientPipe::WriteNonce(const std::string& nonce_hex, std::string& error)
{
    if (m_handle == nullptr) {
        error = "the identity-check pipe is not open";
        return false;
    }
    if (!IsValidNonceHex(nonce_hex)) {
        error = "the node sent an identity-check nonce that is not 64 lower-case hex digits";
        return false;
    }

    const std::vector<unsigned char> nonce = ParseHex(nonce_hex);
    size_t written = 0;
    while (written < nonce.size()) {
        DWORD n = 0;
        if (!::WriteFile(m_handle, nonce.data() + written, static_cast<DWORD>(nonce.size() - written), &n, nullptr)) {
            error = "could not write to the node's identity-check pipe: " + Win32Failure("WriteFile", ::GetLastError());
            return false;
        }
        if (n == 0) {
            error = "the node's identity-check pipe took nothing";
            return false;
        }
        written += n;
    }

    return true;
}

AttestClientPipe::~AttestClientPipe()
{
    if (m_handle != nullptr) ::CloseHandle(m_handle);
}

AttestClientPipe::AttestClientPipe(AttestClientPipe&& other) noexcept : m_handle(std::exchange(other.m_handle, nullptr))
{
}

AttestClientPipe& AttestClientPipe::operator=(AttestClientPipe&& other) noexcept
{
    if (this != &other) {
        if (m_handle != nullptr) ::CloseHandle(m_handle);
        m_handle = std::exchange(other.m_handle, nullptr);
    }

    return *this;
}
#endif // WIN32

} // namespace ipc::win
