// Copyright (c) 2017-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <fs.h>

#ifdef __linux__
#include <sys/vfs.h>
#endif
#include <util/syserror.h>

#ifndef WIN32
#include <fcntl.h>
#include <string>
#include <sys/file.h>
#include <sys/utsname.h>
#else
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <codecvt>
#include <windows.h>
#endif

namespace fsbridge {

FILE *fopen(const fs::path& p, const char *mode)
{
#ifndef WIN32
    return ::fopen(p.string().c_str(), mode);
#else
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>,wchar_t> utf8_cvt;
    return ::_wfopen(p.wstring().c_str(), utf8_cvt.from_bytes(mode).c_str());
#endif
}

fs::path AbsPathJoin(const fs::path& base, const fs::path& path)
{
    assert(base.is_absolute());
    return fs::absolute(path, base);
}

#ifndef WIN32

static std::string GetErrorReason() {
    return SysErrorString(errno);
}

FileLock::FileLock(const fs::path& file)
{
    fd = open(file.string().c_str(), O_RDWR);
    if (fd == -1) {
        reason = GetErrorReason();
    }
}

FileLock::~FileLock()
{
    if (fd != -1) {
        close(fd);
    }
}

bool FileLock::TryLock()
{
    if (fd == -1) {
        return false;
    }

    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    if (fcntl(fd, F_SETLK, &lock) == -1) {
        reason = GetErrorReason();
        return false;
    }

    return true;
}
#else

static std::string GetErrorReason() {
    wchar_t* err;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<WCHAR*>(&err), 0, nullptr);
    std::wstring err_str(err);
    LocalFree(err);
    return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().to_bytes(err_str);
}

FileLock::FileLock(const fs::path& file)
{
    hFile = CreateFileW(file.wstring().c_str(),  GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        reason = GetErrorReason();
    }
}

FileLock::~FileLock()
{
    if (hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
    }
}

bool FileLock::TryLock()
{
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    _OVERLAPPED overlapped = {};
    if (!LockFileEx(hFile, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, std::numeric_limits<DWORD>::max(), std::numeric_limits<DWORD>::max(), &overlapped)) {
        reason = GetErrorReason();
        return false;
    }
    return true;
}
#endif

