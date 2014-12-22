// CkSsh.h: interface for the CkSsh class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkSsh_H
#define _CkSsh_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkMultiByteBase.h"

class CkSshKey;
class CkStringArray;
class CkByteData;
class CkBaseProgress;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkSsh
class CK_VISIBLE_PUBLIC CkSsh  : public CkMultiByteBase
{
    private:
	CkBaseProgress *m_callback;

	// Don't allow assignment or copying these objects.
	CkSsh(const CkSsh &);
	CkSsh &operator=(const CkSsh &);

    public:
	CkSsh(void);
	virtual ~CkSsh(void);

	static CkSsh *createNew(void);
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	CkBaseProgress *get_EventCallbackObject(void) const;
	void put_EventCallbackObject(CkBaseProgress *progress);


	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------
	// If a request to open a channel fails, this property contains a code that
	// identifies the reason for failure. The reason codes are defined in RFC 4254 and
	// are reproduced here:
	//              Symbolic name                           reason code
	//              -------------                           -----------
	//             SSH_OPEN_ADMINISTRATIVELY_PROHIBITED          1
	//             SSH_OPEN_CONNECT_FAILED                       2
	//             SSH_OPEN_UNKNOWN_CHANNEL_TYPE                 3
	//             SSH_OPEN_RESOURCE_SHORTAGE                    4
	int get_ChannelOpenFailCode(void);

	// The descriptive text corresponding to the ChannelOpenFailCode property.
	void get_ChannelOpenFailReason(CkString &str);
	// The descriptive text corresponding to the ChannelOpenFailCode property.
	const char *channelOpenFailReason(void);

	// The client-identifier string to be used when connecting to an SSH/SFTP server.
	// Defaults to "SSH-2.0-PuTTY_Local:_Jun_27_2008_16:28:58". (This string is used to
	// mimic PuTTY because some servers are known to disconnect from clients with
	// unknown client identifiers.)
	void get_ClientIdentifier(CkString &str);
	// The client-identifier string to be used when connecting to an SSH/SFTP server.
	// Defaults to "SSH-2.0-PuTTY_Local:_Jun_27_2008_16:28:58". (This string is used to
	// mimic PuTTY because some servers are known to disconnect from clients with
	// unknown client identifiers.)
	const char *clientIdentifier(void);
	// The client-identifier string to be used when connecting to an SSH/SFTP server.
	// Defaults to "SSH-2.0-PuTTY_Local:_Jun_27_2008_16:28:58". (This string is used to
	// mimic PuTTY because some servers are known to disconnect from clients with
	// unknown client identifiers.)
	void put_ClientIdentifier(const char *newVal);

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
	const char *clientIpAddress(void);
	// The IP address to use for computers with multiple network interfaces or IP
	// addresses. For computers with a single network interface (i.e. most computers),
	// this property should not be set. For multihoming computers, the default IP
	// address is automatically used if this property is not set.
	// 
	// The IP address is a string such as in dotted notation using numbers, not domain
	// names, such as "165.164.55.124".
	// 
	void put_ClientIpAddress(const char *newVal);

	// Maximum number of milliseconds to wait when connecting to an SSH server.
	int get_ConnectTimeoutMs(void);
	// Maximum number of milliseconds to wait when connecting to an SSH server.
	void put_ConnectTimeoutMs(int newVal);

	// If the SSH server sent a DISCONNECT message when closing the connection, this
	// property contains the "reason code" as specified in RFC 4253:
	//            Symbolic name                                reason code
	//            -------------                                -----------
	//       SSH_DISCONNECT_HOST_NOT_ALLOWED_TO_CONNECT             1
	//       SSH_DISCONNECT_PROTOCOL_ERROR                          2
	//       SSH_DISCONNECT_KEY_EXCHANGE_FAILED                     3
	//       SSH_DISCONNECT_RESERVED                                4
	//       SSH_DISCONNECT_MAC_ERROR                               5
	//       SSH_DISCONNECT_COMPRESSION_ERROR                       6
	//       SSH_DISCONNECT_SERVICE_NOT_AVAILABLE                   7
	//       SSH_DISCONNECT_PROTOCOL_VERSION_NOT_SUPPORTED          8
	//       SSH_DISCONNECT_HOST_KEY_NOT_VERIFIABLE                 9
	//       SSH_DISCONNECT_CONNECTION_LOST                        10
	//       SSH_DISCONNECT_BY_APPLICATION                         11
	//       SSH_DISCONNECT_TOO_MANY_CONNECTIONS                   12
	//       SSH_DISCONNECT_AUTH_CANCELLED_BY_USER                 13
	//       SSH_DISCONNECT_NO_MORE_AUTH_METHODS_AVAILABLE         14
	//       SSH_DISCONNECT_ILLEGAL_USER_NAME                      15
	int get_DisconnectCode(void);

