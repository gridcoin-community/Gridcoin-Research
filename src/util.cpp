// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "netbase.h" // for AddTimeData
#include "sync.h"
#include "version.h"
#include "ui_interface.h"
#include "util.h"
#include <util/strencodings.h>

#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp> // for startswith() and endswith()
#include <boost/date_time/posix_time/posix_time.hpp>  //For day of year
#include <cmath>
#include <boost/lexical_cast.hpp>

// Work around clang compilation problem in Boost 1.46:
// /usr/include/boost/program_options/detail/config_file.hpp:163:17: error: call to function 'to_internal' that is neither visible in the template definition nor found by argument-dependent lookup
// See also: https://stackoverflow.com/questions/10020179/compilation-fail-in-boost-librairies-program-options
//           https://clang.debian.net/status.php?version=3.0&key=CANNOT_FIND_FUNCTION
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

using namespace std;

bool fPrintToConsole = false;
bool fPrintToDebugger = false;
bool fRequestShutdown = false;
bool fShutdown = false;
bool fDaemon = false;
bool fServer = false;
bool fCommandLine = false;
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
        CRYPTO_set_locking_callback(nullptr);
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
    memory_cleanse(&nCounter, sizeof(nCounter));
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
    memory_cleanse(pdata, sizeof(pdata));
    unsigned long nSize = sizeof(pdata);
    long ret = RegQueryValueExA(HKEY_PERFORMANCE_DATA, "Global", nullptr, nullptr, pdata, &nSize);
    RegCloseKey(HKEY_PERFORMANCE_DATA);
    if (ret == ERROR_SUCCESS)
    {
        RAND_add(pdata, nSize, nSize/100.0);
        memory_cleanse(pdata, nSize);
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
    for (int i = str.size()-1; (str[i] == '0' && IsDigit(str[i-2])); --i)
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
    while (IsSpace(*p))
        p++;
    for (; *p; p++)
    {
        if (*p == '.')
        {
            p++;
            int64_t nMult = CENT*10;
            while (IsDigit(*p) && (nMult > 0))
            {
                nUnits += nMult * (*p++ - '0');
                nMult /= 10;
            }
            break;
        }
        if (IsSpace(*p))
            break;
        if (!IsDigit(*p))
            return false;
        strWhole.insert(strWhole.end(), *p);
    }
    for (; *p; p++)
        if (!IsSpace(*p))
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

#ifndef WIN32
void CreatePidFile(const fs::path &path, pid_t pid)
{
    fsbridge::ofstream file{path};
    if (file)
    {
        tfm::format(file, "%d\n", pid);
    }
}
#endif

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
    auto lock = std::make_unique<fsbridge::FileLock>(pathLockFile);
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
