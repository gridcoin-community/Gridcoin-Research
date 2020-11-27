// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "netbase.h" // for AddTimeData
#include "sync.h"
#include "version.h"
#include "ui_interface.h"
#include "util.h"
#include "util/memory.h"

#include <boost/algorithm/string/case_conv.hpp> // for to_lower()
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp> // for startswith() and endswith()
#include <boost/date_time/posix_time/posix_time.hpp>  //For day of year
#include <cmath>
#include <boost/lexical_cast.hpp>

// Work around clang compilation problem in Boost 1.46:
// /usr/include/boost/program_options/detail/config_file.hpp:163:17: error: call to function 'to_internal' that is neither visible in the template definition nor found by argument-dependent lookup
// See also: http://stackoverflow.com/questions/10020179/compilation-fail-in-boost-librairies-program-options
//           http://clang.debian.net/status.php?version=3.0&key=CANNOT_FIND_FUNCTION
namespace boost {
    namespace program_options {
        std::string to_internal(const std::string&);
    }
}

#include <boost/program_options/detail/config_file.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/thread.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/newline.hpp>
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <cstdarg>

#ifdef WIN32
#ifdef _MSC_VER
#pragma warning(disable:4786)
#pragma warning(disable:4804)
#pragma warning(disable:4805)
#pragma warning(disable:4717)
#endif
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0501
#ifdef _WIN32_IE
#undef _WIN32_IE
#endif
#define _WIN32_IE 0x0501
#define WIN32_LEAN_AND_MEAN 1
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <codecvt>
#include <io.h> /* for _commit */
#include <shellapi.h>
#include "shlobj.h"
#elif defined(__linux__)
# include <sys/prctl.h>
#endif

using namespace std;

ArgsMap mapArgs;
ArgsMultiMap mapMultiArgs;

bool fPrintToConsole = false;
bool fPrintToDebugger = false;
bool fRequestShutdown = false;
bool fShutdown = false;
bool fDaemon = false;
bool fServer = false;
bool fCommandLine = false;
string strMiscWarning;
bool fTestNet = false;
bool fNoListen = false;
bool fLogTimestamps = false;
CMedianFilter<int64_t> vTimeOffsets(200,0);
bool fReopenDebugLog = false;

bool fDevbuildCripple;

/** A map that contains all the currently held directory locks. After
 * successful locking, these will be held here until the global destructor
 * cleans them up and thus automatically unlocks them, or ReleaseDirectoryLocks
 * is called.
 */
static std::map<std::string, std::unique_ptr<fsbridge::FileLock>> dir_locks;
/** Mutex to protect dir_locks. */
static std::mutex cs_dir_locks;

// An absolute hack but required due to possible bitcoingui early call, and it goes here because it has to be guaranteed
// to be initialized here with the bitcoingui object.
std::atomic_bool miner_first_pass_complete {false};

void SetupEnvironment()
{
    // On most POSIX systems (e.g. Linux, but not BSD) the environment's locale
    // may be invalid, in which case the "C.UTF-8" locale is used as fallback.
#if !defined(WIN32) && !defined(MAC_OSX) && !defined(__FreeBSD__) && !defined(__OpenBSD__)
    try {
        std::locale(""); // Raises a runtime error if current locale is invalid
    } catch (const std::runtime_error&) {
        setenv("LC_ALL", "C.UTF-8", 1);
    }
#elif defined(WIN32)
    // Set the default input/output charset is utf-8
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif
    // The path locale is lazy initialized and to avoid deinitialization errors
    // in multithreading environments, it is set explicitly by the main thread.
    // A dummy locale is used to extract the internal default locale, used by
    // fs::path, which is then used to explicitly imbue the path.
    std::locale loc = fs::path::imbue(std::locale::classic());
#ifndef WIN32
    fs::path::imbue(loc);
#else
    fs::path::imbue(std::locale(loc, new std::codecvt_utf8_utf16<wchar_t>()));
#endif
}

// Init OpenSSL library multithreading support
static CCriticalSection** ppmutexOpenSSL;
void locking_callback(int mode, int i, const char* file, int line) NO_THREAD_SAFETY_ANALYSIS
{
    if (mode & CRYPTO_LOCK) {
        ENTER_CRITICAL_SECTION(*ppmutexOpenSSL[i]);
    } else {
        LEAVE_CRITICAL_SECTION(*ppmutexOpenSSL[i]);
    }
}