	// If the SSH/ server sent a DISCONNECT message when closing the connection, this
	// property contains a descriptive string for the "reason code" as specified in RFC
	// 4253.
	void get_DisconnectReason(CkString &str);
	// If the SSH/ server sent a DISCONNECT message when closing the connection, this
	// property contains a descriptive string for the "reason code" as specified in RFC
	// 4253.
	const char *disconnectReason(void);

	// Set to one of the following encryption algorithms to force that cipher to be
	// used. By default, the component will automatically choose the first cipher
	// supported by the server in the order listed here: "aes256-cbc", "aes128-cbc",
	// "twofish256-cbc", "twofish128-cbc", "blowfish-cbc", "3des-cbc", "arcfour128",
	// "arcfour256". (If blowfish is chosen, the encryption strength is 128 bits.)
	// 
	// Important: If this is property is set and the server does NOT support then
	// encryption algorithm, then the Connect will fail.
	// 
	void get_ForceCipher(CkString &str);
	// Set to one of the following encryption algorithms to force that cipher to be
	// used. By default, the component will automatically choose the first cipher
	// supported by the server in the order listed here: "aes256-cbc", "aes128-cbc",
	// "twofish256-cbc", "twofish128-cbc", "blowfish-cbc", "3des-cbc", "arcfour128",
	// "arcfour256". (If blowfish is chosen, the encryption strength is 128 bits.)
	// 
	// Important: If this is property is set and the server does NOT support then
	// encryption algorithm, then the Connect will fail.
	// 
	const char *forceCipher(void);
	// Set to one of the following encryption algorithms to force that cipher to be
	// used. By default, the component will automatically choose the first cipher
	// supported by the server in the order listed here: "aes256-cbc", "aes128-cbc",
	// "twofish256-cbc", "twofish128-cbc", "blowfish-cbc", "3des-cbc", "arcfour128",
	// "arcfour256". (If blowfish is chosen, the encryption strength is 128 bits.)
	// 
	// Important: If this is property is set and the server does NOT support then
	// encryption algorithm, then the Connect will fail.
	// 
	void put_ForceCipher(const char *newVal);

	// This is the number of milliseconds between each AbortCheck event callback. The
	// AbortCheck callback allows an application to abort any SSH operation prior to
	// completion. If HeartbeatMs is 0 (the default), no AbortCheck event callbacks
	// will fire.
	int get_HeartbeatMs(void);
	// This is the number of milliseconds between each AbortCheck event callback. The
	// AbortCheck callback allows an application to abort any SSH operation prior to
	// completion. If HeartbeatMs is 0 (the default), no AbortCheck event callbacks
	// will fire.
	void put_HeartbeatMs(int newVal);

	// Indicates the preferred host key algorithm to be used in establishing the SSH
	// secure connection. The default is "DSS". It may be changed to "RSA" if needed.
	// Chilkat recommends not changing this unless a problem warrants the change.
	void get_HostKeyAlg(CkString &str);
	// Indicates the preferred host key algorithm to be used in establishing the SSH
	// secure connection. The default is "DSS". It may be changed to "RSA" if needed.
	// Chilkat recommends not changing this unless a problem warrants the change.
	const char *hostKeyAlg(void);
	// Indicates the preferred host key algorithm to be used in establishing the SSH
	// secure connection. The default is "DSS". It may be changed to "RSA" if needed.
	// Chilkat recommends not changing this unless a problem warrants the change.
	void put_HostKeyAlg(const char *newVal);

	// Set after connecting to an SSH server. The format of the fingerprint looks like
	// this: "ssh-rsa 1024 68:ff:d1:4e:6c:ff:d7:b0:d6:58:73:85:07:bc:2e:d5"
	void get_HostKeyFingerprint(CkString &str);
	// Set after connecting to an SSH server. The format of the fingerprint looks like
	// this: "ssh-rsa 1024 68:ff:d1:4e:6c:ff:d7:b0:d6:58:73:85:07:bc:2e:d5"
	const char *hostKeyFingerprint(void);

	// If an HTTP proxy requiring authentication is to be used, set this property to
	// the HTTP proxy authentication method name. Valid choices are "LOGIN" or "NTLM".
	void get_HttpProxyAuthMethod(CkString &str);
	// If an HTTP proxy requiring authentication is to be used, set this property to
	// the HTTP proxy authentication method name. Valid choices are "LOGIN" or "NTLM".
	const char *httpProxyAuthMethod(void);
	// If an HTTP proxy requiring authentication is to be used, set this property to
	// the HTTP proxy authentication method name. Valid choices are "LOGIN" or "NTLM".
	void put_HttpProxyAuthMethod(const char *newVal);

