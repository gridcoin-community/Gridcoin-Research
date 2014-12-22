// CkFtp2W.h: interface for the CkFtp2W class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkFtp2W_H
#define _CkFtp2W_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkWideCharBase.h"

class CkByteData;
class CkDateTimeW;
class CkCertW;
class CkFtp2ProgressW;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkFtp2W
class CK_VISIBLE_PUBLIC CkFtp2W  : public CkWideCharBase
{
    private:
	bool m_cbOwned;
	CkFtp2ProgressW *m_callback;

	// Don't allow assignment or copying these objects.
	CkFtp2W(const CkFtp2W &);
	CkFtp2W &operator=(const CkFtp2W &);

    public:
	CkFtp2W(void);
	virtual ~CkFtp2W(void);

	static CkFtp2W *createNew(void);
	

	CkFtp2W(bool bCallbackOwned);
	static CkFtp2W *createNew(bool bCallbackOwned);

	
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	CkFtp2ProgressW *get_EventCallbackObject(void) const;
	void put_EventCallbackObject(CkFtp2ProgressW *progress);


	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------
	// Some FTP servers require an Account name in addition to login/password. This
	// property can be set for those servers with this requirement.
	void get_Account(CkString &str);
	// Some FTP servers require an Account name in addition to login/password. This
	// property can be set for those servers with this requirement.
	const wchar_t *account(void);
	// Some FTP servers require an Account name in addition to login/password. This
	// property can be set for those servers with this requirement.
	void put_Account(const wchar_t *newVal);

	// When Active (i.e. PORT) mode is used (opposite of Passive), the client-side is
	// responsible for choosing a random port for each data connection. (Note: In the
	// FTP protocol, each data transfer occurs on a separate TCP/IP connection.
	// Commands are sent over the control channel (port 21 for non-SSL, port 990 for
	// SSL).)
	// 
	// This property, along with ActivePortRangeStart, allows the client to specify a
	// range of ports for data connections.
	// 
	int get_ActivePortRangeEnd(void);
	// When Active (i.e. PORT) mode is used (opposite of Passive), the client-side is
	// responsible for choosing a random port for each data connection. (Note: In the
	// FTP protocol, each data transfer occurs on a separate TCP/IP connection.
	// Commands are sent over the control channel (port 21 for non-SSL, port 990 for
	// SSL).)
	// 
	// This property, along with ActivePortRangeStart, allows the client to specify a
	// range of ports for data connections.
	// 
	void put_ActivePortRangeEnd(int newVal);

	// This property, along with ActivePortRangeEnd, allows the client to specify a
	// range of ports for data connections when in Active mode.
	int get_ActivePortRangeStart(void);
	// This property, along with ActivePortRangeEnd, allows the client to specify a
	// range of ports for data connections when in Active mode.
	void put_ActivePortRangeStart(int newVal);

	// If set to a non-zero value, causes an ALLO command, with this size as the
	// parameter, to be automatically sent when uploading files to an FTP server.
	// 
	// This command could be required by some servers to reserve sufficient storage
	// space to accommodate the new file to be transferred.
	// 
	unsigned long get_AllocateSize(void);
	// If set to a non-zero value, causes an ALLO command, with this size as the
	// parameter, to be automatically sent when uploading files to an FTP server.
	// 
	// This command could be required by some servers to reserve sufficient storage
	// space to accommodate the new file to be transferred.
	// 
	void put_AllocateSize(unsigned long newVal);

	// The number of bytes received during an asynchronous FTP download. This property
	// is updated in real-time and an application may periodically fetch and display
	// it's value while the download is in progress.
	unsigned long get_AsyncBytesReceived(void);

	// Same as AsyncBytesReceived, but returns the value as a 64-bit integer.
	__int64 get_AsyncBytesReceived64(void);

	// The number of bytes received during an asynchronous FTP download. This property
	// is updated in real-time and an application may periodically fetch and display
	// it's value while the download is in progress.
	void get_AsyncBytesReceivedStr(CkString &str);
	// The number of bytes received during an asynchronous FTP download. This property
	// is updated in real-time and an application may periodically fetch and display
	// it's value while the download is in progress.
	const wchar_t *asyncBytesReceivedStr(void);

	// The number of bytes sent during an asynchronous FTP upload. This property is
	// updated in real-time and an application may periodically fetch and display it's
	// value while the upload is in progress.
	unsigned long get_AsyncBytesSent(void);

	// Same as AsyncBytesSent, but returns the value as a 64-bit integer.
	__int64 get_AsyncBytesSent64(void);

	// The number of bytes sent during an asynchronous FTP upload. This string property
	// is updated in real-time and an application may periodically fetch and display
	// it's value while the upload is in progress.
	void get_AsyncBytesSentStr(CkString &str);
	// The number of bytes sent during an asynchronous FTP upload. This string property
	// is updated in real-time and an application may periodically fetch and display
	// it's value while the upload is in progress.
	const wchar_t *asyncBytesSentStr(void);

	// Set to true if the asynchronous transfer (download or upload) is finished.
	bool get_AsyncFinished(void);

	// The last-error information for an asynchronous (background) file transfer.
	void get_AsyncLog(CkString &str);
	// The last-error information for an asynchronous (background) file transfer.
	const wchar_t *asyncLog(void);

	// Set to true if the asynchronous file transfer succeeded.
	bool get_AsyncSuccess(void);

	// Same as AuthTls, except the command sent to the FTP server is "AUTH SSL" instead
	// of "AUTH TLS". Most FTP servers accept either. AuthTls is more commonly used. If
	// a particular server has trouble with AuthTls, try AuthSsl instead.
	bool get_AuthSsl(void);
	// Same as AuthTls, except the command sent to the FTP server is "AUTH SSL" instead
	// of "AUTH TLS". Most FTP servers accept either. AuthTls is more commonly used. If
	// a particular server has trouble with AuthTls, try AuthSsl instead.
	void put_AuthSsl(bool newVal);

	// Set this to true to switch to a TLS 1.0 encrypted channel. This property
	// should be set prior to connecting. If this property is set, the port typically
	// remains at it's default (21) and the Ssl property should *not* be set. When
	// AuthTls is used, all control and data transmissions are encrypted. If your FTP
	// client is behind a network-address-translating router, you may need to call
	// ClearControlChannel after connecting and authenticating (i.e. after calling the
	// Connect method). This keeps all data transmissions encrypted, but clears the
	// control channel so that commands are sent unencrypted, thus allowing the router
	// to translate network IP numbers in FTP commands.
	bool get_AuthTls(void);
	// Set this to true to switch to a TLS 1.0 encrypted channel. This property
	// should be set prior to connecting. If this property is set, the port typically
	// remains at it's default (21) and the Ssl property should *not* be set. When
	// AuthTls is used, all control and data transmissions are encrypted. If your FTP
	// client is behind a network-address-translating router, you may need to call
	// ClearControlChannel after connecting and authenticating (i.e. after calling the
	// Connect method). This keeps all data transmissions encrypted, but clears the
	// control channel so that commands are sent unencrypted, thus allowing the router
	// to translate network IP numbers in FTP commands.
	void put_AuthTls(bool newVal);

	// When true (which is the default value), a "FEAT" command is automatically sent
	// to the FTP server immediately after connecting. This allows the Chilkat FTP2
	// component to know more about the server's capabilities and automatically adjust
	// any applicable internal settings based on the response. In rare cases, some FTP
	// servers reject the "FEAT" command and close the connection. Usually, if an FTP
	// server does not implement FEAT, a harmless "command not understood" response is
	// returned.
	// 
	// Set this property to false to prevent the FEAT command from being sent.
	// 
	bool get_AutoFeat(void);
	// When true (which is the default value), a "FEAT" command is automatically sent
	// to the FTP server immediately after connecting. This allows the Chilkat FTP2
	// component to know more about the server's capabilities and automatically adjust
	// any applicable internal settings based on the response. In rare cases, some FTP
	// servers reject the "FEAT" command and close the connection. Usually, if an FTP
	// server does not implement FEAT, a harmless "command not understood" response is
	// returned.
	// 
	// Set this property to false to prevent the FEAT command from being sent.
	// 
	void put_AutoFeat(bool newVal);

	// If true, then the following will occur when a connection is made to an FTP
	// server:
	// 
	// 1) If the Port property = 990, then sets AuthTls = false, AuthSsl = false,
	// and Ssl = true
	// 2) If the Port property = 21, sets Ssl = false
	// 
	// The default value of this property is true.
	// 
	bool get_AutoFix(void);
	// If true, then the following will occur when a connection is made to an FTP
	// server:
	// 
	// 1) If the Port property = 990, then sets AuthTls = false, AuthSsl = false,
	// and Ssl = true
	// 2) If the Port property = 21, sets Ssl = false
	// 
	// The default value of this property is true.
	// 
	void put_AutoFix(bool newVal);

	// Forces the component to retrieve each file's size prior to downloading for the
	// purpose of monitoring percentage completion progress. For many FTP servers, this
	// is not required and therefore for performance reasons this property defaults to
	// false.
	bool get_AutoGetSizeForProgress(void);
	// Forces the component to retrieve each file's size prior to downloading for the
	// purpose of monitoring percentage completion progress. For many FTP servers, this
	// is not required and therefore for performance reasons this property defaults to
	// false.
	void put_AutoGetSizeForProgress(bool newVal);

	// When true (which is the default value), a "SYST" command is automatically sent
	// to the FTP server immediately after connecting. This allows the Chilkat FTP2
	// component to know more about the server and automatically adjust any applicable
	// internal settings based on the response. If the SYST command causes trouble
	// (which is rare), this behavior can be turned off by setting this property equal
	// to false.
	bool get_AutoSyst(void);
	// When true (which is the default value), a "SYST" command is automatically sent
	// to the FTP server immediately after connecting. This allows the Chilkat FTP2
	// component to know more about the server and automatically adjust any applicable
	// internal settings based on the response. If the SYST command causes trouble
	// (which is rare), this behavior can be turned off by setting this property equal
	// to false.
	void put_AutoSyst(bool newVal);

	// Many FTP servers support the XCRC command. The Chilkat FTP component will
	// automatically know if XCRC is supported because it automatically sends a FEAT
	// command to the server immediately after connecting.
	// 
	// If this property is set to true, then all uploads will be automatically
	// verified by sending an XCRC command immediately after the transfer completes. If
	// the CRC is not verified, the upload method (such as PutFile) will return a
	// failed status.
	// 
	// To prevent XCRC checking, set this property to false.
	// 
	bool get_AutoXcrc(void);
	// Many FTP servers support the XCRC command. The Chilkat FTP component will
	// automatically know if XCRC is supported because it automatically sends a FEAT
	// command to the server immediately after connecting.
	// 
	// If this property is set to true, then all uploads will be automatically
	// verified by sending an XCRC command immediately after the transfer completes. If
	// the CRC is not verified, the upload method (such as PutFile) will return a
	// failed status.
	// 
	// To prevent XCRC checking, set this property to false.
	// 
	void put_AutoXcrc(bool newVal);

	// If set to a non-zero value, the FTP2 component will bandwidth throttle all
	// downloads to this value.
	// 
	// The default value of this property is 0. The value should be specified in
	// bytes/second.
	// 
	// Note: It is difficult to throttle very small downloads. (For example, how do you
	// bandwidth throttle a 1-byte download???) As the downloaded file size gets
	// larger, the transfer rate will better approximate this property's setting.
	// 
	int get_BandwidthThrottleDown(void);
	// If set to a non-zero value, the FTP2 component will bandwidth throttle all
	// downloads to this value.
	// 
	// The default value of this property is 0. The value should be specified in
	// bytes/second.
	// 
	// Note: It is difficult to throttle very small downloads. (For example, how do you
	// bandwidth throttle a 1-byte download???) As the downloaded file size gets
	// larger, the transfer rate will better approximate this property's setting.
	// 
	void put_BandwidthThrottleDown(int newVal);

	// If set to a non-zero value, the FTP2 component will bandwidth throttle all
	// uploads to this value.
	// 
	// The default value of this property is 0. The value should be specified in
	// bytes/second.
	// 
	// Note: It is difficult to throttle very small uploads. (For example, how do you
	// bandwidth throttle a 1-byte upload???) As the uploaded file size gets larger,
	// the transfer rate will better approximate this property's setting.
	// 
	int get_BandwidthThrottleUp(void);
	// If set to a non-zero value, the FTP2 component will bandwidth throttle all
	// uploads to this value.
	// 
	// The default value of this property is 0. The value should be specified in
	// bytes/second.
	// 
	// Note: It is difficult to throttle very small uploads. (For example, how do you
	// bandwidth throttle a 1-byte upload???) As the uploaded file size gets larger,
	// the transfer rate will better approximate this property's setting.
	// 
	void put_BandwidthThrottleUp(int newVal);

