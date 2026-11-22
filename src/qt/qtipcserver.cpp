// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qtipcserver.h"
#include "qt/guilog.h"
#include "guiconstants.h"
#include "util.h"

#ifdef WIN32
#include "crypto/sha256.h"
#include "util/strencodings.h"
#endif

#include <atomic>
#include <functional>
#include <string>
#include <vector>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>

#ifdef WIN32
#include <windows.h>
#include <lmcons.h>
#include <sddl.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

using namespace boost;
using namespace boost::interprocess;
using namespace boost::posix_time;

std::string GetUriQueueName()
{
#ifdef WIN32
    // The first 16 hex characters of the SHA256 of the UTF-8 user name
    wchar_t wname[UNLEN + 1] = {};
    DWORD wlen = UNLEN + 1;
    std::string user;
    if (GetUserNameW(wname, &wlen)) {
        int nbytes = WideCharToMultiByte(CP_UTF8, 0, wname, -1, nullptr, 0, nullptr, nullptr);
        if (nbytes > 1) {
            user.resize(static_cast<size_t>(nbytes));
            WideCharToMultiByte(CP_UTF8, 0, wname, -1, &user[0], nbytes, nullptr, nullptr);
            // Drop the terminating NUL that the conversion counted
            user.resize(static_cast<size_t>(nbytes) - 1);
        }
    }

    if (user.empty()) {
        // Fall back to the string SID of the user of this process token
        static std::atomic<bool> logged{false};
        if (!logged.exchange(true)) {
            GUILogPrintf("could not get the user name for the URI queue name, using the user SID");
        }

        HANDLE token = nullptr;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
            DWORD len = 0;
            GetTokenInformation(token, TokenUser, nullptr, 0, &len);
            std::vector<unsigned char> buf(len);
            if (len > 0 && GetTokenInformation(token, TokenUser, buf.data(), len, &len)) {
                LPWSTR wsid = nullptr;
                if (ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(buf.data())->User.Sid, &wsid)) {
                    int nbytes = WideCharToMultiByte(CP_UTF8, 0, wsid, -1, nullptr, 0, nullptr, nullptr);
                    if (nbytes > 1) {
                        user.resize(static_cast<size_t>(nbytes));
                        WideCharToMultiByte(CP_UTF8, 0, wsid, -1, &user[0], nbytes, nullptr, nullptr);
                        user.resize(static_cast<size_t>(nbytes) - 1);
                    }
                    LocalFree(wsid);
                }
            }
            CloseHandle(token);
        }

        // An empty name makes ipcInit() and the client refuse to use a queue, never a shared name
        if (user.empty()) return std::string();
    }

    unsigned char hash[CSHA256::OUTPUT_SIZE];
    CSHA256().Write(reinterpret_cast<const unsigned char*>(user.data()), user.size()).Finalize(hash);

    return "GridcoinURI-" + HexStr(Span<const unsigned char>(hash, CSHA256::OUTPUT_SIZE)).substr(0, 16);
#else
    return strprintf("GridcoinURI-%u", static_cast<unsigned int>(geteuid()));
#endif
}

#if defined MAC_OSX || defined __FreeBSD__
// URI handling not implemented on OSX yet

void ipcScanRelay(int argc, char *argv[]) { }
void ipcInit(int argc, char *argv[], std::function<void(const std::string&)> uri_handler) { }
void ipcShutdown() { }

#else

//! GUI-process-local stop flag for the URI-listener thread (grc-gui-ipc). Set by
//! ipcShutdown() when the GUI event loop returns; the thread polls it once per
//! 100 ms receive timeout. Replaces the former read of the core fShutdown global
//! -- the URI server is owned by this GUI process, so its exit is gated on the
//! GUI quitting, never on core teardown state.
static std::atomic<bool> g_ipc_shutdown{false};

//! GUI-supplied handler invoked with each received "gridcoin:" URI. Set by
//! ipcInit() before the listener thread starts and only read afterwards (the
//! thread is joined via g_ipc_shutdown before the GUI tears down), so no
//! additional synchronization is needed. Replaces the former
//! uiInterface.ThreadSafeHandleURI signal dispatch.
static std::function<void(const std::string&)> g_uri_handler;

void ipcShutdown()
{
    g_ipc_shutdown = true;
}

static void ipcThread2(void* pArg);

//! True if the named queue is owned by the current user and accessible by the
//! owner only (a restrictive umask can leave the mode below 0600). Only
//! Linux exposes the queue as a file (under /dev/shm) that can be checked; on
//! other systems the per-user name is all there is and this returns true.
static bool UriQueueIsOurs(const std::string& name)
{
#ifdef __linux__
    struct stat st;
    if (stat(("/dev/shm/" + name).c_str(), &st) != 0) return false;

    // mode_t is unsigned on this platform, compare as such
    if (st.st_uid == geteuid() && (st.st_mode & static_cast<::mode_t>(077)) == static_cast<::mode_t>(0)) return true;

    GUILogPrintf("URI queue %s has owner uid %u and mode %o", name, static_cast<unsigned int>(st.st_uid),
                 static_cast<unsigned int>(st.st_mode & static_cast<::mode_t>(07777)));
    return false;
#else
    return true;
#endif
}