	// The NTLM authentication domain (optional) if NTLM authentication is used w/ the
	// HTTP proxy.
	void get_HttpProxyDomain(CkString &str);
	// The NTLM authentication domain (optional) if NTLM authentication is used w/ the
	// HTTP proxy.
	const char *httpProxyDomain(void);
	// The NTLM authentication domain (optional) if NTLM authentication is used w/ the
	// HTTP proxy.
	void put_HttpProxyDomain(const char *newVal);

	// If an HTTP proxy is to be used, set this property to the HTTP proxy hostname or
	// IPv4 address (in dotted decimal notation).
	void get_HttpProxyHostname(CkString &str);
	// If an HTTP proxy is to be used, set this property to the HTTP proxy hostname or
	// IPv4 address (in dotted decimal notation).
	const char *httpProxyHostname(void);
	// If an HTTP proxy is to be used, set this property to the HTTP proxy hostname or
	// IPv4 address (in dotted decimal notation).
	void put_HttpProxyHostname(const char *newVal);

	// If an HTTP proxy requiring authentication is to be used, set this property to
	// the HTTP proxy password.
	void get_HttpProxyPassword(CkString &str);
	// If an HTTP proxy requiring authentication is to be used, set this property to
	// the HTTP proxy password.
	const char *httpProxyPassword(void);
	// If an HTTP proxy requiring authentication is to be used, set this property to
	// the HTTP proxy password.
	void put_HttpProxyPassword(const char *newVal);

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
	const char *httpProxyUsername(void);
	// If an HTTP proxy requiring authentication is to be used, set this property to
	// the HTTP proxy login name.
	void put_HttpProxyUsername(const char *newVal);

	// Causes SSH operations to fail when progress for sending or receiving data halts
	// for more than this number of milliseconds. Setting IdleTimeoutMs = 0 (the
	// default) allows the application to wait indefinitely.
	int get_IdleTimeoutMs(void);
	// Causes SSH operations to fail when progress for sending or receiving data halts
	// for more than this number of milliseconds. Setting IdleTimeoutMs = 0 (the
	// default) allows the application to wait indefinitely.
	void put_IdleTimeoutMs(int newVal);

	// Returns true if the component is connected to an SSH server.
	// 
	// Note: The IsConnected property is set to true after successfully completing
	// the Connect method call. The IsConnected property will only be set to false by
	// calling Disconnect, or by the failure of another method call such that the
	// disconnection is detected.
	// 
	bool get_IsConnected(void);

	// Controls whether communications to/from the SSH server are saved to the
	// SessionLog property. The default value is false. If this property is set to
	// true, the contents of the SessionLog property will continuously grow as SSH
	// activity transpires. The purpose of the KeepSessionLog / SessionLog properties
	// is to help in debugging any future problems that may arise.
	bool get_KeepSessionLog(void);
	// Controls whether communications to/from the SSH server are saved to the
	// SessionLog property. The default value is false. If this property is set to
	// true, the contents of the SessionLog property will continuously grow as SSH
	// activity transpires. The purpose of the KeepSessionLog / SessionLog properties
	// is to help in debugging any future problems that may arise.
	void put_KeepSessionLog(bool newVal);

	// The maximum packet length to be used in the SSH transport protocol. The default
	// value is 8192. (This should generally be left unchanged.)
	int get_MaxPacketSize(void);
	// The maximum packet length to be used in the SSH transport protocol. The default
	// value is 8192. (This should generally be left unchanged.)
	void put_MaxPacketSize(int newVal);

	// The number of currently open channels.
	int get_NumOpenChannels(void);

	// Set by the AuthenticatePw and AuthenticatePwPk methods. If the authenticate
	// method returns a failed status, and this property is set to true, then it
	// indicates the server requested a password change. In this case, re-call the
	// authenticate method, but provide both the old and new passwords in the following
	// format, where vertical bar characters encapsulate the old and new passwords:
	// 
	//     |oldPassword|newPassword|
	// 
	bool get_PasswordChangeRequested(void);

	// The maximum amount of time to allow for reading messages/data from the SSH
	// server. This is different from the IdleTimeoutMs property. The IdleTimeoutMs is
	// the maximum amount of time to wait while no incoming data is arriving. The
	// ReadTimeoutMs is the maximum amount of time to allow for reading data even if
	// data is continuing to arrive. The default value of 0 indicates an infinite
	// timeout value.
	int get_ReadTimeoutMs(void);
	// The maximum amount of time to allow for reading messages/data from the SSH
	// server. This is different from the IdleTimeoutMs property. The IdleTimeoutMs is
	// the maximum amount of time to wait while no incoming data is arriving. The
	// ReadTimeoutMs is the maximum amount of time to allow for reading data even if
	// data is continuing to arrive. The default value of 0 indicates an infinite
	// timeout value.
	void put_ReadTimeoutMs(int newVal);

