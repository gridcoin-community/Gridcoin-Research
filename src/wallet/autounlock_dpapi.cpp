// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "wallet/autounlock_dpapi.h"

#include "chainparams.h"
#include "logging.h"
#include "node/shutdown.h"
#include "support/cleanse.h"
#include "tinyformat.h"
#include "util/dir_permissions.h"
#include "util/syserror.h"
#include "util/time.h"
#include "util/win_acl.h"
#include "util/winservice.h"
#include "wallet/autounlock_blob.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <functional>
#include <system_error>
#include <utility>
#include <vector>

#include <windows.h>
#include <wincrypt.h>

namespace {
namespace winacl = util::winacl;

//! The description DPAPI keeps with what it encrypts.
const wchar_t* const DPAPI_DESCRIPTION = L"Gridcoin autounlock";

//! How many times a read that meets a sharing or lock violation is tried in all, and the
//! wait before the second try. Each later wait is twice the one before.
constexpr int FETCH_ATTEMPTS = 3;
constexpr std::chrono::milliseconds FIRST_RETRY_DELAY{1000};

//! What the user is told to do about a file the core will not use.
const char* const SET_UP_AGAIN = "turn autounlock off and set it up again";

//! SetAutoUnlockReadBackHookForTesting()'s hook. Set only by tests.
std::function<void(const fs::path&)> g_read_back_hook;

std::string Display(const fs::path& path)
{
    return fsbridge::LongPathString(path);
}

//! The scope this mode writes and reads.
BlobScope ModeScope()
{
    return util::winsvc::IsRunningAsWindowsService() ? BlobScope::ServiceAccount : BlobScope::LocalMachine;
}

const char* ScopeName(BlobScope scope)
{
    return scope == BlobScope::ServiceAccount ? "service" : "machine";
}

//! What CryptProtectData or CryptUnprotectData wrote, cleansed and freed on every exit.
struct DpapiOutput
{
    DATA_BLOB blob{0, nullptr};

    DpapiOutput() = default;
    DpapiOutput(const DpapiOutput&) = delete;
    DpapiOutput& operator=(const DpapiOutput&) = delete;

