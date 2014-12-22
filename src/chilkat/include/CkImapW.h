// CkImapW.h: interface for the CkImapW class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkImapW_H
#define _CkImapW_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkWideCharBase.h"

class CkByteData;
class CkEmailW;
class CkMessageSetW;
class CkEmailBundleW;
class CkStringArrayW;
class CkCertW;
class CkMailboxesW;
class CkCspW;
class CkPrivateKeyW;
class CkSshKeyW;
class CkXmlCertVaultW;
class CkBaseProgressW;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkImapW
class CK_VISIBLE_PUBLIC CkImapW  : public CkWideCharBase
{
    private:
	bool m_cbOwned;
	CkBaseProgressW *m_callback;

	// Don't allow assignment or copying these objects.
	CkImapW(const CkImapW &);
	CkImapW &operator=(const CkImapW &);

    public:
	CkImapW(void);
	virtual ~CkImapW(void);

	static CkImapW *createNew(void);
	

	CkImapW(bool bCallbackOwned);
	static CkImapW *createNew(bool bCallbackOwned);

	
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	CkBaseProgressW *get_EventCallbackObject(void) const;
	void put_EventCallbackObject(CkBaseProgressW *progress);


	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------
	// When true (the default) the Append method will mark the email appended to a
	// mailbox as already seen. Otherwise an appended email will be initialized to have
	// a status of unseen.
	bool get_AppendSeen(void);
	// When true (the default) the Append method will mark the email appended to a
	// mailbox as already seen. Otherwise an appended email will be initialized to have
	// a status of unseen.
	void put_AppendSeen(bool newVal);

	// The UID of the last email appended to a mailbox via an Append* method. (Not all
	// IMAP servers report back the UID of the email appended.)
	int get_AppendUid(void);

	// Can be set to "CRAM-MD5", "NTLM", "PLAIN", or "LOGIN" to select the
	// authentication method. NTLM is the most secure, and is a synonym for "Windows
	// Integrated Authentication". The default is "LOGIN" (or the empty string) which
	// is simple plain-text username/password authentication. Not all IMAP servers
	// support all authentication methods.
	void get_AuthMethod(CkString &str);
	// Can be set to "CRAM-MD5", "NTLM", "PLAIN", or "LOGIN" to select the
	// authentication method. NTLM is the most secure, and is a synonym for "Windows
	// Integrated Authentication". The default is "LOGIN" (or the empty string) which
	// is simple plain-text username/password authentication. Not all IMAP servers
	// support all authentication methods.
	const wchar_t *authMethod(void);
	// Can be set to "CRAM-MD5", "NTLM", "PLAIN", or "LOGIN" to select the
	// authentication method. NTLM is the most secure, and is a synonym for "Windows
	// Integrated Authentication". The default is "LOGIN" (or the empty string) which
	// is simple plain-text username/password authentication. Not all IMAP servers
	// support all authentication methods.
	void put_AuthMethod(const wchar_t *newVal);

	// Applies to the PLAIN authentication method. May be set to an authorization ID
	// that is to be sent along with the Login and Password for authentication.
	void get_AuthzId(CkString &str);
	// Applies to the PLAIN authentication method. May be set to an authorization ID
	// that is to be sent along with the Login and Password for authentication.
	const wchar_t *authzId(void);
	// Applies to the PLAIN authentication method. May be set to an authorization ID
	// that is to be sent along with the Login and Password for authentication.
	void put_AuthzId(const wchar_t *newVal);

	// If set to true, then all Fetch* methods will also automatically download
	// attachments. If set to false, then the Fetch* methods download the email
	// without attachments. The default value is true.
	// 
	// Note: Methods that download headers-only, such as FetchSingleHeader, ignore this
	// property and never download attachments. Also, signed and/or encrypted emails
	// will always be downloaded in full (with attachments) regardless of this property
	// setting.
	// 
	bool get_AutoDownloadAttachments(void);
	// If set to true, then all Fetch* methods will also automatically download
	// attachments. If set to false, then the Fetch* methods download the email
	// without attachments. The default value is true.
	// 
	// Note: Methods that download headers-only, such as FetchSingleHeader, ignore this
	// property and never download attachments. Also, signed and/or encrypted emails
	// will always be downloaded in full (with attachments) regardless of this property
	// setting.
	// 
	void put_AutoDownloadAttachments(bool newVal);

	// If true, then the following will occur when a connection is made to an IMAP
	// server:
	// 
	// 1) If the Port property = 993, then sets StartTls = false and Ssl = true
	// 2) If the Port property = 143, sets Ssl = false
	// 
	// The default value of this property is true.
	// 
	bool get_AutoFix(void);
	// If true, then the following will occur when a connection is made to an IMAP
	// server:
	// 
	// 1) If the Port property = 993, then sets StartTls = false and Ssl = true
	// 2) If the Port property = 143, sets Ssl = false
	// 
	// The default value of this property is true.
	// 
	void put_AutoFix(bool newVal);

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

	// Maximum number of seconds to wait when connecting to an IMAP server. The default
	// value is 30 (units are in seconds).
	int get_ConnectTimeout(void);
	// Maximum number of seconds to wait when connecting to an IMAP server. The default
	// value is 30 (units are in seconds).
	void put_ConnectTimeout(int newVal);

	// Contains the IMAP server's domain name (or IP address) if currently connected.
	// Otherwise returns an empty string.
	void get_ConnectedToHost(CkString &str);
	// Contains the IMAP server's domain name (or IP address) if currently connected.
	// Otherwise returns an empty string.
	const wchar_t *connectedToHost(void);

	// The Windows Domain to use for Windows Integrated Authentication (also known as
	// NTLM). This may be empty.
	void get_Domain(CkString &str);
	// The Windows Domain to use for Windows Integrated Authentication (also known as
	// NTLM). This may be empty.
	const wchar_t *domain(void);
	// The Windows Domain to use for Windows Integrated Authentication (also known as
	// NTLM). This may be empty.
	void put_Domain(const wchar_t *newVal);

	// This is the number of milliseconds between each AbortCheck event callback. The
	// AbortCheck callback allows an application to abort any IMAP operation prior to
	// completion. If HeartbeatMs is 0, no AbortCheck event callbacks will occur.
	int get_HeartbeatMs(void);
	// This is the number of milliseconds between each AbortCheck event callback. The
	// AbortCheck callback allows an application to abort any IMAP operation prior to
	// completion. If HeartbeatMs is 0, no AbortCheck event callbacks will occur.
	void put_HeartbeatMs(int newVal);

	// If an HTTP proxy requiring authentication is to be used, set this property to
	// the HTTP proxy authentication method name. Valid choices are "LOGIN" or "NTLM".
	void get_HttpProxyAuthMethod(CkString &str);
	// If an HTTP proxy requiring authentication is to be used, set this property to
	// the HTTP proxy authentication method name. Valid choices are "LOGIN" or "NTLM".
	const wchar_t *httpProxyAuthMethod(void);
	// If an HTTP proxy requiring authentication is to be used, set this property to
	// the HTTP proxy authentication method name. Valid choices are "LOGIN" or "NTLM".
	void put_HttpProxyAuthMethod(const wchar_t *newVal);

	// The NTLM authentication domain (optional) if NTLM authentication is used.
	void get_HttpProxyDomain(CkString &str);
	// The NTLM authentication domain (optional) if NTLM authentication is used.
	const wchar_t *httpProxyDomain(void);
	// The NTLM authentication domain (optional) if NTLM authentication is used.
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