	// Contains a log of the messages sent to/from the SSH server. To enable session
	// logging, set the KeepSessionLog property = true. Note: This property is not a
	// filename -- it is a string property that contains the session log data.
	void get_SessionLog(CkString &str);
	// Contains a log of the messages sent to/from the SSH server. To enable session
	// logging, set the KeepSessionLog property = true. Note: This property is not a
	// filename -- it is a string property that contains the session log data.
	const char *sessionLog(void);

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
	const char *socksHostname(void);
	// The SOCKS4/SOCKS5 hostname or IPv4 address (in dotted decimal notation). This
	// property is only used if the SocksVersion property is set to 4 or 5).
	void put_SocksHostname(const char *newVal);

	// The SOCKS5 password (if required). The SOCKS4 protocol does not include the use
	// of a password, so this does not apply to SOCKS4.
	void get_SocksPassword(CkString &str);
	// The SOCKS5 password (if required). The SOCKS4 protocol does not include the use
	// of a password, so this does not apply to SOCKS4.
	const char *socksPassword(void);
	// The SOCKS5 password (if required). The SOCKS4 protocol does not include the use
	// of a password, so this does not apply to SOCKS4.
	void put_SocksPassword(const char *newVal);

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
	const char *socksUsername(void);
	// The SOCKS4/SOCKS5 proxy username. This property is only used if the SocksVersion
	// property is set to 4 or 5).
	void put_SocksUsername(const char *newVal);

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

	// If true, then stderr is redirected to stdout. In this case, channel output for
	// both stdout and stderr is combined and retrievable via the following methods:
	// GetReceivedData, GetReceivedDataN, GetReceivedText, GetReceivedTextS. If this
	// property is false, then stderr is available separately via the
	// GetReceivedStderr method.
	// 
	// The default value of this property is true.
	// 
	// Note: Most SSH servers do not send stderr output as "extended data" packets as
	// specified in RFC 4254. The SessionLog may be examined to see if any
	// CHANNEL_EXTENDED_DATA messages exist. If not, then all of the output (stdout +
	// stderr) was sent via CHANNEL_DATA messages, and therefore it is not possible to
	// differentiate stderr output from stdout. In summary: This feature will not work
	// for most SSH servers.
	// 
	bool get_StderrToStdout(void);
	// If true, then stderr is redirected to stdout. In this case, channel output for
	// both stdout and stderr is combined and retrievable via the following methods:
	// GetReceivedData, GetReceivedDataN, GetReceivedText, GetReceivedTextS. If this
	// property is false, then stderr is available separately via the
	// GetReceivedStderr method.
	// 
	// The default value of this property is true.
	// 
	// Note: Most SSH servers do not send stderr output as "extended data" packets as
	// specified in RFC 4254. The SessionLog may be examined to see if any
	// CHANNEL_EXTENDED_DATA messages exist. If not, then all of the output (stdout +
	// stderr) was sent via CHANNEL_DATA messages, and therefore it is not possible to
	// differentiate stderr output from stdout. In summary: This feature will not work
	// for most SSH servers.
	// 
	void put_StderrToStdout(bool newVal);

	// Controls whether the TCP_NODELAY socket option is used for the underlying TCP/IP
	// socket. The default value is true. This disables the Nagle algorithm and
	// allows for better performance when small amounts of data are sent to/from the
	// SSH server.
	bool get_TcpNoDelay(void);
	// Controls whether the TCP_NODELAY socket option is used for the underlying TCP/IP
	// socket. The default value is true. This disables the Nagle algorithm and
	// allows for better performance when small amounts of data are sent to/from the
	// SSH server.
	void put_TcpNoDelay(bool newVal);

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
	// Authenticates with the SSH server using public-key authentication. The
	// corresponding public key must have been installed on the SSH server for the
	// username. Authentication will succeed if the matching  privateKey is provided.
	// 
	// Important: When reporting problems, please send the full contents of the
	// LastErrorText property to support@chilkatsoft.com.
	// 
	bool AuthenticatePk(const char *username, CkSshKey &privateKey);

	// Authenticates with the SSH server using a login and  password.
	// 
	// An SSH session always begins by first calling Connect to connect to the SSH
	// server, and then calling either AuthenticatePw or AuthenticatePk to login.
	// 
	// Important: When reporting problems, please send the full contents of the
	// LastErrorText property to support@chilkatsoft.com.
	// Note: To learn about how to handle password change requests, see the
	// PasswordChangeRequested property (above).
	// 
	bool AuthenticatePw(const char *login, const char *password);

	// Authentication for SSH servers that require both a password and private key.
	// (Most SSH servers are configured to require one or the other, but not both.)
	// 
	// Important: When reporting problems, please send the full contents of the
	// LastErrorText property to support@chilkatsoft.com.
	// 
	bool AuthenticatePwPk(const char *username, const char *password, CkSshKey &privateKey);

	// Returns true if the channel indicated by channelNum is open. Otherwise returns
	// false.
	bool ChannelIsOpen(int channelNum);