	// The IP address to use for computers with multiple network interfaces or IP
	// addresses. For computers with a single network interface (i.e. most computers),
	// this property should not be set. For multihoming computers, the default IP
	// address is automatically used if this property is not set.
	// 
	// The IP address is a string such as in dotted notation using numbers, not domain
	// names, such as "165.164.55.124".
	// 
	void get_ClientIpAddress(CkString &str);
	// The IP address to use for computers with multiple network interfaces or IP
	// addresses. For computers with a single network interface (i.e. most computers),
	// this property should not be set. For multihoming computers, the default IP
	// address is automatically used if this property is not set.
	// 
	// The IP address is a string such as in dotted notation using numbers, not domain
	// names, such as "165.164.55.124".
	// 
	const wchar_t *clientIpAddress(void);
	// The IP address to use for computers with multiple network interfaces or IP
	// addresses. For computers with a single network interface (i.e. most computers),
	// this property should not be set. For multihoming computers, the default IP
	// address is automatically used if this property is not set.
	// 
	// The IP address is a string such as in dotted notation using numbers, not domain
	// names, such as "165.164.55.124".
	// 
	void put_ClientIpAddress(const wchar_t *newVal);

	// Indicates the charset to be used for commands sent to the FTP server. The
	// command charset must match what the FTP server is expecting in order to
	// communicate non-English characters correctly. The default value of this property
	// is "ansi".
	// 
	// This property may be updated to "utf-8" after connecting because a "FEAT"
	// command is automatically sent to get the features of the FTP server. If UTF8 is
	// indicated as a feature, then this property is automatically changed to "utf-8".
	// 
	void get_CommandCharset(CkString &str);
	// Indicates the charset to be used for commands sent to the FTP server. The
	// command charset must match what the FTP server is expecting in order to
	// communicate non-English characters correctly. The default value of this property
	// is "ansi".
	// 
	// This property may be updated to "utf-8" after connecting because a "FEAT"
	// command is automatically sent to get the features of the FTP server. If UTF8 is
	// indicated as a feature, then this property is automatically changed to "utf-8".
	// 
	const wchar_t *commandCharset(void);
	// Indicates the charset to be used for commands sent to the FTP server. The
	// command charset must match what the FTP server is expecting in order to
	// communicate non-English characters correctly. The default value of this property
	// is "ansi".
	// 
	// This property may be updated to "utf-8" after connecting because a "FEAT"
	// command is automatically sent to get the features of the FTP server. If UTF8 is
	// indicated as a feature, then this property is automatically changed to "utf-8".
	// 
	void put_CommandCharset(const wchar_t *newVal);

	// If the Connect method fails, this property can be checked to determine the
	// reason for failure.
	// 
	// Possible values are:
	// 0 = success
	// Normal (non-SSL) sockets:
	// 1 = empty hostname
	// 2 = DNS lookup failed
	// 3 = DNS timeout
	// 4 = Aborted by application.
	// 5 = Internal failure.
	// 6 = Connect Timed Out
	// 7 = Connect Rejected (or failed for some other reason)
	// SSL:
	// 100 = Internal schannel error
	// 101 = Failed to create credentials
	// 102 = Failed to send initial message to proxy.
	// 103 = Handshake failed.
	// 104 = Failed to obtain remote certificate.
	// 105 = Failed to verify server certificate.
	// 106 = Server certificate requirement failed .
	//           (set via the SetSslCertRequirement method).
	// FTP:
	// 200 = Connected, but failed to receive greeting from FTP server.
	// 201 = Failed to do AUTH TLS or AUTH SSL.
	// Protocol/Component:
	// 300 = asynch op in progress
	// 301 = login failure.
	// 
	int get_ConnectFailReason(void);

	// Maximum number of seconds to wait when connecting to an FTP server.
	int get_ConnectTimeout(void);
	// Maximum number of seconds to wait when connecting to an FTP server.
	void put_ConnectTimeout(int newVal);

	// True if the FTP2 component was able to establish a TCP/IP connection to the FTP
	// server after calling Connect.
	bool get_ConnectVerified(void);

	// Used to control CRLF line endings when downloading text files in ASCII mode. The
	// default value is 0.
	// 
	// Possible values are:
	// 0 = Do nothing.  The line-endings are not modified as received from the FTP server.
	// 1 = Convert all line-endings to CR+LF
	// 2 = Convert all line-endings to bare LF's
	// 3 = Convert all line-endings to bare CR's
	// 
	int get_CrlfMode(void);
	// Used to control CRLF line endings when downloading text files in ASCII mode. The
	// default value is 0.
	// 
	// Possible values are:
	// 0 = Do nothing.  The line-endings are not modified as received from the FTP server.
	// 1 = Convert all line-endings to CR+LF
	// 2 = Convert all line-endings to bare LF's
	// 3 = Convert all line-endings to bare CR's
	// 
	void put_CrlfMode(int newVal);

	// Indicates the charset of the directory listings received from the FTP server.
	// The FTP2 client must interpret the directory listing bytes using the correct
	// character encoding in order to correctly receive non-English characters. The
	// default value of this property is "ansi".
	// 
	// This property may be updated to "utf-8" after connecting because a "FEAT"
	// command is automatically sent to get the features of the FTP server. If UTF8 is
	// indicated as a feature, then this property is automatically changed to "utf-8".
	// 
	void get_DirListingCharset(CkString &str);
	// Indicates the charset of the directory listings received from the FTP server.
	// The FTP2 client must interpret the directory listing bytes using the correct
	// character encoding in order to correctly receive non-English characters. The
	// default value of this property is "ansi".
	// 
	// This property may be updated to "utf-8" after connecting because a "FEAT"
	// command is automatically sent to get the features of the FTP server. If UTF8 is
	// indicated as a feature, then this property is automatically changed to "utf-8".
	// 
	const wchar_t *dirListingCharset(void);
	// Indicates the charset of the directory listings received from the FTP server.
	// The FTP2 client must interpret the directory listing bytes using the correct
	// character encoding in order to correctly receive non-English characters. The
	// default value of this property is "ansi".
	// 
	// This property may be updated to "utf-8" after connecting because a "FEAT"
	// command is automatically sent to get the features of the FTP server. If UTF8 is
	// indicated as a feature, then this property is automatically changed to "utf-8".
	// 
	void put_DirListingCharset(const wchar_t *newVal);

	// The average download rate in bytes/second. This property is updated in real-time
	// during any FTP download (asynchronous or synchronous).
	int get_DownloadTransferRate(void);

	// If set, forces the IP address used in the PORT command for Active mode (i.e.
	// non-passive) data transfers. This string property should be set to the IP
	// address in dotted notation, such as "233.190.65.31".
	void get_ForcePortIpAddress(CkString &str);
	// If set, forces the IP address used in the PORT command for Active mode (i.e.
	// non-passive) data transfers. This string property should be set to the IP
	// address in dotted notation, such as "233.190.65.31".
	const wchar_t *forcePortIpAddress(void);
	// If set, forces the IP address used in the PORT command for Active mode (i.e.
	// non-passive) data transfers. This string property should be set to the IP
	// address in dotted notation, such as "233.190.65.31".
	void put_ForcePortIpAddress(const wchar_t *newVal);

	// The initial greeting received from the FTP server after connecting.
	void get_Greeting(CkString &str);
	// The initial greeting received from the FTP server after connecting.
	const wchar_t *greeting(void);

	// Chilkat FTP2 supports MODE Z, which is a transfer mode implemented by some FTP
	// servers. It allows for files to be uploaded and downloaded using compressed
	// streams (using the zlib deflate algorithm). This is a read-only property. It
	// will be set to true if the FTP2 component detects that your FTP server
	// supports MODE Z. Otherwise it is set to false.
	bool get_HasModeZ(void);

	// This is the number of milliseconds between each AbortCheck event callback. The
	// AbortCheck callback allows an application to abort any FTP operation prior to
	// completion. If HeartbeatMs is 0, no AbortCheck event callbacks will occur. Also,
	// AbortCheck callbacks do not occur when doing asynchronous transfers.
	int get_HeartbeatMs(void);
	// This is the number of milliseconds between each AbortCheck event callback. The
	// AbortCheck callback allows an application to abort any FTP operation prior to
	// completion. If HeartbeatMs is 0, no AbortCheck event callbacks will occur. Also,
	// AbortCheck callbacks do not occur when doing asynchronous transfers.
	void put_HeartbeatMs(int newVal);

	// The domain name of the FTP server. May also use the IPv4 or IPv6 address in
	// string format.
	void get_Hostname(CkString &str);
	// The domain name of the FTP server. May also use the IPv4 or IPv6 address in
	// string format.
	const wchar_t *hostname(void);
	// The domain name of the FTP server. May also use the IPv4 or IPv6 address in
	// string format.
	void put_Hostname(const wchar_t *newVal);

	// If an HTTP proxy requiring authentication is to be used, set this property to
	// the HTTP proxy authentication method name. Valid choices are "LOGIN" or "NTLM".
	void get_HttpProxyAuthMethod(CkString &str);
	// If an HTTP proxy requiring authentication is to be used, set this property to
	// the HTTP proxy authentication method name. Valid choices are "LOGIN" or "NTLM".
	const wchar_t *httpProxyAuthMethod(void);
	// If an HTTP proxy requiring authentication is to be used, set this property to
	// the HTTP proxy authentication method name. Valid choices are "LOGIN" or "NTLM".
	void put_HttpProxyAuthMethod(const wchar_t *newVal);

	// If an HTTP proxy is used, and it uses NTLM authentication, then this optional
	// property is the NTLM authentication domain.
	void get_HttpProxyDomain(CkString &str);
	// If an HTTP proxy is used, and it uses NTLM authentication, then this optional
	// property is the NTLM authentication domain.
	const wchar_t *httpProxyDomain(void);
	// If an HTTP proxy is used, and it uses NTLM authentication, then this optional
	// property is the NTLM authentication domain.
	void put_HttpProxyDomain(const wchar_t *newVal);

	// If an HTTP proxy is to be used, set this property to the HTTP proxy hostname or
	// IPv4 address (in dotted decimal notation).
	void get_HttpProxyHostname(CkString &str);
	// If an HTTP proxy is to be used, set this property to the HTTP proxy hostname or
	// IPv4 address (in dotted decimal notation).
	const wchar_t *httpProxyHostname(void);
	// If an HTTP proxy is to be used, set this property to the HTTP proxy hostname or
	// IPv4 address (in dotted decimal notation).
	void put_HttpProxyHostname(const wchar_t *newVal);

	// If an HTTP proxy requiring authentication is to be used, set this property to
	// the HTTP proxy password.
	void get_HttpProxyPassword(CkString &str);
	// If an HTTP proxy requiring authentication is to be used, set this property to
	// the HTTP proxy password.
	const wchar_t *httpProxyPassword(void);
	// If an HTTP proxy requiring authentication is to be used, set this property to
	// the HTTP proxy password.
	void put_HttpProxyPassword(const wchar_t *newVal);

	// If an HTTP proxy is to be used, set this property to the HTTP proxy port number.
	// (Two commonly used HTTP proxy ports are 8080 and 3128.)
	int get_HttpProxyPort(void);
	// If an HTTP proxy is to be used, set this property to the HTTP proxy port number.
	// (Two commonly used HTTP proxy ports are 8080 and 3128.)
	void put_HttpProxyPort(int newVal);

	// If an HTTP proxy requiring authentication is to be used, set this property to
	// the HTTP proxy login name.
	void get_HttpProxyUsername(CkString &str);
	// If an HTTP proxy requiring authentication is to be used, set this property to
	// the HTTP proxy login name.
	const wchar_t *httpProxyUsername(void);
	// If an HTTP proxy requiring authentication is to be used, set this property to
	// the HTTP proxy login name.
	void put_HttpProxyUsername(const wchar_t *newVal);

	// Forces a timeout when a response is expected on the control channel, but no
	// response arrives for this number of milliseconds. Setting IdleTimeoutMs = 0
	// allows the application to wait indefinitely. The default value is 60000 (i.e. 60
	// seconds).
	int get_IdleTimeoutMs(void);
	// Forces a timeout when a response is expected on the control channel, but no
	// response arrives for this number of milliseconds. Setting IdleTimeoutMs = 0
	// allows the application to wait indefinitely. The default value is 60000 (i.e. 60
	// seconds).
	void put_IdleTimeoutMs(int newVal);

	// Returns true if currently connected and logged into an FTP server, otherwise
	// returns false.
	// 
	// Note: Accessing this property may cause a NOOP command to be sent to the FTP
	// server.
	// 
	bool get_IsConnected(void);