	// Turns the in-memory session logging on or off. If on, the session log can be
	// obtained via the SessionLog property. The default value is false.
	// 
	// The SessionLog contains the raw commands sent to the IMAP server, and the raw
	// responses received from the IMAP server.
	// 
	bool get_KeepSessionLog(void);
	// Turns the in-memory session logging on or off. If on, the session log can be
	// obtained via the SessionLog property. The default value is false.
	// 
	// The SessionLog contains the raw commands sent to the IMAP server, and the raw
	// responses received from the IMAP server.
	// 
	void put_KeepSessionLog(bool newVal);

	// The MIME source of the email last appended during a call to AppendMail, or
	// AppendMime.
	void get_LastAppendedMime(CkString &str);
	// The MIME source of the email last appended during a call to AppendMail, or
	// AppendMime.
	const wchar_t *lastAppendedMime(void);

	// The last raw command sent to the IMAP server. (This information can be used for
	// debugging if problems occur.)
	void get_LastCommand(CkString &str);
	// The last raw command sent to the IMAP server. (This information can be used for
	// debugging if problems occur.)
	const wchar_t *lastCommand(void);

	// The last intermediate response received from the IMAP server. (This information
	// can be used for debugging if problems occur.)
	void get_LastIntermediateResponse(CkString &str);
	// The last intermediate response received from the IMAP server. (This information
	// can be used for debugging if problems occur.)
	const wchar_t *lastIntermediateResponse(void);

	// The raw data of the last response from the IMAP server. (Useful for debugging if
	// problems occur.) This property is cleared whenever a command is sent to the IMAP
	// server. If no response is received, then this property will remain empty.
	// Otherwise, it will contain the last response received from the IMAP server.
	void get_LastResponse(CkString &str);
	// The raw data of the last response from the IMAP server. (Useful for debugging if
	// problems occur.) This property is cleared whenever a command is sent to the IMAP
	// server. If no response is received, then this property will remain empty.
	// Otherwise, it will contain the last response received from the IMAP server.
	const wchar_t *lastResponse(void);

	// If logged into an IMAP server, the logged-in username.
	void get_LoggedInUser(CkString &str);
	// If logged into an IMAP server, the logged-in username.
	const wchar_t *loggedInUser(void);

	// After selecting a mailbox (by calling SelectMailbox), this property will be
	// updated to reflect the total number of emails in the mailbox.
	int get_NumMessages(void);

	// Set to true to prevent the mail flags (such as the "Seen" flag) from being set
	// when email is retrieved. The default value of this property is false.
	bool get_PeekMode(void);
	// Set to true to prevent the mail flags (such as the "Seen" flag) from being set
	// when email is retrieved. The default value of this property is false.
	void put_PeekMode(bool newVal);

	// The IMAP port number. If using SSL, be sure to set this to the IMAP SSL port
	// number, which is typically port 993. (If this is the case, make sure you also
	// set the Ssl property = true.
	int get_Port(void);
	// The IMAP port number. If using SSL, be sure to set this to the IMAP SSL port
	// number, which is typically port 993. (If this is the case, make sure you also
	// set the Ssl property = true.
	void put_Port(int newVal);

	// The amount of time in seconds to wait during a stall before timing out when
	// reading from an IMAP server. The ReadTimeout is the amount of time that needs to
	// elapse while no additional data is forthcoming. During a long data transfer, if
	// the data stream halts for more than this amount, it will timeout.
	// 
	// The default value is 30 seconds.
	// 
	int get_ReadTimeout(void);
	// The amount of time in seconds to wait during a stall before timing out when
	// reading from an IMAP server. The ReadTimeout is the amount of time that needs to
	// elapse while no additional data is forthcoming. During a long data transfer, if
	// the data stream halts for more than this amount, it will timeout.
	// 
	// The default value is 30 seconds.
	// 
	void put_ReadTimeout(int newVal);

	// The "CHARSET" to be used in searches issued by the Search method. The default
	// value is "UTF-8". (If no 8bit chars are found in the search criteria passed to
	// the Search method, then no CHARSET is needed and this property doesn't apply.)
	// The SearchCharset property can be set to "AUTO" to get the pre-v9.4.0 behavior,
	// which is to examine the 8bit chars found in the search criteria and select an
	// appropriate multibyte charset.
	// 
	// In summary, it is unlikely that this property needs to be changed. It should
	// only be modified if trouble arises with some IMAP servers when non-English chars
	// are used in the search criteria.
	// 
	void get_SearchCharset(CkString &str);
	// The "CHARSET" to be used in searches issued by the Search method. The default
	// value is "UTF-8". (If no 8bit chars are found in the search criteria passed to
	// the Search method, then no CHARSET is needed and this property doesn't apply.)
	// The SearchCharset property can be set to "AUTO" to get the pre-v9.4.0 behavior,
	// which is to examine the 8bit chars found in the search criteria and select an
	// appropriate multibyte charset.
	// 
	// In summary, it is unlikely that this property needs to be changed. It should
	// only be modified if trouble arises with some IMAP servers when non-English chars
	// are used in the search criteria.
	// 
	const wchar_t *searchCharset(void);
	// The "CHARSET" to be used in searches issued by the Search method. The default
	// value is "UTF-8". (If no 8bit chars are found in the search criteria passed to
	// the Search method, then no CHARSET is needed and this property doesn't apply.)
	// The SearchCharset property can be set to "AUTO" to get the pre-v9.4.0 behavior,
	// which is to examine the 8bit chars found in the search criteria and select an
	// appropriate multibyte charset.
	// 
	// In summary, it is unlikely that this property needs to be changed. It should
	// only be modified if trouble arises with some IMAP servers when non-English chars
	// are used in the search criteria.
	// 
	void put_SearchCharset(const wchar_t *newVal);

	// The currently selected mailbox, or an empty string if none.
	void get_SelectedMailbox(CkString &str);
	// The currently selected mailbox, or an empty string if none.
	const wchar_t *selectedMailbox(void);

	// The buffer size to be used with the underlying TCP/IP socket for sending. The
	// default value is 32767.
	int get_SendBufferSize(void);
	// The buffer size to be used with the underlying TCP/IP socket for sending. The
	// default value is 32767.
	void put_SendBufferSize(int newVal);

	// The separator character used by the IMAP server for the mailbox hierarchy. It is
	// typically "/" or ".", but may vary depending on the IMAP server. The
	// ListMailboxes method has the side-effect of setting this property to the correct
	// value because the IMAP server's response when listing mailboxes includes
	// information about the separator char.
	char get_SeparatorChar(void);
	// The separator character used by the IMAP server for the mailbox hierarchy. It is
	// typically "/" or ".", but may vary depending on the IMAP server. The
	// ListMailboxes method has the side-effect of setting this property to the correct
	// value because the IMAP server's response when listing mailboxes includes
	// information about the separator char.
	void put_SeparatorChar(char newVal);

	// Contains an in-memory log of the raw commands sent to the IMAP server, and the
	// raw responses received from the IMAP server. The KeepSessionLog property must be
	// set to true to enable session logging. Call ClearSessionLog to reset the log.
	void get_SessionLog(CkString &str);
	// Contains an in-memory log of the raw commands sent to the IMAP server, and the
	// raw responses received from the IMAP server. The KeepSessionLog property must be
	// set to true to enable session logging. Call ClearSessionLog to reset the log.
	const wchar_t *sessionLog(void);

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

	// true if the IMAP connection should be TLS/SSL.
	// 
	// Note: The typical IMAP TLS/SSL port number is 993. If you set this property =
	// true, it is likely that you should also set the Port property = 993.
	// 
	bool get_Ssl(void);
	// true if the IMAP connection should be TLS/SSL.
	// 
	// Note: The typical IMAP TLS/SSL port number is 993. If you set this property =
	// true, it is likely that you should also set the Port property = 993.
	// 
	void put_Ssl(bool newVal);

