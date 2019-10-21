// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "netbase.h" // for AddTimeData
#include "sync.h"
#include "strlcpy.h"
#include "version.h"
#include "ui_interface.h"
#include "util.h"

#include <boost/algorithm/string/join.hpp>
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
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <cstdarg>

#include "neuralnet/neuralnet.h"

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

bool fDebug = false;
bool fDebugNet = false;
bool fDebug2 = false;
bool fDebug3 = false;
bool fDebug4 = false;
bool fDebug10 = false;

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
std::string GetNeuralVersion();

bool fDevbuildCripple;

/** A map that contains all the currently held directory locks. After
 * successful locking, these will be held here until the global destructor
 * cleans them up and thus automatically unlocks them, or ReleaseDirectoryLocks
 * is called.
 */
static std::map<std::string, std::unique_ptr<fsbridge::FileLock>> dir_locks;
/** Mutex to protect dir_locks. */
static std::mutex cs_dir_locks;

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

void MilliSleep(int64_t n)
{
#if BOOST_VERSION >= 105000
    boost::this_thread::sleep_for(boost::chrono::milliseconds(n));
#else
    boost::this_thread::sleep(boost::posix_time::milliseconds(n));
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
        if (fDebug10) LogPrint("rand", "RandAddSeed() %lu bytes", nSize);
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

bool LogAcceptCategory(const char* category)
{
    if (category != NULL)
    {
        if (!fDebug) return false;
        const vector<string>& categories = mapMultiArgs["-debug"];
        if (find(categories.begin(), categories.end(), string(category)) == categories.end())
            return false;
    }
    return true;
}

void LogPrintStr(const std::string &str)
{
    if (fPrintToConsole)
    {
        // print to console
        fwrite(str.data(), 1, str.size(), stdout);
    }
    //else
    if (!fPrintToDebugger)
    {
        // print to debug.log
        static FILE* fileout = NULL;

        if (!fileout)
        {
            fs::path pathDebug = GetDataDir() / "debug.log";
            fileout = fsbridge::fopen(pathDebug.string().c_str(), "a");
            if (fileout) setbuf(fileout, NULL); // unbuffered
        }
        if (fileout)
        {
            static bool fStartedNewLine = true;

            // This routine may be called by global destructors during shutdown.
            // Since the order of destruction of static/global objects is undefined,
            // allocate mutexDebugLog on the heap the first time this routine
            // is called to avoid crashes during shutdown.
            static boost::mutex* mutexDebugLog = NULL;
            if (mutexDebugLog == NULL) mutexDebugLog = new boost::mutex();
            boost::mutex::scoped_lock scoped_lock(*mutexDebugLog);

            // reopen the log file, if requested
            if (fReopenDebugLog) {
                fReopenDebugLog = false;
                fs::path pathDebug = GetDataDir() / "debug.log";
                if (freopen(pathDebug.string().c_str(),"a",fileout) != NULL)
                    setbuf(fileout, NULL); // unbuffered
            }

            // Debug print useful for profiling
            if (fLogTimestamps && fStartedNewLine)
                fprintf(fileout, "%s ", DateTimeStrFormat("%x %H:%M:%S",  GetAdjustedTime()).c_str());
            if (!str.empty() && str[str.size() - 1] == '\n')
                fStartedNewLine = true;
            else
                fStartedNewLine = false;

            fwrite(str.data(), 1, str.size(), fileout);
            fflush(fileout);
        }
    }

/*#ifdef WIN32
    if (fPrintToDebugger)
    {
        static CCriticalSection cs_OutputDebugStringF;

        // accumulate and output a line at a time
        {
            LOCK(cs_OutputDebugStringF);
            static std::string buffer;

            va_list arg_ptr;
            va_start(arg_ptr, str.c_str());
            buffer += vstrprintf(str.c_str(), arg_ptr);
            va_end(arg_ptr);

            int line_start = 0, line_end;
            while((line_end = buffer.find('\n', line_start)) != -1)
            {
                OutputDebugStringA(buffer.substr(line_start, line_end - line_start).c_str());
                line_start = line_end + 1;
            }
            buffer.erase(0, line_start);
        }
    }
#endif */
    return;
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
        char psz[10000];
        strlcpy(psz, argv[i], sizeof(psz));
        char* pszValue = (char*)"";
        if (strchr(psz, '='))
        {
            pszValue = strchr(psz, '=');
            *pszValue++ = '\0';
        }
        #ifdef WIN32
        _strlwr(psz);
        if (psz[0] == '/')
            psz[0] = '-';
        #endif
        if (psz[0] != '-')
            break;

        mapArgs[psz] = pszValue;
        mapMultiArgs[psz].push_back(pszValue);
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



fs::path GetDefaultDataDir()
{
    // Windows < Vista: C:\Documents and Settings\Username\Application Data\gridcoinresearch
    // Windows >= Vista: C:\Users\Username\AppData\Roaming\gridcoinresearch
    // Mac: ~/Library/Application Support/gridcoinresearch
    // Unix: ~/.gridcoin
#ifdef WIN32
    // Windows
    return GetSpecialFolderPath(CSIDL_APPDATA) / "GridcoinResearch";
#else
        //2-25-2015
        fs::path pathRet;
        char* pszHome = getenv("HOME");

        if (mapArgs.count("-datadir"))
        {
            fs::path path2015 = fs::system_complete(mapArgs["-datadir"]);
            if (fs::is_directory(path2015))
            {
                pathRet = path2015;
            }
        }
        else
        {
            if (pszHome == NULL || strlen(pszHome) == 0)
                pathRet = fs::path("/");
            else
                pathRet = fs::path(pszHome);
        }
#ifdef MAC_OSX
    // Mac
    pathRet /= "Library/Application Support";
    fs::create_directory(pathRet);

    if (mapArgs.count("-datadir"))
    {
        return pathRet;
    }
    else
    {
        return pathRet / "GridcoinResearch";
    }
#else
    // Unix
    if (mapArgs.count("-datadir"))
    {
        return pathRet;
    }
    else
    {
        return pathRet / ".GridcoinResearch";
    }
#endif
#endif
}

const fs::path &GetDataDir(bool fNetSpecific)
{
    static fs::path pathCached[2];
    static CCriticalSection csPathCached;
    static bool cachedPath[2] = {false, false};
    //2-25-2015
    fs::path &path = pathCached[fNetSpecific];

    // This can be called during exceptions by LogPrintf, so we cache the
    // value so we don't have to do memory allocations after that.
    if (cachedPath[fNetSpecific]  && (fs::is_directory(path))  )
    {
        return path;
    }

    LOCK(csPathCached);

    if (mapArgs.count("-datadir"))
    {
            path = fs::system_complete(mapArgs["-datadir"]);
            if (!fs::is_directory(path))
            {
                path = "";
                return path;
            }
    }
    else
    {
        path = GetDefaultDataDir();
    }
    if ( (fNetSpecific && GetBoolArg("-testnet", false))  ||  GetBoolArg("-testnet",false) )
    {
        path /= "testnet";
    }

    if (!fs::exists(path)) fs::create_directory(path);

    cachedPath[fNetSpecific]=true;
    return path;
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
    if (!pathConfigFile.is_absolute()) pathConfigFile = GetDataDir(false) / pathConfigFile;
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





void ReadConfigFile(ArgsMap& mapSettingsRet,
                    ArgsMultiMap& mapMultiSettingsRet)
{
    fsbridge::ifstream streamConfig(GetConfigFile());
    if (!streamConfig.good())
        return; // No bitcoin.conf file is OK

    set<string> setOptions;
    setOptions.insert("*");

    for (boost::program_options::detail::config_file_iterator it(streamConfig, setOptions), end; it != end; ++it)
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
    FILE* file = fsbridge::fopen(path.string().c_str(), "w");
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
    return MoveFileExA(src.string().c_str(), dest.string().c_str(),
                      MOVEFILE_REPLACE_EXISTING);
#else
    int rc = std::rename(src.string().c_str(), dest.string().c_str());
    return (rc == 0);
#endif /* WIN32 */
}

/*
void FileCommit(FILE *fileout)
{
    fflush(fileout);                // harmless if redundantly called
#ifdef WIN32
    _commit(_fileno(fileout));
#else
    fsync(fileno(fileout));
#endif
}
*/

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


void ShrinkDebugFile()
{
    // Scroll debug.log if it's getting too big
    fs::path pathLog = GetDataDir() / "debug.log";
    FILE* file = fsbridge::fopen(pathLog.string().c_str(), "r");
    if (file && fs::file_size(pathLog) > 1000000)
    {
        // Restart the file with some of the end
        char pch[200000];
        fseek(file, -sizeof(pch), SEEK_END);
        int nBytes = fread(pch, 1, sizeof(pch), file);
        fclose(file);

        file = fsbridge::fopen(pathLog.string().c_str(), "w");
        if (file)
        {
            fwrite(pch, 1, nBytes, file);
            fclose(file);
        }
    }
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

std::string GetFileContents(std::string filepath)
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

    if (fDebug10) LogPrintf("loading file to string %s", filepath);

    std::ostringstream out;

    out << in.rdbuf();

    // Immediately close instead of waiting for the destructor to decrease the
    // chance of a race when calling this to read BOINC's client_state.xml:
    in.close();

    return out.str();
}

//
// "Never go to sea with two chronometers; take one or three."
// Our three time sources are:
//  - System clock
//  - Median of other nodes clocks
//  - The user (asking the user to fix the system clock if the first two disagree)
//
static int64_t nMockTime = 0;  // For unit testing

int64_t GetTime()
{
    if (nMockTime) return nMockTime;

    return time(NULL);
}

int64_t GetSystemTimeInSeconds()
{
    return GetTimeMicros()/1000000;
}

void SetMockTime(int64_t nMockTimeIn)
{
    nMockTime = nMockTimeIn;
}

static int64_t nTimeOffset = 0;

int64_t GetTimeOffset()
{
    return nTimeOffset;
}

int64_t GetAdjustedTime()
{
    return GetTime() + GetTimeOffset();
}

bool IsLockTimeWithin14days(int64_t locktime, int64_t reference)
{
    return IsLockTimeWithinMinutes(locktime, 14 * 24 * 60, reference);
}

bool IsLockTimeWithinMinutes(int64_t locktime, int minutes, int64_t reference)
{
    int64_t cutOff = reference - minutes * 60;
    return locktime >= cutOff;
}

#ifndef UPGRADERFLAG
// avoid including unnecessary files for standalone upgrader


void AddTimeData(const CNetAddr& ip, int64_t nOffsetSample)
{
    // Ignore duplicates
    static set<CNetAddr> setKnown;
    if (!setKnown.insert(ip).second)
        return;

    // Add data
    vTimeOffsets.input(nOffsetSample);
    if (fDebug10) LogPrintf("Added time data, samples %d, offset %+" PRId64 " (%+" PRId64 " minutes)", vTimeOffsets.size(), nOffsetSample, nOffsetSample/60);
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
        if (fDebug10) {
            for (auto const& n : vSorted)
                LogPrintf("%+" PRId64 "  ", n);
            LogPrintf("|  ");
        }
        if (fDebug10) LogPrintf("nTimeOffset = %+" PRId64 "  (%+" PRId64 " minutes)", nTimeOffset, nTimeOffset/60);
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

std::string GetNeuralVersion()
{
    std::string neural_v = "0";
    int64_t neural_id = NN::GetInstance()->IsNeuralNet();
    neural_v = ToString(CLIENT_VERSION_MINOR) + "." + ToString(neural_id);
    return neural_v;
}

// Format the subversion field according to BIP 14 spec (https://en.bitcoin.it/wiki/BIP_0014)
std::string FormatSubVersion(const std::string& name, int nClientVersion, const std::vector<std::string>& comments)
{
    std::string neural_v = GetNeuralVersion();

    std::ostringstream ss;
    ss << "/";
    ss << name << ":" << FormatVersion(nClientVersion);

    if (!comments.empty())         ss << "(" << boost::algorithm::join(comments, "; ") << ")";
    ss << "(" << neural_v << ")";

    ss << "/";
    return ss.str();
}

#ifdef WIN32
fs::path GetSpecialFolderPath(int nFolder, bool fCreate)
{
    char pszPath[MAX_PATH] = "";

    if(SHGetSpecialFolderPathA(NULL, pszPath, nFolder, fCreate))
    {
        return fs::path(pszPath);
    }

    LogPrintf("SHGetSpecialFolderPathA() failed, could not obtain requested path.");
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

// Convert characters that can potentially cause problems to safe html
std::string MakeSafeMessage(const std::string& messagestring)
{
    std::string safemessage = "";
    safemessage.reserve(messagestring.size());
    try
    {
        for (auto chk : messagestring)
        {
            switch(chk)
            {
                case '&':     safemessage += "&amp;";     break;
                case '\'':    safemessage += "&apos;";    break;
                case '\\':    safemessage += "&#92;";     break;
                case '\"':    safemessage += "&quot;";    break;
                case '>':     safemessage += "&gt;";      break;
                case '<':     safemessage += "&lt;";      break;
                case '\0':                                break;
                default:      safemessage += chk;         break;
            }
        }
    }
    catch (...)
    {
        LogPrintf("Exception occurred in MakeSafeMessage. Returning an empty message.");
        safemessage = "";
    }
    return safemessage;
}

bool ThreadHandler::createThread(void(*pfn)(ThreadHandlerPtr), ThreadHandlerPtr parg, const std::string tname)
{
    try
    {
        boost::thread *newThread = new boost::thread(pfn, parg);
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
        boost::thread *newThread = new boost::thread(pfn, parg);
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
