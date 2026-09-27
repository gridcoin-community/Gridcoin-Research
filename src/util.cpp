// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "netbase.h" // for AddTimeData
#include "support/cleanse.h"
#include "sync.h"
#include "timedata.h"
#include "version.h"
#include "node/ui_interface.h"
#include <random.h>
#include "util.h"
#include <util/strencodings.h>
#include <util/string.h>

#include <boost/date_time/posix_time/posix_time.hpp>  //For day of year
#include <cmath>
#include <boost/lexical_cast.hpp>

#include <boost/thread.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/newline.hpp>
#include <openssl/crypto.h>
#include <cstdarg>
#include <codecvt>

using namespace std;

// Moved from main.cpp (issue #3125 C9); declared in util.h.
const std::string strMessageMagic = "Gridcoin Signed Message:\n";

double CoinToDouble(double surrogate)
{
    //Converts satoshis to a human double amount
    double coin = (double)surrogate/(double)COIN;
    return coin;
}

/** A map that contains all the currently held directory locks. After
 * successful locking, these will be held here until the global destructor
 * cleans them up and thus automatically unlocks them, or ReleaseDirectoryLocks
 * is called.
 */
static std::map<std::string, std::unique_ptr<fsbridge::FileLock>> dir_locks;
/** Mutex to protect dir_locks. */
static std::mutex cs_dir_locks;

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
    if (!ValidAsCString(str)) {
        return false;
    }
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

    int64_t nWhole = 0;

    // strWhole is empty for inputs that carry no leading digits -- ".5", ".",
    // "" and " " all reach here with nothing accumulated -- and ParseInt64
    // rejects an empty string. Return false so the caller reports a bad value
    // rather than terminating. ParseFixedPoint, which the RPC amount path uses,
    // already rejects the same inputs the same way.
    if (!ParseInt64(strWhole, &nWhole))
        return false;

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
    fs::path tmpFile = directory / fs::path(HexStr(FastRandomContext().randbytes(8)));

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

// Atomic: written under g_time_offset_votes_mutex by AddTimeData() (the
// message-handler thread, from ProcessMessage's VERSION-handling path),
// RemoveTimeData() (the socket thread, deleting a disconnected node) and
// ResetTimeDataForTesting() (unit tests), and read without a lock through
// GetTimeOffset() -> GetAdjustedTime() from many threads (GUI, miner,
// validation, RPC). Bitcoin Core fixed the same pattern years ago by making
// this atomic.
static std::atomic<int64_t> nTimeOffset{0};

int64_t GetTimeOffset()
{
    return nTimeOffset;
}

int64_t GetAdjustedTime()
{
    return GetTime() + GetTimeOffset();
}

// Peer time votes: one per connected outbound peer, counted once per network
// group (see TimeOffsetVotes). Only AddTimeData(), RemoveTimeData() and
// ResetTimeDataForTesting() touch them. The mutex is a leaf: nothing else is
// acquired while it is held, and AddTimeData() releases it before it logs,
// warns or calls into the UI.
static Mutex g_time_offset_votes_mutex;
static TimeOffsetVotes g_time_offset_votes GUARDED_BY(g_time_offset_votes_mutex);

//! Store the offset the live votes give, and return whether the 70-minute check
//! rejected it. Called under the votes mutex, so the stored offset always
//! matches the latest vote set, whichever thread changed it last.
static bool StoreTimeOffset(const std::optional<int64_t>& median) EXCLUSIVE_LOCKS_REQUIRED(g_time_offset_votes_mutex)
{
    // We believe the median of the other nodes 95% and our own node's time ("0" initial offset) 5%. This will also act to gently converge the network to consensus UTC, in case
    // the entire network is displaced for some reason. Without a quorum of live votes, our own
    // clock is all we have.
    int64_t offset = median ? static_cast<int64_t>(0.95 * *median) : 0;
    // Only let other nodes change our time by so much
    const bool rejected = abs64(offset) >= 70 * 60;
    if (rejected)
        offset = 0;
    nTimeOffset = offset;
    return rejected;
}