// Init
class CInit
{
public:
    CInit()
    {
        // Init OpenSSL library multithreading support
        ppmutexOpenSSL = (CCriticalSection**)OPENSSL_malloc(CRYPTO_num_locks() * sizeof(CCriticalSection*));
        for (int i = 0; i < CRYPTO_num_locks(); i++)
            ppmutexOpenSSL[i] = new CCriticalSection();
        CRYPTO_set_locking_callback(locking_callback);

#ifdef WIN32
        // Seed OpenSSL PRNG with current contents of the screen
        RAND_screen();
#endif

        // Seed OpenSSL PRNG with performance counter
        RandAddSeed();
    }
    ~CInit()
    {
        // Securely erase the memory used by the PRNG
        RAND_cleanup();
        // Shutdown OpenSSL library multithreading support
        CRYPTO_set_locking_callback(NULL);
        for (int i = 0; i < CRYPTO_num_locks(); i++)
            delete ppmutexOpenSSL[i];
        OPENSSL_free(ppmutexOpenSSL);
    }
}
instance_of_cinit;

void RandAddSeed()
{
    // Seed with CPU performance counter
    int64_t nCounter = GetPerformanceCounter();
    RAND_add(&nCounter, sizeof(nCounter), 1.5);
    memset(&nCounter, 0, sizeof(nCounter));
}

void RandAddSeedPerfmon()
{
    RandAddSeed();

    // This can take up to 2 seconds, so only do it every 10 minutes
    static int64_t nLastPerfmon;
    if ( GetAdjustedTime() < nLastPerfmon + 10 * 60)
        return;
    nLastPerfmon =  GetAdjustedTime();

#ifdef WIN32
    // Don't need this on Linux, OpenSSL automatically uses /dev/urandom
    // Seed with the entire set of perfmon data
    unsigned char pdata[250000];
    memset(pdata, 0, sizeof(pdata));
    unsigned long nSize = sizeof(pdata);
    long ret = RegQueryValueExA(HKEY_PERFORMANCE_DATA, "Global", NULL, NULL, pdata, &nSize);
    RegCloseKey(HKEY_PERFORMANCE_DATA);
    if (ret == ERROR_SUCCESS)
    {
        RAND_add(pdata, nSize, nSize/100.0);
        memset(pdata, 0, nSize);
        LogPrint(BCLog::LogFlags::NOISY, "rand", "RandAddSeed() %lu bytes", nSize);
    }
#endif
}

uint64_t GetRand(uint64_t nMax)
{
    if (nMax == 0)
        return 0;

    // The range of the random source must be a multiple of the modulus
    // to give every possible output value an equal possibility
    uint64_t nRange = (std::numeric_limits<uint64_t>::max() / nMax) * nMax;
    uint64_t nRand = 0;
    do
        RAND_bytes((unsigned char*)&nRand, sizeof(nRand));
    while (nRand >= nRange);
    return (nRand % nMax);
}

int GetRandInt(int nMax)
{
    return GetRand(nMax);
}

uint256 GetRandHash()
{
    uint256 hash;
    RAND_bytes((unsigned char*)&hash, sizeof(hash));
    return hash;
}

int GetDayOfYear(int64_t timestamp)
{
    try
    {
        boost::gregorian::date d=boost::posix_time::from_time_t(timestamp).date();
        //      boost::gregorian::date d(year, month, day);
        int dayNumber = d.day_of_year();
        return dayNumber;
    }
    catch (std::out_of_range& e)
    {
    // Alternatively catch bad_year etc exceptions.
        return 0;
    }
}

void ParseString(const string& str, char c, vector<string>& v)
{
    if (str.empty())
        return;
    string::size_type i1 = 0;
    string::size_type i2;
    while (true)
    {
        i2 = str.find(c, i1);
        if (i2 == str.npos)
        {
            v.push_back(str.substr(i1));
            return;
        }
        v.push_back(str.substr(i1, i2-i1));
        i1 = i2+1;
    }
}


string FormatMoney(int64_t n, bool fPlus)
{
    // Note: not using straight sprintf here because we do NOT want
    // localized number formatting.
    int64_t n_abs = (n > 0 ? n : -n);
    int64_t quotient = n_abs/COIN;
    int64_t remainder = n_abs%COIN;
    string str = strprintf("%" PRId64 ".%08" PRId64, quotient, remainder);

    // Right-trim excess zeros before the decimal point:
    int nTrim = 0;
    for (int i = str.size()-1; (str[i] == '0' && isdigit(str[i-2])); --i)
        ++nTrim;
    if (nTrim)
        str.erase(str.size()-nTrim, nTrim);

    if (n < 0)
        str.insert((unsigned int)0, 1, '-');
    else if (fPlus && n > 0)
        str.insert((unsigned int)0, 1, '+');
    return str;
}


bool ParseMoney(const string& str, int64_t& nRet)
{
    return ParseMoney(str.c_str(), nRet);
}