	// Polls for incoming data on an open channel. This method will read a channel,
	// waiting at most  pollTimeoutMs milliseconds for data to arrive. Return values are as
	// follows:
	// 
	// -1 -- Error. Check the IsConnected property to see if the connection to the SSH
	// server is still valid. Also, call ChannelIsOpen to see if the channel remains
	// open. The LastErrorText property will contain more detailed information
	// regarding the error.
	// 
	// -2 -- No additional data was received prior to the poll timeout.
	// 
	// >0 -- Additional data was received and the return value indicates how many bytes
	// are available to be "picked up". Methods that read data on a channel do not
	// return the received data directly. Instead, they return an integer to indicate
	// how many bytes are available to be "picked up". An application picks up the
	// available data by calling GetReceivedData or GetReceivedText.
	// 
	int ChannelPoll(int channelNum, int pollTimeoutMs);

	// Reads incoming data on an open channel. This method will read a channel, waiting
	// at most IdleTimeoutMs milliseconds for data to arrive. Return values are as
	// follows:
	// 
	// -1 -- Error. Check the IsConnected property to see if the connection to the SSH
	// server is still valid. Also, call ChannelIsOpen to see if the channel remains
	// open. The LastErrorText property will contain more detailed information
	// regarding the error.
	// 
	// -2 -- No additional data was received prior to the IdleTimeoutMs timeout.
	// 
	// >0 -- Additional data was received and the return value indicates how many bytes
	// are available to be "picked up". Methods that read data on a channel do not
	// return the received data directly. Instead, they return an integer to indicate
	// how many bytes are available to be "picked up". An application picks up the
	// available data by calling GetReceivedData or GetReceivedText.
	// 
	int ChannelRead(int channelNum);

	// Reads incoming data on an open channel and continues reading until no data
	// arrives for  pollTimeoutMs milliseconds. The first read will wait a max of IdleTimeoutMs
	// milliseconds before timing out. Subsequent reads wait a max of  pollTimeoutMs milliseconds
	// before timing out.
	// 
	// The idea behind ChannelReadAndPoll is to capture the output of a shell command.
	// One might imagine the typical sequence of events when executing a shell command
	// to be like this: (1) client sends command to server, (2) server executes the
	// command (i.e. it's computing...), potentially taking some amount of time, (3)
	// output is streamed back to the client. It makes sense for the client to wait a
	// longer period of time for the first data to arrive, but once it begins arriving,
	// the timeout can be shortened. This is exactly what ChannelReadAndPoll does --
	// the first timeout is controlled by the IdleTimeoutMs property, while the
	// subsequent reads (once output starts flowing) is controlled by  pollTimeoutMs.
	// 
	// Return values are as follows:
	// -1 -- Error. Check the IsConnected property to see if the connection to the SSH
	// server is still valid. Also, call ChannelIsOpen to see if the channel remains
	// open. The LastErrorText property will contain more detailed information
	// regarding the error.
	// 
	// -2 -- No additional data was received prior to the IdleTimeoutMs timeout.
	// 
	// >0 -- Additional data was received and the return value indicates how many bytes
	// are available to be "picked up". Methods that read data on a channel do not
	// return the received data directly. Instead, they return an integer to indicate
	// how many bytes are available to be "picked up". An application picks up the
	// available data by calling GetReceivedData or GetReceivedText.
	// 
	int ChannelReadAndPoll(int channelNum, int pollTimeoutMs);

	// The same as ChannelReadAndPoll, except this method will return as soon as  maxNumBytes
	// is exceeded, which may be as large as the MaxPacketSize property setting.
	int ChannelReadAndPoll2(int channelNum, int pollTimeoutMs, int maxNumBytes);

	// Reads incoming data on an open channel until the channel is closed by the
	// server. If successful, the number of bytes available to be "picked up" can be
	// determined by calling GetReceivedNumBytes. The received data may be retrieved by
	// calling GetReceivedData or GetReceivedText.
	bool ChannelReceiveToClose(int channelNum);

	// Reads incoming text data on an open channel until the received data matches the
	//  matchPattern. For example, to receive data until the string "Hello World" arrives, set
	//  matchPattern equal to "*Hello World*".  charset indicates the character encoding of the text
	// being received ("iso-8859-1" for example).  caseSensitive may be set to true for case
	// sensitive matching, or false for case insensitive matching.
	// 
	// Returns true if text data matching  matchPattern was received and is available to be
	// picked up by calling GetReceivedText (or GetReceivedTextS). IMPORTANT: This
	// method may read beyond the matching text. Call GetReceivedTextS to extract only
	// the data up-to and including the matching text.
	// 
	bool ChannelReceiveUntilMatch(int channelNum, const char *matchPattern, const char *charset, bool caseSensitive);