	// Selects the secure protocol to be used for secure (SSL/TLS) implicit and
	// explicit (StartTls) connections . Possible values are:
	// 
	//     TLS 1.0
	//     SSL 3.0
	//     SSL 2.0
	//     PCT 1.0
	//     
	// 
	// The default value is "TLS 1.0".
	void get_SslProtocol(CkString &str);
	// Selects the secure protocol to be used for secure (SSL/TLS) implicit and
	// explicit (StartTls) connections . Possible values are:
	// 
	//     TLS 1.0
	//     SSL 3.0
	//     SSL 2.0
	//     PCT 1.0
	//     
	// 
	// The default value is "TLS 1.0".
	const wchar_t *sslProtocol(void);
	// Selects the secure protocol to be used for secure (SSL/TLS) implicit and
	// explicit (StartTls) connections . Possible values are:
	// 
	//     TLS 1.0
	//     SSL 3.0
	//     SSL 2.0
	//     PCT 1.0
	//     
	// 
	// The default value is "TLS 1.0".
	void put_SslProtocol(const wchar_t *newVal);

	// Read-only property that returns true if the IMAP server's digital certificate
	// was verified when connecting via SSL / TLS.
	bool get_SslServerCertVerified(void);

	// If true, then the Connect method will (internallly) convert the connection to
	// TLS/SSL via the STARTTLS IMAP command. This is called "explict SSL/TLS" because
	// the client explicitly requests the connection be transformed into a TLS/SSL
	// secure channel. The alternative is "implicit SSL/TLS" where the "Ssl" property
	// is set to true and the IMAP client connects to the well-known TLS/SSL IMAP
	// port of 993.
	bool get_StartTls(void);
	// If true, then the Connect method will (internallly) convert the connection to
	// TLS/SSL via the STARTTLS IMAP command. This is called "explict SSL/TLS" because
	// the client explicitly requests the connection be transformed into a TLS/SSL
	// secure channel. The alternative is "implicit SSL/TLS" where the "Ssl" property
	// is set to true and the IMAP client connects to the well-known TLS/SSL IMAP
	// port of 993.
	void put_StartTls(bool newVal);

	// A positive integer value containing the UIDNEXT of the currently selected
	// folder, or 0 if it's not available or no folder is selected.
	int get_UidNext(void);

	// An integer value containing the UIDVALIDITY of the currently selected mailbox,
	// or 0 if no mailbox is selected.
	// 
	// A client can save the UidValidity value for a mailbox and then compare it with
	// the UidValidity on a subsequent session. If the new value is larger, the IMAP
	// server is not keeping UID's unchanged between sessions. Most IMAP servers
	// maintain UID's between sessions.
	// 
	int get_UidValidity(void);

	// If true, then the FTP2 client will verify the server's SSL certificate. The
	// certificate is expired, or if the cert's signature is invalid, the connection is
	// not allowed. The default value of this property is false.
	bool get_RequireSslCertVerify(void);
	// If true, then the FTP2 client will verify the server's SSL certificate. The
	// certificate is expired, or if the cert's signature is invalid, the connection is
	// not allowed. The default value of this property is false.
	void put_RequireSslCertVerify(bool newVal);

	// If true, then use IPv6 over IPv4 when both are supported for a particular
	// domain. The default value of this property is false, which will choose IPv4
	// over IPv6.
	bool get_PreferIpv6(void);
	// If true, then use IPv6 over IPv4 when both are supported for a particular
	// domain. The default value of this property is false, which will choose IPv4
	// over IPv6.
	void put_PreferIpv6(bool newVal);



	// ----------------------
	// Methods
	// ----------------------
	// Adds a PFX to the object's internal list of sources to be searched for
	// certificates and private keys when decrypting. Multiple PFX sources can be added
	// by calling this method once for each. (On the Windows operating system, the
	// registry-based certificate stores are also automatically searched, so it is
	// commonly not required to explicitly add PFX sources.)
	// 
	// The pfxBytes contains the bytes of a PFX file (also known as PKCS12 or .p12).
	// 
	bool AddPfxSourceData(const CkByteData &pfxData, const wchar_t *password);

	// Adds a PFX file to the object's internal list of sources to be searched for
	// certificates and private keys when decrypting. Multiple PFX files can be added
	// by calling this method once for each. (On the Windows operating system, the
	// registry-based certificate stores are also automatically searched, so it is
	// commonly not required to explicitly add PFX sources.)
	// 
	// The pfxFilePath contains the bytes of a PFX file (also known as PKCS12 or .p12).
	// 
	bool AddPfxSourceFile(const wchar_t *pfxFilePath, const wchar_t *password);

	// Appends an email to an IMAP mailbox.
	bool AppendMail(const wchar_t *mailbox, const CkEmailW &email);

	// Appends an email (represented as MIME text) to an IMAP mailbox.
	bool AppendMime(const wchar_t *mailbox, const wchar_t *mimeText);

	// The same as AppendMime, but with an extra argument to allow the internal date of
	// the email on the server to be explicitly specified.
	bool AppendMimeWithDate(const wchar_t *mailbox, const wchar_t *mimeText, SYSTEMTIME &internalDate);

	// The same as AppendMimeWithDate, except the date/time is provided in RFC822
	// string format.
	bool AppendMimeWithDateStr(const wchar_t *mailbox, const wchar_t *mimeText, const wchar_t *internalDateStr);

	// Same as AppendMime, but allows the flags associated with the email to be set at
	// the same time. A flag is on if true, and off if false.
	bool AppendMimeWithFlags(const wchar_t *mailbox, const wchar_t *mimeText, bool seen, bool flagged, bool answered, bool draft);

	// Checks for new email that has arrived since the mailbox was selected (via the
	// SelectMailbox or ExamineMailbox methods), or since the last call to
	// CheckForNewEmail (whichever was most recent). This method works by closing and
	// re-opening the currently selected mailbox, and then sending a "SEARCH" command
	// for either RECENT emails, or emails having a UID greater than the UIDNEXT value.
	// A message set object containing the UID's of the new emails is returned, and
	// this may be passed to methods such as FetchBundle to download the new emails.
	// The caller is responsible for deleting the object returned by this method.
	CkMessageSetW *CheckForNewEmail(void);

	// Clears the contents of the SessionLog property.
	void ClearSessionLog(void);

	// Closes the currently selected mailbox.
	bool CloseMailbox(const wchar_t *mailbox);

	// Connects to an IMAP server, but does not login. The domainName is the domain name of
	// the IMAP server. (May also use the IPv4 or IPv6 address in string format.)
	bool Connect(const wchar_t *hostname);

	// Copies a message from the selected mailbox to  copyToMailbox. If  bUid is true, then msgId
	// represents a UID. If  bUid is false, then msgId represents a sequence number.
	bool Copy(int msgId, bool bUid, const wchar_t *copyToMailbox);

	// Same as the Copy method, except an entire set of emails is copied at once. The
	// set of emails is specified in messageSet.
	bool CopyMultiple(CkMessageSetW &messageSet, const wchar_t *copyToMailbox);

	// Copies one or more emails from one mailbox to another. The emails are specified
	// as a range of sequence numbers. The 1st email in a mailbox is always at sequence
	// number 1.
	bool CopySequence(int startSeqNum, int count, const wchar_t *copyToMailbox);

	// Creates a new mailbox.
	bool CreateMailbox(const wchar_t *mailbox);

	// Deletes an existing mailbox.
	bool DeleteMailbox(const wchar_t *mailbox);