	// Turns the in-memory session logging on or off. If on, the session log can be
	// obtained via the SessionLog property.
	bool get_KeepSessionLog(void);
	// Turns the in-memory session logging on or off. If on, the session log can be
	// obtained via the SessionLog property.
	void put_KeepSessionLog(bool newVal);

	// Contains the last control-channel reply. For example: "550 Failed to change
	// directory." or "250 Directory successfully changed." The control channel reply
	// is typically formatted as an integer status code followed by a one-line
	// description.
	void get_LastReply(CkString &str);
	// Contains the last control-channel reply. For example: "550 Failed to change
	// directory." or "250 Directory successfully changed." The control channel reply
	// is typically formatted as an integer status code followed by a one-line
	// description.
	const wchar_t *lastReply(void);

	// A wildcard pattern, defaulting to "*" that determines the files and directories
	// included in the following properties and methods: NumFilesAndDirs,
	// GetCreateTime, GetFilename, GetIsDirectory, GetLastAccessTime, GetModifiedTime,
	// GetSize.
	// 
	// Note: Do not include a directory path in the ListPattern. For example, do not
	// set the ListPattern equal to a string such as this: "subdir/*.txt". The correct
	// solution is to first change the remote directory to "subdir" by calling
	// ChangeRemoteDir, and then set the ListPattern equal to "*.txt".
	// 
	void get_ListPattern(CkString &str);
	// A wildcard pattern, defaulting to "*" that determines the files and directories
	// included in the following properties and methods: NumFilesAndDirs,
	// GetCreateTime, GetFilename, GetIsDirectory, GetLastAccessTime, GetModifiedTime,
	// GetSize.
	// 
	// Note: Do not include a directory path in the ListPattern. For example, do not
	// set the ListPattern equal to a string such as this: "subdir/*.txt". The correct
	// solution is to first change the remote directory to "subdir" by calling
	// ChangeRemoteDir, and then set the ListPattern equal to "*.txt".
	// 
	const wchar_t *listPattern(void);
	// A wildcard pattern, defaulting to "*" that determines the files and directories
	// included in the following properties and methods: NumFilesAndDirs,
	// GetCreateTime, GetFilename, GetIsDirectory, GetLastAccessTime, GetModifiedTime,
	// GetSize.
	// 
	// Note: Do not include a directory path in the ListPattern. For example, do not
	// set the ListPattern equal to a string such as this: "subdir/*.txt". The correct
	// solution is to first change the remote directory to "subdir" by calling
	// ChangeRemoteDir, and then set the ListPattern equal to "*.txt".
	// 
	void put_ListPattern(const wchar_t *newVal);

	// True if the FTP2 component was able to login to the FTP server after calling
	// Connect.
	bool get_LoginVerified(void);

	// The number of files and sub-directories in the current remote directory that
	// match the ListPattern. (The ListPattern defaults to "*", so unless changed, this
	// is the total number of files and sub-directories.)
	// 
	// Important: Accessing this property can cause the directory listing to be
	// retrieved from the FTP server. For FTP servers that doe not support the
	// MLST/MLSD commands, this is technically a data transfer that requires a
	// temporary data connection to be established in the same way as when uploading or
	// downloading files. If your program hangs while accessing NumFilesAndDirs, it
	// probably means that the data connection could not be established. The most
	// common solution is to switch to using Passive mode by setting the Passive
	// property = true. If this does not help, examine the contents of the
	// LastErrorText property after NumFilesAndDirs finally returns (after timing out).
	// Also, see this Chilkat blog post about FTP connection settings
	// <http://www.cknotes.com/?p=282> .
	// 
	int get_NumFilesAndDirs(void);

	// A read-only property that indicates whether a partial transfer was received in
	// the last method call to download a file. Set to true if a partial transfer was
	// received. Set to false if nothing was received, or if the full file was
	// received.
	bool get_PartialTransfer(void);

	// Set to true for FTP to operate in passive mode, otherwise set to false for
	// non-passive (.i.e. "active" or "port" mode). The default value of this property
	// is true.
	bool get_Passive(void);
	// Set to true for FTP to operate in passive mode, otherwise set to false for
	// non-passive (.i.e. "active" or "port" mode). The default value of this property
	// is true.
	void put_Passive(bool newVal);

	// This can handle problems that may arise when an FTP server is located behind a
	// NAT router. FTP servers respond to the PASV command by sending the IP address
	// and port where it will be listening for the data connection. If the control
	// connection is SSL encrypted, the NAT router is not able to convert from an
	// internal IP address (typically beginning with 192.168) to an external address.
	// When set to true, PassiveUseHostAddr property tells the FTP client to discard
	// the IP address part of the PASV response and replace it with the IP address of
	// the already-established control connection. The default value of this property
	// is false.
	bool get_PassiveUseHostAddr(void);
	// This can handle problems that may arise when an FTP server is located behind a
	// NAT router. FTP servers respond to the PASV command by sending the IP address
	// and port where it will be listening for the data connection. If the control
	// connection is SSL encrypted, the NAT router is not able to convert from an
	// internal IP address (typically beginning with 192.168) to an external address.
	// When set to true, PassiveUseHostAddr property tells the FTP client to discard
	// the IP address part of the PASV response and replace it with the IP address of
	// the already-established control connection. The default value of this property
	// is false.
	void put_PassiveUseHostAddr(bool newVal);

	// Password for logging into the FTP server.
	void get_Password(CkString &str);
	// Password for logging into the FTP server.
	const wchar_t *password(void);
	// Password for logging into the FTP server.
	void put_Password(const wchar_t *newVal);

	// Port number. Automatically defaults to the default port for the FTP service.
	int get_Port(void);
	// Port number. Automatically defaults to the default port for the FTP service.
	void put_Port(int newVal);

	// If true, the NLST command is used instead of LIST when fetching a directory
	// listing. This can help in very rare cases where the FTP server returns truncated
	// filenames. The drawback to using NLST is that it won"™t return size or
	// date/time info (but it should return the full filename).
	// 
	// The default value of this property is false.
	// 
	bool get_PreferNlst(void);
	// If true, the NLST command is used instead of LIST when fetching a directory
	// listing. This can help in very rare cases where the FTP server returns truncated
	// filenames. The drawback to using NLST is that it won"™t return size or
	// date/time info (but it should return the full filename).
	// 
	// The default value of this property is false.
	// 
	void put_PreferNlst(bool newVal);

	// Progress monitoring for FTP downloads rely on the FTP server indicating the file
	// size within the RETR response. Some FTP servers however, do not indicate the
	// file size and therefore it is not possible to monitor progress based on
	// percentage completion. This property allows the application to explicitly tell
	// the FTP component the size of the file about to be downloaded for the next
	// GetFile call.
	int get_ProgressMonSize(void);
	// Progress monitoring for FTP downloads rely on the FTP server indicating the file
	// size within the RETR response. Some FTP servers however, do not indicate the
	// file size and therefore it is not possible to monitor progress based on
	// percentage completion. This property allows the application to explicitly tell
	// the FTP component the size of the file about to be downloaded for the next
	// GetFile call.
	void put_ProgressMonSize(int newVal);

	// Same as ProgressMonSize, but allows for sizes greater than the 32-bit integer
	// limit.
	__int64 get_ProgressMonSize64(void);
	// Same as ProgressMonSize, but allows for sizes greater than the 32-bit integer
	// limit.
	void put_ProgressMonSize64(__int64 newVal);

	// The hostname of your FTP proxy, if a proxy server is used.
	void get_ProxyHostname(CkString &str);
	// The hostname of your FTP proxy, if a proxy server is used.
	const wchar_t *proxyHostname(void);
	// The hostname of your FTP proxy, if a proxy server is used.
	void put_ProxyHostname(const wchar_t *newVal);

	// The proxy scheme used by your FTP proxy server. Valid values are 0 to 9. The
	// default value is 0 which indicates that no proxy server is used. Supported proxy
	// methods are as follows:
	// 
	// Note: The ProxyHostname is the hostname of the firewall, if the proxy is a
	// firewall. Also, the ProxyUsername and ProxyPassword are the firewall
	// username/password (if the proxy is a firewall).
	// 
	//     ProxyMethod = 1 (SITE site)
	// 
	//         USER ProxyUsername
	//         PASS ProxyPassword
	//         SITE Hostname
	//         USER Username
	//         PASS Password
	// 
	//     ProxyMethod = 2 (USER user@site)
	// 
	//         USER Username@Hostname:Port
	//         PASS Password
	// 
	//     ProxyMethod = 3 (USER with login)
	// 
	//         USER ProxyUsername
	//         PASS ProxyPassword
	//         USER Username@Hostname:Port
	//         PASS Password
	// 
	//     ProxyMethod = 4 (USER/PASS/ACCT)
	// 
	//         USER Username@Hostname:Port ProxyUsername
	//         PASS Password
	//         ACCT ProxyPassword
	// 
	//     ProxyMethod = 5 (OPEN site)
	// 
	//         USER ProxyUsername
	//         PASS ProxyPassword
	//         OPEN Hostname
	//         USER Username
	//         PASS Password
	// 
	//     ProxyMethod = 6 (firewallId@site)
	// 
	//         USER ProxyUsername@Hostname
	//         USER Username
	//         PASS Password
	// 
	//     ProxyMethod = 7
	// 
	//         USER ProxyUsername
	//         USER ProxyPassword
	//         SITE Hostname:Port USER Username
	//         PASS Password
	// 
	//     ProxyMethod = 8
	// 
	//         USER Username@ProxyUsername@Hostname
	//         PASS Password@ProxyPassword
	// 
	//     ProxyMethod = 9
	// 
	//         ProxyUsername ProxyPassword Username Password
	// 
	int get_ProxyMethod(void);
	// The proxy scheme used by your FTP proxy server. Valid values are 0 to 9. The
	// default value is 0 which indicates that no proxy server is used. Supported proxy
	// methods are as follows:
	// 
	// Note: The ProxyHostname is the hostname of the firewall, if the proxy is a
	// firewall. Also, the ProxyUsername and ProxyPassword are the firewall
	// username/password (if the proxy is a firewall).
	// 
	//     ProxyMethod = 1 (SITE site)
	// 
	//         USER ProxyUsername
	//         PASS ProxyPassword
	//         SITE Hostname
	//         USER Username
	//         PASS Password
	// 
	//     ProxyMethod = 2 (USER user@site)
	// 
	//         USER Username@Hostname:Port
	//         PASS Password
	// 
	//     ProxyMethod = 3 (USER with login)
	// 
	//         USER ProxyUsername
	//         PASS ProxyPassword
	//         USER Username@Hostname:Port
	//         PASS Password
	// 
	//     ProxyMethod = 4 (USER/PASS/ACCT)
	// 
	//         USER Username@Hostname:Port ProxyUsername
	//         PASS Password
	//         ACCT ProxyPassword
	// 
	//     ProxyMethod = 5 (OPEN site)
	// 
	//         USER ProxyUsername
	//         PASS ProxyPassword
	//         OPEN Hostname
	//         USER Username
	//         PASS Password
	// 
	//     ProxyMethod = 6 (firewallId@site)
	// 
	//         USER ProxyUsername@Hostname
	//         USER Username
	//         PASS Password
	// 
	//     ProxyMethod = 7
	// 
	//         USER ProxyUsername
	//         USER ProxyPassword
	//         SITE Hostname:Port USER Username
	//         PASS Password
	// 
	//     ProxyMethod = 8
	// 
	//         USER Username@ProxyUsername@Hostname
	//         PASS Password@ProxyPassword
	// 
	//     ProxyMethod = 9
	// 
	//         ProxyUsername ProxyPassword Username Password
	// 
	void put_ProxyMethod(int newVal);

	// The password for authenticating with the FTP proxy server.
	void get_ProxyPassword(CkString &str);
	// The password for authenticating with the FTP proxy server.
	const wchar_t *proxyPassword(void);
	// The password for authenticating with the FTP proxy server.
	void put_ProxyPassword(const wchar_t *newVal);

	// If an FTP proxy server is used, this is the port number at which the proxy
	// server is listening for connections.
	int get_ProxyPort(void);
	// If an FTP proxy server is used, this is the port number at which the proxy
	// server is listening for connections.
	void put_ProxyPort(int newVal);

	// The username for authenticating with the FTP proxy server.
	void get_ProxyUsername(CkString &str);
	// The username for authenticating with the FTP proxy server.
	const wchar_t *proxyUsername(void);
	// The username for authenticating with the FTP proxy server.
	void put_ProxyUsername(const wchar_t *newVal);