	// The same as ChannelReceiveUntilMatch except that the method returns when any one
	// of the match patterns specified in  matchPatterns are received on the channel.
	bool ChannelReceiveUntilMatchN(int channelNum, CkStringArray &matchPatterns, const char *charset, bool caseSensitive);

	// true if a CLOSE message has been received on the channel indicated by channelNum.
	// When a CLOSE is received, no subsequent data should be sent in either direction
	// -- the channel is closed in both directions.
	bool ChannelReceivedClose(int channelNum);

	// true if an EOF message has been received on the channel indicated by channelNum.
	// When an EOF is received, no more data will be forthcoming on the channel.
	// However, data may still be sent in the opposite direction.
	bool ChannelReceivedEof(int channelNum);

	// true if an exit status code was received on the channel. Otherwise false.
	bool ChannelReceivedExitStatus(int channelNum);

	// Sends a CLOSE message to the server for the channel indicated by channelNum. This
	// closes both directions of the bidirectional channel.
	bool ChannelSendClose(int channelNum);

	// Sends byte data on the channel indicated by channelNum.
	bool ChannelSendData(int channelNum, const CkByteData &data);

	// Sends an EOF for the channel indicated by channelNum. Once an EOF is sent, no
	// additional data may be sent on the channel. However, the channel remains open
	// and additional data may still be received from the server.
	bool ChannelSendEof(int channelNum);

	// Sends character data on the channel indicated by channelNum. The text is converted to
	// the charset indicated by  charset prior to being sent. A list of supported charset
	// values may be found on this page: Supported Charsets
	// <http://www.chilkatsoft.com/p/p_463.asp> .
	bool ChannelSendString(int channelNum, const char *strData, const char *charset);

	// Clears the collection of TTY modes that are sent with the SendReqPty method.
	void ClearTtyModes(void);

	// Connects to the SSH server at domainName: port
	// 
	// The domainName may be a domain name or an IPv4 or IPv6 address in string format.
	// 
	bool Connect(const char *hostname, int port);

	// Disconnects from the SSH server.
	void Disconnect(void);

	// Returns the exit status code for a channel. This method should only be called if
	// an exit status has been received. You may check to see if the exit status was
	// received by calling ChannelReceivedExitStatus.
	int GetChannelExitStatus(int channelNum);

	// Returns the channel number for the Nth open channel. Indexing begins at 0, and
	// the number of currently open channels is indicated by the NumOpenChannels
	// property. Returns -1 if the index is out of range.
	int GetChannelNumber(int index);

	// Returns a string describing the channel type for the Nth open channel. Channel
	// types are: "session", "x11", "forwarded-tcpip", and "direct-tcpip".
	bool GetChannelType(int index, CkString &outStr);
	// Returns a string describing the channel type for the Nth open channel. Channel
	// types are: "session", "x11", "forwarded-tcpip", and "direct-tcpip".
	const char *getChannelType(int index);
	// Returns a string describing the channel type for the Nth open channel. Channel
	// types are: "session", "x11", "forwarded-tcpip", and "direct-tcpip".
	const char *channelType(int index);

	// Returns the accumulated data received on the channel indicated by channelNum and
	// clears the channel's internal receive buffer.
	bool GetReceivedData(int channelNum, CkByteData &outBytes);

	// Same as GetReceivedData, but a maximum of  maxNumBytes bytes is returned.
	bool GetReceivedDataN(int channelNum, int numBytes, CkByteData &outBytes);

	// Returns the number of bytes available in the internal receive buffer for the
	// specified channelNum. The received data may be retrieved by calling GetReceivedData or
	// GetReceivedText.
	int GetReceivedNumBytes(int channelNum);

	// Returns the accumulated stderr bytes received on the channel indicated by ARG1
	// and clears the channel's internal stderr receive buffer.
	// 
	// Note: If the StderrToStdout property is set to true, then stderr is
	// automatically redirected to stdout. This is the default behavior. The following
	// methods can be called to retrieve the channel's stdout: GetReceivedData,
	// GetReceivedDataN, GetReceivedText, and GetReceivedTextS.
	// 
	bool GetReceivedStderr(int channelNum, CkByteData &outBytes);

	// Returns the accumulated text received on the channel indicated by channelNum and
	// clears the channel's internal receive buffer. The  charset indicates the charset of
	// the character data in the internal receive buffer. A list of supported charset
	// values may be found on this page: Supported Charsets
	// <http://www.chilkatsoft.com/p/p_463.asp> .
	bool GetReceivedText(int channelNum, const char *charset, CkString &outStr);
	// Returns the accumulated text received on the channel indicated by channelNum and
	// clears the channel's internal receive buffer. The  charset indicates the charset of
	// the character data in the internal receive buffer. A list of supported charset
	// values may be found on this page: Supported Charsets
	// <http://www.chilkatsoft.com/p/p_463.asp> .
	const char *getReceivedText(int channelNum, const char *charset);
	// Returns the accumulated text received on the channel indicated by channelNum and
	// clears the channel's internal receive buffer. The  charset indicates the charset of
	// the character data in the internal receive buffer. A list of supported charset
	// values may be found on this page: Supported Charsets
	// <http://www.chilkatsoft.com/p/p_463.asp> .
	const char *receivedText(int channelNum, const char *charset);