bool ParseMoney(const char* pszIn, int64_t& nRet)
{
    string strWhole;
    int64_t nUnits = 0;
    const char* p = pszIn;
    while (isspace(*p))
        p++;
    for (; *p; p++)
    {
        if (*p == '.')
        {
            p++;
            int64_t nMult = CENT*10;
            while (isdigit(*p) && (nMult > 0))
            {
                nUnits += nMult * (*p++ - '0');
                nMult /= 10;
            }
            break;
        }
        if (isspace(*p))
            break;
        if (!isdigit(*p))
            return false;
        strWhole.insert(strWhole.end(), *p);
    }
    for (; *p; p++)
        if (!isspace(*p))
            return false;
    if (strWhole.size() > 10) // guard against 63 bit overflow
        return false;
    if (nUnits < 0 || nUnits > COIN)
        return false;
    int64_t nWhole = atoi64(strWhole);
    int64_t nValue = nWhole*COIN + nUnits;

    nRet = nValue;
    return true;
}

static void InterpretNegativeSetting(string name, ArgsMap& mapSettingsRet)
{
    // interpret -nofoo as -foo=0 (and -nofoo=0 as -foo=1) as long as -foo not set
    if (name.find("-no") == 0)
    {
        std::string positive("-");
        positive.append(name.begin()+3, name.end());
        if (mapSettingsRet.count(positive) == 0)
        {
            bool value = !GetBoolArg(name);
            mapSettingsRet[positive] = (value ? "1" : "0");
        }
    }
}

void ParseParameters(int argc, const char* const argv[])
{
    mapArgs.clear();
    mapMultiArgs.clear();
    for (int i = 1; i < argc; i++)
    {
        std::string str(argv[i]);
        std::string strValue;
        size_t is_index = str.find('=');
        if (is_index != std::string::npos)
        {
            strValue = str.substr(is_index+1);
            str = str.substr(0, is_index);
        }
#ifdef WIN32
        boost::to_lower(str);
        if (boost::algorithm::starts_with(str, "/"))
            str = "-" + str.substr(1);
#endif
        if (str[0] != '-')
            break;

        mapArgs[str] = strValue;
        mapMultiArgs[str].push_back(strValue);
    }

    // New 0.6 features:
    for (auto const& entry : mapArgs)
    {
        string name = entry.first;

        //  interpret --foo as -foo (as long as both are not set)
        if (name.find("--") == 0)
        {
            std::string singleDash(name.begin()+1, name.end());
            if (mapArgs.count(singleDash) == 0)
                mapArgs[singleDash] = entry.second;
            name = singleDash;
        }

        // interpret -nofoo as -foo=0 (and -nofoo=0 as -foo=1) as long as -foo not set
        InterpretNegativeSetting(name, mapArgs);
    }
}

std::string GetArgument(const std::string& arg, const std::string& defaultvalue)
{
    if (mapArgs.count("-" + arg))
        return mapArgs["-" + arg];

    return defaultvalue;
}

// SetArgument - Set or alter arguments stored in memory
void SetArgument(
            const string &argKey,
            const string &argValue)
{
    mapArgs["-" + argKey] = argValue;
}

std::string GetArg(const std::string& strArg, const std::string& strDefault)
{
    if (mapArgs.count(strArg))
        return mapArgs[strArg];
    return strDefault;
}

int64_t GetArg(const std::string& strArg, int64_t nDefault)
{
    if (mapArgs.count(strArg))
        return atoi64(mapArgs[strArg]);
    return nDefault;
}

bool GetBoolArg(const std::string& strArg, bool fDefault)
{
    if (mapArgs.count(strArg))
    {
        if (mapArgs[strArg].empty())
            return true;
        return (atoi(mapArgs[strArg]) != 0);
    }
    return fDefault;
}

bool IsArgSet(const std::string& strArg)
{
    return !(GetArg(strArg, "never_used_as_argument") == "never_used_as_argument");
}

bool IsArgNegated(const std::string& strArg)
{
    return GetArg(strArg, "true") == "false";
}

bool SoftSetArg(const std::string& strArg, const std::string& strValue)
{
    if (mapArgs.count(strArg))
        return false;
    ForceSetArg(strArg, strValue);
    return true;
}

bool SoftSetBoolArg(const std::string& strArg, bool fValue)
{
    if (fValue)
        return SoftSetArg(strArg, std::string("1"));
    else
        return SoftSetArg(strArg, std::string("0"));
}

void ForceSetArg(const std::string& strArg, const std::string& strValue)
{
    mapArgs[strArg] = strValue;
    mapMultiArgs[strArg].clear();
    mapMultiArgs[strArg].push_back(strValue);
}

bool WildcardMatch(const char* psz, const char* mask)
{
    while (true)
    {
        switch (*mask)
        {
        case '\0':
            return (*psz == '\0');
        case '*':
            return WildcardMatch(psz, mask+1) || (*psz && WildcardMatch(psz+1, mask));
        case '?':
            if (*psz == '\0')
                return false;
            break;
        default:
            if (*psz != *mask)
                return false;
            break;
        }
        psz++;
        mask++;
    }
}