	// Disconnects cleanly from the IMAP server. A non-success return from this method
	// only indicates that the disconnect was not clean -- and this can typically be
	// ignored.
	bool Disconnect(void);

	// Selects a mailbox such that only read-only transactions are allowed. This method
	// would be called instead of SelectMailbox if the logged-on user has read-only
	// permission.
	bool ExamineMailbox(const wchar_t *mailbox);

	// Permanently removes from the currently selected mailbox all messages that have
	// the Deleted flag set.
	bool Expunge(void);

	// Permanently removes from the currently selected mailbox all messages that have
	// the Deleted flag set, and closes the mailbox.
	bool ExpungeAndClose(void);

	// Downloads one of an email's attachments and saves it to a file. If the emailObject
	// already contains the full email (including the attachments), then no
	// communication with the IMAP server is necessary because the attachment data is
	// already contained within the emailObject. In this case, the attachment is simply
	// extracted and saved to  saveToPath. (As with all Chilkat methods, indexing begins at 0.
	// The 1st attachment is at  attachmentIndex 0.)
	// 
	// Additional Notes:
	// 
	// If the AutoDownloadAttachments property is set to false, then emails
	// downloaded via any of the Fetch* methods will not include attachments.
	// 
	// Note: "related" items are not considered attachments and are downloaded. These
	// are images, style sheets, etc. that are embedded within the HTML body of an
	// email.
	// 
	// Also: All signed and/or encrypted emails must be downloaded in full.
	// 
	// When an email is downloaded without attachments, the attachment information is
	// included in header fields. The header fields have names beginning with
	// "ckx-imap-". The attachment information can be obtained via the following
	// methods:
	// 
	//     imap.GetMailNumAttach
	//     imap.GetMailAttachFilename
	//     imap.GetMailAttachSize
	//     
	// 
	bool FetchAttachment(CkEmailW &email, int attachIndex, const wchar_t *saveToPath);

	// Downloads one of an email's attachments and returns the attachment data as
	// in-memory bytes that may be accessed by an application. ***See the
	// FetchAttachment method description for more information about fetching
	// attachments.
	bool FetchAttachmentBytes(CkEmailW &email, int attachIndex, CkByteData &outBytes);

	// Downloads one of an email's attachments and returns the attachment data as a
	// string. It only makes sense to call this method for attachments that contain
	// text data. The  charset indicates the character encoding of the text, such as
	// "utf-8" or "windows-1252". ***See the FetchAttachment method description for
	// more information about fetching attachments.
	bool FetchAttachmentString(CkEmailW &email, int attachIndex, const wchar_t *charset, CkString &outStr);
	// Downloads one of an email's attachments and returns the attachment data as a
	// string. It only makes sense to call this method for attachments that contain
	// text data. The  charset indicates the character encoding of the text, such as
	// "utf-8" or "windows-1252". ***See the FetchAttachment method description for
	// more information about fetching attachments.
	const wchar_t *fetchAttachmentString(CkEmailW &email, int attachIndex, const wchar_t *charset);

	// Retrieves a set of messages from the IMAP server and returns them in an email
	// bundle object. If the method fails, it may return a NULL reference.
	// The caller is responsible for deleting the object returned by this method.
	CkEmailBundleW *FetchBundle(const CkMessageSetW &messageSet);

	// Retrieves a set of messages from the IMAP server and returns them in a string
	// array object (NOTE: it does not return a string array, but an object that
	// represents a string array.) Each string within the returned object is the
	// complete MIME source of an email. On failure, a NULL object reference is
	// returned.
	// The caller is responsible for deleting the object returned by this method.
	CkStringArrayW *FetchBundleAsMime(const CkMessageSetW &messageSet);

	// Fetches a chunk of emails starting at a specific sequence number. A bundle of
	// fetched emails is returned. The last two arguments are message sets that are
	// updated with the ids of messages successfully/unsuccessfully fetched.
	// The caller is responsible for deleting the object returned by this method.
	CkEmailBundleW *FetchChunk(int startSeqNum, int count, CkMessageSetW &failedSet, CkMessageSetW &fetchedSet);

	// Fetches the flags for an email. The bUid argument determines whether the msgId
	// argument is a UID or sequence number.
	// 
	// Returns the SPACE separated list of flags set for the email, such as "\Flagged
	// \Seen $label1".
	// 
	bool FetchFlags(int msgId, bool bUid, CkString &outStrFlags);
	// Fetches the flags for an email. The bUid argument determines whether the msgId
	// argument is a UID or sequence number.
	// 
	// Returns the SPACE separated list of flags set for the email, such as "\Flagged
	// \Seen $label1".
	// 
	const wchar_t *fetchFlags(int msgId, bool bUid);

	// Retrieves a set of message headers from the IMAP server and returns them in an
	// email bundle object. If the method fails, it may return a NULL reference. The
	// following methods are useful for retrieving information about attachments and
	// flags after email headers are retrieved: GetMailNumAttach, GetMailAttachSize,
	// GetMailAttachFilename, GetMailFlag.
	// The caller is responsible for deleting the object returned by this method.
	CkEmailBundleW *FetchHeaders(const CkMessageSetW &messageSet);

	// Downloads email for a range of sequence numbers. The 1st email in a mailbox is
	// always at sequence number 1. The total number of emails in the currently
	// selected mailbox is available in the NumMessages property. If the  numMessages is too
	// large, the method will still succeed, but will return a bundle of emails from
	// startSeqNum to the last email in the mailbox.
	// The caller is responsible for deleting the object returned by this method.
	CkEmailBundleW *FetchSequence(int startSeqNum, int numMessages);

	// Same as FetchSequence, but instead of returning email objects in a bundle, the
	// raw MIME of each email is returned.
	// The caller is responsible for deleting the object returned by this method.
	CkStringArrayW *FetchSequenceAsMime(int startSeqNum, int numMessages);

	// Same as FetchSequence, but only the email headers are returned. The email
	// objects within the bundle will be lacking bodies and attachments.
	// The caller is responsible for deleting the object returned by this method.
	CkEmailBundleW *FetchSequenceHeaders(int startSeqNum, int numMessages);

	// Retrieves a single message from the IMAP server. If the method fails, it may
	// return a NULL reference. If bUid is true, then msgID represents a UID. If bUid
	// is false, then msgID represents a sequence number.
	// The caller is responsible for deleting the object returned by this method.
	CkEmailW *FetchSingle(int msgId, bool bUid);

	// Retrieves a single message from the IMAP server and returns a string containing
	// the complete MIME source of the email. If the method fails, it returns a NULL
	// reference. If bUid is true, then msgID represents a UID. If bUid is false, then
	// msgID represents a sequence number.
	bool FetchSingleAsMime(int msgId, bool bUid, CkString &outStrMime);
	// Retrieves a single message from the IMAP server and returns a string containing
	// the complete MIME source of the email. If the method fails, it returns a NULL
	// reference. If bUid is true, then msgID represents a UID. If bUid is false, then
	// msgID represents a sequence number.
	const wchar_t *fetchSingleAsMime(int msgId, bool bUid);

	// Retrieves a single message header from the IMAP server. If the method fails, it
	// may return a NULL reference. The following methods are useful for retrieving
	// information about attachments and flags after an email header is retrieved:
	// GetMailNumAttach, GetMailAttachSize, GetMailAttachFilename, GetMailFlag. If bUid
	// is true, then msgID represents a UID. If bUid is false, then msgID represents a
	// sequence number.
	// The caller is responsible for deleting the object returned by this method.
	CkEmailW *FetchSingleHeader(int msgId, bool bUid);