	// Forces a timeout when incoming data is expected on a data channel, but no data
	// arrives for this number of seconds. The ReadTimeout is the amount of time that
	// needs to elapse while no additional data is forthcoming. During a long download,
	// if the data stream halts for more than this amount, it will timeout. Otherwise,
	// there is no limit on the length of time for the entire download.
	// 
	// The default value is 60.
	// 
	int get_ReadTimeout(void);
	// Forces a timeout when incoming data is expected on a data channel, but no data
	// arrives for this number of seconds. The ReadTimeout is the amount of time that
	// needs to elapse while no additional data is forthcoming. During a long download,
	// if the data stream halts for more than this amount, it will timeout. Otherwise,
	// there is no limit on the length of time for the entire download.
	// 
	// The default value is 60.
	// 
	void put_ReadTimeout(int newVal);

	// If true, then the FTP2 client will verify the server's SSL certificate. The
	// certificate is expired, or if the cert's signature is invalid, the connection is
	// not allowed. The default value of this property is false.
	bool get_RequireSslCertVerify(void);
	// If true, then the FTP2 client will verify the server's SSL certificate. The
	// certificate is expired, or if the cert's signature is invalid, the connection is
	// not allowed. The default value of this property is false.
	void put_RequireSslCertVerify(bool newVal);

	// Both uploads and downloads may be resumed by simply setting this property =
	// true and re-calling the upload or download method.
	bool get_RestartNext(void);
	// Both uploads and downloads may be resumed by simply setting this property =
	// true and re-calling the upload or download method.
	void put_RestartNext(bool newVal);

	// The buffer size to be used with the underlying TCP/IP socket for sending. The
	// default value is 65536 (64K). Set it to a smaller value for more frequent
	// percentage completion event callbacks. A larger SendBufferSize may improve
	// performance, but at the expense not having as frequent progress monitoring
	// callbacks (if used and if supported by your programming language).
	int get_SendBufferSize(void);
	// The buffer size to be used with the underlying TCP/IP socket for sending. The
	// default value is 65536 (64K). Set it to a smaller value for more frequent
	// percentage completion event callbacks. A larger SendBufferSize may improve
	// performance, but at the expense not having as frequent progress monitoring
	// callbacks (if used and if supported by your programming language).
	void put_SendBufferSize(int newVal);

	// Contains the session log if KeepSessionLog is turned on.
	void get_SessionLog(CkString &str);
	// Contains the session log if KeepSessionLog is turned on.
	const wchar_t *sessionLog(void);

	// Special property used to help specific customers with a certain type of embedded
	// FTP server. Do not use unless advised by Chilkat.
	bool get_SkipFinalReply(void);
	// Special property used to help specific customers with a certain type of embedded
	// FTP server. Do not use unless advised by Chilkat.
	void put_SkipFinalReply(bool newVal);

	// Sets the receive buffer size socket option. Normally, this property should be
	// left unchanged. The default value is 0, which indicates that the receive buffer
	// size socket option should not be explicitly set (i.e. the system default value,
	// which may vary from system to system, should be used).
	// 
	// This property can be changed if download performance seems slow. It is
	// recommended to be a multiple of 4096. To see the current system's default
	// receive buffer size, examine the LastErrorText property after calling any method
	// that establishes a connection. It should be reported under the heading
	// "SO_RCVBUF". To boost performance, try setting it equal to 2, 3, or 4 times the
	// default value.
	// 
	// Note: This property only applies to FTP data connections. The FTP control
	// connection is not used for uploading or downloading files, and is therefore not
	// performance sensitive.
	// 
	int get_SoRcvBuf(void);
	// Sets the receive buffer size socket option. Normally, this property should be
	// left unchanged. The default value is 0, which indicates that the receive buffer
	// size socket option should not be explicitly set (i.e. the system default value,
	// which may vary from system to system, should be used).
	// 
	// This property can be changed if download performance seems slow. It is
	// recommended to be a multiple of 4096. To see the current system's default
	// receive buffer size, examine the LastErrorText property after calling any method
	// that establishes a connection. It should be reported under the heading
	// "SO_RCVBUF". To boost performance, try setting it equal to 2, 3, or 4 times the
	// default value.
	// 
	// Note: This property only applies to FTP data connections. The FTP control
	// connection is not used for uploading or downloading files, and is therefore not
	// performance sensitive.
	// 
	void put_SoRcvBuf(int newVal);

	// Sets the send buffer size socket option. Normally, this property should be left
	// unchanged. The default value is 0, which indicates that the send buffer size
	// socket option should not be explicitly set (i.e. the system default value, which
	// may vary from system to system, should be used).
	// 
	// This property can be changed if upload performance seems slow. It is recommended
	// to be a multiple of 4096. To see the current system's default send buffer size,
	// examine the LastErrorText property after calling any method that establishes a
	// connection. It should be reported under the heading "SO_SNDBUF". To boost
	// performance, try setting it equal to 2, 3, or 4 times the default value.
	// 
	int get_SoSndBuf(void);
	// Sets the send buffer size socket option. Normally, this property should be left
	// unchanged. The default value is 0, which indicates that the send buffer size
	// socket option should not be explicitly set (i.e. the system default value, which
	// may vary from system to system, should be used).
	// 
	// This property can be changed if upload performance seems slow. It is recommended
	// to be a multiple of 4096. To see the current system's default send buffer size,
	// examine the LastErrorText property after calling any method that establishes a
	// connection. It should be reported under the heading "SO_SNDBUF". To boost
	// performance, try setting it equal to 2, 3, or 4 times the default value.
	// 
	void put_SoSndBuf(int newVal);

	// The SOCKS4/SOCKS5 hostname or IPv4 address (in dotted decimal notation). This
	// property is only used if the SocksVersion property is set to 4 or 5).
	void get_SocksHostname(CkString &str);
	// The SOCKS4/SOCKS5 hostname or IPv4 address (in dotted decimal notation). This
	// property is only used if the SocksVersion property is set to 4 or 5).
	const wchar_t *socksHostname(void);
	// The SOCKS4/SOCKS5 hostname or IPv4 address (in dotted decimal notation). This
	// property is only used if the SocksVersion property is set to 4 or 5).
	void put_SocksHostname(const wchar_t *newVal);

	// The SOCKS5 password (if required). The SOCKS4 protocol does not include the use
	// of a password, so this does not apply to SOCKS4.
	void get_SocksPassword(CkString &str);
	// The SOCKS5 password (if required). The SOCKS4 protocol does not include the use
	// of a password, so this does not apply to SOCKS4.
	const wchar_t *socksPassword(void);
	// The SOCKS5 password (if required). The SOCKS4 protocol does not include the use
	// of a password, so this does not apply to SOCKS4.
	void put_SocksPassword(const wchar_t *newVal);

	// The SOCKS4/SOCKS5 proxy port. The default value is 1080. This property only
	// applies if a SOCKS proxy is used (if the SocksVersion property is set to 4 or
	// 5).
	int get_SocksPort(void);
	// The SOCKS4/SOCKS5 proxy port. The default value is 1080. This property only
	// applies if a SOCKS proxy is used (if the SocksVersion property is set to 4 or
	// 5).
	void put_SocksPort(int newVal);

	// The SOCKS4/SOCKS5 proxy username. This property is only used if the SocksVersion
	// property is set to 4 or 5).
	void get_SocksUsername(CkString &str);
	// The SOCKS4/SOCKS5 proxy username. This property is only used if the SocksVersion
	// property is set to 4 or 5).
	const wchar_t *socksUsername(void);
	// The SOCKS4/SOCKS5 proxy username. This property is only used if the SocksVersion
	// property is set to 4 or 5).
	void put_SocksUsername(const wchar_t *newVal);

	// SocksVersion May be set to one of the following integer values:
	// 
	// 0 - No SOCKS proxy is used. This is the default.
	// 4 - Connect via a SOCKS4 proxy.
	// 5 - Connect via a SOCKS5 proxy.
	// 
	int get_SocksVersion(void);
	// SocksVersion May be set to one of the following integer values:
	// 
	// 0 - No SOCKS proxy is used. This is the default.
	// 4 - Connect via a SOCKS4 proxy.
	// 5 - Connect via a SOCKS5 proxy.
	// 
	void put_SocksVersion(int newVal);

	// Use TLS/SSL for FTP connections. You would typically set Ssl = true when
	// connecting to port 990 on FTP servers that support TLS/SSL mode. Note: It is
	// more common to use AuthTls.
	bool get_Ssl(void);
	// Use TLS/SSL for FTP connections. You would typically set Ssl = true when
	// connecting to port 990 on FTP servers that support TLS/SSL mode. Note: It is
	// more common to use AuthTls.
	void put_Ssl(bool newVal);

	// Selects the secure protocol to be used for secure (SSL/TLS) implicit and
	// explicit (AUTH TLS / AUTH SSL) connections . Possible values are:
	// 
	//     default
	//     TLS 1.0
	//     SSL 3.0
	//     
	// 
	// The default value is "default", which means the client/server negotiate the
	// protocol.
	void get_SslProtocol(CkString &str);
	// Selects the secure protocol to be used for secure (SSL/TLS) implicit and
	// explicit (AUTH TLS / AUTH SSL) connections . Possible values are:
	// 
	//     default
	//     TLS 1.0
	//     SSL 3.0
	//     
	// 
	// The default value is "default", which means the client/server negotiate the
	// protocol.
	const wchar_t *sslProtocol(void);
	// Selects the secure protocol to be used for secure (SSL/TLS) implicit and
	// explicit (AUTH TLS / AUTH SSL) connections . Possible values are:
	// 
	//     default
	//     TLS 1.0
	//     SSL 3.0
	//     
	// 
	// The default value is "default", which means the client/server negotiate the
	// protocol.
	void put_SslProtocol(const wchar_t *newVal);

	// Read-only property that returns true if the FTP server's digital certificate
	// was verified when connecting via SSL / TLS.
	bool get_SslServerCertVerified(void);

	// Contains the list of files that would be transferred in a call to
	// SyncRemoteTree2 when the previewOnly argument is set to true. This string
	// property contains one filepath per line, separated by CRLF line endings. After
	// SyncRemoteTree2 is called, this property contains the filepaths of the local
	// files that would be uploaded to the FTP server.
	void get_SyncPreview(CkString &str);
	// Contains the list of files that would be transferred in a call to
	// SyncRemoteTree2 when the previewOnly argument is set to true. This string
	// property contains one filepath per line, separated by CRLF line endings. After
	// SyncRemoteTree2 is called, this property contains the filepaths of the local
	// files that would be uploaded to the FTP server.
	const wchar_t *syncPreview(void);

	// The average upload rate in bytes/second. This property is updated in real-time
	// during any FTP upload (asynchronous or synchronous).
	int get_UploadTransferRate(void);

	// If true, the FTP2 component will use the EPSV command instead of PASV for
	// passive mode data transfers. The default value of this property is false. (It
	// is somewhat uncommon for FTP servers to support EPSV.)
	// 
	// Note: If the AutoFeat property is true, then the FTP server's features are
	// automatically queried after connecting. In this case, the UseEpsv property is
	// automatically set to true if the FTP server supports EPSV. It is recommended
	// that AutoFeat be kept on. If EPSV is not desired, then make sure to set UseEpsv
	// = false after connecting.
	// 
	bool get_UseEpsv(void);
	// If true, the FTP2 component will use the EPSV command instead of PASV for
	// passive mode data transfers. The default value of this property is false. (It
	// is somewhat uncommon for FTP servers to support EPSV.)
	// 
	// Note: If the AutoFeat property is true, then the FTP server's features are
	// automatically queried after connecting. In this case, the UseEpsv property is
	// automatically set to true if the FTP server supports EPSV. It is recommended
	// that AutoFeat be kept on. If EPSV is not desired, then make sure to set UseEpsv
	// = false after connecting.
	// 
	void put_UseEpsv(bool newVal);

	// Username for logging into the FTP server. Defaults to "anonymous".
	void get_Username(CkString &str);
	// Username for logging into the FTP server. Defaults to "anonymous".
	const wchar_t *username(void);
	// Username for logging into the FTP server. Defaults to "anonymous".
	void put_Username(const wchar_t *newVal);

	// Can contain a wildcarded list of file patterns separated by semicolons. For
	// example, "*.xml; *.txt; *.csv". If set, the Sync* upload and download methods
	// will only transfer files that match any one of these patterns.
	void get_SyncMustMatch(CkString &str);
	// Can contain a wildcarded list of file patterns separated by semicolons. For
	// example, "*.xml; *.txt; *.csv". If set, the Sync* upload and download methods
	// will only transfer files that match any one of these patterns.
	const wchar_t *syncMustMatch(void);
	// Can contain a wildcarded list of file patterns separated by semicolons. For
	// example, "*.xml; *.txt; *.csv". If set, the Sync* upload and download methods
	// will only transfer files that match any one of these patterns.
	void put_SyncMustMatch(const wchar_t *newVal);