bool WildcardMatch(const string& str, const string& mask)
{
    return WildcardMatch(str.c_str(), mask.c_str());
}

static std::string FormatException(std::exception* pex, const char* pszThread)
{
#ifdef WIN32
    char pszModule[MAX_PATH] = "";
    GetModuleFileNameA(NULL, pszModule, sizeof(pszModule));
#else
    const char* pszModule = "gridcoin";
#endif
    if (pex)
        return strprintf(
            "EXCEPTION: %s       \n%s       \n%s in %s\n", typeid(*pex).name(), pex->what(), pszModule, pszThread);
    else
        return strprintf(
            "UNKNOWN EXCEPTION       \n%s in %s\n", pszModule, pszThread);
}

void LogException(std::exception* pex, const char* pszThread)
{
    std::string message = FormatException(pex, pszThread);
    LogPrintf("%s", message);
}

void PrintException(std::exception* pex, const char* pszThread)
{
    std::string message = FormatException(pex, pszThread);
    LogPrintf("\n\n************************\n%s", message);
    fprintf(stderr, "\n\n************************\n%s\n", message.c_str());
    strMiscWarning = message;
    throw;
}

void PrintExceptionContinue(std::exception* pex, const char* pszThread)
{
    std::string message = FormatException(pex, pszThread);
    LogPrintf("\n\n************************\n%s", message);
    fprintf(stderr, "\n\n************************\n%s\n", message.c_str());
    strMiscWarning = message;
}

fs::path AbsPathForConfigVal(const fs::path& path, bool net_specific)
{
    if (path.is_absolute()) {
        return path;
    }
    return fs::absolute(path, GetDataDir(net_specific));
}

fs::path GetDefaultDataDir()
{
    // Windows < Vista: C:\Documents and Settings\Username\Application Data\GridcoinResearch
    // Windows >= Vista: C:\Users\Username\AppData\Roaming\GridcoinResearch
    // Mac: ~/Library/Application Support/GridcoinResearch
    // Linux/Unix: ~/.GridcoinResearch
#ifdef WIN32
    // Windows

    // This is the user's base roaming AppData path with GridcoinResearch added.
    return GetSpecialFolderPath(CSIDL_APPDATA) / "GridcoinResearch";
#else
    fs::path pathRet;

    // For everything except for Windows, use the environment variable to get
    // the home path.
    char* pszHome = getenv("HOME");

    // There is no home path, so default to the root directory.
    if (pszHome == nullptr || strlen(pszHome) == 0) {
        pathRet = fs::path("/");
    } else {
        pathRet = fs::path(pszHome);
    }
#ifdef MAC_OSX
    // The pathRet here represents the HOME directory. Apple
    // applications are expected to store their files in
    // "~/Library/Application Support/[AppDir].
    return pathRet / "Library" / "Application Support" / "GridcoinResearch";
#else
    // Linux/Unix
    return pathRet / ".GridcoinResearch";
#endif // MAC_OSX
#endif // WIN32
}

const fs::path &GetDataDir(bool fNetSpecific)
{
    static fs::path pathCached[2];
    static CCriticalSection cs_PathCached;
    static bool cachedPath[2] = {false, false};

    fs::path &path = pathCached[fNetSpecific];

    // This can be called during exceptions by LogPrintf, so we cache the
    // value so we don't have to do memory allocations after that.
    if (cachedPath[fNetSpecific] && fs::is_directory(path))
    {
        return path;
    }

    LOCK(cs_PathCached);

    if (mapArgs.count("-datadir"))
    {
            path = fs::system_complete(mapArgs["-datadir"]);
            if (!fs::is_directory(path))
            {
                throw std::runtime_error("Supplied datadir is invalid! "
                                         "Please check your datadir argument. Aborting.");
            }
    }
    else
    {
        path = GetDefaultDataDir();
    }

    if (fNetSpecific && GetBoolArg("-testnet", false))
    {
        path /= "testnet";
    }

    if (!fs::exists(path)) fs::create_directories(path);

    cachedPath[fNetSpecific] = true;

    return path;
}


bool CheckDataDirOption()
{
    std::string datadir = GetArg("-datadir", "");
    return datadir.empty() || fs::is_directory(fs::system_complete(datadir));
}

