// Copyright (c) 2010-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UTIL_SYSERROR_H
#define BITCOIN_UTIL_SYSERROR_H

#include <string>

/** Return system error string from errno value. Use this instead of
 * std::strerror, which is not thread-safe. For network errors use
 * NetworkErrorString from sock.h instead.
 */
std::string SysErrorString(int err);

/** \p message without the line break, spaces and full stop Windows ends a system message with,
 * or an empty string when what is left is not entirely printable ASCII (0x20 to 0x7E). Windows words
 * its messages in the display language, and a log line, a console or an error text carries ASCII only.
 */
std::string PrintableAscii(std::string message);

#ifdef WIN32
/** The Win32 error \p error (a DWORD) as "<message> (error <n>)", the message being Windows' own when
 * PrintableAscii accepts it, and as "error <n>" otherwise.
 */
std::string Win32ErrorText(unsigned long error);
#endif

#endif // BITCOIN_UTIL_SYSERROR_H
