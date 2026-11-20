// Copyright (c) 2010-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UTIL_SYSERROR_H
#define BITCOIN_UTIL_SYSERROR_H

#include <exception>
#include <optional>
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

/** What std::system_error(code, category, what_arg) makes of its what(): what_arg + ": " + message.
 * Returns the what_arg of such a \p what given the code's \p message (empty when there was none, and
 * the message alone counts as that), or nothing when \p what does not end as that, so that what_arg
 * cannot be told from the message. Only the final message is removed.
 */
std::optional<std::string> WithoutSystemMessage(const std::string& what, const std::string& message);

/** e.what(), except that a std::system_error of the system category on Windows is worded from its
 * what_arg and the error code alone (Win32ErrorText), because Windows words the code's message in the
 * display language and a log line, a console or an error text carries ASCII only. When the what_arg
 * cannot be found in what(), only the error text is returned. Off Windows this is e.what().
 */
std::string AsciiWhat(const std::exception& e);

#ifdef WIN32
/** The Win32 error \p error (a DWORD) as "<message> (error <n>)", the message being Windows' own when
 * PrintableAscii accepts it, and as "error <n>" otherwise.
 */
std::string Win32ErrorText(unsigned long error);
#endif

#endif // BITCOIN_UTIL_SYSERROR_H