fs::path GetProgramDir()
{
    fs::path path;

    if (mapArgs.count("-programdir"))
    {
        path = fs::system_complete(mapArgs["-programdir"]);

        if (!fs::is_directory(path))
        {
            path = "";
            LogPrintf("Invalid path stated in gridcoinresearch.conf");
        }
        else
        {
            return path;
        }

    }

    #ifdef WIN32
    const char* const list[] = {"gridcoind.exe", "gridcoin-qt.exe", "gridcoinupgrader.exe"};
    #elif defined MAC_OSX
    const char* const list[] = {"gridcoind.exe", "gridcoin-qt.exe", "gridcoinupgrader.exe"};
    #else
    const char* const list[] = {"gridcoinresearchd", "gridcoin-qt", "gridcoinupgrader"};
    #endif

    for (int i = 0; i < 3; ++i)
    {
        if (fs::exists((fs::current_path() / list[i]).c_str()))
        {
            return fs::current_path();
        }
    }

        LogPrintf("Please specify program directory in config file using the 'programdir' argument");
        path = "";
        return path;

}

fs::path GetConfigFile()
{
    fs::path pathConfigFile(GetArg("-conf", "gridcoinresearch.conf"));
    if (!pathConfigFile.is_absolute()) pathConfigFile = GetDataDir() / pathConfigFile;
    return pathConfigFile;
}

bool IsConfigFileEmpty()
{
    fsbridge::ifstream streamConfig(GetConfigFile());
    if (!streamConfig.good())
    {
        return true;
    }
    return false;

}

bool ReadConfigFile(ArgsMap& mapSettingsRet,
                    ArgsMultiMap& mapMultiSettingsRet)
{
    fsbridge::ifstream streamConfig(GetConfigFile());

    if (!streamConfig.good())
        return true; // No gridcoinresearch.conf file is OK

    try {
        boost::iostreams::filtering_istream streamFilteredConfig;
        streamFilteredConfig.push(boost::iostreams::newline_filter(boost::iostreams::newline::posix));
        streamFilteredConfig.push(streamConfig);

        set<string> setOptions;
        setOptions.insert("*");

        for (boost::program_options::detail::config_file_iterator it(streamFilteredConfig, setOptions), end; it != end; ++it)
        {
            // Don't overwrite existing settings so command line settings override bitcoin.conf
            string strKey = string("-") + it->string_key;
            if (mapSettingsRet.count(strKey) == 0)
            {
                mapSettingsRet[strKey] = it->value[0];
                // interpret nofoo=1 as foo=0 (and nofoo=0 as foo=1) as long as foo not set)
                InterpretNegativeSetting(strKey, mapSettingsRet);
            }
            mapMultiSettingsRet[strKey].push_back(it->value[0]);
        }

        return true;
    } catch (...) {
        return false;
    }
}

fs::path GetPidFile()
{
    fs::path pathPidFile(GetArg("-pid", "gridcoinresearch.pid"));
    if (!pathPidFile.is_absolute()) pathPidFile = GetDataDir() / pathPidFile;
    return pathPidFile;
}

#ifndef WIN32
void CreatePidFile(const fs::path &path, pid_t pid)
{
    FILE* file = fsbridge::fopen(path, "w");
    if (file)
    {
        fprintf(file, "%d\n", pid);
        fclose(file);
    }
}
#endif

bool RenameOver(fs::path src, fs::path dest)
{
#ifdef WIN32
    return MoveFileExW(src.wstring().c_str(), dest.wstring().c_str(),
                      MOVEFILE_REPLACE_EXISTING);
#else
    int rc = std::rename(src.string().c_str(), dest.string().c_str());
    return (rc == 0);
#endif /* WIN32 */
}

/**
 * Ignores exceptions thrown by Boost's create_directories if the requested directory exists.
 * Specifically handles case where path p exists, but it wasn't possible for the user to
 * write to the parent directory.
 */
bool TryCreateDirectories(const fs::path& p)
{
    try
    {
        return fs::create_directories(p);
    } catch (const fs::filesystem_error&) {
        if (!fs::exists(p) || !fs::is_directory(p))
            throw;
    }

    // create_directories didn't create the directory, it had to have existed already
    return false;
}

// Newer FileCommit overload from Bitcoin.
bool FileCommit(FILE *file)
{
    if (fflush(file) != 0) { // harmless if redundantly called
        LogPrintf("%s: fflush failed: %d\n", __func__, errno);
        return false;
    }
#ifdef WIN32
    HANDLE hFile = (HANDLE)_get_osfhandle(_fileno(file));
    if (FlushFileBuffers(hFile) == 0) {
        LogPrintf("%s: FlushFileBuffers failed: %d\n", __func__, GetLastError());
        return false;
    }
#else
    #if defined(__linux__) || defined(__NetBSD__)
    if (fdatasync(fileno(file)) != 0 && errno != EINVAL) { // Ignore EINVAL for filesystems that don't support sync
        LogPrintf("%s: fdatasync failed: %d\n", __func__, errno);
        return false;
    }
    #elif defined(MAC_OSX) && defined(F_FULLFSYNC)
    if (fcntl(fileno(file), F_FULLFSYNC, 0) == -1) { // Manpage says "value other than -1" is returned on success
        LogPrintf("%s: fcntl F_FULLFSYNC failed: %d\n", __func__, errno);
        return false;
    }
    #else
    if (fsync(fileno(file)) != 0 && errno != EINVAL) {
        LogPrintf("%s: fsync failed: %d\n", __func__, errno);
        return false;
    }
    #endif
#endif
    return true;
}