	// Fetches and returns the MIME of a single email header.
	bool FetchSingleHeaderAsMime(int msgId, bool bUID, CkString &outStr);
	// Fetches and returns the MIME of a single email header.
	const wchar_t *fetchSingleHeaderAsMime(int msgId, bool bUID);

	// Returns a message set object containing all the UIDs in the currently selected
	// mailbox. A NULL object reference is returned on failure.
	// The caller is responsible for deleting the object returned by this method.
	CkMessageSetW *GetAllUids(void);

	// Returns the Nth attachment filename. Indexing begins at 0.
	bool GetMailAttachFilename(const CkEmailW &email, int attachIndex, CkString &outStrFilename);
	// Returns the Nth attachment filename. Indexing begins at 0.
	const wchar_t *getMailAttachFilename(const CkEmailW &email, int attachIndex);
	// Returns the Nth attachment filename. Indexing begins at 0.
	const wchar_t *mailAttachFilename(const CkEmailW &email, int attachIndex);

	// Returns the Nth attachment size in bytes. Indexing begins at 0.
	int GetMailAttachSize(const CkEmailW &email, int attachIndex);

	// Returns the value of a flag (true = yes, false = no) for an email. Both
	// standard system flags as well as custom flags may be set. Standard system flags
	// typically begin with a backslash character, such as "\Seen", "\Answered",
	// "\Flagged", "\Draft", "\Deleted", and "\Answered". Custom flags can be anything,
	// such as "NonJunk", "$label1", "$MailFlagBit1", etc. .
	int GetMailFlag(const CkEmailW &email, const wchar_t *flagName);

	// Returns the number of email attachments.
	int GetMailNumAttach(const CkEmailW &email);

	// Returns the size (in bytes) of the entire email including attachments.
	int GetMailSize(const CkEmailW &email);

	// Returns the IMAP server's digital certificate (for SSL / TLS connections).
	// The caller is responsible for deleting the object returned by this method.
	CkCertW *GetSslServerCert(void);

	// Returns the last known "connected" state with the IMAP server. IsConnected does
	// not send a message to the IMAP server to determine if it is still connected. The
	// Noop method may be called to specifically send a no-operation message to
	// determine actual connectivity.
	// 
	// The IsConnected method is useful for checking to see if the component is already
	// in a known disconnected state.
	// 
	bool IsConnected(void);

	// Returns true if already logged into an IMAP server, otherwise returns false.
	bool IsLoggedIn(void);

	// Returns true if the component is unlocked, false if not.
	bool IsUnlocked(void);

	// Returns a subset of the complete list of mailboxes available on the IMAP server.
	// This method has the side-effect of setting the SeparatorChar property to the
	// correct character used by the IMAP server, which is typically "/" or ".".
	// 
	// The reference and wildcardedMailbox parameters are passed unaltered to the IMAP
	// LIST command:
	// FROM RFC 3501 (IMAP Protocol)
	// 
	//       The LIST command returns a subset of names from the complete set
	//       of all names available to the client.  Zero or more untagged LIST
	//       replies are returned, containing the name attributes, hierarchy
	//       delimiter, and name; see the description of the LIST reply for
	//       more detail.
	// 
	//       An empty ("" string) reference name argument indicates that the
	//       mailbox name is interpreted as by SELECT.  The returned mailbox
	//       names MUST match the supplied mailbox name pattern.  A non-empty
	//       reference name argument is the name of a mailbox or a level of
	//       mailbox hierarchy, and indicates the context in which the mailbox
	//       name is interpreted.
	// 
	//       An empty ("" string) mailbox name argument is a special request to
	//       return the hierarchy delimiter and the root name of the name given
	//       in the reference.  The value returned as the root MAY be the empty
	//       string if the reference is non-rooted or is an empty string.  In
	//       all cases, a hierarchy delimiter (or NIL if there is no hierarchy)
	//       is returned.  This permits a client to get the hierarchy delimiter
	//       (or find out that the mailbox names are flat) even when no
	//       mailboxes by that name currently exist.
	// 
	//       The reference and mailbox name arguments are interpreted into a
	//       canonical form that represents an unambiguous left-to-right
	//       hierarchy.  The returned mailbox names will be in the interpreted
	//       form.
	// 
	//            Note: The interpretation of the reference argument is
	//            implementation-defined.  It depends upon whether the
	//            server implementation has a concept of the "current
	//            working directory" and leading "break out characters",
	//            which override the current working directory.
	// 
	//            For example, on a server which exports a UNIX or NT
	//            filesystem, the reference argument contains the current
	//            working directory, and the mailbox name argument would
	//            contain the name as interpreted in the current working
	//            directory.
	// 
	//            If a server implementation has no concept of break out
	//            characters, the canonical form is normally the reference
	//            name appended with the mailbox name.  Note that if the
	//            server implements the namespace convention (section
	//            5.1.2), "#" is a break out character and must be treated
	//            as such.
	// 
	//            If the reference argument is not a level of mailbox
	//            hierarchy (that is, it is a \NoInferiors name), and/or
	//            the reference argument does not end with the hierarchy
	//            delimiter, it is implementation-dependent how this is
	//            interpreted.  For example, a reference of "foo/bar" and
	//            mailbox name of "rag/baz" could be interpreted as
	//            "foo/bar/rag/baz", "foo/barrag/baz", or "foo/rag/baz".
	//            A client SHOULD NOT use such a reference argument except
	//            at the explicit request of the user.  A hierarchical
	//            browser MUST NOT make any assumptions about server
	//            interpretation of the reference unless the reference is
	//            a level of mailbox hierarchy AND ends with the hierarchy
	//            delimiter.
	// 
	//       Any part of the reference argument that is included in the
	//       interpreted form SHOULD prefix the interpreted form.  It SHOULD
	//       also be in the same form as the reference name argument.  This
	//       rule permits the client to determine if the returned mailbox name
	//       is in the context of the reference argument, or if something about
	//       the mailbox argument overrode the reference argument.  Without
	//       this rule, the client would have to have knowledge of the server's
	//       naming semantics including what characters are "breakouts" that
	//       override a naming context.
	// 
	//            For example, here are some examples of how references
	//            and mailbox names might be interpreted on a UNIX-based
	//            server:
	// 
	//                Reference     Mailbox Name  Interpretation
	//                ------------  ------------  --------------
	//                ~smith/Mail/  foo.*         ~smith/Mail/foo.*
	//                archive/      %             archive/%
	//                #news.        comp.mail.*   #news.comp.mail.*
	//                ~smith/Mail/  /usr/doc/foo  /usr/doc/foo
	//                archive/      ~fred/Mail/*  ~fred/Mail/*
	// 
	//            The first three examples demonstrate interpretations in
	//            the context of the reference argument.  Note that
	//            "~smith/Mail" SHOULD NOT be transformed into something
	//            like "/u2/users/smith/Mail", or it would be impossible
	//            for the client to determine that the interpretation was
	//            in the context of the reference.
	// 
	//       The character "*" is a wildcard, and matches zero or more
	//       characters at this position.  The character "%" is similar to "*",
	//       but it does not match a hierarchy delimiter.  If the "%" wildcard
	//       is the last character of a mailbox name argument, matching levels
	//       of hierarchy are also returned.  If these levels of hierarchy are
	//       not also selectable mailboxes, they are returned with the
	//       \Noselect mailbox name attribute (see the description of the LIST
	//       response for more details).
	// 
	//       Server implementations are permitted to "hide" otherwise
	//       accessible mailboxes from the wildcard characters, by preventing
	//       certain characters or names from matching a wildcard in certain
	//       situations.  For example, a UNIX-based server might restrict the
	//       interpretation of "*" so that an initial "/" character does not
	//       match.
	// 
	//       The special name INBOX is included in the output from LIST, if
	//       INBOX is supported by this server for this user and if the
	//       uppercase string "INBOX" matches the interpreted reference and
	//       mailbox name arguments with wildcards as described above.  The
	//       criteria for omitting INBOX is whether SELECT INBOX will return
	//       failure; it is not relevant whether the user's real INBOX resides
	//       on this or some other server.
	// 
	// The caller is responsible for deleting the object returned by this method.
	CkMailboxesW *ListMailboxes(const wchar_t *reference, const wchar_t *wildcardedMailbox);