	// Can contain a wildcarded list of file patterns separated by semicolons. For
	// example, "*.xml; *.txt; *.csv". If set, the Sync* upload and download methods
	// will not transfer files that match any one of these patterns.
	void get_SyncMustNotMatch(CkString &str);
	// Can contain a wildcarded list of file patterns separated by semicolons. For
	// example, "*.xml; *.txt; *.csv". If set, the Sync* upload and download methods
	// will not transfer files that match any one of these patterns.
	const wchar_t *syncMustNotMatch(void);
	// Can contain a wildcarded list of file patterns separated by semicolons. For
	// example, "*.xml; *.txt; *.csv". If set, the Sync* upload and download methods
	// will not transfer files that match any one of these patterns.
	void put_SyncMustNotMatch(const wchar_t *newVal);

	// If true, then use IPv6 over IPv4 when both are supported for a particular
	// domain. The default value of this property is false, which will choose IPv4
	// over IPv6.
	bool get_PreferIpv6(void);
	// If true, then use IPv6 over IPv4 when both are supported for a particular
	// domain. The default value of this property is false, which will choose IPv4
	// over IPv6.
	void put_PreferIpv6(bool newVal);

	// The current percentage completed of an asynchronous FTP upload or download. This
	// property is updated in real-time and an application may periodically fetch and
	// display it's value while the asynchronous data transfer is in progress.
	unsigned long get_AsyncPercentDone(void);



	// ----------------------
	// Methods
	// ----------------------
	// Same as PutFile but the file on the FTP server is appended.
	// 
	// If the  remoteFilePath contains non-English characters, it may be necessary to set the
	// DirListingCharset property equal to "utf-8". Please refer to the documentation
	// for the DirListingCharset property.
	// 
	bool AppendFile(const wchar_t *localFilename, const wchar_t *remoteFilename);

	// Same as PutFileFromBinaryData, except the file on the FTP server is appended.
	bool AppendFileFromBinaryData(const wchar_t *remoteFilename, const CkByteData &content);

	// Same as PutFileFromTextData, except the file on the FTP server is appended.
	bool AppendFileFromTextData(const wchar_t *remoteFilename, const wchar_t *textData, const wchar_t *charset);

	// Causes an asynchronous Get or Put to abort.
	void AsyncAbort(void);

	// Initiates an asynchronous append. The file is uploaded and appended to an
	// existing file on the FTP server. The append happens in a background thread and
	// can be aborted by calling AsyncAbort. The AsyncFinished property can be checked
	// periodically to determine when the background transfer is finished. The status
	// of the transfer is available in the AsyncSuccess property. The last-error
	// information is available in the AsyncLog property. The AsyncBytesSent property
	// is updated in real time to reflect the current number of bytes sent while the
	// transfer is in progress. The UploadRate is also updated with the current upload
	// rate in bytes/second. While a transfer is in progress, a program may
	// periodically read the UploadRate and AsyncBytesSent properties to display
	// progress.
	bool AsyncAppendFileStart(const wchar_t *localFilename, const wchar_t *remoteFilename);

	// Initiates an asynchronous file download. The download happens in a background
	// thread and can be aborted by calling AsyncAbort. The AsyncFinished property can
	// be checked periodically to determine when the background transfer is finished.
	// The status of the transfer is available in the AsyncSuccess property. The
	// last-error information is available in the AsyncLog property. The
	// AsyncBytesReceived property is updated in real time to reflect the current
	// number of bytes received while the transfer is in progress. The DownloadRate is
	// also updated with the current download rate in bytes/second. While a transfer is
	// in progress, a program may periodically read the DownloadRate and
	// AsyncBytesReceived properties to display progress.
	bool AsyncGetFileStart(const wchar_t *remoteFilename, const wchar_t *localFilename);

	// Initiates an asynchronous file upload. The file is uploaded and creates a new
	// file on the FTP server, or overwrites an existing file. The upload happens in a
	// background thread and can be aborted by calling AsyncAbort. The AsyncFinished
	// property can be checked periodically to determine when the background transfer
	// is finished. The status of the transfer is available in the AsyncSuccess
	// property. The last-error information is available in the AsyncLog property. The
	// AsyncBytesSent property is updated in real time to reflect the current number of
	// bytes sent while the transfer is in progress. The UploadRate is also updated
	// with the current upload rate in bytes/second. While a transfer is in progress, a
	// program may periodically read the UploadRate and AsyncBytesSent properties to
	// display progress.
	bool AsyncPutFileStart(const wchar_t *localFilename, const wchar_t *remoteFilename);

	// Changes the current remote directory. The remoteDirPath should be relative to the current
	// remote directory, which is initially the HOME directory of the FTP user account.
	// 
	// If the remoteDirPath contains non-English characters, it may be necessary to set the
	// DirListingCharset property equal to "utf-8". Please refer to the documentation
	// for the DirListingCharset property.
	// 
	bool ChangeRemoteDir(const wchar_t *relativeDirPath);

	// Reverts the FTP control channel from SSL/TLS to an unencrypted channel. This may
	// be required when using FTPS with AUTH TLS where the FTP client is behind a DSL
	// or cable-modem router that performs NAT (network address translation). If the
	// control channel is encrypted, the router is unable to translate the IP address
	// sent in the PORT command for data transfers. By clearing the control channel,
	// the data transfers will remain encrypted, but the FTP commands are passed
	// unencrypted. Your program would typically clear the control channel after
	// authenticating.
	bool ClearControlChannel(void);

	// TheNumFilesAndDirs property returns the count of files and sub-directories in
	// the current remote FTP directory, according to the ListPattern property. For
	// example, if ListPattern is set to "*.xml", then NumFilesAndDirs returns the
	// count of XML files in the remote directory.
	// 
	// The 1st time it is accessed, the component will (behind the scenes) fetch the
	// directory listing from the FTP server. This information is cached in the
	// component until (1) the current remote directory is changed, or (2) the
	// ListPattern is changed, or (3) the this method (ClearDirCache) is called.
	// 
	void ClearDirCache(void);

	// Clears the in-memory session log.
	void ClearSessionLog(void);

	// Connects and logs in to the FTP server using the username/password provided in
	// the component properties. Check the integer value of the ConnectFailReason if
	// this method returns false (indicating failure).
	// 
	// Note: To separately establish the connection and then authenticate (in separate
	// method calls), call ConnectOnly followed by LoginAfterConnectOnly.
	// 
	bool Connect(void);

	// Connects to the FTP server, but does not authenticate. The combination of
	// calling this method followed by LoginAfterConnectOnly is the equivalent of
	// calling the Connect method (which both connects and authenticates).
	bool ConnectOnly(void);

	// Explicitly converts the control channel to a secure SSL/TLS connection.
	// 
	// Note: If you initially connect with either the AuthTls or AuthSsl property set
	// to true, then DO NOT call ConvertToTls. The control channel is automatically
	// converted to SSL/TLS from within the Connect method when these properties are
	// set.
	// 
	// Note: It is very uncommon for this method to be needed.
	// 
	bool ConvertToTls(void);

	// Creates an "FTP plan" that lists the FTP operations that would be performed when
	// PutTree is called. Additionally, the PutPlan method executes an "FTP plan" and
	// logs each successful operation to a plan log file. If a large-scale upload is
	// interrupted, the PutPlan can be resumed, skipping over the operations already
	// listed in the plan log file.
	bool CreatePlan(const wchar_t *localDir, CkString &outStr);
	// Creates an "FTP plan" that lists the FTP operations that would be performed when
	// PutTree is called. Additionally, the PutPlan method executes an "FTP plan" and
	// logs each successful operation to a plan log file. If a large-scale upload is
	// interrupted, the PutPlan can be resumed, skipping over the operations already
	// listed in the plan log file.
	const wchar_t *createPlan(const wchar_t *localDir);

	// Creates a directory on the FTP server. If the directory already exists, a new
	// one is not created and false is returned.
	// 
	// If the remoteDirPath contains non-English characters, it may be necessary to set the
	// DirListingCharset property equal to "utf-8". Please refer to the documentation
	// for the DirListingCharset property.
	// 
	bool CreateRemoteDir(const wchar_t *dir);

	// Deletes all the files in the current remote FTP directory matching the pattern.
	// Returns the number of files deleted, or -1 for failure. The pattern is a string
	// such as "*.txt", where any number of "*" wildcard characters can be used. "*"
	// matches 0 or more of any character.
	int DeleteMatching(const wchar_t *remotePattern);

	// Deletes a file on the FTP server.
	// 
	// If the remoteFilePath contains non-English characters, it may be necessary to set the
	// DirListingCharset property equal to "utf-8". Please refer to the documentation
	// for the DirListingCharset property.
	// 
	bool DeleteRemoteFile(const wchar_t *filename);

	// Deletes the entire subtree and all files from the current remote FTP directory.
	// To delete a subtree on the FTP server, your program would first navigate to the
	// root of the subtree to be deleted by calling ChangeRemoteDir, and then call
	// DeleteTree. There are two event callbacks: VerifyDeleteFile and VerifyDeleteDir.
	// Both are called prior to deleting each file or directory. The arguments to the
	// callback include the full filepath of the file or directory, and an output-only
	// "skip" flag. If your application sets the skip flag to true, the file or
	// directory is NOT deleted. If a directory is not deleted, all files and
	// sub-directories will remain. Example programs can be found at
	// http://www.example-code.com/
	bool DeleteTree(void);

	// Automatically determines the ProxyMethod that should be used with an FTP proxy
	// server. Tries each of the five possible ProxyMethod settings and returns the
	// value (1-5) of the ProxyMethod that succeeded.
	// 
	// This method may take a minute or two to complete. Returns 0 if no proxy methods
	// were successful. Returns -1 to indicate an error (i.e. it was unable to test all
	// proxy methods.)
	// 
	int DetermineProxyMethod(void);

	// Discovers which combinations of FTP2 property settings result in successful data
	// transfers.
	// 
	// DetermineSettings tries 13 different combinations of these properties:
	// Ssl
	// AuthTls
	// AuthSsl
	// Port
	// Passive
	// PassiveUseHostAddr
	// Within the FTP protocol, the process of fetching a directory listing is also
	// considered a "data transfer". The DetermineSettings method works by checking to
	// see which combinations result in a successful directory listing download. The
	// method takes no arguments and returns a string containing an XML report of the
	// results. It is a blocking call that may take approximately a minute to run. If
	// you are unsure about how to interpret the results, cut-and-paste it into an
	// email and send it to support@chilkatsoft.com.
	// 
	bool DetermineSettings(CkString &outXmlReport);
	// Discovers which combinations of FTP2 property settings result in successful data
	// transfers.
	// 
	// DetermineSettings tries 13 different combinations of these properties:
	// Ssl
	// AuthTls
	// AuthSsl
	// Port
	// Passive
	// PassiveUseHostAddr
	// Within the FTP protocol, the process of fetching a directory listing is also
	// considered a "data transfer". The DetermineSettings method works by checking to
	// see which combinations result in a successful directory listing download. The
	// method takes no arguments and returns a string containing an XML report of the
	// results. It is a blocking call that may take approximately a minute to run. If
	// you are unsure about how to interpret the results, cut-and-paste it into an
	// email and send it to support@chilkatsoft.com.
	// 
	const wchar_t *determineSettings(void);

	// Recursively downloads the structure of a complete remote directory tree. Returns
	// an XML document with the directory structure. A zero-length string is returned
	// to indicate failure.
	bool DirTreeXml(CkString &outStrXml);
	// Recursively downloads the structure of a complete remote directory tree. Returns
	// an XML document with the directory structure. A zero-length string is returned
	// to indicate failure.
	const wchar_t *dirTreeXml(void);

	// Disconnects from the FTP server, ending the current session.
	bool Disconnect(void);

	// Downloads an entire tree from the FTP server and recreates the directory tree on
	// the local filesystem.
	// 
	// This method downloads all the files and subdirectories in the current remote
	// directory. An application would first navigate to the directory to be downloaded
	// via ChangeRemoteDir and then call this method.
	// 
	// There are three event callbacks (for programming languages supporting events):
	// BeginDownloadFile, EndDownloadFile, and VerifyDownloadDir. The 1st argument to
	// each callback is the fully qualified pathname of the file or directory. The
	// BeginDownloadFile event callbacks has a "skip" argument, which is output-only.
	// The application can set it to true to prevent the file from being downloaded.
	// The VerifyDownloadDir also has a "skip" argument where the application can cause
	// an entire sub-tree to be skipped. The EndDownloadFile event includes a
	// "numBytes" argument containing the size of the file in bytes.
	// 
	bool DownloadTree(const wchar_t *localRoot);