//! Creation permissions for the queue, owner only on POSIX.
#ifndef WIN32
static boost::interprocess::permissions UriQueuePermissions()
{
    return boost::interprocess::permissions(0600);
}
#else
static boost::interprocess::permissions UriQueuePermissions()
{
    return boost::interprocess::permissions();
}
#endif

static bool ipcScanCmd(int argc, char *argv[], bool fRelay)
{
    // Check for URI in argv
    bool fSent = false;
    for (int i = 1; i < argc; i++)
    {
        if (boost::algorithm::istarts_with(argv[i], "gridcoin:"))
        {
            const char *strURI = argv[i];
            try {
                const std::string queue_name = GetUriQueueName();
                if (queue_name.empty()) continue;
                boost::interprocess::message_queue mq(boost::interprocess::open_only, queue_name.c_str());

                // Never send into a queue that belongs to another user. Treat it as if there were no queue.
                if (!UriQueueIsOurs(queue_name)) {
                    tfm::format(std::cerr, "URI handling disabled: the queue %s belongs to another user\n", queue_name);
                    continue;
                }

                if (mq.try_send(strURI, strlen(strURI), 0))
                    fSent = true;
                else if (fRelay)
                    break;
            }
            catch (boost::interprocess::interprocess_exception &ex) {
                // don't log the "file not found" exception, because that's normal for
                // the first start of the first instance
                if (ex.get_error_code() != boost::interprocess::not_found_error || !fRelay)
                {
                    tfm::format(std::cerr, "main() - boost interprocess exception #%d: %s", ex.get_error_code(), ex.what());
                    break;
                }
            }
        }
    }
    return fSent;
}

void ipcScanRelay(int argc, char *argv[])
{
    if (ipcScanCmd(argc, argv, true))
        exit(0);
}

static void ipcThread(void* pArg)
{
    // Make this thread recognisable as the GUI-IPC thread
    RenameThread("grc-gui-ipc");

    try
    {
        ipcThread2(pArg);
    }
    catch (std::exception& e) {
        PrintExceptionContinue(&e, "ipcThread()");
    } catch (...) {
        PrintExceptionContinue(nullptr, "ipcThread()");
    }
    GUILogPrintf("ipcThread exited");
}

static void ipcThread2(void* pArg)
{
    GUILogPrint(GUILogCategory::VERBOSE, "ipcThread started");

    message_queue* mq = (message_queue*)pArg;
    char buffer[MAX_URI_LENGTH + 1] = "";
    size_t nSize = 0;
    unsigned int nPriority = 0;

    while (true)
    {
        ptime d = boost::posix_time::microsec_clock::universal_time() + millisec(100);
        if (mq->timed_receive(&buffer, sizeof(buffer), nSize, nPriority, d))
        {
            if (g_uri_handler)
                g_uri_handler(std::string(buffer, nSize));
            UninterruptibleSleep(std::chrono::seconds{1});
        }

        if (g_ipc_shutdown)
            break;
    }

    // Remove message queue. The queue was verified as ours before this thread started.
    message_queue::remove(GetUriQueueName().c_str());
    // Cleanup allocated memory
    delete mq;
}

void ipcInit(int argc, char *argv[], std::function<void(const std::string&)> uri_handler)
{
    g_uri_handler = std::move(uri_handler);
    g_ipc_shutdown = false;

    const std::string queue_name = GetUriQueueName();
    if (queue_name.empty()) {
        GUILogPrintf("URI handling disabled: the per-user queue name could not be determined");
        return;
    }

    message_queue* mq = nullptr;
    char buffer[MAX_URI_LENGTH + 1] = "";
    size_t nSize = 0;
    unsigned int nPriority = 0;

    try {
        mq = new message_queue(open_or_create, queue_name.c_str(), 2, MAX_URI_LENGTH, UriQueuePermissions());

        // Do not use, drain or remove a queue that belongs to another user
        if (!UriQueueIsOurs(queue_name)) {
            GUILogPrintf("URI handling disabled: the queue %s belongs to another user", queue_name);
            delete mq;
            return;
        }

        // Make sure we don't lose any bitcoin: URIs
        for (int i = 0; i < 2; i++)
        {
            ptime d = boost::posix_time::microsec_clock::universal_time() + millisec(1);
            if (mq->timed_receive(&buffer, sizeof(buffer), nSize, nPriority, d))
            {
                if (g_uri_handler)
                    g_uri_handler(std::string(buffer, nSize));
            }
            else
                break;
        }

        // Make sure only one bitcoin instance is listening
        message_queue::remove(queue_name.c_str());
        delete mq;

        mq = new message_queue(open_or_create, queue_name.c_str(), 2, MAX_URI_LENGTH, UriQueuePermissions());

        if (!UriQueueIsOurs(queue_name)) {
            GUILogPrintf("URI handling disabled: the queue %s belongs to another user", queue_name);
            delete mq;
            mq = nullptr;
            return;
        }
    }
    catch (interprocess_exception &ex) {
        GUILogPrintf("ipcInit() - boost interprocess exception #%d: %s", ex.get_error_code(), ex.what());
        return;
    }

    if (!NewThread(ipcThread, mq))
    {
        delete mq;
        return;
    }

    ipcScanCmd(argc, argv, false);
}

#endif