	// The same as ListMailboxes, but returns only the subscribed mailboxes. (See
	// ListMailboxes for more information.)
	// The caller is responsible for deleting the object returned by this method.
	CkMailboxesW *ListSubscribed(const wchar_t *reference, const wchar_t *wildcardedMailbox);

	// Logs into the IMAP server. The component must first be connected to an IMAP
	// server by calling Connect.
	bool Login(const wchar_t *login, const wchar_t *password);

	// Logs out of the IMAP server.
	bool Logout(void);

	// Sends a NOOP command to the IMAP server and receives the response. The component
	// must be connected and authenticated for this to succeed. Sending a NOOP is a
	// good way of determining whether the connection to the IMAP server is up and
	// active.
	bool Noop(void);

	// Fetches the flags for an email and updates the flags in the email's header. When
	// an email is retrieved from the IMAP server, it embeds the flags into the header
	// in fields beginning with "ckx-". Methods such as GetMailFlag read these header
	// fields.
	bool RefetchMailFlags(CkEmailW &email);

	// Renames a mailbox.
	bool RenameMailbox(const wchar_t *fromMailbox, const wchar_t *toMailbox);

	// Searches the selected mailbox for messages that meet a given criteria and
	// returns a message set of all matching messages. If bUid is true, then UIDs are
	// returned in the message set, otherwise sequence numbers are returned. The
	// criteria is passed through to the low-level IMAP protocol unmodified, so the
	// rules for the IMAP SEARCH command (RFC 3501) apply and are reproduced here:
	// FROM RFC 3501 (IMAP Protocol)
	// 
	//       The SEARCH command searches the mailbox for messages that match
	//       the given searching criteria.  Searching criteria consist of one
	//       or more search keys.  The untagged SEARCH response from the server
	//       contains a listing of message sequence numbers corresponding to
	//       those messages that match the searching criteria.
	// 
	//       When multiple keys are specified, the result is the intersection
	//       (AND function) of all the messages that match those keys.  For
	//       example, the criteria DELETED FROM "SMITH" SINCE 1-Feb-1994 refers
	//       to all deleted messages from Smith that were placed in the mailbox
	//       since February 1, 1994.  A search key can also be a parenthesized
	//       list of one or more search keys (e.g., for use with the OR and NOT
	//       keys).
	// 
	//       Server implementations MAY exclude [MIME-IMB] body parts with
	//       terminal content media types other than TEXT and MESSAGE from
	//       consideration in SEARCH matching.
	// 
	//       The OPTIONAL [CHARSET] specification consists of the word
	//       "CHARSET" followed by a registered [CHARSET].  It indicates the
	//       [CHARSET] of the strings that appear in the search criteria.
	//       [MIME-IMB] content transfer encodings, and [MIME-HDRS] strings in
	//       [RFC-2822]/[MIME-IMB] headers, MUST be decoded before comparing
	//       text in a [CHARSET] other than US-ASCII.  US-ASCII MUST be
	//       supported; other [CHARSET]s MAY be supported.
	// 
	//       If the server does not support the specified [CHARSET], it MUST
	//       return a tagged NO response (not a BAD).  This response SHOULD
	//       contain the BADCHARSET response code, which MAY list the
	//       [CHARSET]s supported by the server.
	// 
	//       In all search keys that use strings, a message matches the key if
	//       the string is a substring of the field.  The matching is
	//       case-insensitive.
	// 
	//       The defined search keys are as follows.  Refer to the Formal
	//       Syntax section for the precise syntactic definitions of the
	//       arguments.
	// 
	//       
	//          Messages with message sequence numbers corresponding to the
	//          specified message sequence number set.
	// 
	//       ALL
	//          All messages in the mailbox; the default initial key for
	//          ANDing.
	// 
	//       ANSWERED
	//          Messages with the \Answered flag set.
	// 
	//       BCC 
	//          Messages that contain the specified string in the envelope
	//          structure's BCC field.
	// 
	//       BEFORE 
	//          Messages whose internal date (disregarding time and timezone)
	//          is earlier than the specified date.
	// 
	//       BODY 
	//          Messages that contain the specified string in the body of the
	//          message.
	// 
	//       CC 
	//          Messages that contain the specified string in the envelope
	//          structure's CC field.
	// 
	//       DELETED
	//          Messages with the \Deleted flag set.
	// 
	//       DRAFT
	//          Messages with the \Draft flag set.
	// 
	//       FLAGGED
	//          Messages with the \Flagged flag set.
	// 
	//       FROM 
	//          Messages that contain the specified string in the envelope
	//          structure's FROM field.
	// 
	//       HEADER  
	//          Messages that have a header with the specified field-name (as
	//          defined in [RFC-2822]) and that contains the specified string
	//          in the text of the header (what comes after the colon).  If the
	//          string to search is zero-length, this matches all messages that
	//          have a header line with the specified field-name regardless of
	//          the contents.
	// 
	//       KEYWORD 
	//          Messages with the specified keyword flag set.
	// 
	//       LARGER 
	//          Messages with an [RFC-2822] size larger than the specified
	//          number of octets.
	// 
	//       NEW
	//          Messages that have the \Recent flag set but not the \Seen flag.
	//          This is functionally equivalent to "(RECENT UNSEEN)".
	// 
	//       NOT 
	//          Messages that do not match the specified search key.
	// 
	//       OLD
	//          Messages that do not have the \Recent flag set.  This is
	//          functionally equivalent to "NOT RECENT" (as opposed to "NOT
	//          NEW").
	// 
	//       ON 
	//          Messages whose internal date (disregarding time and timezone)
	//          is within the specified date.
	// 
	//       OR  
	//          Messages that match either search key.
	// 
	//       RECENT
	//          Messages that have the \Recent flag set.
	// 
	//       SEEN
	//          Messages that have the \Seen flag set.
	// 
	//       SENTBEFORE 
	//          Messages whose [RFC-2822] Date: header (disregarding time and
	//          timezone) is earlier than the specified date.
	// 
	//       SENTON 
	//          Messages whose [RFC-2822] Date: header (disregarding time and
	//          timezone) is within the specified date.
	// 
	//       SENTSINCE 
	//          Messages whose [RFC-2822] Date: header (disregarding time and
	//          timezone) is within or later than the specified date.
	// 
	//       SINCE 
	//          Messages whose internal date (disregarding time and timezone)
	//          is within or later than the specified date.
	// 
	//       SMALLER 
	//          Messages with an [RFC-2822] size smaller than the specified
	//          number of octets.
	// 
	//       SUBJECT 
	//          Messages that contain the specified string in the envelope
	//          structure's SUBJECT field.
	// 
	//       TEXT 
	//          Messages that contain the specified string in the header or
	//          body of the message.
	// 
	//       TO 
	//          Messages that contain the specified string in the envelope
	//          structure's TO field.
	// 
	//       UID 
	//          Messages with unique identifiers corresponding to the specified
	//          unique identifier set.  Sequence set ranges are permitted.
	// 
	//       UNANSWERED
	//          Messages that do not have the \Answered flag set.
	// 
	//       UNDELETED
	//          Messages that do not have the \Deleted flag set.
	// 
	//       UNDRAFT
	//          Messages that do not have the \Draft flag set.
	// 
	//       UNFLAGGED
	//          Messages that do not have the \Flagged flag set.
	// 
	//       UNKEYWORD 
	//          Messages that do not have the specified keyword flag set.
	// 
	//       UNSEEN
	//          Messages that do not have the \Seen flag set.
	// 
	// The caller is responsible for deleting the object returned by this method.
	CkMessageSetW *Search(const wchar_t *criteria, bool bUid);