bool DirIsWritable(const fs::path& directory)
{
    fs::path tmpFile = directory / fs::unique_path();

    FILE* file = fsbridge::fopen(tmpFile, "a");
    if (!file) return false;

    fclose(file);
    remove(tmpFile);

    return true;
}

bool LockDirectory(const fs::path& directory, const std::string lockfile_name, bool probe_only)
{
    std::lock_guard<std::mutex> ulock(cs_dir_locks);
    fs::path pathLockFile = directory / lockfile_name;

    // If a lock for this directory already exists in the map, don't try to re-lock it
    if (dir_locks.count(pathLockFile.string())) {
        return true;
    }

    // Create empty lock file if it doesn't exist.
    FILE* file = fsbridge::fopen(pathLockFile, "a");
    if (file) fclose(file);
    auto lock = MakeUnique<fsbridge::FileLock>(pathLockFile);
    if (!lock->TryLock()) {
        return error("Error while attempting to lock directory %s: %s", directory.string(), lock->GetReason());
    }
    if (!probe_only) {
        // Lock successful and we're not just probing, put it into the map
        dir_locks.emplace(pathLockFile.string(), std::move(lock));
    }
    return true;
}

std::string GetFileContents(const fs::path filepath)
{
    if (!fs::exists(filepath)) {
        LogPrintf("GetFileContents: file does not exist %s", filepath);
        return "-1";
    }

    fsbridge::ifstream in(filepath, std::ios::in | std::ios::binary);

    if (in.fail()) {
        LogPrintf("GetFileContents: error opening file %s", filepath);
        return "-1";
    }

    LogPrint(BCLog::LogFlags::NOISY, "loading file to string %s", filepath);

    std::ostringstream out;

    out << in.rdbuf();

    // Immediately close instead of waiting for the destructor to decrease the
    // chance of a race when calling this to read BOINC's client_state.xml:
    in.close();

    return out.str();
}

#ifndef UPGRADERFLAG
// avoid including unnecessary files for standalone upgrader

static int64_t nTimeOffset = 0;

int64_t GetTimeOffset()
{
    return nTimeOffset;
}

int64_t GetAdjustedTime()
{
    return GetTime() + GetTimeOffset();
}

void AddTimeData(const CNetAddr& ip, int64_t nOffsetSample)
{
    // Ignore duplicates
    static set<CNetAddr> setKnown;
    if (!setKnown.insert(ip).second)
        return;

    // Add data
    vTimeOffsets.input(nOffsetSample);
    LogPrint(BCLog::LogFlags::NOISY, "Added time data, samples %d, offset %+" PRId64 " (%+" PRId64 " minutes)", vTimeOffsets.size(), nOffsetSample, nOffsetSample/60);
    if (vTimeOffsets.size() >= 5 && vTimeOffsets.size() % 2 == 1)
    {
        // We believe the median of the other nodes 95% and our own node's time ("0" initial offset) 5%. This will also act to gently converge the network to consensus UTC, in case
        // the entire network is displaced for some reason.
        nTimeOffset = 0.95 * vTimeOffsets.median();
        std::vector<int64_t> vSorted = vTimeOffsets.sorted();
        // Only let other nodes change our time by so much
        if (abs64(nTimeOffset) >= 70 * 60)
        {
            nTimeOffset = 0;

            static bool fDone;
            if (!fDone)
            {
                // If nobody has a time different than ours but within 5 minutes of ours, give a warning
                bool fMatch = false;
                for (auto const& nOffset : vSorted)
                    if (nOffset != 0 && abs64(nOffset) < 5 * 60)
                        fMatch = true;

                if (!fMatch)
                {
                    fDone = true;
                    string strMessage = _("Warning: Please check that your computer's date and time are correct! If your clock is wrong Gridcoin will not work properly.");
                    strMiscWarning = strMessage;
                    LogPrintf("*** %s", strMessage);
                    uiInterface.ThreadSafeMessageBox(strMessage+" ", string("Gridcoin"), CClientUIInterface::OK | CClientUIInterface::ICON_EXCLAMATION);
                }
            }
        }
        if (LogInstance().WillLogCategory(BCLog::LogFlags::NOISY)) {
            for (auto const& n : vSorted)
                LogPrintf("%+" PRId64 "  ", n);
            LogPrintf("|  ");
        }
        LogPrint(BCLog::LogFlags::NOISY, "nTimeOffset = %+" PRId64 "  (%+" PRId64 " minutes)", nTimeOffset, nTimeOffset/60);
    }
}