	// Sends a FEAT command to the FTP server and returns the response. Returns a
	// zero-length string to indicate failure. Here is a typical response:
	// 211-Features:
	//  MDTM
	//  REST STREAM
	//  SIZE
	//  MLST type*;size*;modify*;
	//  MLSD
	//  AUTH SSL
	//  AUTH TLS
	//  UTF8
	//  CLNT
	//  MFMT
	// 211 End
	bool Feat(CkString &outStr);
	// Sends a FEAT command to the FTP server and returns the response. Returns a
	// zero-length string to indicate failure. Here is a typical response:
	// 211-Features:
	//  MDTM
	//  REST STREAM
	//  SIZE
	//  MLST type*;size*;modify*;
	//  MLSD
	//  AUTH SSL
	//  AUTH TLS
	//  UTF8
	//  CLNT
	//  MFMT
	// 211 End
	const wchar_t *feat(void);

	// Returns the create date/time for the Nth file or sub-directory in the current
	// remote directory. The first file/dir is at index 0, and the last one is at index
	// (NumFilesAndDirs-1)
	// The caller is responsible for deleting the object returned by this method.
	CkDateTimeW *GetCreateDt(int index);

	// Returns the file-creation date/time for a remote file by filename.
	// 
	// Note: The filename passed to this method must NOT include a path. Prior to calling
	// this method, make sure to set the current remote directory (via the
	// ChangeRemoteDir method) to the remote directory where this file exists.
	// 
	// Note: Prior to calling this method, it should be ensured that the ListPattern
	// property is set to a pattern that would match the requested filename. (The default
	// value of ListPattern is "*", which will match all filenames.)
	// 
	// Note: Linux/Unix type filesystems do not store "create" date/times. Therefore,
	// if the FTP server is on such as system, this method will return a date/time
	// equal to the last-modified date/time.
	// 
	// The caller is responsible for deleting the object returned by this method.
	CkDateTimeW *GetCreateDtByName(const wchar_t *filename);

	// Returns the create time for the Nth file or sub-directory in the current remote
	// directory. The first file/dir is at index 0, and the last one is at index
	// (NumFilesAndDirs-1)
	// 
	// Note: The FILETIME is a Windows-based format. See
	// http://support.microsoft.com/kb/188768 for more information.
	// 
	bool GetCreateFTime(int index, FILETIME &outFileTime);

	// Returns the create time for the Nth file or sub-directory in the current remote
	// directory. The first file/dir is at index 0, and the last one is at index
	// (NumFilesAndDirs-1)
	bool GetCreateTime(int index, SYSTEMTIME &outSysTime);

	// Returns the file-creation date/time for a remote file by filename.
	// 
	// Note: The filename passed to this method must NOT include a path. Prior to calling
	// this method, make sure to set the current remote directory (via the
	// ChangeRemoteDir method) to the remote directory where this file exists.
	// 
	// Note: Prior to calling this method, it should be ensured that the ListPattern
	// property is set to a pattern that would match the requested filename. (The default
	// value of ListPattern is "*", which will match all filenames.)
	// 
	// Note: Linux/Unix type filesystems do not store "create" date/times. If the FTP
	// server is on such as system, this method will return a date/time equal to the
	// last-modified date/time.
	// 
	bool GetCreateTimeByName(const wchar_t *filename, SYSTEMTIME &outSysTime);

	// Returns the file-creation date/time (in RFC822 string format, such as "Tue, 25
	// Sep 2012 12:25:32 -0500") for a remote file by filename.
	// 
	// Note: The filename passed to this method must NOT include a path. Prior to calling
	// this method, make sure to set the current remote directory (via the
	// ChangeRemoteDir method) to the remote directory where this file exists.
	// 
	// Note: Prior to calling this method, it should be ensured that the ListPattern
	// property is set to a pattern that would match the requested filename. (The default
	// value of ListPattern is "*", which will match all filenames.)
	// 
	// Note: Linux/Unix type filesystems do not store "create" date/times. If the FTP
	// server is on such as system, this method will return a date/time equal to the
	// last-modified date/time.
	// 
	bool GetCreateTimeByNameStr(const wchar_t *filename, CkString &outStr);
	// Returns the file-creation date/time (in RFC822 string format, such as "Tue, 25
	// Sep 2012 12:25:32 -0500") for a remote file by filename.
	// 
	// Note: The filename passed to this method must NOT include a path. Prior to calling
	// this method, make sure to set the current remote directory (via the
	// ChangeRemoteDir method) to the remote directory where this file exists.
	// 
	// Note: Prior to calling this method, it should be ensured that the ListPattern
	// property is set to a pattern that would match the requested filename. (The default
	// value of ListPattern is "*", which will match all filenames.)
	// 
	// Note: Linux/Unix type filesystems do not store "create" date/times. If the FTP
	// server is on such as system, this method will return a date/time equal to the
	// last-modified date/time.
	// 
	const wchar_t *getCreateTimeByNameStr(const wchar_t *filename);
	// Returns the file-creation date/time (in RFC822 string format, such as "Tue, 25
	// Sep 2012 12:25:32 -0500") for a remote file by filename.
	// 
	// Note: The filename passed to this method must NOT include a path. Prior to calling
	// this method, make sure to set the current remote directory (via the
	// ChangeRemoteDir method) to the remote directory where this file exists.
	// 
	// Note: Prior to calling this method, it should be ensured that the ListPattern
	// property is set to a pattern that would match the requested filename. (The default
	// value of ListPattern is "*", which will match all filenames.)
	// 
	// Note: Linux/Unix type filesystems do not store "create" date/times. If the FTP
	// server is on such as system, this method will return a date/time equal to the
	// last-modified date/time.
	// 
	const wchar_t *createTimeByNameStr(const wchar_t *filename);

	// Returns the create time (in RFC822 string format, such as "Tue, 25 Sep 2012
	// 12:25:32 -0500") for the Nth file or sub-directory in the current remote
	// directory. The first file/dir is at index 0, and the last one is at index
	// (NumFilesAndDirs-1)
	bool GetCreateTimeStr(int index, CkString &outStr);
	// Returns the create time (in RFC822 string format, such as "Tue, 25 Sep 2012
	// 12:25:32 -0500") for the Nth file or sub-directory in the current remote
	// directory. The first file/dir is at index 0, and the last one is at index
	// (NumFilesAndDirs-1)
	const wchar_t *getCreateTimeStr(int index);
	// Returns the create time (in RFC822 string format, such as "Tue, 25 Sep 2012
	// 12:25:32 -0500") for the Nth file or sub-directory in the current remote
	// directory. The first file/dir is at index 0, and the last one is at index
	// (NumFilesAndDirs-1)
	const wchar_t *createTimeStr(int index);

	// Returns the current remote directory.
	bool GetCurrentRemoteDir(CkString &outStr);
	// Returns the current remote directory.
	const wchar_t *getCurrentRemoteDir(void);
	// Returns the current remote directory.
	const wchar_t *currentRemoteDir(void);

	// Downloads a file from the FTP server to the local filesystem.
	// 
	// If the remoteFilePath contains non-English characters, it may be necessary to set the
	// DirListingCharset property equal to "utf-8". Please refer to the documentation
	// for the DirListingCharset property.
	// 
	bool GetFile(const wchar_t *remoteFilename, const wchar_t *localFilename);

	// Returns the filename for the Nth file or sub-directory in the current remote
	// directory. The first file/dir is at index 0, and the last one is at index
	// (NumFilesAndDirs-1)
	bool GetFilename(int index, CkString &outStr);
	// Returns the filename for the Nth file or sub-directory in the current remote
	// directory. The first file/dir is at index 0, and the last one is at index
	// (NumFilesAndDirs-1)
	const wchar_t *getFilename(int index);
	// Returns the filename for the Nth file or sub-directory in the current remote
	// directory. The first file/dir is at index 0, and the last one is at index
	// (NumFilesAndDirs-1)
	const wchar_t *filename(int index);

	// Returns true for a sub-directory and false for a file, for the Nth entry in
	// the current remote directory. The first file/dir is at index 0, and the last one
	// is at index (NumFilesAndDirs-1)
	bool GetIsDirectory(int index);

	// Returns true if the remote file is a symbolic link. (Symbolic links only exist
	// on Unix/Linux systems, not on Windows filesystems.)
	bool GetIsSymbolicLink(int index);

	// Returns the last access date/time for the Nth file or sub-directory in the
	// current remote directory. The first file/dir is at index 0, and the last one is
	// at index (NumFilesAndDirs-1)
	// The caller is responsible for deleting the object returned by this method.
	CkDateTimeW *GetLastAccessDt(int index);

	// Returns a remote file's last-access date/time.
	// 
	// Note: The filename passed to this method must NOT include a path. Prior to calling
	// this method, make sure to set the current remote directory (via the
	// ChangeRemoteDir method) to the remote directory where this file exists.
	// 
	// Note: Prior to calling this method, it should be ensured that the ListPattern
	// property is set to a pattern that would match the requested filename. (The default
	// value of ListPattern is "*", which will match all filenames.)
	// 
	// The caller is responsible for deleting the object returned by this method.
	CkDateTimeW *GetLastAccessDtByName(const wchar_t *filename);

	// Returns the last access date/time for the Nth file or sub-directory in the
	// current remote directory. The first file/dir is at index 0, and the last one is
	// at index (NumFilesAndDirs-1)
	// 
	// Note: The FILETIME is a Windows-based format. See
	// http://support.microsoft.com/kb/188768 for more information.
	// 
	bool GetLastAccessFTime(int index, FILETIME &outFileTime);

	// Returns the last access date/time for the Nth file or sub-directory in the
	// current remote directory. The first file/dir is at index 0, and the last one is
	// at index (NumFilesAndDirs-1)
	bool GetLastAccessTime(int index, SYSTEMTIME &outSysTime);

	// Returns a remote file's last-access date/time.
	// 
	// Note: The filename passed to this method must NOT include a path. Prior to calling
	// this method, make sure to set the current remote directory (via the
	// ChangeRemoteDir method) to the remote directory where this file exists.
	// 
	// Note: Prior to calling this method, it should be ensured that the ListPattern
	// property is set to a pattern that would match the requested filename. (The default
	// value of ListPattern is "*", which will match all filenames.)
	// 
	bool GetLastAccessTimeByName(const wchar_t *filename, SYSTEMTIME &outSysTime);

	// Returns a remote file's last-access date/time in RFC822 string format, such as
	// "Tue, 25 Sep 2012 12:25:32 -0500".
	// 
	// Note: The filename passed to this method must NOT include a path. Prior to calling
	// this method, make sure to set the current remote directory (via the
	// ChangeRemoteDir method) to the remote directory where this file exists.
	// 
	// Note: Prior to calling this method, it should be ensured that the ListPattern
	// property is set to a pattern that would match the requested filename. (The default
	// value of ListPattern is "*", which will match all filenames.)
	// 
	bool GetLastAccessTimeByNameStr(const wchar_t *filename, CkString &outStr);
	// Returns a remote file's last-access date/time in RFC822 string format, such as
	// "Tue, 25 Sep 2012 12:25:32 -0500".
	// 
	// Note: The filename passed to this method must NOT include a path. Prior to calling
	// this method, make sure to set the current remote directory (via the
	// ChangeRemoteDir method) to the remote directory where this file exists.
	// 
	// Note: Prior to calling this method, it should be ensured that the ListPattern
	// property is set to a pattern that would match the requested filename. (The default
	// value of ListPattern is "*", which will match all filenames.)
	// 
	const wchar_t *getLastAccessTimeByNameStr(const wchar_t *filename);
	// Returns a remote file's last-access date/time in RFC822 string format, such as
	// "Tue, 25 Sep 2012 12:25:32 -0500".
	// 
	// Note: The filename passed to this method must NOT include a path. Prior to calling
	// this method, make sure to set the current remote directory (via the
	// ChangeRemoteDir method) to the remote directory where this file exists.
	// 
	// Note: Prior to calling this method, it should be ensured that the ListPattern
	// property is set to a pattern that would match the requested filename. (The default
	// value of ListPattern is "*", which will match all filenames.)
	// 
	const wchar_t *lastAccessTimeByNameStr(const wchar_t *filename);

