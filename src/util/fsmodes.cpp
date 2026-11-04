// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef WIN32

#include "util/fsmodes.h"

#include "util/dir_permissions.h"
#include "tinyformat.h"
#include "util/string.h"
#include "util/syserror.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <set>

namespace fsmodes {

namespace {

//! The path as it is written into a command line a user can paste into a shell.
std::string ShellQuote(const std::string& text)
{
    const bool plain = !text.empty() && text.find_first_not_of(
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_./-+=@:,") == std::string::npos;

    if (plain) return text;

    std::string quoted = "'";
    for (const char c : text) {
        if (c == '\'') {
            quoted += "'\\''";
        } else {
            quoted += c;
        }
    }
    return quoted + "'";
}

class ModeChecker
{
public:
    std::vector<ModeFinding> m_findings;

    //! follow_link is for the data directory root only, which may be a link to another disk. Every
    //! other path keeps the rule that a symbolic link is reported and not followed.
    void Check(const fs::path& path, bool follow_link = false)
    {
        if (path.empty() || !m_seen.insert(path).second) return;

        struct stat st;
        if ((follow_link ? ::stat(path.string().c_str(), &st) : ::lstat(path.string().c_str(), &st)) != 0) return;

        if (S_ISLNK(st.st_mode)) {
            m_findings.push_back({path, ModeFinding::Kind::Symlink, 0,
                                  "WARNING: " + path.string() + " is a symbolic link, so its mode was not checked"});
            return;
        }

        // The permission bits fit an unsigned.
        const unsigned mode = static_cast<unsigned>(st.st_mode & 07777);

        if (NotOwnedModeAcceptable(mode)) return;

        const bool ours = st.st_uid == ::geteuid();
        const std::string fix = std::string("chmod ") + (S_ISDIR(st.st_mode) ? "700 " : "600 ") + ShellQuote(path.string());

        std::string message = "WARNING: " + path.string() + " has mode " + strprintf("%04o", mode)
                            + ", which gives other accounts access to wallet data. ";
        if (ours) {
            message += "To make it owner-only run: " + fix;
        } else {
            message += "It is owned by another account, so only its owner can change it: " + fix;
        }

        m_findings.push_back({path, ours ? ModeFinding::Kind::WideMode : ModeFinding::Kind::WideModeNotOwned,
                              mode, message});
    }

    //! The entries of a directory whose names pass the filter, each checked as a path of its own.
    template <typename Filter>
    void CheckEntries(const fs::path& dir, Filter filter)
    {
        struct stat st;
        if (dir.empty() || ::lstat(dir.string().c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) return;

        boost::system::error_code ec;
        for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
            const fs::path entry = it->path();
            struct stat entry_st;
            if (::lstat(entry.string().c_str(), &entry_st) != 0) continue;

            // Subdirectories are not part of the check. A symbolic link is, so it is reported.
            if (S_ISDIR(entry_st.st_mode)) continue;

            if (filter(entry.filename().string())) Check(entry);
        }
    }

private:
    std::set<fs::path> m_seen;
};

} // anonymous namespace

std::vector<ModeFinding> CheckOwnerOnlyModes(const fs::path& datadir, const fs::path& conf,
                                             const fs::path& settings, const fs::path& wallet_file,
                                             const fs::path& backupdir, const fs::path& debuglog)
{
    ModeChecker checker;

    checker.Check(datadir, /*follow_link=*/true);
    checker.Check(wallet_file);

    if (!datadir.empty()) {
        const fs::path database = datadir / "database";
        checker.Check(database);
        checker.CheckEntries(database, [](const std::string& name) { return name.compare(0, 4, "log.") == 0; });
    }

    checker.Check(conf);
    checker.Check(settings);

    checker.Check(backupdir);
    checker.CheckEntries(backupdir, [](const std::string&) { return true; });

    checker.Check(debuglog);

    return std::move(checker.m_findings);
}

bool OwnedByUs(const fs::path& path)
{
    struct stat st;
    if (::lstat(path.string().c_str(), &st) != 0) return false;

    return !S_ISLNK(st.st_mode) && st.st_uid == ::geteuid();
}

bool NotOwnedModeAcceptable(unsigned mode)
{
    // No access at all for "other" and no write for the group.
    return (mode & 007) == 0 && (mode & 020) == 0;
}

std::optional<std::string> CheckBackupDirWritable(const fs::path& backupdir)
{
    std::string error;
    if (!util::CreateOwnerOnlyDirectory(backupdir, error)) return error;

    static std::atomic<unsigned> counter{0};

    const fs::path probe = backupdir / (".gridcoin-write-test-" + ToString(::getpid()) + "-"
                                        + ToString(counter.fetch_add(1)));

    const int fd = ::open(probe.string().c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (fd < 0) return SysErrorString(errno);

    ::close(fd);
    ::unlink(probe.string().c_str());

    return std::nullopt;
}

void CopyFileOwnerOnly(const fs::path& source, const fs::path& target)
{
    const auto fail = [](const char* what, const fs::path& path, int err) {
        throw fs::filesystem_error(what, path, boost::system::error_code(err, boost::system::generic_category()));
    };

    const int in = ::open(source.string().c_str(), O_RDONLY | O_CLOEXEC);
    if (in < 0) fail("CopyFileOwnerOnly: cannot open the source", source, errno);

    // A new target is created with mode 0600, so it is owner-only before any data is written. A
    // target that exists is opened as it is, and keeps its mode.
    int out = ::open(target.string().c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (out < 0 && errno == EEXIST) {
        out = ::open(target.string().c_str(), O_WRONLY | O_CLOEXEC);

        struct stat in_st, out_st;
        if (out >= 0 && ::fstat(in, &in_st) == 0 && ::fstat(out, &out_st) == 0
            && in_st.st_dev == out_st.st_dev && in_st.st_ino == out_st.st_ino) {
            ::close(in);
            ::close(out);
            fail("CopyFileOwnerOnly: the source and the target are the same file", target, EEXIST);
        }

        // Truncated only after the same-file check above.
        if (out >= 0 && ::ftruncate(out, 0) != 0) {
            const int err = errno;
            ::close(in);
            ::close(out);
            fail("CopyFileOwnerOnly: cannot truncate the target", target, err);
        }
    }
    if (out < 0) {
        const int err = errno;
        ::close(in);
        fail("CopyFileOwnerOnly: cannot open the target", target, err);
    }

    int err = 0;
    char buffer[65536];
    for (;;) {
        const ssize_t n = ::read(in, buffer, sizeof(buffer));
        if (n < 0) {
            if (errno == EINTR) continue;
            err = errno;
            break;
        }
        if (n == 0) break;

        // write() returns at most n, which is non-negative here.
        const size_t size = static_cast<size_t>(n);
        size_t done = 0;
        while (done < size) {
            const ssize_t w = ::write(out, buffer + done, size - done);
            if (w < 0) {
                if (errno == EINTR) continue;
                err = errno;
                break;
            }
            done += static_cast<size_t>(w);
        }
        if (err != 0) break;
    }

    ::close(in);
    if (::close(out) != 0 && err == 0) err = errno;

    if (err != 0) fail("CopyFileOwnerOnly: copy failed", target, err);
}

} // namespace fsmodes

#endif // !WIN32