	// Selects a mailbox. A mailbox must be selected before some methods, such as
	// Search or FetchSingle, can be called. If the logged-on user does not have
	// write-access to the mailbox, call ExamineMailbox instead.
	// 
	// Calling this method updates the NumMessages property.
	// 
	bool SelectMailbox(const wchar_t *mailbox);

	// Allows for the sending of arbitrary commands to the IMAP server.
	bool SendRawCommand(const wchar_t *cmd, CkString &outRawResponse);
	// Allows for the sending of arbitrary commands to the IMAP server.
	const wchar_t *sendRawCommand(const wchar_t *cmd);

	// The same as SendRawCommand, but instead of returning the response as a string,
	// the binary bytes of the response are returned.
	bool SendRawCommandB(const wchar_t *cmd, CkByteData &outBytes);

	// The same as SendRawCommandB, except that the command is provided as binary bytes
	// rather than a string.
	bool SendRawCommandC(const CkByteData &cmd, CkByteData &outBytes);

#if defined(CK_CSP_INCLUDED)
	// (Only applies to the Microsoft Windows OS) Sets the Cryptographic Service
	// Provider (CSP) to be used for encryption or digital signing, or decryption /
	// signature verification.
	// 
	// This is not commonly used becaues the default Microsoft CSP is typically
	// appropriate. One instance where SetCSP is necessary is when using the Crypto-Pro
	// CSP for the GOST R 34.10-2001 and GOST R 34.10-94 providers.
	// 
	bool SetCSP(CkCspW &csp);
#endif

	// Used to explicitly specify the certificate and associated private key to be used
	// for decrypting S/MIME (PKCS7) email.
	bool SetDecryptCert2(const CkCertW &cert, CkPrivateKeyW &key);

	// Sets a flag for a single message on the IMAP server. If  value = 1, the flag is
	// turned on, if  value = 0, the flag is turned off. Standard system flags such as
	// "\Deleted", "\Seen", "\Answered", "\Flagged", "\Draft", and "\Answered" may be
	// set. Custom flags such as "NonJunk", "$label1", "$MailFlagBit1", etc. may also
	// be set.
	// 
	// If  bUid is true, then msgId represents a UID. If  bUid is false, then msgId
	// represents a sequence number.
	// 
	bool SetFlag(int msgId, bool bUid, const wchar_t *flagName, int value);

	// Sets a flag for each message in the message set on the IMAP server. If  value = 1,
	// the flag is turned on, if  value = 0, the flag is turned off. Standard system
	// flags such as "\Deleted", "\Seen", "\Answered", "\Flagged", "\Draft", and
	// "\Answered" may be set. Custom flags such as "NonJunk", "$label1",
	// "$MailFlagBit1", etc. may also be set.
	bool SetFlags(const CkMessageSetW &messageSet, const wchar_t *flagName, int value);

	// Sets a flag for a single message on the IMAP server. The UID of the email object
	// is used to find the message on the IMAP server that is to be affected. If  value =
	// 1, the flag is turned on, if  value = 0, the flag is turned off.
	// 
	// Both standard system flags as well as custom flags may be set. Standard system
	// flags typically begin with a backslash character, such as "\Deleted", "\Seen",
	// "\Answered", "\Flagged", "\Draft", and "\Answered". Custom flags can be
	// anything, such as "NonJunk", "$label1", "$MailFlagBit1", etc. .
	// 
	// Note: When the Chilkat IMAP component downloads an email from an IMAP server, it
	// inserts a "ckx-imap-uid" header field in the email object. This is subsequently
	// used by this method to get the UID associated with the email. The "ckx-imap-uid"
	// header must be present for this method to be successful.
	// 
	// Note: Calling this method is identical to calling the SetFlag method, except the
	// UID is automatically obtained from the email object.
	// 
	bool SetMailFlag(const CkEmailW &email, const wchar_t *flagName, int value);

	// Specifies a client-side certificate to be used for the SSL / TLS connection. In
	// most cases, servers do not require client-side certificates for SSL/TLS. A
	// client-side certificate is typically used in high-security situations where the
	// certificate is an additional means to indentify the client to the server.
	bool SetSslClientCert(CkCertW &cert);

	// (Same as SetSslClientCert, but allows a .pfx/.p12 file to be used directly)
	// Specifies a client-side certificate to be used for the SSL / TLS connection. In
	// most cases, servers do not require client-side certificates for SSL/TLS. A
	// client-side certificate is typically used in high-security situations where the
	// certificate is an additional means to indentify the client to the server.
	// 
	// The pemDataOrFilename may contain the actual PEM data, or it may contain the path of the PEM
	// file. This method will automatically recognize whether it is a path or the PEM
	// data itself.
	// 
	bool SetSslClientCertPem(const wchar_t *pemDataOrFilename, const wchar_t *pemPassword);

	// (Same as SetSslClientCert, but allows a .pfx/.p12 file to be used directly)
	// Specifies a client-side certificate to be used for the SSL / TLS connection. In
	// most cases, servers do not require client-side certificates for SSL/TLS. A
	// client-side certificate is typically used in high-security situations where the
	// certificate is an additional means to indentify the client to the server.
	bool SetSslClientCertPfx(const wchar_t *pfxFilename, const wchar_t *pfxPassword);

	// Authenticates with the SSH server using public-key authentication. The
	// corresponding public key must have been installed on the SSH server for the
	// sshLogin. Authentication will succeed if the matching  privateKey is provided.
	// 
	// Important: When reporting problems, please send the full contents of the
	// LastErrorText property to support@chilkatsoft.com.
	// 
	bool SshAuthenticatePk(const wchar_t *sshLogin, CkSshKeyW &privateKey);

	// Authenticates with the SSH server using a sshLogin and  sshPassword.
	// 
	// An SSH tunneling (port forwarding) session always begins by first calling
	// SshTunnel to connect to the SSH server, then calling either AuthenticatePw or
	// AuthenticatePk to authenticate. Following this, your program should call Connect
	// to connect with the IMAP server (via the SSH tunnel) and then Login to
	// authenticate with the IMAP server.
	// 
	// Note: Once the SSH tunnel is setup by calling SshTunnel and SshAuthenticatePw
	// (or SshAuthenticatePk), all underlying communcations with the IMAP server use
	// the SSH tunnel. No changes in programming are required other than making two
	// initial calls to setup the tunnel.
	// 
	// Important: When reporting problems, please send the full contents of the
	// LastErrorText property to support@chilkatsoft.com.
	// 
	bool SshAuthenticatePw(const wchar_t *sshLogin, const wchar_t *sshPassword);

	// Connects to an SSH server and creates a tunnel for IMAP. The sshServerHostname is the
	// hostname (or IP address) of the SSH server. The  sshPort is typically 22, which is
	// the standard SSH port number.
	// 
	// An SSH tunneling (port forwarding) session always begins by first calling
	// SshTunnel to connect to the SSH server, followed by calling either
	// AuthenticatePw or AuthenticatePk to authenticate. Your program would then call
	// Connect to connect with the IMAP server (via the SSH tunnel) and then Login to
	// authenticate with the IMAP server.
	// 
	// Note: Once the SSH tunnel is setup by calling SshTunnel and SshAuthenticatePw
	// (or SshAuthenticatePk), all underlying communcations with the IMAP server use
	// the SSH tunnel. No changes in programming are required other than making two
	// initial calls to setup the tunnel.
	// 
	// Important: When reporting problems, please send the full contents of the
	// LastErrorText property to support@chilkatsoft.com.
	// 
	bool SshTunnel(const wchar_t *sshServerHostname, int sshServerPort);