	// Returns the last access date/time (in RFC822 string format, such as "Tue, 25 Sep
	// 2012 12:25:32 -0500") for the Nth file or sub-directory in the current remote
	// directory. The first file/dir is at index 0, and the last one is at index
	// (NumFilesAndDirs-1)
	bool GetLastAccessTimeStr(int index, CkString &outStr);
	// Returns the last access date/time (in RFC822 string format, such as "Tue, 25 Sep
	// 2012 12:25:32 -0500") for the Nth file or sub-directory in the current remote
	// directory. The first file/dir is at index 0, and the last one is at index
	// (NumFilesAndDirs-1)
	const wchar_t *getLastAccessTimeStr(int index);
	// Returns the last access date/time (in RFC822 string format, such as "Tue, 25 Sep
	// 2012 12:25:32 -0500") for the Nth file or sub-directory in the current remote
	// directory. The first file/dir is at index 0, and the last one is at index
	// (NumFilesAndDirs-1)
	const wchar_t *lastAccessTimeStr(int index);

	// Returns the last modified date/time for the Nth file or sub-directory in the
	// current remote directory. The first file/dir is at index 0, and the last one is
	// at index (NumFilesAndDirs-1)
	// The caller is responsible for deleting the object returned by this method.
	CkDateTimeW *GetLastModDt(int index);

	// Returns the last-modified date/time for a remote file.
	// 
	// Note: The filename passed to this method must NOT include a path. Prior to calling
	// this method, make sure to set the current remote directory (via the
	// ChangeRemoteDir method) to the remote directory where this file exists.
	// 
	// Note: Prior to calling this method, it should be ensured that the ListPattern
	// property is set to a pattern that would match the requested filename. (The default
	// value of ListPattern is "*", which will match all filenames.)
	// 
	// The caller is responsible for deleting the object returned by this method.
	CkDateTimeW *GetLastModDtByName(const wchar_t *filename);

	// Returns the last modified date/time for the Nth file or sub-directory in the
	// current remote directory. The first file/dir is at index 0, and the last one is
	// at index (NumFilesAndDirs-1)
	// 
	// Note: The FILETIME is a Windows-based format. See
	// http://support.microsoft.com/kb/188768 for more information.
	// 
	bool GetLastModifiedFTime(int index, FILETIME &outFileTime);

	// Returns the last modified date/time for the Nth file or sub-directory in the
	// current remote directory. The first file/dir is at index 0, and the last one is
	// at index (NumFilesAndDirs-1)
	bool GetLastModifiedTime(int index, SYSTEMTIME &outSysTime);

	// Returns the last-modified date/time for a remote file.
	// 
	// Note: The filename passed to this method must NOT include a path. Prior to calling
	// this method, make sure to set the current remote directory (via the
	// ChangeRemoteDir method) to the remote directory where this file exists.
	// 
	// Note: Prior to calling this method, it should be ensured that the ListPattern
	// property is set to a pattern that would match the requested filename. (The default
	// value of ListPattern is "*", which will match all filenames.)
	// 
	bool GetLastModifiedTimeByName(const wchar_t *filename, SYSTEMTIME &outSysTime);

	// Returns a remote file's last-modified date/time in RFC822 string format, such as
	// "Tue, 25 Sep 2012 12:25:32 -0500".
	// 
	// Note: The filename passed to this method must NOT include a path. Prior to calling
	// this method, make sure to set the current remote directory (via the
	// ChangeRemoteDir method) to the remote directory where this file exists.
	// 
	// Note: Prior to calling this method, it should be ensured that the ListPattern
	// property is set to a pattern that would match the requested filename. (The default
	// value of ListPattern is "*", which will match all filenames.)
	// 
	bool GetLastModifiedTimeByNameStr(const wchar_t *filename, CkString &outStr);
	// Returns a remote file's last-modified date/time in RFC822 string format, such as
	// "Tue, 25 Sep 2012 12:25:32 -0500".
	// 
	// Note: The filename passed to this method must NOT include a path. Prior to calling
	// this method, make sure to set the current remote directory (via the
	// ChangeRemoteDir method) to the remote directory where this file exists.
	// 
	// Note: Prior to calling this method, it should be ensured that the ListPattern
	// property is set to a pattern that would match the requested filename. (The default
	// value of ListPattern is "*", which will match all filenames.)
	// 
	const wchar_t *getLastModifiedTimeByNameStr(const wchar_t *filename);
	// Returns a remote file's last-modified date/time in RFC822 string format, such as
	// "Tue, 25 Sep 2012 12:25:32 -0500".
	// 
	// Note: The filename passed to this method must NOT include a path. Prior to calling
	// this method, make sure to set the current remote directory (via the
	// ChangeRemoteDir method) to the remote directory where this file exists.
	// 
	// Note: Prior to calling this method, it should be ensured that the ListPattern
	// property is set to a pattern that would match the requested filename. (The default
	// value of ListPattern is "*", which will match all filenames.)
	// 
	const wchar_t *lastModifiedTimeByNameStr(const wchar_t *filename);

	// Returns the last modified date/time (in RFC822 string format, such as "Tue, 25
	// Sep 2012 12:25:32 -0500") for the Nth file or sub-directory in the current
	// remote directory. The first file/dir is at index 0, and the last one is at index
	// (NumFilesAndDirs-1)
	bool GetLastModifiedTimeStr(int index, CkString &outStr);
	// Returns the last modified date/time (in RFC822 string format, such as "Tue, 25
	// Sep 2012 12:25:32 -0500") for the Nth file or sub-directory in the current
	// remote directory. The first file/dir is at index 0, and the last one is at index
	// (NumFilesAndDirs-1)
	const wchar_t *getLastModifiedTimeStr(int index);
	// Returns the last modified date/time (in RFC822 string format, such as "Tue, 25
	// Sep 2012 12:25:32 -0500") for the Nth file or sub-directory in the current
	// remote directory. The first file/dir is at index 0, and the last one is at index
	// (NumFilesAndDirs-1)
	const wchar_t *lastModifiedTimeStr(int index);

	// Downloads the contents of a remote file into a byte array.
	bool GetRemoteFileBinaryData(const wchar_t *remoteFilename, CkByteData &outData);

	// Downloads a text file directly into a string variable. The character encoding of
	// the text file is specified by the  charset argument, which is a value such as utf-8,
	// iso-8859-1, Shift_JIS, etc.
	bool GetRemoteFileTextC(const wchar_t *remoteFilename, const wchar_t *charset, CkString &outStr);
	// Downloads a text file directly into a string variable. The character encoding of
	// the text file is specified by the  charset argument, which is a value such as utf-8,
	// iso-8859-1, Shift_JIS, etc.
	const wchar_t *getRemoteFileTextC(const wchar_t *remoteFilename, const wchar_t *charset);
	// Downloads a text file directly into a string variable. The character encoding of
	// the text file is specified by the  charset argument, which is a value such as utf-8,
	// iso-8859-1, Shift_JIS, etc.
	const wchar_t *remoteFileTextC(const wchar_t *remoteFilename, const wchar_t *charset);

	// Downloads the content of a remote text file directly into an in-memory string.
	// 
	// Note: If the remote text file does not use the ANSI character encoding, call
	// GetRemoteFileTextC instead, which allows for the character encoding to be
	// specified so that characters are properly interpreted.
	// 
	bool GetRemoteFileTextData(const wchar_t *remoteFilename, CkString &outStr);
	// Downloads the content of a remote text file directly into an in-memory string.
	// 
	// Note: If the remote text file does not use the ANSI character encoding, call
	// GetRemoteFileTextC instead, which allows for the character encoding to be
	// specified so that characters are properly interpreted.
	// 
	const wchar_t *getRemoteFileTextData(const wchar_t *remoteFilename);
	// Downloads the content of a remote text file directly into an in-memory string.
	// 
	// Note: If the remote text file does not use the ANSI character encoding, call
	// GetRemoteFileTextC instead, which allows for the character encoding to be
	// specified so that characters are properly interpreted.
	// 
	const wchar_t *remoteFileTextData(const wchar_t *remoteFilename);

	// Returns the size of the Nth remote file in the current directory.
	int GetSize(int index);

	// Returns the size of the Nth remote file in the current directory as a 64-bit
	// integer. Returns -1 if the file does not exist.
	__int64 GetSize64(int index);

	// Returns a remote file's size in bytes. Returns -1 if the file does not exist.
	// 
	// Note: The filename passed to this method must NOT include a path. Prior to calling
	// this method, make sure to set the current remote directory (via the
	// ChangeRemoteDir method) to the remote directory where this file exists.
	// 
	// Note: Prior to calling this method, it should be ensured that the ListPattern
	// property is set to a pattern that would match the requested filename. (The default
	// value of ListPattern is "*", which will match all filenames.)
	// 
	int GetSizeByName(const wchar_t *filename);

	// Returns a remote file's size in bytes as a 64-bit integer.
	// 
	// Note: The filename passed to this method must NOT include a path. Prior to calling
	// this method, make sure to set the current remote directory (via the
	// ChangeRemoteDir method) to the remote directory where this file exists.
	// 
	// Note: Prior to calling this method, it should be ensured that the ListPattern
	// property is set to a pattern that would match the requested filename. (The default
	// value of ListPattern is "*", which will match all filenames.)
	// 
	__int64 GetSizeByName64(const wchar_t *filename);

	// Returns the size in decimal string format of the Nth remote file in the current
	// directory. This is helpful for cases when the file size (in bytes) is greater
	// than what can fit in a 32-bit integer.
	bool GetSizeStr(int index, CkString &outStr);
	// Returns the size in decimal string format of the Nth remote file in the current
	// directory. This is helpful for cases when the file size (in bytes) is greater
	// than what can fit in a 32-bit integer.
	const wchar_t *getSizeStr(int index);
	// Returns the size in decimal string format of the Nth remote file in the current
	// directory. This is helpful for cases when the file size (in bytes) is greater
	// than what can fit in a 32-bit integer.
	const wchar_t *sizeStr(int index);

	// Returns the size of a remote file as a string. This is helpful when file a file
	// size is greater than what can fit in a 32-bit integer.
	// 
	// Note: The filename passed to this method must NOT include a path. Prior to calling
	// this method, make sure to set the current remote directory (via the
	// ChangeRemoteDir method) to the remote directory where this file exists.
	// 
	// Note: Prior to calling this method, it should be ensured that the ListPattern
	// property is set to a pattern that would match the requested filename. (The default
	// value of ListPattern is "*", which will match all filenames.)
	// 
	bool GetSizeStrByName(const wchar_t *filename, CkString &outStr);
	// Returns the size of a remote file as a string. This is helpful when file a file
	// size is greater than what can fit in a 32-bit integer.
	// 
	// Note: The filename passed to this method must NOT include a path. Prior to calling
	// this method, make sure to set the current remote directory (via the
	// ChangeRemoteDir method) to the remote directory where this file exists.
	// 
	// Note: Prior to calling this method, it should be ensured that the ListPattern
	// property is set to a pattern that would match the requested filename. (The default
	// value of ListPattern is "*", which will match all filenames.)
	// 
	const wchar_t *getSizeStrByName(const wchar_t *filename);
	// Returns the size of a remote file as a string. This is helpful when file a file
	// size is greater than what can fit in a 32-bit integer.
	// 
	// Note: The filename passed to this method must NOT include a path. Prior to calling
	// this method, make sure to set the current remote directory (via the
	// ChangeRemoteDir method) to the remote directory where this file exists.
	// 
	// Note: Prior to calling this method, it should be ensured that the ListPattern
	// property is set to a pattern that would match the requested filename. (The default
	// value of ListPattern is "*", which will match all filenames.)
	// 
	const wchar_t *sizeStrByName(const wchar_t *filename);

	// Returns the FTP server's digital certificate (for SSL / TLS connections).
	// The caller is responsible for deleting the object returned by this method.
	CkCertW *GetSslServerCert(void);

	// Returns a listing of the files and directories in the current directory matching
	// the pattern. Passing "*.*" will return all the files and directories.
	bool GetTextDirListing(const wchar_t *pattern, CkString &outStrRawListing);
	// Returns a listing of the files and directories in the current directory matching
	// the pattern. Passing "*.*" will return all the files and directories.
	const wchar_t *getTextDirListing(const wchar_t *pattern);
	// Returns a listing of the files and directories in the current directory matching
	// the pattern. Passing "*.*" will return all the files and directories.
	const wchar_t *textDirListing(const wchar_t *pattern);

	// Returns (in XML format) the files and directories in the current directory
	// matching the pattern. Passing "*.*" will return all the files and directories.
	bool GetXmlDirListing(const wchar_t *pattern, CkString &outStrXmlListing);
	// Returns (in XML format) the files and directories in the current directory
	// matching the pattern. Passing "*.*" will return all the files and directories.
	const wchar_t *getXmlDirListing(const wchar_t *pattern);
	// Returns (in XML format) the files and directories in the current directory
	// matching the pattern. Passing "*.*" will return all the files and directories.
	const wchar_t *xmlDirListing(const wchar_t *pattern);

	// Return true if the component is already unlocked.
	bool IsUnlocked(void);

