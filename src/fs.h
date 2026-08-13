// Copyright (c) 2017-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_FS_H
#define BITCOIN_FS_H

#include <stdio.h>
#include <string>
#if defined WIN32 && defined __GLIBCXX__
#include <ext/stdio_filebuf.h>
#endif

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

/** Filesystem operations and types */
namespace fs = boost::filesystem;

/** Bridge operations to C stdio */
namespace fsbridge {
    FILE *fopen(const fs::path& p, const char *mode);

    /**
     * Helper function for joining two paths
     *
     * @param[in] base  Base path
     * @param[in] path  Path to combine with base
     * @returns path unchanged if it is an absolute path, otherwise returns base joined with path. Returns base unchanged if path is empty.
     * @pre  Base path must be absolute
     * @post Returned path will always be absolute
     */
    fs::path AbsPathJoin(const fs::path& base, const fs::path& path);

    class FileLock
    {
    public:
        FileLock() = delete;
        FileLock(const FileLock&) = delete;
        FileLock(FileLock&&) = delete;
        explicit FileLock(const fs::path& file);
        ~FileLock();
        bool TryLock();
        std::string GetReason() { return reason; }

    private:
        std::string reason;
#ifndef WIN32
        int fd = -1;
#else
        void* hFile = (void*)-1; // INVALID_HANDLE_VALUE
#endif
    };

    std::string get_filesystem_error_message(const fs::filesystem_error& e);

    /** Check if a path contains characters not representable in the system code page.
     *
     * On Windows, fs::path::string() converts to the system code page, which silently
     * corrupts characters outside that code page. This function detects such paths so
     * callers can engage appropriate workarounds.
     *
     * Always returns false on non-Windows platforms (UTF-8 is the native encoding).
     */
    bool PathHasNonCodepageChars(const fs::path& path);

    /** Return a narrow string safe for BDB's DbEnv::open() and gArgs storage.
     *
     * BDB only accepts narrow (const char*) paths, and gArgs stores narrow strings. On Windows,
     * fs::path::string() converts to the system code page, which corrupts characters outside that
     * code page. This function converts to the Windows short (8.3) form via GetShortPathNameW()
     * only when the path contains non-codepage characters. 8.3 names are ASCII-safe and work as
     * valid aliases for the original Unicode path.
     *
     * If the path is representable in the system code page, returns path.string() unchanged.
     * Returns an empty string if conversion is needed but 8.3 names are unavailable on the volume.
     *
     * On non-Windows platforms, simply returns path.string().
     */
    std::string ShortPathString(const fs::path& path);

    /** Return a UTF-8 display string, converting from Windows 8.3 short form if needed.
     *
     * On Windows, converts from 8.3 short form back to the original long (Unicode) path via
     * GetLongPathNameW(), then encodes as UTF-8. Qt interprets UTF-8 correctly via
     * QString::fromStdString(), and log output preserves the original characters.
     * Use this for error messages, log output, and any user-facing display.
     *
     * On non-Windows platforms, simply returns path.string().
     */
    std::string LongPathString(const fs::path& path);

    /** Return a UTF-8 narrow string safe for LevelDB's leveldb::DB::Open().
     *
     * LevelDB's Windows port (env_windows.cc) internally converts paths from UTF-8 to UTF-16
     * via MultiByteToWideChar(CP_UTF8). But fs::path::string() produces system code page bytes,
     * not UTF-8, which causes LevelDB's conversion to produce garbled wide strings. This function
     * provides proper UTF-8 encoding only when the path contains non-codepage characters.
     *
     * If the path is representable in the system code page, returns path.string() unchanged
     * (code page and UTF-8 are identical for ASCII characters).
     *
     * On non-Windows platforms, simply returns path.string() (already UTF-8).
     */
    std::string Utf8PathString(const fs::path& path);

    /**
     * True when path is on a filesystem whose contents do not survive the process
     * exiting or the machine rebooting -- tmpfs or ramfs.
     *
     * A data directory there accepts every write and loses the chain, the wallet
     * and debug.log on exit, with no error at any point. Linux only: elsewhere
     * this returns false, since there is no equivalent test. It also returns
     * false when the filesystem cannot be determined, so an unreadable path is
     * never called volatile on the strength of a failed syscall.
     */
    bool IsVolatileFilesystem(const fs::path& path);

    // GNU libstdc++ specific workaround for opening UTF-8 paths on Windows.
    //
    // On Windows, it is only possible to reliably access multibyte file paths through
    // `wchar_t` APIs, not `char` APIs. But because the C++ standard doesn't
    // require ifstream/ofstream `wchar_t` constructors, and the GNU library doesn't
    // provide them (in contrast to the Microsoft C++ library, see
    // https://stackoverflow.com/questions/821873/how-to-open-an-stdfstream-ofstream-or-ifstream-with-a-unicode-filename/822032#822032),
    // Boost is forced to fall back to `char` constructors which may not work properly.
    //
    // Work around this issue by creating stream objects with `_wfopen` in
    // combination with `__gnu_cxx::stdio_filebuf`. This workaround can be removed
    // with an upgrade to C++17, where streams can be constructed directly from
    // `std::filesystem::path` objects.

#if defined WIN32 && defined __GLIBCXX__
    class ifstream : public std::istream
    {
    public:
        ifstream() = default;
        explicit ifstream(const fs::path& p, std::ios_base::openmode mode = std::ios_base::in) { open(p, mode); }
        ~ifstream() { close(); }
        void open(const fs::path& p, std::ios_base::openmode mode = std::ios_base::in);
        bool is_open() { return m_filebuf.is_open(); }
        void close();

    private:
        __gnu_cxx::stdio_filebuf<char> m_filebuf;
        FILE* m_file = nullptr;
    };
    class ofstream : public std::ostream
    {
    public:
        ofstream() = default;
        explicit ofstream(const fs::path& p, std::ios_base::openmode mode = std::ios_base::out) { open(p, mode); }
        ~ofstream() { close(); }
        void open(const fs::path& p, std::ios_base::openmode mode = std::ios_base::out);
        bool is_open() { return m_filebuf.is_open(); }
        void close();

    private:
        __gnu_cxx::stdio_filebuf<char> m_filebuf;
        FILE* m_file = nullptr;
    };
#else  // !(WIN32 && __GLIBCXX__)
    typedef fs::ifstream ifstream;
    typedef fs::ofstream ofstream;
#endif // WIN32 && __GLIBCXX__
};

#endif // BITCOIN_FS_H