    ~DpapiOutput()
    {
        if (blob.pbData != nullptr) {
            memory_cleanse(blob.pbData, blob.cbData);
            ::LocalFree(blob.pbData);
        }
    }
};

//! \p size bytes at \p data as DPAPI input. DPAPI only reads its input but takes it as
//! non-const. Every caller passes at most AUTOUNLOCK_BLOB_MAX_BYTES.
DATA_BLOB InputBlob(const unsigned char* data, size_t size)
{
    DATA_BLOB blob;
    blob.cbData = static_cast<DWORD>(size);
    blob.pbData = const_cast<BYTE*>(data);

    return blob;
}

//! Wait \p delay in short slices, so that a shutdown request ends the wait. False on
//! shutdown.
bool WaitUnlessShutdown(std::chrono::milliseconds delay)
{
    const std::chrono::milliseconds slice{100};

    while (delay.count() > 0) {
        if (ShutdownRequested()) return false;

        const std::chrono::milliseconds step = std::min(delay, slice);
        UninterruptibleSleep(step);
        delay -= step;
    }

    return !ShutdownRequested();
}

enum class FileState
{
    //! In the layout, of this mode's scope and this network, and its own account's alone
    //! when that matters.
    Usable,
    //! Not there.
    Missing,
    //! There, and not usable; reason says why.
    Refused,
};

//! What reading the autounlock file found.
struct CheckedFile
{
    FileState state{FileState::Refused};
    //! What is wrong, worded for the user.
    std::string reason;
    //! Where, and anything more the reason has to say.
    std::string detail;
    //! Whether the refusal is about the file itself: its size, layout, scope, network or
    //! access control list, or those of the directories above it. Such a refusal stands until
    //! autounlock is set up again, or the access control lists are fixed. False when the file
    //! could not be opened or read, which may pass.
    bool definite{false};
    //! What the user is told to do about a definite refusal, when it is not SET_UP_AGAIN.
    std::string fix;
    //! For a usable file, its bytes and what they hold.
    std::vector<unsigned char> bytes;
    AutoUnlockBlob blob;
};

//! \p file's reason as the fetch reports it.
std::string Describe(const CheckedFile& file)
{
    const std::string fix = file.fix.empty() ? SET_UP_AGAIN : file.fix;

    return file.reason + (file.definite ? "; " + fix : std::string()) + " (" + file.detail + ")";
}

CheckedFile Refused(std::string reason, std::string detail, bool definite)
{
    CheckedFile file;
    file.reason = std::move(reason);
    file.detail = std::move(detail);
    file.definite = definite;

    return file;
}

//! Whether \p error is one that passes once another process lets go of the file.
bool IsTransient(DWORD error)
{
    return error == ERROR_SHARING_VIOLATION || error == ERROR_LOCK_VIOLATION;
}

//! Why the file \p handle (at \p path) is not this account's alone, or nullopt. No account
//! but this one, SYSTEM and Administrators may read it, nor change it: a right to change
//! it, its access control list included, lets an account grant itself read.
std::optional<CheckedFile> ForeignAccess(HANDLE handle, const fs::path& path)
{
    winacl::ObservedAcl acl;
    std::vector<winacl::Sid> allowed;

    try {
        acl = winacl::ReadAcl(handle, /*kernel_object=*/false);
        allowed = {winacl::CurrentTokenUser(), winacl::wellknown::System(), winacl::wellknown::Administrators()};
    } catch (const std::exception& e) {
        return Refused("could not read the autounlock file's access control list", Display(path) + ": " + AsciiWhat(e),
                       /*definite=*/false);
    }

    if (const std::optional<std::string> reader = winacl::FindForeignReader(acl, allowed)) {
        return Refused("the autounlock file is readable by other accounts", Display(path) + ": " + *reader,
                       /*definite=*/true);
    }

    if (const std::optional<std::string> writer = winacl::FindForeignWriter(acl, allowed, winacl::mask::FILE_WRITEISH)) {
        return Refused("the autounlock file can be changed by other accounts, which could make it readable to them",
                       Display(path) + ": " + *writer, /*definite=*/true);
    }

    return std::nullopt;
}

//! Why the service's file at \p path, opened as \p handle, is not used: an account other than
//! this one, SYSTEM, Administrators and TrustedInstaller can change it, create a file in its
//! directory, or replace a directory above (winacl::FindForeignWriterOnDataChain), or its path
//! is not one the check can walk. nullopt when none can. The file's own access list is read
//! through \p handle, the directories' by name. Nothing is changed.
std::optional<CheckedFile> ForeignWriterOnChain(HANDLE handle, const fs::path& path)
{
    // A path the walk does not take is no finding about who can change the file.
    if (const std::optional<std::string> problem = winacl::ChainPathProblem(path)) {
        CheckedFile file = Refused("the autounlock file's path cannot be checked", *problem, /*definite=*/true);
        file.fix = "name it as an absolute path with no '.' or '..' component (-autounlockblob, -datadir)";
        return file;
    }

    std::optional<std::string> writer;

    try {
        const std::vector<winacl::Sid> allowed{winacl::CurrentTokenUser(), winacl::wellknown::System(),
                                               winacl::wellknown::Administrators(),
                                               winacl::wellknown::TrustedInstaller()};
        const winacl::ObservedAcl leaf = winacl::ReadAcl(handle, /*kernel_object=*/false);
        writer = winacl::FindForeignWriterOnDataChain(path, allowed, &leaf);
    } catch (const std::exception& e) {
        return Refused("could not read the access control lists of the autounlock file and the directories above it",
                       Display(path) + ": " + AsciiWhat(e), /*definite=*/false);
    }

    if (!writer) return std::nullopt;

    CheckedFile file = Refused("the autounlock file, or a directory above it, can be changed by another account",
                               *writer, /*definite=*/true);
    file.fix = "remove that account's right to change it (an administrator can, with icacls); the core does not "
               "change access control lists";

    return file;
}

//! One attempt to read and check the file at \p path, without decrypting it. \p
//! transient is set when it failed with a sharing or lock violation.
CheckedFile ReadOnce(const fs::path& path, bool& transient)
{
    transient = false;

    // Delete sharing lets Store() replace the file, and Remove() delete it, while it is
    // being read here; this handle goes on reading what it opened.
    const HANDLE opened = ::CreateFileW(path.wstring().c_str(), GENERIC_READ | READ_CONTROL,
                                        FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                                        FILE_ATTRIBUTE_NORMAL, nullptr);
    const DWORD open_error = ::GetLastError();
    const winacl::UniqueHandle file(opened);

    if (!file) {
        if (open_error == ERROR_FILE_NOT_FOUND || open_error == ERROR_PATH_NOT_FOUND) {
            CheckedFile missing;
            missing.state = FileState::Missing;
            return missing;
        }

        transient = IsTransient(open_error);
        return Refused("could not open the autounlock file", Display(path) + ": CreateFileW failed: " + Win32ErrorText(open_error),
                       /*definite=*/false);
    }

    const BlobScope expected = ModeScope();

    // Every account on the machine can decrypt a LocalMachine file, so it must be this
    // account's alone. Checked on the handle that is read, before it is read.
    if (expected == BlobScope::LocalMachine) {
        if (std::optional<CheckedFile> foreign = ForeignAccess(file.get(), path)) return std::move(*foreign);
    }

    // The service's file, and the directories above it, may be changed only by the service's
    // own account and the system's. Checked before the file is read.
    if (expected == BlobScope::ServiceAccount) {
        if (std::optional<CheckedFile> foreign = ForeignWriterOnChain(file.get(), path)) return std::move(*foreign);
    }

    // One byte more than the limit tells a file that is too large.
    std::vector<unsigned char> bytes(AUTOUNLOCK_BLOB_MAX_BYTES + 1);
    size_t total = 0;

    while (total < bytes.size()) {
        DWORD read = 0;

        if (!::ReadFile(file.get(), bytes.data() + total, static_cast<DWORD>(bytes.size() - total), &read, nullptr)) {
            const DWORD read_error = ::GetLastError();
            transient = IsTransient(read_error);
            return Refused("could not read the autounlock file", Display(path) + ": ReadFile failed: " + Win32ErrorText(read_error),
                           /*definite=*/false);
        }

        if (read == 0) break;

        total += read;
    }

    if (total > AUTOUNLOCK_BLOB_MAX_BYTES) {
        return Refused(strprintf("the autounlock file is larger than %u bytes", AUTOUNLOCK_BLOB_MAX_BYTES), Display(path),
                       /*definite=*/true);
    }

    bytes.resize(total);

    std::optional<AutoUnlockBlob> blob = ParseAutoUnlockBlob(bytes);

    if (!blob) {
        return Refused("the autounlock file is not an autounlock file this version reads", Display(path),
                       /*definite=*/true);
    }

    if (blob->scope != expected) {
        return Refused(expected == BlobScope::ServiceAccount
                           ? "the autounlock file was set up for a core that runs as a user, but this core runs as a "
                             "Windows service"
                           : "the autounlock file was set up for the Windows service, but this core does not run as one",
                       Display(path), /*definite=*/true);
    }

    const std::string network = Params().NetworkIDString();

    if (blob->network != network) {
        return Refused(strprintf("the autounlock file is for the %s network, not %s", blob->network, network),
                       Display(path), /*definite=*/true);
    }

    CheckedFile usable;
    usable.state = FileState::Usable;
    usable.bytes = std::move(bytes);
    usable.blob = std::move(*blob);

    return usable;
}

//! ReadOnce, tried again after a sharing or lock violation, up to \p attempts times in
//! all. A shutdown request ends the waits.
CheckedFile ReadChecked(const fs::path& path, int attempts)
{
    std::chrono::milliseconds delay = FIRST_RETRY_DELAY;

    for (int attempt = 1;; ++attempt) {
        bool transient = false;
        CheckedFile file = ReadOnce(path, transient);

        if (!transient || attempt >= attempts) return file;

        LogPrintf("autounlock: attempt %d failed: %s; trying again in %d ms", attempt, Describe(file), delay.count());

        if (!WaitUnlessShutdown(delay)) {
            file.detail += "; stopped by shutdown";
            return file;
        }

        delay *= 2;
    }
}

//! The passphrase in \p blob, decrypted with DPAPI.
std::optional<SecureString> Decrypt(const AutoUnlockBlob& blob, const fs::path& path, std::string& error)
{
    const std::vector<unsigned char> entropy = AutoUnlockEntropy(blob.network);
    DATA_BLOB encrypted = InputBlob(blob.dpapi.data(), blob.dpapi.size());
    DATA_BLOB entropy_blob = InputBlob(entropy.data(), entropy.size());
    DpapiOutput plaintext;

    if (!::CryptUnprotectData(&encrypted, nullptr, &entropy_blob, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN,
                              &plaintext.blob)) {
        const DWORD decrypt_error = ::GetLastError();
        error = strprintf("the autounlock file could not be decrypted; %s (%s: %s)", SET_UP_AGAIN, Display(path),
                          "CryptUnprotectData failed: " + Win32ErrorText(decrypt_error));
        return std::nullopt;
    }

    // Reserved past the short-string buffer, so that the passphrase is only ever in
    // memory the secure allocator cleanses.
    SecureString passphrase;
    passphrase.reserve(std::max<size_t>(plaintext.blob.cbData, 32));
    passphrase.assign(reinterpret_cast<const char*>(plaintext.blob.pbData), plaintext.blob.cbData);

    return passphrase;
}

//! Write \p bytes to the new file \p path, owner-only from the moment it exists, and
//! flush it to disk. A file there already, left by a Store() that did not finish, is
//! removed first: CREATE_NEW never writes into a file this call did not make. With
//! FILE_FLAG_OPEN_REPARSE_POINT a link at the name counts as taken, as a file does, and is
//! removed itself, so nothing is created where it leads.
bool WriteNewFile(const fs::path& path, const std::vector<unsigned char>& bytes, std::string& error)
{
    const winacl::UniqueSecurityDescriptor sd(winacl::MakeSecurityDescriptor(util::OwnerOnlyAclSpec(/*inheritable=*/false)));
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = sd.get();
    sa.bInheritHandle = FALSE;

    const std::wstring wpath = path.wstring();
    const DWORD flags = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT;
    HANDLE created = ::CreateFileW(wpath.c_str(), GENERIC_WRITE, 0, &sa, CREATE_NEW, flags, nullptr);
    DWORD create_error = ::GetLastError();

    if (created == INVALID_HANDLE_VALUE && create_error == ERROR_FILE_EXISTS) {
        if (::DeleteFileW(wpath.c_str())) {
            created = ::CreateFileW(wpath.c_str(), GENERIC_WRITE, 0, &sa, CREATE_NEW, flags, nullptr);
            create_error = ::GetLastError();
        } else {
            create_error = ::GetLastError();
        }
    }

    winacl::UniqueHandle file(created);

    if (!file) {
        error = strprintf("could not create %s: %s", Display(path), "CreateFileW failed: " + Win32ErrorText(create_error));
        return false;
    }

    std::string failure;
    size_t total = 0;

    while (total < bytes.size() && failure.empty()) {
        DWORD written = 0;

        if (!::WriteFile(file.get(), bytes.data() + total, static_cast<DWORD>(bytes.size() - total), &written, nullptr)) {
            failure = "WriteFile failed: " + Win32ErrorText(::GetLastError());
        } else if (written == 0) {
            failure = "WriteFile wrote nothing";
        }

        total += written;
    }

    if (failure.empty() && !::FlushFileBuffers(file.get())) {
        failure = "FlushFileBuffers failed: " + Win32ErrorText(::GetLastError());
    }

    if (!failure.empty()) {
        file.reset();
        ::DeleteFileW(wpath.c_str());
        error = strprintf("could not write %s: %s", Display(path), failure);
        return false;
    }

    return true;
}

AutoUnlockStoreOutcome StoreBlob(const SecureString& passphrase, std::string& error)
{
    const std::optional<fs::path> path = AutoUnlockBlobPath(error);

    if (!path) return AutoUnlockStoreOutcome::Failed;

    if (passphrase.size() > AUTOUNLOCK_BLOB_MAX_BYTES) {
        error = "the passphrase is too long to store for autounlock";
        return AutoUnlockStoreOutcome::Failed;
    }

    std::string dir_error;

    if (!util::CreateOwnerOnlyDirectory(path->parent_path(), dir_error)) {
        error = "could not create the autounlock directory: " + dir_error;
        return AutoUnlockStoreOutcome::Failed;
    }

    AutoUnlockBlob blob;
    blob.scope = ModeScope();
    blob.network = Params().NetworkIDString();

    {
        const std::vector<unsigned char> entropy = AutoUnlockEntropy(blob.network);
        DATA_BLOB plaintext = InputBlob(reinterpret_cast<const unsigned char*>(passphrase.data()), passphrase.size());
        DATA_BLOB entropy_blob = InputBlob(entropy.data(), entropy.size());
        DWORD flags = CRYPTPROTECT_UI_FORBIDDEN | CRYPTPROTECT_AUDIT;

        if (blob.scope == BlobScope::LocalMachine) flags |= CRYPTPROTECT_LOCAL_MACHINE;

        DpapiOutput encrypted;

        if (!::CryptProtectData(&plaintext, DPAPI_DESCRIPTION, &entropy_blob, nullptr, nullptr, flags, &encrypted.blob)) {
            error = "could not encrypt the passphrase: CryptProtectData failed: " + Win32ErrorText(::GetLastError());
            return AutoUnlockStoreOutcome::Failed;
        }

        blob.dpapi.assign(encrypted.blob.pbData, encrypted.blob.pbData + encrypted.blob.cbData);
    }

    const std::vector<unsigned char> bytes = SerializeAutoUnlockBlob(blob);

    if (bytes.empty()) {
        error = strprintf("the encrypted passphrase does not fit in an autounlock file of %u bytes",
                          AUTOUNLOCK_BLOB_MAX_BYTES);
        return AutoUnlockStoreOutcome::Failed;
    }

    fs::path temporary = *path;
    temporary += ".tmp";

    if (!WriteNewFile(temporary, bytes, error)) return AutoUnlockStoreOutcome::Failed;

    if (!::MoveFileExW(temporary.wstring().c_str(), path->wstring().c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const DWORD move_error = ::GetLastError();
        ::DeleteFileW(temporary.wstring().c_str());
        error = strprintf("could not move the autounlock file into place at %s: %s", Display(*path),
                          "MoveFileExW failed: " + Win32ErrorText(move_error));
        return AutoUnlockStoreOutcome::Failed;
    }

    if (g_read_back_hook) g_read_back_hook(*path);

    // Read back as the fetch reads it. A file the fetch would refuse for what it is, as on
    // a volume that keeps no access control lists, is removed now rather than left in
    // place. One that could not be read back, as when another process holds it for longer
    // than the retries wait, or a shutdown cut the wait short, is kept: it is the file just
    // written, and only the check is missing.
    const CheckedFile written = ReadChecked(*path, FETCH_ATTEMPTS);

    if (written.state == FileState::Usable && written.bytes == bytes) return AutoUnlockStoreOutcome::Verified;

    if (written.state == FileState::Missing) {
        error = strprintf("the autounlock file written to %s was gone when read back", Display(*path));
        return AutoUnlockStoreOutcome::Failed;
    }

    if (written.state == FileState::Refused && !written.definite) {
        LogPrintf("autounlock: the passphrase is stored in %s, but the file could not be read back to check it: "
                  "%s (%s)", Display(*path), written.reason, written.detail);
        return AutoUnlockStoreOutcome::Unverified;
    }

    const std::string reason = written.state == FileState::Usable ? "what was read back differs from what was written"
                                                                   : written.reason + " (" + written.detail + ")";

    if (::DeleteFileW(path->wstring().c_str()) || ::GetLastError() == ERROR_FILE_NOT_FOUND) {
        error = strprintf("the autounlock file written to %s would not be used, so it was removed: %s", Display(*path),
                          reason);
    } else {
        error = strprintf("the autounlock file written to %s would not be used, and it could not be removed: %s; "
                          "remove it by hand",
                          Display(*path), reason);
    }

    return AutoUnlockStoreOutcome::Failed;
}
} // namespace

std::optional<SecureString> FetchAutoUnlockFromDpapiBlob(const fs::path& path, std::string& error)
{
    error.clear();

    try {
        const CheckedFile file = ReadChecked(path, FETCH_ATTEMPTS);

        if (file.state == FileState::Missing) {
            error = strprintf("autounlock is not set up (%s)", Display(path));
            LogPrintf("%s; the wallet stays locked", error);
            return std::nullopt;
        }

        if (file.state == FileState::Usable) {
            std::optional<SecureString> passphrase = Decrypt(file.blob, path, error);

            if (passphrase) return passphrase;
        } else {
            error = Describe(file);
        }
    } catch (const std::exception& e) {
        error = strprintf("could not read the autounlock file (%s: %s)", Display(path), AsciiWhat(e));
    }

    LogPrintf("autounlock: %s; the wallet stays locked", error);

    return std::nullopt;
}

AutoUnlockStoreOutcome DpapiBlobStore::Store(const SecureString& passphrase, std::string& error)
{
    error.clear();

    try {
        return StoreBlob(passphrase, error);
    } catch (const std::exception& e) {
        error = strprintf("could not store the autounlock passphrase: %s", AsciiWhat(e));
        return AutoUnlockStoreOutcome::Failed;
    }
}

bool DpapiBlobStore::Remove(std::string& error)
{
    error.clear();

    try {
        const std::optional<fs::path> path = AutoUnlockBlobPath(error);

        if (!path) return false;

        // A Store() that did not finish may have left its temporary file.
        fs::path temporary = *path;
        temporary += ".tmp";
        ::DeleteFileW(temporary.wstring().c_str());

        if (::DeleteFileW(path->wstring().c_str())) return true;

        const DWORD delete_error = ::GetLastError();

        if (delete_error == ERROR_FILE_NOT_FOUND || delete_error == ERROR_PATH_NOT_FOUND) return true;

        error = strprintf("could not remove the autounlock file %s: %s", Display(*path),
                          "DeleteFileW failed: " + Win32ErrorText(delete_error));
    } catch (const std::exception& e) {
        error = strprintf("could not remove the autounlock file: %s", AsciiWhat(e));
    }

    return false;
}

AutoUnlockStoreStatus DpapiBlobStore::Status()
{
    AutoUnlockStoreStatus status;
    status.supported = true;
    status.scope = ScopeName(ModeScope());

    try {
        std::string error;
        const std::optional<fs::path> path = AutoUnlockBlobPath(error);

        if (!path) {
            status.problem = error;
            return status;
        }

        status.location = Display(*path);

        // One attempt: a status is not worth waiting for.
        const CheckedFile file = ReadChecked(*path, /*attempts=*/1);

        status.configured = file.state != FileState::Missing;

        if (file.state == FileState::Refused) status.problem = Describe(file);
    } catch (const std::exception& e) {
        status.problem = AsciiWhat(e);
    }

    return status;
}

void SetAutoUnlockReadBackHookForTesting(std::function<void(const fs::path&)> hook)
{
    g_read_back_hook = std::move(hook);
}

AutoUnlockSecretStore* PlatformAutoUnlockSecretStore()
{
    // It keeps no state: every call reads the path and the mode afresh.
    static DpapiBlobStore store;

    return &store;
}