	// Authenticates with the FTP server using the values provided in the Username,
	// Password, and/or other properties. This can be called after establishing the
	// connection via the ConnectOnly method. (The Connect method both connects and
	// authenticates.) The combination of calling ConnectOnly followed by
	// LoginAfterConnectOnly is the equivalent of calling the Connect method.
	// 
	// Note: After successful authentication, the FEAT and SYST commands are
	// automatically sent to help the client understand what is supported by the FTP
	// server. To prevent these commands from being sent, set the AutoFeat and/or
	// AutoSyst properties equal to false.
	// 
	bool LoginAfterConnectOnly(void);

	// Copies all the files in the current remote FTP directory to a local directory.
	// To copy all the files in a remote directory, set remotePattern to "*.*" The
	// pattern can contain any number of "*"characters, where "*" matches 0 or more of
	// any character. The return value is the number of files transferred, and on
	// error, a value of -1 is returned. Detailed information about the transfer can be
	// obtained from the last-error information
	// (LastErrorText/LastErrorHtml/LastErrorXml/SaveLastError).
	// 
	// About case sensitivity: The MGetFiles command works by sending the "LIST"
	// command to the FTP server. For example: "LIST *.txt". The FTP server responds
	// with a directory listing of the files matching the wildcarded pattern, and it is
	// these files that are downloaded. Case sensitivity depends on the
	// case-sensitivity of the remote file system. If the FTP server is running on a
	// Windows-based computer, it is likely to be case insensitive. However, if the FTP
	// server is running on Linux, MAC OS X, etc. it is likely to be case sensitive.
	// There is no good way to force case-insensitivity if the remote filesystem is
	// case-sensitive because it is not possible for the FTP client to send a LIST
	// command indicating that it wants the matching to be case-insensitive.
	// 
	int MGetFiles(const wchar_t *remotePattern, const wchar_t *localDir);

	// Uploads all the files matching pattern on the local computer to the current
	// remote FTP directory. The pattern parameter can include directory information,
	// such as "C:/my_dir/*.txt" or it can simply be a pattern such as "*.*" that
	// matches the files in the application's current directory. Subdirectories are not
	// recursed. The return value is the number of files copied, with a value of -1
	// returned for errors. Detailed information about the transfer can be obtained
	// from the XML log.[
	int MPutFiles(const wchar_t *pattern);

	// Sends an NLST command to the FTP server and returns the results in XML format.
	// The NLST command returns a list of filenames in the given directory (matching
	// the pattern). The remoteDirPattern should be a pattern such as "*", "*.*", "*.txt",
	// "subDir/*.xml", etc.
	// 
	// The format of the XML returned is:
	// <nlst>
	// <e>filename_or_dir_1</e>
	// <e>filename_or_dir_2</e>
	// <e>filename_or_dir_3</e>
	// <e>filename_or_dir_4</e>
	// ...
	// </nlst>
	// 
	bool NlstXml(const wchar_t *pattern, CkString &outStr);
	// Sends an NLST command to the FTP server and returns the results in XML format.
	// The NLST command returns a list of filenames in the given directory (matching
	// the pattern). The remoteDirPattern should be a pattern such as "*", "*.*", "*.txt",
	// "subDir/*.xml", etc.
	// 
	// The format of the XML returned is:
	// <nlst>
	// <e>filename_or_dir_1</e>
	// <e>filename_or_dir_2</e>
	// <e>filename_or_dir_3</e>
	// <e>filename_or_dir_4</e>
	// ...
	// </nlst>
	// 
	const wchar_t *nlstXml(const wchar_t *pattern);

	// Issues a no-op command to the FTP server.
	bool Noop(void);

	// Uploads a local file to the current directory on the FTP server.
	// 
	// If the  remoteFilePath contains non-English characters, it may be necessary to set the
	// DirListingCharset property equal to "utf-8". Please refer to the documentation
	// for the DirListingCharset property.
	// 
	bool PutFile(const wchar_t *localFilename, const wchar_t *remoteFilename);

	// Creates a file on the remote server containing the data passed in a byte array.
	bool PutFileFromBinaryData(const wchar_t *remoteFilename, const CkByteData &content);

	// Creates a file on the remote server containing the data passed in a string.
	bool PutFileFromTextData(const wchar_t *remoteFilename, const wchar_t *textData, const wchar_t *charset);

	// Executes an "FTP plan" (created by the CreatePlan method) and logs each
	// successful operation to a plan log file. If a large-scale upload is interrupted,
	// the PutPlan can be resumed, skipping over the operations already listed in the
	// plan log file. When resuming an interrupted PutPlan method, use the same log
	// file. All completed operations found in the already-existing log will
	// automatically be skipped.
	bool PutPlan(const wchar_t *plan, const wchar_t *alreadyDoneFilename);

	// Uploads an entire directory tree from the local filesystem to the remote FTP
	// server, recreating the directory tree on the server. The PutTree method copies a
	// directory tree to the current remote directory on the FTP server.
	bool PutTree(const wchar_t *localDir);

	// Sends an arbitrary (raw) command to the FTP server.
	bool Quote(const wchar_t *cmd);

	// Removes a directory from the FTP server.
	// 
	// If the remoteDirPath contains non-English characters, it may be necessary to set the
	// DirListingCharset property equal to "utf-8". Please refer to the documentation
	// for the DirListingCharset property.
	// 
	bool RemoveRemoteDir(const wchar_t *dir);

	// Renames a file or directory on the FTP server. To move a file from one directory
	// to another on a remote FTP server, call this method and include the source and
	// destination directory filepath.
	// 
	// If the existingRemoteFilePath or  newRemoteFilePath contains non-English characters, it may be necessary to set
	// the DirListingCharset property equal to "utf-8". Please refer to the
	// documentation for the DirListingCharset property.
	// 
	bool RenameRemoteFile(const wchar_t *existingFilename, const wchar_t *newFilename);

	// Sends an raw command to the FTP server and returns the raw response.
	bool SendCommand(const wchar_t *cmd, CkString &outReply);
	// Sends an raw command to the FTP server and returns the raw response.
	const wchar_t *sendCommand(const wchar_t *cmd);

	// Chilkat FTP2 supports MODE Z, which is a transfer mode implemented by some FTP
	// servers. It allows for files to be uploaded and downloaded using compressed
	// streams (using the zlib deflate algorithm).
	// 
	// Call this method after connecting to enable Mode Z. Once enabled, all transfers
	// (uploads, downloads, and directory listings) are compressed.
	// 
	bool SetModeZ(void);

	// Used in conjunction with the DownloadTree method. Call this method prior to
	// calling DownloadTree to set the oldest date for a file to be downloaded. When
	// DownloadTree is called, any file older than this date will not be downloaded.
	void SetOldestDate(SYSTEMTIME &oldestDateTime);

	// Used in conjunction with the DownloadTree method. Call this method prior to
	// calling DownloadTree to set the oldest date for a file to be downloaded. When
	// DownloadTree is called, any file older than this date will not be downloaded.
	// 
	// The oldestDateTimeStr should be a date/time string in RFC822 format, such as "Tue, 25 Sep
	// 2012 12:25:32 -0500".
	// 
	void SetOldestDateStr(const wchar_t *oldestDateTimeStr);

	// Sets the last-modified date/time of a file on the FTP server. Important: Not all
	// FTP servers support this functionality. Please see the information at the
	// Chilkat blog below:
	bool SetRemoteFileDateTime(SYSTEMTIME &dt, const wchar_t *remoteFilename);

	// Sets the last-modified date/time of a file on the FTP server. The dateTimeStr should be
	// a date/time string in RFC822 format, such as "Tue, 25 Sep 2012 12:25:32 -0500".
	// 
	// Important: Not all FTP servers support this functionality. Please see the
	// information at the Chilkat blog below:
	// 
	bool SetRemoteFileDateTimeStr(const wchar_t *dateTimeStr, const wchar_t *remoteFilename);

	// Sets the last-modified date/time of a file on the FTP server. Important: Not all
	// FTP servers support this functionality. Please see the information at the
	// Chilkat blog below:
	bool SetRemoteFileDt(CkDateTimeW &dt, const wchar_t *remoteFilename);

	// Enforces a requirement on the FTP server's certificate. The reqName can be
	// "SubjectDN", "SubjectCN", "IssuerDN", or "IssuerCN". The reqName specifies the part
	// of the certificate, and the  reqValue is the value that it must match (exactly). If
	// the FTP server's certificate does not match, the SSL / TLS connection is
	// aborted.
	void SetSslCertRequirement(const wchar_t *name, const wchar_t *value);

	// Allows for a client-side certificate to be used for the SSL / TLS connection.
	bool SetSslClientCert(CkCertW &cert);

	// Allows for a client-side certificate to be used for the SSL / TLS connection.
	bool SetSslClientCertPem(const wchar_t *pemDataOrFilename, const wchar_t *pemPassword);

	// Allows for a client-side certificate to be used for the SSL / TLS connection.
	bool SetSslClientCertPfx(const wchar_t *pfxFilename, const wchar_t *pfxPassword);

	// Set the FTP transfer mode to us-ascii.
	bool SetTypeAscii(void);

	// Set the FTP transfer mode to binary.
	bool SetTypeBinary(void);

	// Sends an arbitrary "site" command to the FTP server. The params argument should
	// contain the parameters to the site command as they would appear on a command
	// line. For example: "recfm=fb lrecl=600".
	bool Site(const wchar_t *siteCommand);

	// Causes the calling process to sleep for a number of milliseconds.
	void SleepMs(int millisec);

	// Sends a STAT command to the FTP server and returns the server's reply.
	bool Stat(CkString &outStr);
	// Sends a STAT command to the FTP server and returns the server's reply.
	const wchar_t *ck_stat(void);

	// Delete remote files that do not exist locally. The remote directory tree rooted
	// at the current remote directory is traversed and remote files that have no
	// corresponding local file are deleted.
	bool SyncDeleteRemote(const wchar_t *localRoot);

	// The same as SyncLocalTree, except the sub-directories are not traversed. The
	// files in the current remote directory are synchronized (downloaded) with the
	// files in localRoot. For possible  mode settings, see SyncLocalTree.
	bool SyncLocalDir(const wchar_t *localRoot, int mode);

	// Downloads files from the FTP server to a local directory tree. Synchronization
	// modes include:
	// 
	//     mode=0: Download all files
	//     mode=1: Download all files that do not exist on the local filesystem.
	//     mode=2: Download newer or non-existant files.
	//     mode=3: Download only newer files. If a file does not already exist on the
	//     local filesystem, it is not downloaded from the server.
	//     mode=5: Download only missing files or files with size differences.
	//     mode=6: Same as mode 5, but also download newer files.
	//     mode=99: Do not download files, but instead delete remote files that do not
	//     exist locally.
	//     * There is no mode #4. It is a mode used internally by the DirTreeXml
	//     method.
	//     
	// 
	bool SyncLocalTree(const wchar_t *localRoot, int mode);

	// Uploads a directory tree from the local filesystem to the FTP server.
	// Synchronization modes include:
	// 
	//     mode=0: Upload all files
	//     mode=1: Upload all files that do not exist on the FTP server.
	//     mode=2: Upload newer or non-existant files.
	//     mode=3: Upload only newer files. If a file does not already exist on the FTP
	//     server, it is not uploaded.
	//     mode=4: transfer missing files or files with size differences.
	//     mode=5: same as mode 4, but also newer files.
	// 
	bool SyncRemoteTree(const wchar_t *localRoot, int mode);

	// Same as SyncRemoteTree, except two extra arguments are added to allow for more
	// flexibility. If  bDescend is false, then the directory tree is not descended and
	// only the files in localDirPath are synchronized. If  bPreviewOnly is true then no files are
	// transferred and instead the files that would've been transferred (had  bPreviewOnly been
	// set to false) are listed in the SyncPreview property.
	// 
	// Note: If  bPreviewOnly is set to true, the remote directories (if they do not exist)
	// are created. It is only the files that are not uploaded.
	// 
	bool SyncRemoteTree2(const wchar_t *localRoot, int mode, bool bDescend, bool bPreviewOnly);

	// Sends a SYST command to the FTP server to find out the type of operating system
	// at the server. The method returns the FTP server's response string. Refer to RFC
	// 959 for details.
	bool Syst(CkString &outStr);
	// Sends a SYST command to the FTP server to find out the type of operating system
	// at the server. The method returns the FTP server's response string. Refer to RFC
	// 959 for details.
	const wchar_t *syst(void);

	// Unlocks the component. This must be called once prior to calling any other
	// method. A permanent unlock code for FTP2 should contain the substring "FTP".
	bool UnlockComponent(const wchar_t *unlockCode);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
