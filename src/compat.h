// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef _BITCOIN_COMPAT_H
#define _BITCOIN_COMPAT_H 1

#include <type_traits>

// GCC 4.8 is missing some C++11 type_traits,
// https://www.gnu.org/software/gcc/gcc-5/changes.html
#if defined(__GNUC__) && __GNUC__ < 5
#define IS_TRIVIALLY_CONSTRUCTIBLE std::is_trivial
#else
#define IS_TRIVIALLY_CONSTRUCTIBLE std::is_trivially_constructible
#endif

#ifdef WIN32
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0501
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifdef FD_SETSIZE
#undef FD_SETSIZE
#endif
#define FD_SETSIZE 1024 // max number of fds in fd_set (Allows us to surpass the artificial 64 connection maximum inbound connection limit in Windows)

#include <winsock2.h>
#include <mswsock.h>
#include <windows.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <unistd.h>
typedef u_int SOCKET;
#endif

#ifdef WIN32
#define MSG_NOSIGNAL        0
#define MSG_DONTWAIT        0
typedef int socklen_t;
#else
#include "errno.h"
#define WSAGetLastError()   errno
#define WSAEINVAL           EINVAL
#define WSAEALREADY         EALREADY
#define WSAEWOULDBLOCK      EWOULDBLOCK
#define WSAEMSGSIZE         EMSGSIZE
#define WSAEINTR            EINTR
#define WSAEINPROGRESS      EINPROGRESS
#define WSAEADDRINUSE       EADDRINUSE
#define WSAENOTSOCK         EBADF
#define INVALID_SOCKET      (SOCKET)(~0)
#define SOCKET_ERROR        -1
#endif

inline int myclosesocket(SOCKET& hSocket)
{
    if (hSocket == INVALID_SOCKET)
        return WSAENOTSOCK;
#ifdef WIN32
    int ret = closesocket(hSocket);
#else
    int ret = close(hSocket);
#endif
    hSocket = INVALID_SOCKET;
    return ret;
}
#define closesocket(s)      myclosesocket(s)


#endif