bool PathHasNonCodepageChars(const fs::path& path)
{
#ifdef WIN32
    std::wstring wide = path.wstring();
    BOOL used_default = FALSE;
    // First call: determine the buffer size needed for the code page conversion.
    int needed = WideCharToMultiByte(CP_ACP, 0, wide.c_str(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return true;
    // Second call: perform the actual conversion. WideCharToMultiByte sets used_default to TRUE if any
    // character in the wide string could not be represented in the system code page and was replaced
    // with a default substitution character.
    std::string narrow(needed, '\0');
    WideCharToMultiByte(CP_ACP, 0, wide.c_str(), (int)wide.size(), narrow.data(), needed, nullptr, &used_default);
    return used_default != FALSE;
#else
    return false;
#endif
}

std::string LongPathString(const fs::path& path)
{
#ifdef WIN32
    std::wstring wpath = path.wstring();
    wchar_t long_path[MAX_PATH];
    DWORD result = GetLongPathNameW(wpath.c_str(), long_path, MAX_PATH);
    if (result > 0 && result < MAX_PATH) {
        // Return as UTF-8 so Qt (QString::fromStdString) and log output display correctly.
        int needed = WideCharToMultiByte(CP_UTF8, 0, long_path, (int)result, nullptr, 0, nullptr, nullptr);
        if (needed > 0) {
            std::string utf8(needed, '\0');
            WideCharToMultiByte(CP_UTF8, 0, long_path, (int)result, utf8.data(), needed, nullptr, nullptr);
            return utf8;
        }
    }
    return path.string();
#else
    return path.string();
#endif
}

std::string ShortPathString(const fs::path& path)
{
#ifdef WIN32
    if (!PathHasNonCodepageChars(path)) {
        return path.string();
    }
    std::wstring wpath = path.wstring();
    wchar_t short_path[MAX_PATH];
    DWORD result = GetShortPathNameW(wpath.c_str(), short_path, MAX_PATH);
    if (result > 0 && result < MAX_PATH) {
        // Short path names are ASCII-safe, so narrow conversion is lossless.
        return std::string(short_path, short_path + result);
    }
    // 8.3 name generation is disabled on this volume.
    return "";
#else
    return path.string();
#endif
}

std::string Utf8PathString(const fs::path& path)
{
#ifdef WIN32
    if (!PathHasNonCodepageChars(path)) {
        return path.string();
    }
    std::wstring wide = path.wstring();
    int needed = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return path.string();
    std::string utf8(needed, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), utf8.data(), needed, nullptr, nullptr);
    return utf8;
#else
    return path.string();
#endif
}

std::string get_filesystem_error_message(const fs::filesystem_error& e)
{
#ifndef WIN32
    return e.what();
#else
    // Convert from Multi Byte to utf-16
    std::string mb_string(e.what());
    int size = MultiByteToWideChar(CP_ACP, 0, mb_string.c_str(), mb_string.size(), nullptr, 0);

    std::wstring utf16_string(size, L'\0');
    MultiByteToWideChar(CP_ACP, 0, mb_string.c_str(), mb_string.size(), &*utf16_string.begin(), size);
    // Convert from utf-16 to utf-8
    return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t>().to_bytes(utf16_string);
#endif
}

#ifdef WIN32
#ifdef __GLIBCXX__

// reference: https://github.com/gcc-mirror/gcc/blob/gcc-7_3_0-release/libstdc%2B%2B-v3/include/std/fstream#L270

static std::string openmodeToStr(std::ios_base::openmode mode)
{
    switch (mode & ~std::ios_base::ate) {
    case std::ios_base::out:
    case std::ios_base::out | std::ios_base::trunc:
        return "w";
    case std::ios_base::out | std::ios_base::app:
    case std::ios_base::app:
        return "a";
    case std::ios_base::in:
        return "r";
    case std::ios_base::in | std::ios_base::out:
        return "r+";
    case std::ios_base::in | std::ios_base::out | std::ios_base::trunc:
        return "w+";
    case std::ios_base::in | std::ios_base::out | std::ios_base::app:
    case std::ios_base::in | std::ios_base::app:
        return "a+";
    case std::ios_base::out | std::ios_base::binary:
    case std::ios_base::out | std::ios_base::trunc | std::ios_base::binary:
        return "wb";
    case std::ios_base::out | std::ios_base::app | std::ios_base::binary:
    case std::ios_base::app | std::ios_base::binary:
        return "ab";
    case std::ios_base::in | std::ios_base::binary:
        return "rb";
    case std::ios_base::in | std::ios_base::out | std::ios_base::binary:
        return "r+b";
    case std::ios_base::in | std::ios_base::out | std::ios_base::trunc | std::ios_base::binary:
        return "w+b";
    case std::ios_base::in | std::ios_base::out | std::ios_base::app | std::ios_base::binary:
    case std::ios_base::in | std::ios_base::app | std::ios_base::binary:
        return "a+b";
    default:
        return std::string();
    }
}

void ifstream::open(const fs::path& p, std::ios_base::openmode mode)
{
    close();
    mode |= std::ios_base::in;
    m_file = fsbridge::fopen(p, openmodeToStr(mode).c_str());
    if (m_file == nullptr) {
        return;
    }
    m_filebuf = __gnu_cxx::stdio_filebuf<char>(m_file, mode);
    rdbuf(&m_filebuf);
    if (mode & std::ios_base::ate) {
        seekg(0, std::ios_base::end);
    }
}

void ifstream::close()
{
    if (m_file != nullptr) {
        m_filebuf.close();
        fclose(m_file);
    }
    m_file = nullptr;
}

void ofstream::open(const fs::path& p, std::ios_base::openmode mode)
{
    close();
    mode |= std::ios_base::out;
    m_file = fsbridge::fopen(p, openmodeToStr(mode).c_str());
    if (m_file == nullptr) {
        return;
    }
    m_filebuf = __gnu_cxx::stdio_filebuf<char>(m_file, mode);
    rdbuf(&m_filebuf);
    if (mode & std::ios_base::ate) {
        seekp(0, std::ios_base::end);
    }
}

void ofstream::close()
{
    if (m_file != nullptr) {
        m_filebuf.close();
        fclose(m_file);
    }
    m_file = nullptr;
}
#else // __GLIBCXX__

#if BOOST_VERSION >= 107700
static_assert(sizeof(*BOOST_FILESYSTEM_C_STR(fs::path())) == sizeof(wchar_t),
#else
static_assert(sizeof(*fs::path().BOOST_FILESYSTEM_C_STR) == sizeof(wchar_t),
#endif // BOOST_VERSION >= 107700
    "Warning: This build is using boost::filesystem ofstream and ifstream "
    "implementations which will fail to open paths containing multibyte "
    "characters. You should delete this static_assert to ignore this warning, "
    "or switch to a different C++ standard library like the Microsoft C++ "
    "Standard Library (where boost uses non-standard extensions to construct "
    "stream objects with wide filenames), or the GNU libstdc++ library (where "
    "a more complicated workaround has been implemented above).");

#endif // __GLIBCXX__
#endif // WIN32

} // fsbridge

bool fsbridge::IsVolatileFilesystem(const fs::path& path)
{
#ifdef __linux__
    struct statfs fs_info;

    // Cannot tell: say no. Reporting "volatile" because a syscall failed would
    // turn an unreadable path into a hard startup error.
    if (statfs(path.string().c_str(), &fs_info) != 0) return false;

    // From <linux/magic.h>, inlined so that header is not a build dependency.
    constexpr decltype(fs_info.f_type) TMPFS_MAGIC_VALUE = 0x01021994;
    constexpr decltype(fs_info.f_type) RAMFS_MAGIC_VALUE = 0x858458f6;

    return fs_info.f_type == TMPFS_MAGIC_VALUE || fs_info.f_type == RAMFS_MAGIC_VALUE;
#else
    (void)path;
    return false;
#endif
}