	// Sets one or more flags to a specific value for an email. The email is indicated
	// by either a UID or sequence number, depending on whether  bUid is true (UID) or
	// false (sequence number).
	// 
	//  flagNames should be a space separated string of flag names. Both standard and
	// customer flags may be set. Standard flag names typically begin with a backslash
	// character. For example: "\Seen \Answered". Custom flag names may also be
	// included. Custom flags often begin with a $ character, such as "$label1", or
	// "$MailFlagBit0". Other customer flags may begin with any character, such as
	// "NonJunk".
	// 
	//  value should be 1 to turn the flags on, or 0 to turn the flags off.
	// 
	bool StoreFlags(int msgId, bool bUid, const wchar_t *flagNames, int value);

	// Subscribe to an IMAP mailbox.
	bool Subscribe(const wchar_t *mailbox);

	// Unlocks the component. This must be called once at the beginning of your program
	// to unlock the component. A permanent unlock code is provided when the IMAP
	// component is licensed. Any string, such as "Hello World", may be passed to this
	// method to automatically begin a fully-functional 30-day trial.
	// 
	// A valid permanent unlock code for this object will always included the substring
	// "IMAP".
	// 
	// Note: A permanent unlock code for IMAP will also always include the substring
	// "MAIL", and therefore it may be used for the MailMan (or CkMailMan)
	// object/class. The IMAP license includes POP3/SMTP functionality, and therefore
	// the MailMan object/class may be unlocked using the same unlock code.
	// 
	bool UnlockComponent(const wchar_t *unlockCode);

	// Unsubscribe from an IMAP mailbox.
	bool Unsubscribe(const wchar_t *mailbox);

	// Sends a CAPABILITY command to the IMAP server and returns the raw response.
	bool Capability(CkString &outStr);
	// Sends a CAPABILITY command to the IMAP server and returns the raw response.
	const wchar_t *capability(void);

	// Sends an IDLE command to the IMAP server to begin receiving real-time updates.
	bool IdleStart(void);

	// Sends a command to the IMAP server to stop receiving real-time updates.
	bool IdleDone(void);

	// Polls the connection to see if any real-time updates are available. The ARG1
	// indicates how long to wait for incoming updates. This method does not send a
	// command to the IMAP server, it simply checks the connection for already-arrived
	// messages that the IMAP server sent. This method would only be called after IDLE
	// has already been started via the IdleStart method.
	// 
	// If updates are available, they are returned in an XML string having the format
	// as shown below. There is one child node for each notification. The possible
	// notifcations are:
	//     flags -- lists flags that have been set or unset for an email.
	//     expunge -- provides the sequence number for an email that has been deleted.
	//     exists -- reports the new number of messages in the currently selected
	//     mailbox.
	//     recent -- reports the new number of messages with the /RECENT flag set.
	//     raw -- reports an unanticipated response line that was not parsed by
	//     Chilkat. This should be reported to support@chilkatoft.com
	// 
	// A sample showing all possible notifications (except for "raw") is shown below.
	// _LT_idle_GT_
	//     _LT_flags seqnum="59" uid="11876"_GT_
	//         _LT_flag_GT_\Deleted_LT_/flag_GT_
	//         _LT_flag_GT_\Seen_LT_/flag_GT_
	//     _LT_/flags_GT_
	//     _LT_flags seqnum="69" uid="11889"_GT_
	//         _LT_flag_GT_\Seen_LT_/flag_GT_
	//     _LT_/flags_GT_
	//     _LT_expunge_GT_58_LT_/expunge_GT_
	//     _LT_expunge_GT_58_LT_/expunge_GT_
	//     _LT_expunge_GT_67_LT_/expunge_GT_
	//     _LT_exists_GT_115_LT_/exists_GT_
	//     _LT_recent_GT_0_LT_/recent_GT_
	// _LT_/idle_GT_
	// 
	// If no updates have been received, the returned XML string has the following
	// format, as shown below. The
	// _LT_idle_GT__LT_/idle_GT_
	// 
	// NOTE:Once IdleStart has been called, this method can and should be called
	// frequently to see if any updates have arrived. This is NOT the same as polling
	// the IMAP server because it does not send any requests to the IMAP server. It
	// simply checks to see if any messages (i.e. updates) from the IMAP server are
	// available and waiting to be read.
	// 
	bool IdleCheck(int timeoutMs, CkString &outStr);
	// Polls the connection to see if any real-time updates are available. The ARG1
	// indicates how long to wait for incoming updates. This method does not send a
	// command to the IMAP server, it simply checks the connection for already-arrived
	// messages that the IMAP server sent. This method would only be called after IDLE
	// has already been started via the IdleStart method.
	// 
	// If updates are available, they are returned in an XML string having the format
	// as shown below. There is one child node for each notification. The possible
	// notifcations are:
	//     flags -- lists flags that have been set or unset for an email.
	//     expunge -- provides the sequence number for an email that has been deleted.
	//     exists -- reports the new number of messages in the currently selected
	//     mailbox.
	//     recent -- reports the new number of messages with the /RECENT flag set.
	//     raw -- reports an unanticipated response line that was not parsed by
	//     Chilkat. This should be reported to support@chilkatoft.com
	// 
	// A sample showing all possible notifications (except for "raw") is shown below.
	// _LT_idle_GT_
	//     _LT_flags seqnum="59" uid="11876"_GT_
	//         _LT_flag_GT_\Deleted_LT_/flag_GT_
	//         _LT_flag_GT_\Seen_LT_/flag_GT_
	//     _LT_/flags_GT_
	//     _LT_flags seqnum="69" uid="11889"_GT_
	//         _LT_flag_GT_\Seen_LT_/flag_GT_
	//     _LT_/flags_GT_
	//     _LT_expunge_GT_58_LT_/expunge_GT_
	//     _LT_expunge_GT_58_LT_/expunge_GT_
	//     _LT_expunge_GT_67_LT_/expunge_GT_
	//     _LT_exists_GT_115_LT_/exists_GT_
	//     _LT_recent_GT_0_LT_/recent_GT_
	// _LT_/idle_GT_
	// 
	// If no updates have been received, the returned XML string has the following
	// format, as shown below. The
	// _LT_idle_GT__LT_/idle_GT_
	// 
	// NOTE:Once IdleStart has been called, this method can and should be called
	// frequently to see if any updates have arrived. This is NOT the same as polling
	// the IMAP server because it does not send any requests to the IMAP server. It
	// simply checks to see if any messages (i.e. updates) from the IMAP server are
	// available and waiting to be read.
	// 
	const wchar_t *idleCheck(int timeoutMs);

	// Adds an XML certificate vault to the object's internal list of sources to be
	// searched for certificates and private keys when encrypting/decrypting or
	// signing/verifying. Unlike the AddPfxSourceData and AddPfxSourceFile methods,
	// only a single XML certificate vault can be used. If UseCertVault is called
	// multiple times, only the last certificate vault will be used, as each call to
	// UseCertVault will replace the certificate vault provided in previous calls.
	bool UseCertVault(CkXmlCertVaultW &vault);

	// Explicitly specifies the certificate to be used for decrypting encrypted email.
	bool SetDecryptCert(CkCertW &cert);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