#endif


uint32_t insecure_rand_Rz = 11;
uint32_t insecure_rand_Rw = 11;
void seed_insecure_rand(bool fDeterministic)
{
    //The seed values have some unlikely fixed points which we avoid.
    if(fDeterministic)
    {
        insecure_rand_Rz = insecure_rand_Rw = 11;
    } else {
        uint32_t tmp;
        do{
            RAND_bytes((unsigned char*)&tmp,4);
        }while(tmp==0 || tmp==0x9068ffffU);
        insecure_rand_Rz=tmp;
        do{
            RAND_bytes((unsigned char*)&tmp,4);
        }while(tmp==0 || tmp==0x464fffffU);
        insecure_rand_Rw=tmp;
    }
}

string FormatVersion(int nVersion)
{
    if (nVersion%100 == 0)
        return strprintf("%d.%d.%d", nVersion/1000000, (nVersion/10000)%100, (nVersion/100)%100);
    else
        return strprintf("%d.%d.%d.%d", nVersion/1000000, (nVersion/10000)%100, (nVersion/100)%100, nVersion%100);
}

#ifndef UPGRADERFLAG
// avoid including unnecessary files for standalone upgrader


string FormatFullVersion()
{
    return CLIENT_BUILD;
}

#endif

double Round(double d, int place)
{
    const double accuracy = std::pow(10, place);
    return std::round(d * accuracy) / accuracy;
}

std::string RoundToString(double d, int place)
{
    std::ostringstream ss;
    ss.imbue(std::locale::classic());
    ss << std::fixed << std::setprecision(place) << d;
    return ss.str();
}

double RoundFromString(const std::string& s, int place)
{
    try
    {
        double num = boost::lexical_cast<double>(s);
        return Round(num, place);
    }
    catch(const boost::bad_lexical_cast& e)
    {
        return 0;
    }
}

bool Contains(const std::string& data, const std::string& instring)
{
    return data.find(instring) != std::string::npos;
}

std::vector<std::string> split(const std::string& s, const std::string& delim)
{
    size_t pos = 0;
    size_t end = 0;
    std::vector<std::string> elems;

    while((end = s.find(delim, pos)) != std::string::npos)
    {
        elems.push_back(s.substr(pos, end - pos));
        pos = end + delim.size();
    }

    // Append final value
    elems.push_back(s.substr(pos, end - pos));
    return elems;
}

// Format the subversion field according to BIP 14 spec (https://en.bitcoin.it/wiki/BIP_0014)
std::string FormatSubVersion(const std::string& name, int nClientVersion, const std::vector<std::string>& comments)
{
    std::ostringstream ss;
    ss << "/";
    ss << name << ":" << FormatVersion(nClientVersion);

    if (!comments.empty())         ss << "(" << boost::algorithm::join(comments, "; ") << ")";

    ss << "/";
    return ss.str();
}

#ifdef WIN32
fs::path GetSpecialFolderPath(int nFolder, bool fCreate)
{
    wchar_t pszPath[MAX_PATH] = L"";

    if (SHGetSpecialFolderPathW(nullptr, pszPath, nFolder, fCreate))
    {
        return fs::path(pszPath);
    }

    LogPrintf("SHGetSpecialFolderPathW() failed, could not obtain requested path.");
    return fs::path("");
}
#endif

void runCommand(std::string strCommand)
{
    int nErr = ::system(strCommand.c_str());
    if (nErr)
        LogPrintf("runCommand error: system(%s) returned %d", strCommand, nErr);
}

void RenameThread(const char* name)
{
#if defined(PR_SET_NAME)
    // Only the first 15 characters are used (16 - NUL terminator)
    ::prctl(PR_SET_NAME, name, 0, 0, 0);
#elif 0 && (defined(__FreeBSD__) || defined(__OpenBSD__))
    // TODO : This is currently disabled because it needs to be verified to work
    //       on FreeBSD or OpenBSD first. When verified the '0 &&' part can be
    //       removed.
    pthread_set_name_np(pthread_self(), name);

// This is XCode 10.6-and-later; bring back if we drop 10.5 support:
// #elif defined(MAC_OSX)
//    pthread_setname_np(name);

#else
    // Prevent warnings for unused parameters...
    (void)name;
#endif
}

bool NewThread(void(*pfn)(void*), void* parg)
{
    try
    {
        boost::thread(pfn, parg); // thread detaches when out of scope
    } catch(boost::thread_resource_error &e) {
        LogPrintf("Error creating thread: %s", e.what());
        return false;
    }
    return true;
}