	// The same as GetReceivedText, except only the text up to and including  substr is
	// returned. The text returned is removed from the internal receive buffer. If the
	//  substr was not found in the internal receive buffer, an empty string is returned
	// and the internal receive buffer is not modified.
	bool GetReceivedTextS(int channelNum, const char *substr, const char *charset, CkString &outStr);
	// The same as GetReceivedText, except only the text up to and including  substr is
	// returned. The text returned is removed from the internal receive buffer. If the
	//  substr was not found in the internal receive buffer, an empty string is returned
	// and the internal receive buffer is not modified.
	const char *getReceivedTextS(int channelNum, const char *substr, const char *charset);
	// The same as GetReceivedText, except only the text up to and including  substr is
	// returned. The text returned is removed from the internal receive buffer. If the
	//  substr was not found in the internal receive buffer, an empty string is returned
	// and the internal receive buffer is not modified.
	const char *receivedTextS(int channelNum, const char *substr, const char *charset);

	// Opens a custom channel with a custom server that uses the SSH protocol. The channelType
	// is application-defined.
	// 
	// If successful, the channel number is returned. This is the number that should be
	// passed to any method requiring a channel number. A -1 is returned upon failure.
	// 
	int OpenCustomChannel(const char *channelType);

	// Open a direct-tcpip channel for port forwarding. Data sent on the channel via
	// ChannelSend* methods is sent to the SSH server and then forwarded to targetHostname: targetPort.
	// The SSH server automatically forwards data received from targetHostname: targetPort to the SSH
	// client. Therefore, calling ChannelRead* and ChannelReceive* methods is
	// equivalent to reading directly from targetHostname: targetPort.
	// 
	// If successful, the channel number is returned. This is the number that should be
	// passed to any method requiring a channel number. A -1 is returned upon failure.
	// 
	int OpenDirectTcpIpChannel(const char *hostname, int port);

	// Opens a new session channel. Almost everything you will do with the Chilkat SSH
	// component will involve opening a session channel. The normal sequence of
	// operation is typically this: 1) Connect to the SSH server. 2) Authenticate. 3)
	// Open a session channel. 4) do something on the channel such as opening a shell,
	// execute a command, etc.
	// 
	// If successful, the channel number is returned. This is the number that should be
	// passed to any method requiring a channel number. A -1 is returned upon failure.
	// 
	int OpenSessionChannel(void);

	// This is the same as GetReceivedText, except the internal receive buffer is not
	// cleared.
	bool PeekReceivedText(int channelNum, const char *charset, CkString &outStr);
	// This is the same as GetReceivedText, except the internal receive buffer is not
	// cleared.
	const char *peekReceivedText(int channelNum, const char *charset);

	// Initiates a re-key with the SSH server. The ReKey method does not return until
	// the key re-exchange is complete.
	// 
	// RFC 4253 (the SSH Transport Layer Protocol) recommends that keys be changed
	// after each gigabyte of transmitted data or after each hour of connection time,
	// whichever comes sooner. Key re-exchange is a public-key operation and requires a
	// fair amount of processing power and should not be performed too often. Either
	// side (client or server) may initiate a key re-exchange at any time.
	// 
	// In most cases, a server will automatically initiate key re-exchange whenever it
	// deems necessary, and the Chilkat SSH component handles these transparently. For
	// example, if the Chilkat SSH component receives a re-key message from the server
	// while in the process of receiving data on a channel, it will automatically
	// handle the key re-exchange and the application will not even realize that an
	// underlying key re-exchange occurred.
	// 
	bool ReKey(void);

	// Sends an IGNORE message to the SSH server. This is one way of verifying that the
	// connection to the SSH server is open and valid. The SSH server does not response
	// it an IGNORE message, it simply ignores it. IGNORE messages are not associated
	// with a channel (in other words, you do not need to first open a channel prior to
	// sending an IGNORE message).
	bool SendIgnore(void);

	// Initiates execution of a command on the channel specified by channelNum. The  commandLine
	// contains the full command line including any command-line parameters (just as
	// you would type the command at a shell prompt).
	// 
	// The user's default shell (typically defined in /etc/password in UNIX systems) is
	// started on the SSH server to execute the command.
	// 
	// Important: A channel only exists for a single request. You may not call
	// SendReqExec multiple times on the same open channel. The reason is that the SSH
	// server automatically closes the channel at the end of the exec. The solution is
	// to call OpenSessionChannel to get a new channel, and then call SendReqExec using
	// the new channel. It is OK to have more than one channel open simultaneously.
	// 
	bool SendReqExec(int channelNum, const char *command);