void AddTimeData(int64_t node_id, const CNetAddr& ip, int64_t nOffsetSample)
{
    // The vote lasts until RemoveTimeData() withdraws it when this peer's
    // connection is deleted, and peers in one network group count once.
    const TimeOffsetVotes::Group group = ip.GetGroup();
    std::vector<int64_t> vSorted;
    bool rejected;
    {
        LOCK(g_time_offset_votes_mutex);
        rejected = StoreTimeOffset(g_time_offset_votes.Add(node_id, group, nOffsetSample));
        vSorted = g_time_offset_votes.Sorted();
    }

    LogPrint(BCLog::LogFlags::NOISY, "Added time data, votes %d, offset %+" PRId64 " (%+" PRId64 " minutes)", vSorted.size(), nOffsetSample, nOffsetSample/60);
    if (rejected)
    {
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
                uiInterface.ThreadSafeMessageBox(strMessage+" ", string("Gridcoin"), CClientUIInterface::MSG_WARNING);
            }
        }
    }
    if (LogInstance().WillLogCategory(BCLog::LogFlags::NOISY)) {
        for (auto const& n : vSorted)
            LogPrintf("%+" PRId64 "  ", n);
        LogPrintf("|  ");
    }
    const int64_t offset = GetTimeOffset();
    LogPrint(BCLog::LogFlags::NOISY, "nTimeOffset = %+" PRId64 "  (%+" PRId64 " minutes)", offset, offset/60);
}

void RemoveTimeData(int64_t node_id)
{
    // Recompute over the votes still live, so a departed peer leaves no trace
    // in the offset either. The 70-minute check still applies, silently: its
    // warning calls into the UI, and this runs on the socket thread under
    // m_nodes_mutex.
    {
        LOCK(g_time_offset_votes_mutex);
        if (!g_time_offset_votes.Remove(node_id)) {
            return;
        }
        StoreTimeOffset(g_time_offset_votes.Median());
    }

    LogPrint(BCLog::LogFlags::NOISY, "Withdrew time vote of peer %d, nTimeOffset = %+" PRId64, node_id, GetTimeOffset());
}

void ResetTimeDataForTesting()
{
    LOCK(g_time_offset_votes_mutex);
    g_time_offset_votes = TimeOffsetVotes{};
    nTimeOffset = 0;
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

#if HAVE_SYSTEM
void runCommand(std::string strCommand)
{
#ifndef WIN32
    int nErr = ::system(strCommand.c_str());
#else
    int nErr = ::_wsystem(std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>,wchar_t>().from_bytes(strCommand).c_str());
#endif
    if (nErr)
        LogPrintf("runCommand error: system(%s) returned %d", strCommand, nErr);
}
#endif

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
#if defined(__linux__) && !defined(__GLIBC__)
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
        // the libc implementation. For now, we let glibc do its thing: 2 MB
        // matches glibc's typical x86 default, so this only raises musl to
        // parity rather than changing glibc behaviour.
        //
        // The guard tests __GLIBC__, the C library macro, and NOT __GLIBCXX__,
        // which libstdc++ defines. They are different libraries: Alpine's
        // ordinary C++ toolchain is musl + libstdc++, so __GLIBCXX__ IS
        // defined there and a !__GLIBCXX__ guard silently skipped this on the
        // one platform it was written for. Verified on alpine:latest with g++:
        // __GLIBCXX__ defined, __GLIBC__ absent, default pthread stack 131072
        // bytes -- 128 KB, i.e. exactly the size the comment above says scrypt
        // overruns.
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
#if defined(__linux__) && !defined(__GLIBC__)
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
        // the libc implementation. For now, we let glibc do its thing: 2 MB
        // matches glibc's typical x86 default, so this only raises musl to
        // parity rather than changing glibc behaviour.
        //
        // The guard tests __GLIBC__, the C library macro, and NOT __GLIBCXX__,
        // which libstdc++ defines. They are different libraries: Alpine's
        // ordinary C++ toolchain is musl + libstdc++, so __GLIBCXX__ IS
        // defined there and a !__GLIBCXX__ guard silently skipped this on the
        // one platform it was written for. Verified on alpine:latest with g++:
        // __GLIBCXX__ defined, __GLIBC__ absent, default pthread stack 131072
        // bytes -- 128 KB, i.e. exactly the size the comment above says scrypt
        // overruns.
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