bool ThreadHandler::createThread(void(*pfn)(ThreadHandlerPtr), ThreadHandlerPtr parg, const std::string tname)
{
    try
    {
#if defined(__linux__) && !defined(__GLIBCXX__)
        //
        // Explicitly set the stack size for this thread to 2 MB.
        //
        // This supports compilation with musl libc which provides a default
        // stack size of 128 KB. Gridcoin chose the scrypt algorithm to hash
        // blocks early in the chain. The selected scrypt parameters require
        // more than 128 KB of stack space, so we need to increase the stack
        // size for threads that hash blocks.
        //
        // This function is used to create those threads. Since we will port
        // Bitcoin's newer thread management utilities, I will not take time
        // to generalize this patch. Ideally, we should specify a stack size
        // suitable for the application instead of relying on the default of
        // the libc implementation. For now, we will let glibc do its thing.
        // 2 MB is the typical default for glibc on x86 platforms so this is
        // the size we'll start with. After testing, we may choose a smaller
        // stack size. This patch may apply to other libc implementations as
        // well.
        //
        boost::thread::attributes attrs;
        attrs.set_stack_size(2 << 20);

        boost::thread *newThread = new boost::thread(attrs, std::bind(pfn, parg));
#else
        boost::thread *newThread = new boost::thread(pfn, parg);
#endif
        threadGroup.add_thread(newThread);
        threadMap[tname] = newThread;
    } catch(boost::thread_resource_error &e) {
        LogPrintf("Error creating thread: %s", e.what());
        return false;
    }
    return true;
}

bool ThreadHandler::createThread(void(*pfn)(void*), void* parg, const std::string tname)
{
    try
    {
#if defined(__linux__) && !defined(__GLIBCXX__)
        //
        // Explicitly set the stack size for this thread to 2 MB.
        //
        // This supports compilation with musl libc which provides a default
        // stack size of 128 KB. Gridcoin chose the scrypt algorithm to hash
        // blocks early in the chain. The selected scrypt parameters require
        // more than 128 KB of stack space, so we need to increase the stack
        // size for threads that hash blocks.
        //
        // This function is used to create those threads. Since we will port
        // Bitcoin's newer thread management utilities, I will not take time
        // to generalize this patch. Ideally, we should specify a stack size
        // suitable for the application instead of relying on the default of
        // the libc implementation. For now, we will let glibc do its thing.
        // 2 MB is the typical default for glibc on x86 platforms so this is
        // the size we'll start with. After testing, we may choose a smaller
        // stack size. This patch may apply to other libc implementations as
        // well.
        //
        boost::thread::attributes attrs;
        attrs.set_stack_size(2 << 20);

        boost::thread *newThread = new boost::thread(attrs, std::bind(pfn, parg));
#else
        boost::thread *newThread = new boost::thread(pfn, parg);
#endif
        threadGroup.add_thread(newThread);
        threadMap[tname] = newThread;
    } catch(boost::thread_resource_error &e) {
        LogPrintf("Error creating thread: %s", e.what());
        return false;
    }
    return true;
}

int ThreadHandler::numThreads()
{
    return threadGroup.size();
}

bool ThreadHandler::threadExists(const string tname)
{
    if(threadMap.count(tname) > 0)
        return true;
    else
        return false;
}

void ThreadHandler::interruptAll(){
    threadGroup.interrupt_all();
}

void ThreadHandler::removeByName(const std::string tname)
{
    threadGroup.remove_thread(threadMap[tname]);
    threadMap[tname]->join();
    threadMap.erase(tname);
}

void ThreadHandler::removeAll()
{
    LogPrintf("Wait for %d threads to join.",numThreads());
    threadGroup.join_all();
    for (auto it=threadMap.begin(); it!=threadMap.end(); ++it)
    {
        threadGroup.remove_thread(it->second);
    }
    threadMap.clear();
}

std::string TimestampToHRDate(double dtm)
{
    if (dtm == 0) return "1-1-1970 00:00:00";
    if (dtm > 9888888888) return "1-1-2199 00:00:00";
    std::string sDt = DateTimeStrFormat("%m-%d-%Y %H:%M:%S",dtm);
    return sDt;
}

namespace util {
#ifdef WIN32
WinCmdLineArgs::WinCmdLineArgs()
{
    wchar_t** wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> utf8_cvt;
    argv = new char*[argc];
    args.resize(argc);
    for (int i = 0; i < argc; i++) {
        args[i] = utf8_cvt.to_bytes(wargv[i]);
        argv[i] = &*args[i].begin();
    }
    LocalFree(wargv);
}

WinCmdLineArgs::~WinCmdLineArgs()
{
    delete[] argv;
}

std::pair<int, char**> WinCmdLineArgs::get()
{
    return std::make_pair(argc, argv);
}
#endif
} // namespace util