	// Requests a pseudo-terminal for a session channel. If the  termType is a character
	// oriented terminal ("vt100" for example), then  widthInChars and  heightInChars would be set to
	// non-zero values, while  widthInPixels and  heightInPixels may be set to 0. If  termType is pixel-oriented,
	// such as "xterm", the reverse is true (i.e. set  widthInPixels and  heightInPixels, but set  widthInChars and
	//  heightInChars equal to 0).
	// 
	// In most cases, you probably don't even want terminal emulation. In that case,
	// try setting  termType = "dumb". Terminal emulation causes terminal escape sequences
	// to be included with shell command output. A "dumb" terminal should have no
	// escape sequences.
	// 
	// Some SSH servers allow a shell to be started (via the SendReqShell method)
	// without the need to first request a pseudo-terminal. The normal sequence for
	// starting a remote shell is as follows:
	// 1) Connect
	// 2) Authenticate
	// 3) OpenSessionChannel
	// 4) Request a PTY via this method if necessary.
	// 5) Start a shell by calling SendReqShell
	// 
	bool SendReqPty(int channelNum, const char *xTermEnvVar, int widthInChars, int heightInRows, int pixWidth, int pixHeight);

	// Sets an environment variable in the remote shell.
	bool SendReqSetEnv(int channelNum, const char *name, const char *value);

	// Starts a shell on an open session channel. Some SSH servers require that a PTY
	// (pseudo-terminal) first be requested prior to starting a shell. In that case,
	// call SendReqPty prior to calling this method. Once a shell is started, commands
	// may be sent by calling ChannelSendString. (Don't forget to terminate commands
	// with a CRLF).
	bool SendReqShell(int channelNum);

	// Delivers a signal to the remote process/service.  signalName is one of the following:
	// ABRT, ALRM, FPE, HUP, ILL, INT, KILL, PIPE, QUIT, SEGV, TERM, USR1, USR2.
	// (Obviously, these are UNIX signals, so the remote SSH server would need to be a
	// Unix/Linux system.)
	bool SendReqSignal(int channelNum, const char *signalName);

	// Executes a pre-defined subsystem. The SFTP protocol (Secure File Transfer
	// Protocol) is started by the Chilkat SFTP component by starting the "sftp"
	// subsystem.
	bool SendReqSubsystem(int channelNum, const char *subsystemName);

	// When the client-side window (terminal) size changes, this message may be sent to
	// the server to inform it of the new size.
	bool SendReqWindowChange(int channelNum, int widthInChars, int heightInRows, int pixWidth, int pixHeight);

	// Allows the client to send an X11 forwarding request to the server. Chilkat only
	// provides this functionality because it is a message defined in the SSH
	// connection protocol. Chilkat has no advice for when or why it would be needed.
	bool SendReqX11Forwarding(int channelNum, bool singleConnection, const char *authProt, const char *authCookie, int screenNum);

	// This method should be ignored and not used.
	bool SendReqXonXoff(int channelNum, bool clientCanDo);

	// Sets a TTY mode that is included in the SendReqPty method call. Most commonly,
	// it is not necessary to call this method at all. Chilkat has no recommendations
	// or expertise as to why or when a particular mode might be useful. This
	// capability is provided because it is defined in the SSH connection protocol
	// specification.
	// 
	// This method can be called multiple times to set many terminal mode flags (one
	// per call).
	// 
	// The  ttyValue is an integer, typically 0 or 1. Valid ttyName flag names include: VINTR,
	// VQUIT, VERASE, VKILL, VEOF, VEOL, VEOL2, VSTART, VSTOP, VSUSP, VDSUSP, VREPRINT,
	// VWERASE, VLNEXT, VFLUSH, VSWTCH, VSTATUS, VDISCARD, IGNPAR, PARMRK, INPCK,
	// ISTRIP, INLCR, IGNCR, ICRNL, IUCLC, IXON, IXANY, IXOFF, IMAXBEL, ISIG, ICANON,
	// XCASE, ECHO, ECHOE, ECHOK, ECHONL, NOFLSH, TOSTOP, IEXTEN, ECHOCTL, ECHOKE,
	// PENDIN, OPOST, OLCUC, ONLCR, OCRNL, ONOCR, ONLRET, CS7, CS8, PARENB, PARODD,
	// TTY_OP_ISPEED, TTY_OP_OSPEED
	// 
	bool SetTtyMode(const char *name, int value);

	// Unlocks the component. This must be called once prior to calling any other
	// method. A fully-functional 30-day trial is automatically started when an
	// arbitrary string is passed to this method. For example, passing "Hello", or
	// "abc123" will unlock the component for the 1st thirty days after the initial
	// install.
	bool UnlockComponent(const char *unlockCode);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
