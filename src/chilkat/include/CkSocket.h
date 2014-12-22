// CkSocket.h: interface for the CkSocket class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkSocket_H
#define _CkSocket_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkMultiByteBase.h"

class CkByteData;
class CkCert;
class CkBaseProgress;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkSocket
class CK_VISIBLE_PUBLIC CkSocket  : public CkMultiByteBase
{
    private:
	CkBaseProgress *m_callback;

	// Don't allow assignment or copying these objects.
	CkSocket(const CkSocket &);
	CkSocket &operator=(const CkSocket &);

    public:
	CkSocket(void);
	virtual ~CkSocket(void);

	static CkSocket *createNew(void);
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
	// Set to true when an asynchronous accept operation completes. Once the
	// asynchronous accept has finished, the success/failure is available in the
	// AsyncAcceptSuccess boolean property.
	bool get_AsyncAcceptFinished(void);

	// Contains the last-error information for an asynchronous accept operation.
	void get_AsyncAcceptLog(CkString &str);
	// Contains the last-error information for an asynchronous accept operation.
	const char *asyncAcceptLog(void);

	// Set to true when an asynchronous accept operation completes and is successful.
	bool get_AsyncAcceptSuccess(void);

	// Set to true when an asynchronous connect operation completes. Once the
	// asynchronous connect has finished, the success/failure is available in the
	// AsyncConnectSuccess boolean property.
	bool get_AsyncConnectFinished(void);

	// Contains the last-error information for an asynchronous connect operation.
	void get_AsyncConnectLog(CkString &str);
	// Contains the last-error information for an asynchronous connect operation.
	const char *asyncConnectLog(void);

	// Set to true when an asynchronous connect operation completes and is
	// successful.
	bool get_AsyncConnectSuccess(void);

	// Set to true when an asynchronous DNS query completes. The success status is
	// available in the AsyncDnsSuccess property.
	bool get_AsyncDnsFinished(void);

	// Contains the last-error information for an asynchronous DNS query.
	void get_AsyncDnsLog(CkString &str);
	// Contains the last-error information for an asynchronous DNS query.
	const char *asyncDnsLog(void);

	// The IP address of the last asynchronous DNS query completed. The IP address is
	// in nnn.nnn.nnn.nnn string form.
	void get_AsyncDnsResult(CkString &str);
	// The IP address of the last asynchronous DNS query completed. The IP address is
	// in nnn.nnn.nnn.nnn string form.
	const char *asyncDnsResult(void);

	// Set to true when an asynchronous DNS query completes and is successful.
	bool get_AsyncDnsSuccess(void);

	// Set to true when an asynchronous receive operation completes. Once the
	// asynchronous receive has finished, the success/failure is available in the
	// AsyncReceiveSuccess boolean property.
	bool get_AsyncReceiveFinished(void);

	// Contains the last-error information for an asynchronous receive operation.
	void get_AsyncReceiveLog(CkString &str);
	// Contains the last-error information for an asynchronous receive operation.
	const char *asyncReceiveLog(void);

	// Set to true when an asynchronous receive operation completes and is
	// successful.
	bool get_AsyncReceiveSuccess(void);

	// Contains the data received in an asynchronous receive operation (when receiving
	// bytes asynchronously).
	void get_AsyncReceivedBytes(CkByteData &outBytes);

	// Contains the string received in an asynchronous receive operation (when
	// receiving a string asynchronously).
	void get_AsyncReceivedString(CkString &str);
	// Contains the string received in an asynchronous receive operation (when
	// receiving a string asynchronously).
	const char *asyncReceivedString(void);

	// Set to true when an asynchronous send operation completes. Once the
	// asynchronous send has finished, the success/failure is available in the
	// AsyncSendSuccess boolean property.
	bool get_AsyncSendFinished(void);

	// Contains the last-error information for an asynchronous send operation.
	void get_AsyncSendLog(CkString &str);
	// Contains the last-error information for an asynchronous send operation.
	const char *asyncSendLog(void);

	// Set to true when an asynchronous send operation completes and is successful.
	bool get_AsyncSendSuccess(void);

	// Applies to the SendCount and ReceiveCount methods. If BigEndian is set to true
	// (the default) then the 4-byte count is in big endian format. Otherwise it is
	// little endian.
	bool get_BigEndian(void);
	// Applies to the SendCount and ReceiveCount methods. If BigEndian is set to true
	// (the default) then the 4-byte count is in big endian format. Otherwise it is
	// little endian.
	void put_BigEndian(bool newVal);

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

	// Normally left at the default value of 0, in which case a unique port is assigned
	// with a value between 1024 and 5000. This property would only be changed if it is
	// specifically required. For example, one customer's requirements are as follows:
	// 
	//     "I have to connect to a Siemens PLC IP server on a technical network. This
	//     machine expects that I connect to its server from a specific IP address using a
	//     specific port otherwise the build in security disconnect the IP connection."
	// 
	int get_ClientPort(void);
	// Normally left at the default value of 0, in which case a unique port is assigned
	// with a value between 1024 and 5000. This property would only be changed if it is
	// specifically required. For example, one customer's requirements are as follows:
	// 
	//     "I have to connect to a Siemens PLC IP server on a technical network. This
	//     machine expects that I connect to its server from a specific IP address using a
	//     specific port otherwise the build in security disconnect the IP connection."
	// 
	void put_ClientPort(int newVal);

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
	// 
	int get_ConnectFailReason(void);

	// Used to simulate a long wait when connecting to a remote server. If your
	// application wishes to test for the handling of timeouts, you may set this value
	// to a number of milliseconds greater than max-wait specified in the Connect
	// method call. The default value is 0.
	int get_DebugConnectDelayMs(void);
	// Used to simulate a long wait when connecting to a remote server. If your
	// application wishes to test for the handling of timeouts, you may set this value
	// to a number of milliseconds greater than max-wait specified in the Connect
	// method call. The default value is 0.
	void put_DebugConnectDelayMs(int newVal);

	// Used to simulate a long wait when doing a DNS lookup. If your application wishes
	// to test for the handling of timeouts, you may set this value to a number of
	// milliseconds greater than max-wait specified in the DnsLookup method call. The
	// default value is 0.
	int get_DebugDnsDelayMs(void);
	// Used to simulate a long wait when doing a DNS lookup. If your application wishes
	// to test for the handling of timeouts, you may set this value to a number of
	// milliseconds greater than max-wait specified in the DnsLookup method call. The
	// default value is 0.
	void put_DebugDnsDelayMs(int newVal);

	// Contains the number of seconds since the last call to StartTiming, otherwise
	// contains 0. (The StartTiming method and ElapsedSeconds property is provided for
	// convenience.)
	int get_ElapsedSeconds(void);

	// The number of milliseconds between periodic heartbeat callbacks for blocking
	// socket operations (connect, accept, dns query, send, receive). Set this to 0 to
	// disable heartbeat events. The default value is 1000 (i.e. 1 heartbeat callback
	// per second).
	int get_HeartbeatMs(void);
	// The number of milliseconds between periodic heartbeat callbacks for blocking
	// socket operations (connect, accept, dns query, send, receive). Set this to 0 to
	// disable heartbeat events. The default value is 1000 (i.e. 1 heartbeat callback
	// per second).
	void put_HeartbeatMs(int newVal);

	// If an HTTP proxy requiring authentication is to be used, set this property to
	// the HTTP proxy authentication method name. Valid choices are "Basic" or "NTLM".
	void get_HttpProxyAuthMethod(CkString &str);
	// If an HTTP proxy requiring authentication is to be used, set this property to
	// the HTTP proxy authentication method name. Valid choices are "Basic" or "NTLM".
	const char *httpProxyAuthMethod(void);
	// If an HTTP proxy requiring authentication is to be used, set this property to
	// the HTTP proxy authentication method name. Valid choices are "Basic" or "NTLM".
	void put_HttpProxyAuthMethod(const char *newVal);

	// The NTLM authentication domain (optional) if NTLM authentication is used.
	void get_HttpProxyDomain(CkString &str);
	// The NTLM authentication domain (optional) if NTLM authentication is used.
	const char *httpProxyDomain(void);
	// The NTLM authentication domain (optional) if NTLM authentication is used.
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

	// Returns true if the socket is connected. Otherwise returns false.
	// 
	// Note: This returns the last known state of the socket's connected state. It does
	// not try to send any data on the socket. If a previous call resulted in the
	// socket becoming disconnected, then false will be returned. However, if the
	// socket was connected and the peer disconnects prior to any Chilkat Socket method
	// calls that would send/received data (and discover the disconnection), then this
	// will return true. In a nutshell, if this returns false, then it is known for
	// sure that the socket is not connected, if it returns true then the last-known
	// state of the socket was connected.
	// 
	bool get_IsConnected(void);

	// Controls whether socket (or SSL) communications are logged to the SessionLog
	// string property. To turn on session logging, set this property = true,
	// otherwise set to false (which is the default value).
	bool get_KeepSessionLog(void);
	// Controls whether socket (or SSL) communications are logged to the SessionLog
	// string property. To turn on session logging, set this property = true,
	// otherwise set to false (which is the default value).
	void put_KeepSessionLog(bool newVal);

	// true if the last method called on this object failed. This provides an easier
	// (less confusing) way of determining whether a method such as ReceiveBytes
	// succeeded or failed.
	bool get_LastMethodFailed(void);

	// If set to true, then a socket that listens for incoming connections (via the
	// BindAndList and AcceptNextConnection method calls) will use IPv6 and not IPv4.
	// The default value is false for IPv4.
	bool get_ListenIpv6(void);
	// If set to true, then a socket that listens for incoming connections (via the
	// BindAndList and AcceptNextConnection method calls) will use IPv6 and not IPv4.
	// The default value is false for IPv4.
	void put_ListenIpv6(bool newVal);

	// The local IP address for a bound or connected socket.
	void get_LocalIpAddress(CkString &str);
	// The local IP address for a bound or connected socket.
	const char *localIpAddress(void);

	// The local port for a bound or connected socket.
	int get_LocalPort(void);

	// The maximum number of milliseconds to wait on a socket read operation while no
	// additional data is forthcoming. To wait indefinitely, set this property to 0.
	// The default value is 0.
	int get_MaxReadIdleMs(void);
	// The maximum number of milliseconds to wait on a socket read operation while no
	// additional data is forthcoming. To wait indefinitely, set this property to 0.
	// The default value is 0.
	void put_MaxReadIdleMs(int newVal);

	// The maximum number of milliseconds to wait for the socket to become writeable on
	// a socket write operation. To wait indefinitely, set this property to 0. The
	// default value is 0.
	int get_MaxSendIdleMs(void);
	// The maximum number of milliseconds to wait for the socket to become writeable on
	// a socket write operation. To wait indefinitely, set this property to 0. The
	// default value is 0.
	void put_MaxSendIdleMs(int newVal);

	// The local IP address of the local computer. For multi-homed computers (i.e.
	// computers with multiple IP adapters) this property returns the default IP
	// address.
	// 
	// Note: This will be the internal IP address, not an external IP address. (For
	// example, if your computer is on a LAN, it is likely to be an IP address
	// beginning with "192.168.".
	// 
	// Important: Use LocalIpAddress and LocalIpPort to get the local IP/port for a
	// bound or connected socket.
	// 
	void get_MyIpAddress(CkString &str);
	// The local IP address of the local computer. For multi-homed computers (i.e.
	// computers with multiple IP adapters) this property returns the default IP
	// address.
	// 
	// Note: This will be the internal IP address, not an external IP address. (For
	// example, if your computer is on a LAN, it is likely to be an IP address
	// beginning with "192.168.".
	// 
	// Important: Use LocalIpAddress and LocalIpPort to get the local IP/port for a
	// bound or connected socket.
	// 
	const char *myIpAddress(void);

	// If the socket is the server-side of an SSL/TLS connection, the property
	// represents the number of client-side certificates received during the SSL/TLS
	// handshake (i.e. connection process). Each client-side cert may be retrieved by
	// calling the GetReceivedClientCert method and passing an integer index value from
	// 0 to N-1, where N is the number of client certs received.
	// 
	// Note: A client only sends a certificate if 2-way SSL/TLS is required. In other
	// words, if the server demands a certificate from the client.
	// 
	int get_NumReceivedClientCerts(void);

	// If this socket is a "socket set", then NumSocketsInSet returns the number of
	// sockets contained in the set. A socket object can become a "socket set" by
	// calling the TakeSocket method on one or more connected sockets. This makes it
	// possible to select for reading on the set (i.e. wait for data to arrive from any
	// one of multiple sockets). See the following methods and properties for more
	// information: TakeSocket, SelectorIndex, SelectorReadIndex, SelectorWriteIndex,
	// SelectForReading, SelectForWriting.
	int get_NumSocketsInSet(void);

	// If connected as an SSL/TLS client to an SSL/TLS server where the server requires
	// a client-side certificate for authentication, then this property contains the
	// number of acceptable certificate authorities sent by the server during
	// connection establishment handshake. The GetSslAcceptableClientCaDn method may be
	// called to get the Distinguished Name (DN) of each acceptable CA.
	int get_NumSslAcceptableClientCAs(void);

	// Each socket object is assigned a unique object ID. This ID is passed in event
	// callbacks to allow your application to associate the event with the socket
	// object.
	int get_ObjectId(void);

	// The number of bytes to receive at a time (internally). This setting has an
	// effect on methods such as ReadBytes and ReadString where the number of bytes to
	// read is not explicitly specified. The default value is 4096.
	int get_ReceivePacketSize(void);
	// The number of bytes to receive at a time (internally). This setting has an
	// effect on methods such as ReadBytes and ReadString where the number of bytes to
	// read is not explicitly specified. The default value is 4096.
	void put_ReceivePacketSize(int newVal);

	// Any method that receives data will increase the value of this property by the
	// number of bytes received. The application may reset this property to 0 at any
	// point. It is provided as a way to keep count of the total number of bytes
	// received on a socket connection, regardless of which method calls are used to
	// receive the data.
	int get_ReceivedCount(void);
	// Any method that receives data will increase the value of this property by the
	// number of bytes received. The application may reset this property to 0 at any
	// point. It is provided as a way to keep count of the total number of bytes
	// received on a socket connection, regardless of which method calls are used to
	// receive the data.
	void put_ReceivedCount(int newVal);

	// When a socket is connected, the remote IP address of the connected peer is
	// available in this property.
	void get_RemoteIpAddress(CkString &str);
	// When a socket is connected, the remote IP address of the connected peer is
	// available in this property.
	const char *remoteIpAddress(void);

	// When a socket is connected, the remote port of the connected peer is available
	// in this property.
	int get_RemotePort(void);

	// If this socket contains a collection of connected sockets (i.e. it is a "socket
	// set") then method calls and property gets/sets are routed to the contained
	// socket indicated by this property. Indexing begins at 0. See the TakeSocket
	// method and SelectForReading method for more information.
	int get_SelectorIndex(void);
	// If this socket contains a collection of connected sockets (i.e. it is a "socket
	// set") then method calls and property gets/sets are routed to the contained
	// socket indicated by this property. Indexing begins at 0. See the TakeSocket
	// method and SelectForReading method for more information.
	void put_SelectorIndex(int newVal);

	// When SelectForReading returns a number greater than 0 indicating that 1 or more
	// sockets are ready for reading, this property is used to select the socket in the
	// "ready set" for reading. See the example below:
	int get_SelectorReadIndex(void);
	// When SelectForReading returns a number greater than 0 indicating that 1 or more
	// sockets are ready for reading, this property is used to select the socket in the
	// "ready set" for reading. See the example below:
	void put_SelectorReadIndex(int newVal);

	// When SelectForWriting returns a number greater than 0 indicating that one or
	// more sockets are ready for writing, this property is used to select the socket
	// in the "ready set" for writing.
	int get_SelectorWriteIndex(void);
	// When SelectForWriting returns a number greater than 0 indicating that one or
	// more sockets are ready for writing, this property is used to select the socket
	// in the "ready set" for writing.
	void put_SelectorWriteIndex(int newVal);

	// The number of bytes to send at a time (internally). This can also be though of
	// as the "chunk size". If a large amount of data is to be sent, the data is sent
	// in chunks equal to this size in bytes. The default value is 65535. (Note: This
	// only applies to non-SSL/TLS connections. SSL and TLS have their own pre-defined
	// packet sizes.)
	int get_SendPacketSize(void);
	// The number of bytes to send at a time (internally). This can also be though of
	// as the "chunk size". If a large amount of data is to be sent, the data is sent
	// in chunks equal to this size in bytes. The default value is 65535. (Note: This
	// only applies to non-SSL/TLS connections. SSL and TLS have their own pre-defined
	// packet sizes.)
	void put_SendPacketSize(int newVal);

	// Contains a log of the bytes sent and received on this socket. The KeepSessionLog
	// property must be set to true for logging to occur.
	void get_SessionLog(CkString &str);
	// Contains a log of the bytes sent and received on this socket. The KeepSessionLog
	// property must be set to true for logging to occur.
	const char *sessionLog(void);

	// Controls how the data is encoded in the SessionLog. Possible values are "esc"
	// and "hex". The default value is "esc".
	// 
	// When set to "hex", the bytes are encoded as a hexidecimalized string. The "esc"
	// encoding is a C-string like encoding, and is more compact than hex if most of
	// the data to be logged is text. Printable us-ascii chars are unmodified. Common
	// "C" control chars are represented as "\r", "\n", "\t", etc. Non-printable and
	// byte values greater than 0x80 are escaped using a backslash and hex encoding:
	// \xHH. Certain printable chars are backslashed: SPACE, double-quote,
	// single-quote, etc.
	// 
	void get_SessionLogEncoding(CkString &str);
	// Controls how the data is encoded in the SessionLog. Possible values are "esc"
	// and "hex". The default value is "esc".
	// 
	// When set to "hex", the bytes are encoded as a hexidecimalized string. The "esc"
	// encoding is a C-string like encoding, and is more compact than hex if most of
	// the data to be logged is text. Printable us-ascii chars are unmodified. Common
	// "C" control chars are represented as "\r", "\n", "\t", etc. Non-printable and
	// byte values greater than 0x80 are escaped using a backslash and hex encoding:
	// \xHH. Certain printable chars are backslashed: SPACE, double-quote,
	// single-quote, etc.
	// 
	const char *sessionLogEncoding(void);
	// Controls how the data is encoded in the SessionLog. Possible values are "esc"
	// and "hex". The default value is "esc".
	// 
	// When set to "hex", the bytes are encoded as a hexidecimalized string. The "esc"
	// encoding is a C-string like encoding, and is more compact than hex if most of
	// the data to be logged is text. Printable us-ascii chars are unmodified. Common
	// "C" control chars are represented as "\r", "\n", "\t", etc. Non-printable and
	// byte values greater than 0x80 are escaped using a backslash and hex encoding:
	// \xHH. Certain printable chars are backslashed: SPACE, double-quote,
	// single-quote, etc.
	// 
	void put_SessionLogEncoding(const char *newVal);

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

	// Sets the SO_REUSEADDR socket option for a socket that will bind to a port and
	// listen for incoming connections. The default value is true, meaning that the
	// SO_REUSEADDR socket option is set. If the socket option must be unset, set this
	// property equal to false prior to calling BindAndListen or InitSslServer.
	bool get_SoReuseAddr(void);
	// Sets the SO_REUSEADDR socket option for a socket that will bind to a port and
	// listen for incoming connections. The default value is true, meaning that the
	// SO_REUSEADDR socket option is set. If the socket option must be unset, set this
	// property equal to false prior to calling BindAndListen or InitSslServer.
	void put_SoReuseAddr(bool newVal);

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

	// Set this property to true if the socket requires an SSL connection. The
	// default value is false.
	bool get_Ssl(void);
	// Set this property to true if the socket requires an SSL connection. The
	// default value is false.
	void put_Ssl(bool newVal);

	// Selects the secure protocol to be used for secure (SSL) connections. Possible
	// values are:
	// 
	//     default
	//     TLS 1.0
	//     SSL 3.0
	//     
	// 
	// The default value is "default", which allows for the protocol to be selected
	// dynamically at runtime based on the requirements of the server or client.
	void get_SslProtocol(CkString &str);
	// Selects the secure protocol to be used for secure (SSL) connections. Possible
	// values are:
	// 
	//     default
	//     TLS 1.0
	//     SSL 3.0
	//     
	// 
	// The default value is "default", which allows for the protocol to be selected
	// dynamically at runtime based on the requirements of the server or client.
	const char *sslProtocol(void);
	// Selects the secure protocol to be used for secure (SSL) connections. Possible
	// values are:
	// 
	//     default
	//     TLS 1.0
	//     SSL 3.0
	//     
	// 
	// The default value is "default", which allows for the protocol to be selected
	// dynamically at runtime based on the requirements of the server or client.
	void put_SslProtocol(const char *newVal);

	// A charset such as "utf-8", "windows-1252", "Shift_JIS", "iso-8859-1", etc.
	// Methods for sending and receiving strings will use this charset as the encoding.
	// Strings sent on the socket are first converted (if necessary) to this encoding.
	// When reading, it is assumed that the bytes received are converted FROM this
	// charset if necessary. This ONLY APPLIES TO THE SendString and ReceiveString
	// methods. The default value is "ansi".
	void get_StringCharset(CkString &str);
	// A charset such as "utf-8", "windows-1252", "Shift_JIS", "iso-8859-1", etc.
	// Methods for sending and receiving strings will use this charset as the encoding.
	// Strings sent on the socket are first converted (if necessary) to this encoding.
	// When reading, it is assumed that the bytes received are converted FROM this
	// charset if necessary. This ONLY APPLIES TO THE SendString and ReceiveString
	// methods. The default value is "ansi".
	const char *stringCharset(void);
	// A charset such as "utf-8", "windows-1252", "Shift_JIS", "iso-8859-1", etc.
	// Methods for sending and receiving strings will use this charset as the encoding.
	// Strings sent on the socket are first converted (if necessary) to this encoding.
	// When reading, it is assumed that the bytes received are converted FROM this
	// charset if necessary. This ONLY APPLIES TO THE SendString and ReceiveString
	// methods. The default value is "ansi".
	void put_StringCharset(const char *newVal);

	// Controls whether the TCP_NODELAY socket option is used for the underlying TCP/IP
	// socket. The default value is false. Setting the value to true disables the
	// Nagle algorithm and allows for better performance when small amounts of data are
	// sent on the socket connection.
	bool get_TcpNoDelay(void);
	// Controls whether the TCP_NODELAY socket option is used for the underlying TCP/IP
	// socket. The default value is false. Setting the value to true disables the
	// Nagle algorithm and allows for better performance when small amounts of data are
	// sent on the socket connection.
	void put_TcpNoDelay(bool newVal);

	// Provides a way to store text data with the socket object. The UserData is purely
	// for convenience and is not involved in the socket communications in any way. An
	// application might use this property to keep extra information associated with
	// the socket.
	void get_UserData(CkString &str);
	// Provides a way to store text data with the socket object. The UserData is purely
	// for convenience and is not involved in the socket communications in any way. An
	// application might use this property to keep extra information associated with
	// the socket.
	const char *userData(void);
	// Provides a way to store text data with the socket object. The UserData is purely
	// for convenience and is not involved in the socket communications in any way. An
	// application might use this property to keep extra information associated with
	// the socket.
	void put_UserData(const char *newVal);

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
	// Blocking call to accept the next incoming connection on the socket. maxWaitMs
	// specifies the maximum time to wait (in milliseconds). Set this to 0 to wait
	// indefinitely. If successful, a new socket object is returned.
	// 
	// Important: If accepting an SSL/TLS connection, the SSL handshake is part of the
	// connection establishment process. This involves a few back-and-forth messages
	// between the client and server to establish algorithms and a shared key to create
	// the secure channel. The sending and receiving of these messages are governed by
	// the MaxReadIdleMs and MaxSendIdleMs properties. If these properties are set to 0
	// (and this is the default unless changed by your application), then the
	// AcceptNextConnection can hang indefinitely during the SSL handshake process.
	// Make sure these properties are set to appropriate values before calling this
	// method.
	// 
	// The caller is responsible for deleting the object returned by this method.
	CkSocket *AcceptNextConnection(int maxWaitMs);

	// If this object is a server-side socket accepting SSL/TLS connections, and wishes
	// to require a client-side certificate for authentication, then it should make one
	// or more calls to this method to identify the CA's it will accept for client-side
	// certificates.
	// 
	// Important: If calling this method, it must be called before calling
	// InitSslServer.
	// 
	bool AddSslAcceptableClientCaDn(const char *certAuthDN);

	// Call this to abort an asynchronous socket connect that is running in a
	// background thread. Asynchronous connects are initiated by calling
	// AsyncAcceptStart.
	void AsyncAcceptAbort(void);

	// Note: Version 9.4.0, released in Dec 2012, introduced a bug that causes this
	// method to crash. A fix is available in the stable pre-release of the next
	// version (v9.4.1). Please contact support@chilkatsoft.com to obtain a download
	// link. Please make sure to indicate the exact build required: programming
	// language, operating system, 32-bit or 64-bit, .NET Framework (if applicable),
	// etc.
	// 
	// Returns the socket object for the connection accepted asynchronously in a
	// background thread (via AsyncAcceptStart). The connected socket can only be
	// retrieved once. A subsequent call to AsyncAcceptSocket will return a NULL
	// reference until another connection is accepted asynchronously.
	// 
	// The caller is responsible for deleting the object returned by this method.
	CkSocket *AsyncAcceptSocket(void);

	// Initiates a background thread to wait for and accept the next incoming TCP
	// connection. The method will fail if an asynchronous operation is already in
	// progress. The timeout (in milliseconds) is passed in maxWaitMs. To wait indefinitely,
	// set maxWaitMs to 0. Asynchronous accept operations can be aborted by calling
	// AsyncAcceptAbort. When the async accept operation completes, the
	// AsyncAcceptFinished property will become true. If the accept was successful,
	// the AsyncAcceptSuccess property is set to true and the connected socket can be
	// retrieved via the AsyncAcceptSocket method. A debug log is available in the
	// AsyncAcceptLog property.
	bool AsyncAcceptStart(int maxWaitMs);

	// Aborts an asynchronous connect operation running in a background thread (started
	// by calling AsyncConnectStart).
	void AsyncConnectAbort(void);

	// Initiates a background thread to establish a TCP connection with a remote
	// host:port. The method will fail if an asynchronous operation is already in
	// progress, or if the timeout expired. The timeout (in milliseconds) is passed in
	//  maxWaitMs. To wait indefinitely, set  maxWaitMs to 0. Set  ssl = true to esablish an SSL
	// connection. Asynchronous connect operations can be aborted by calling
	// AsyncConnectAbort. When the async connect operation completes, the
	// AsyncConnectFinished property will become true. If the connect was successful,
	// the AsyncConnectSuccess property is set to true. A debug log is available in
	// the AsyncConnectLog property.
	bool AsyncConnectStart(const char *hostname, int port, bool ssl, int maxWaitMs);

	// Aborts an asynchronous DNS lookup running in a background thread (started via
	// the AsyncDnsStart method).
	void AsyncDnsAbort(void);

	// Initiates a background thread to do a DNS query (i.e. to resolve a hostname to
	// an IP address). The method will fail if an asynchronous operation is already in
	// progress, or if the timeout expired. The timeout (in milliseconds) is passed in
	//  maxWaitMs. To wait indefinitely, set  maxWaitMs to 0. Asynchronous DNS lookups can be
	// aborted by calling AsyncDnsAbort. When the async DNS operation completes, the
	// AsyncDnsFinished property will become true. If the DNS query was successful,
	// the AsyncDnsSuccess property is set to true. A debug log is available in the
	// AsyncDnsLog property. Finally, the DNS query result (i.e. IP address) is
	// available in nnn.nnn.nnn.nnn string form in the AsyncDnsResult property.
	bool AsyncDnsStart(const char *hostname, int maxWaitMs);

	// Aborts an asynchronous receive running in a background thread (started via one
	// of the AsyncReceive* methods).
	void AsyncReceiveAbort(void);

	// Initiates a background thread to receive bytes on an already-connected socket
	// (ssl or non-ssl).
	bool AsyncReceiveBytes(void);

	// Initiates a background thread to receive exactly numBytes bytes on an
	// already-connected socket (ssl or non-ssl).
	bool AsyncReceiveBytesN(unsigned long numBytes);

	// Initiates a background thread to receive text on an already-connected socket
	// (ssl or non-ssl). The component interprets the received bytes according to the
	// charset specified in the StringCharset property.
	bool AsyncReceiveString(void);

	// Initiates a background thread to receive text on an already-connected socket
	// (ssl or non-ssl). The asynchronous receive does not complete until a CRLF is
	// received. The component interprets the received bytes according to the charset
	// specified in the StringCharset property.
	bool AsyncReceiveToCRLF(void);

	// Initiates a background thread to receive text on an already-connected socket
	// (ssl or non-ssl). The asynchronous receive does not complete until the exact
	// string specified by matchStr is received. The component interprets the received
	// bytes according to the charset specified in the StringCharset property.
	bool AsyncReceiveUntilMatch(const char *matchStr);

	// Aborts an asynchronous send running in a background thread (started via one of
	// the AsyncSend* methods).
	void AsyncSendAbort(void);

	// Initiates a background thread to send bytes on an already-connected socket
	// (SSL/TLS or unencrypted). This method is redundant and identical to SendBytes.
	bool AsyncSendByteData(const CkByteData &data);

	// Initiates a background thread to send bytes on an already-connected socket (ssl
	// or non-ssl).
	bool AsyncSendBytes(const CkByteData &data);

	// Initiates a background thread to send text on an already-connected socket (ssl
	// or non-ssl). Before sending, the stringToSend is first converted (if necessary) to the
	// charset specified by the StringCharset property.
	bool AsyncSendString(const char *str);

	// Binds a TCP socket to a port and configures it to listen for incoming
	// connections. The size of the backlog is passed in  backLog. The backlog is necessary
	// when multiple connections arrive at the same time, or close enough in time such
	// that they cannot be serviced immediately. (A typical value to use for  backLog is
	// 5.) This method should be called once prior to receiving incoming connection
	// requests via the AcceptNextConnection or AsyncAcceptStart methods.
	// 
	// To bind and listen using IPv6, set the ListenIpv6 property = true prior to
	// calling this method.
	// 
	bool BindAndListen(int port, int backlog);

	// Determines if the socket is writeable. Returns one of the following integer
	// values:
	// 
	// 1: If the socket is connected and ready for writing.
	// 0: If a timeout occurred or if the application aborted the method during an
	// event callback.
	// -1: The socket is not connected.
	// 
	int CheckWriteable(int maxWaitMs);

	// Clears the contents of the SessionLog property.
	void ClearSessionLog(void);

	// To be documented soon.
	// The caller is responsible for deleting the object returned by this method.
	CkSocket *CloneSocket(void);

	// Cleanly terminates and closes a TCP/IP (SSL or non-SSL) connection. The maxWaitMs
	// applies to SSL connections because there is a handshake that occurs during
	// secure channel shutdown.
	void Close(int maxWaitMs);

	// Establishes an SSL or non-SSL connection with a remote host:port. This is a
	// blocking call. To initiate a non-blocking (asynchronous) connection in a
	// background thread, call AsyncConnectStart. The maximum wait time (in
	// milliseconds) is passed in  maxWaitMs. To establish an SSL connection, set  ssl =
	// true, otherwise set  ssl = false.
	// 
	// Note: Connections do not automatically close because of inactivity. A connection
	// will remain open indefinitely even if there is no activity.
	// 
	bool Connect(const char *hostname, int port, bool ssl, int maxWaitMs);

	// Closes the secure (TLS/SSL) channel leaving the socket in a connected state
	// where data sent and received is unencrypted.
	bool ConvertFromSsl(void);

	// Converts a non-SSL/TLS connected socket to a secure channel using TLS/SSL.
	bool ConvertToSsl(void);

	// Performs a DNS query to resolve a hostname to an IP address. The IP address is
	// returned if successful. The maximum time to wait (in milliseconds) is passed in
	//  maxWaitMs. To wait indefinitely, set  maxWaitMs = 0.
	bool DnsLookup(const char *hostname, int maxWaitMs, CkString &outStr);
	// Performs a DNS query to resolve a hostname to an IP address. The IP address is
	// returned if successful. The maximum time to wait (in milliseconds) is passed in
	//  maxWaitMs. To wait indefinitely, set  maxWaitMs = 0.
	const char *dnsLookup(const char *hostname, int maxWaitMs);

	// Returns the digital certificate to be used for SSL connections. This method
	// would only be called by an SSL server application. The SSL certificate is
	// initially specified by calling InitSslServer.
	// 
	// The caller is responsible for deleting the object returned by this method.
	CkCert *GetMyCert(void);

	// Returns the Nth client certificate received during an SSL/TLS handshake. This
	// method only applies to the server-side of an SSL/TLS connection. The 1st client
	// certificate is at index 0. The NumReceivedClientCerts property indicates the
	// number of client certificates received during the SSL/TLS connection
	// establishment.
	// 
	// Client certificates are customarily only sent when the server demands
	// client-side authentication, as in 2-way SSL/TLS. This method provides the
	// ability for the server to access and examine the client-side certs immediately
	// after a connection is established. (Of course, if the client-side certs are
	// inadequate for authentication, then the application can choose to immediately
	// disconnect.)
	// 
	// The caller is responsible for deleting the object returned by this method.
	CkCert *GetReceivedClientCert(int index);

	// If connected as an SSL/TLS client to an SSL/TLS server where the server requires
	// a client-side certificate for authentication, then the NumSslAcceptableClientCAs
	// property contains the number of acceptable certificate authorities sent by the
	// server during connection establishment handshake. This method may be called to
	// get the Distinguished Name (DN) of each acceptable CA. The index should range
	// from 0 to NumSslAcceptableClientCAs - 1.
	bool GetSslAcceptableClientCaDn(int index, CkString &outStr);
	// If connected as an SSL/TLS client to an SSL/TLS server where the server requires
	// a client-side certificate for authentication, then the NumSslAcceptableClientCAs
	// property contains the number of acceptable certificate authorities sent by the
	// server during connection establishment handshake. This method may be called to
	// get the Distinguished Name (DN) of each acceptable CA. The index should range
	// from 0 to NumSslAcceptableClientCAs - 1.
	const char *getSslAcceptableClientCaDn(int index);
	// If connected as an SSL/TLS client to an SSL/TLS server where the server requires
	// a client-side certificate for authentication, then the NumSslAcceptableClientCAs
	// property contains the number of acceptable certificate authorities sent by the
	// server during connection establishment handshake. This method may be called to
	// get the Distinguished Name (DN) of each acceptable CA. The index should range
	// from 0 to NumSslAcceptableClientCAs - 1.
	const char *sslAcceptableClientCaDn(int index);

	// Returns the SSL server's digital certificate. This method would only be called
	// by the client-side of an SSL connection. It returns the certificate of the
	// remote SSL server for the current SSL connection. If the socket is not
	// connected, or is not connected via SSL, then a NULL reference is returned.
	// The caller is responsible for deleting the object returned by this method.
	CkCert *GetSslServerCert(void);

	// SSL Server applications should call this method with the SSL server certificate
	// to be used for SSL connections. It should be called prior to accepting
	// connections. This method has an intended side-effect: If not already connected,
	// then the Ssl property is set to true.
	bool InitSslServer(CkCert &cert);

	// Returns true if the component is unlocked.
	bool IsUnlocked(void);

	// Check to see if data is available for reading on the socket. Returns true if
	// data is waiting and false if no data is waiting to be read.
	bool PollDataAvailable(void);

	// Receives as much data as is immediately available on a connected TCP socket. If
	// no data is immediately available, it waits up to MaxReadIdleMs milliseconds for
	// data to arrive.
	bool ReceiveBytes(CkByteData &outData);

	// The same as ReceiveBytes, except the bytes are returned in encoded string form
	// according to encodingAlg. The encodingAlg can be "Base64", "modBase64", "Base32", "UU", "QP"
	// (for quoted-printable), "URL" (for url-encoding), "Hex", "Q", "B", "url_oath",
	// "url_rfc1738", "url_rfc2396", or "url_rfc3986".
	bool ReceiveBytesENC(const char *encodingAlg, CkString &outStr);
	// The same as ReceiveBytes, except the bytes are returned in encoded string form
	// according to encodingAlg. The encodingAlg can be "Base64", "modBase64", "Base32", "UU", "QP"
	// (for quoted-printable), "URL" (for url-encoding), "Hex", "Q", "B", "url_oath",
	// "url_rfc1738", "url_rfc2396", or "url_rfc3986".
	const char *receiveBytesENC(const char *encodingAlg);

	// Reads exactly numBytes bytes from a connected SSL or non-SSL socket. This method
	// blocks until numBytes bytes are read or the read times out. The timeout is specified
	// by the MaxReadIdleMs property (in milliseconds).
	bool ReceiveBytesN(unsigned long numBytes, CkByteData &outData);

	// Receives as much data as is immediately available on a connected TCP socket. If
	// no data is immediately available, it waits up to MaxReadIdleMs milliseconds for
	// data to arrive.
	// 
	// The received data is appended to the file specified by appendFilename.
	// 
	bool ReceiveBytesToFile(const char *appendFilename);

	// Receives a 4-byte signed integer and returns the value received. Returns -1 on
	// error.
	int ReceiveCount(void);

	// The same as ReceiveBytesN, except the bytes are returned in encoded string form
	// using the encoding specified by numBytes. The numBytes can be "Base64", "modBase64",
	// "Base32", "UU", "QP" (for quoted-printable), "URL" (for url-encoding), "Hex",
	// "Q", "B", "url_oath", "url_rfc1738", "url_rfc2396", or "url_rfc3986".
	bool ReceiveNBytesENC(unsigned long numBytes, const char *encodingAlg, CkString &outStr);
	// The same as ReceiveBytesN, except the bytes are returned in encoded string form
	// using the encoding specified by numBytes. The numBytes can be "Base64", "modBase64",
	// "Base32", "UU", "QP" (for quoted-printable), "URL" (for url-encoding), "Hex",
	// "Q", "B", "url_oath", "url_rfc1738", "url_rfc2396", or "url_rfc3986".
	const char *receiveNBytesENC(unsigned long numBytes, const char *encodingAlg);

	// Receives as much data as is immediately available on a TCP/IP or SSL socket. If
	// no data is immediately available, it waits up to MaxReadIdleMs milliseconds for
	// data to arrive. The incoming bytes are interpreted according to the
	// StringCharset property and returned as a string.
	bool ReceiveString(CkString &outStr);
	// Receives as much data as is immediately available on a TCP/IP or SSL socket. If
	// no data is immediately available, it waits up to MaxReadIdleMs milliseconds for
	// data to arrive. The incoming bytes are interpreted according to the
	// StringCharset property and returned as a string.
	const char *receiveString(void);

	// Same as ReceiveString, but limits the amount of data returned to a maximum of
	// maxByteCount bytes.
	// 
	// (Receives as much data as is immediately available on the TCP/IP or SSL socket.
	// If no data is immediately available, it waits up to MaxReadIdleMs milliseconds
	// for data to arrive. The incoming bytes are interpreted according to the
	// StringCharset property and returned as a string.)
	// 
	bool ReceiveStringMaxN(int maxBytes, CkString &outStr);
	// Same as ReceiveString, but limits the amount of data returned to a maximum of
	// maxByteCount bytes.
	// 
	// (Receives as much data as is immediately available on the TCP/IP or SSL socket.
	// If no data is immediately available, it waits up to MaxReadIdleMs milliseconds
	// for data to arrive. The incoming bytes are interpreted according to the
	// StringCharset property and returned as a string.)
	// 
	const char *receiveStringMaxN(int maxBytes);

	// Receives bytes on a connected SSL or non-SSL socket until a specific 1-byte
	// value is read. Returns a string containing all the bytes up to but excluding the
	// lookForByte.
	bool ReceiveStringUntilByte(int byteValue, CkString &outStr);
	// Receives bytes on a connected SSL or non-SSL socket until a specific 1-byte
	// value is read. Returns a string containing all the bytes up to but excluding the
	// lookForByte.
	const char *receiveStringUntilByte(int byteValue);

	// Reads text from the connected TCP/IP or SSL socket until a CRLF is received.
	// Returns the text up to and including the CRLF. The incoming bytes are
	// interpreted according to the charset specified by the StringCharset property.
	bool ReceiveToCRLF(CkString &outStr);
	// Reads text from the connected TCP/IP or SSL socket until a CRLF is received.
	// Returns the text up to and including the CRLF. The incoming bytes are
	// interpreted according to the charset specified by the StringCharset property.
	const char *receiveToCRLF(void);

	// Receives bytes on the TCP/IP or SSL socket until a specific 1-byte value is
	// read. Returns all the bytes up to and including the lookForByte.
	bool ReceiveUntilByte(int byteValue, CkByteData &outBytes);

	// Reads text from the connected TCP/IP or SSL socket until a matching string
	// (matchStr) is received. Returns the text up to and including the matching string. As
	// an example, to one might read the header of an HTTP request or a MIME message by
	// reading up to the first double CRLF ("\r\n\r\n"). The incoming bytes are
	// interpreted according to the charset specified by the StringCharset property.
	bool ReceiveUntilMatch(const char *matchStr, CkString &outStr);
	// Reads text from the connected TCP/IP or SSL socket until a matching string
	// (matchStr) is received. Returns the text up to and including the matching string. As
	// an example, to one might read the header of an HTTP request or a MIME message by
	// reading up to the first double CRLF ("\r\n\r\n"). The incoming bytes are
	// interpreted according to the charset specified by the StringCharset property.
	const char *receiveUntilMatch(const char *matchStr);

	// Wait for data to arrive on this socket, or any of the contained sockets if the
	// caller is a "socket set". (see the example at the link below for more detailed
	// information) Waits a maximum of timeoutMs milliseconds. If maxWaitMs = 0, then it is
	// effectively a poll. Returns the number of sockets with data available for
	// reading. If no sockets have data available for reading, then a value of 0 is
	// returned. A value of -1 indicates an error condition. Note: when the remote peer
	// (in this case the web server) disconnects, the socket will appear as if it has
	// data available. A "ready" socket is one where either data is available for
	// reading or the socket has become disconnected.
	// 
	// If the peer closed the connection, it will not be discovered until an attempt is
	// made to read the socket. If the read fails, then the IsConnected property may be
	// checked to see if the connection was closed.
	// 
	int SelectForReading(int timeoutMs);

	// Waits until it is known that data can be written to one or more sockets without
	// it blocking.
	// 
	// Socket writes are typically buffered by the operating system. When an
	// application writes data to a socket, the operating system appends it to the
	// socket's outgoing send buffers and returns immediately. However, if the OS send
	// buffers become filled up (because the sender is sending data faster than the
	// remote receiver can read it), then a socket write can block (until outgoing send
	// buffer space becomes available).
	// 
	// Waits a maximum of ARG1 milliseconds. If maxWaitMs = 0, then it is effectively a
	// poll. Returns the number of sockets such that data can be written without
	// blocking. A value of -1 indicates an error condition.
	// 
	int SelectForWriting(int timeoutMs);

	// Sends bytes over a connected SSL or non-SSL socket. If transmission halts for
	// more than MaxSendIdleMs milliseconds, the send is aborted. This is a blocking
	// (synchronous) method. It returns only after the bytes have been sent.
	bool SendBytes(const CkByteData &data);

	// The same as SendBytes, except the bytes are provided in encoded string form as
	// specified by  encodingAlg. The  encodingAlg can be "Base64", "modBase64", "Base32", "UU", "QP"
	// (for quoted-printable), "URL" (for url-encoding), "Hex", "Q", "B", "url_oath",
	// "url_rfc1738", "url_rfc2396", or "url_rfc3986".
	bool SendBytesENC(const char *encodedBytes, const char *encodingAlg);

	// Sends a 4-byte signed integer on the connection. The receiver may call
	// ReceiveCount to receive the integer. The SendCount and ReceiveCount methods are
	// handy for sending byte counts prior to sending data. The sender would send a
	// count followed by the data, and the receiver would receive the count first, and
	// then knows how many data bytes it should expect to receive.
	bool SendCount(int byteCount);

	// Sends a string over a connected SSL or non-SSL (TCP/IP) socket. If transmission
	// halts for more than MaxSendIdleMs milliseconds, the send is aborted. The string
	// is sent in the charset encoding specified by the StringCharset property.
	// 
	// This is a blocking (synchronous) method. It returns after the string has been
	// sent.
	// 
	bool SendString(const char *str);

	// A client-side certificate for SSL/TLS connections is optional. It should be used
	// only if the server demands it. This method allows the certificate to be
	// specified using a certificate object.
	bool SetSslClientCert(CkCert &cert);

	// A client-side certificate for SSL/TLS connections is optional. It should be used
	// only if the server demands it. This method allows the certificate to be
	// specified using a PEM file.
	bool SetSslClientCertPem(const char *pemDataOrFilename, const char *pemPassword);

	// A client-side certificate for SSL/TLS connections is optional. It should be used
	// only if the server demands it. This method allows the certificate to be
	// specified using a PFX file.
	bool SetSslClientCertPfx(const char *pfxFilename, const char *pfxPassword);

	// Convenience method to force the calling process to sleep for a number of
	// milliseconds.
	void SleepMs(int millisec);

	// Used in combination with the ElapsedSeconds property, which will contain the
	// number of seconds since the last call to this method. (The StartTiming method
	// and ElapsedSeconds property is provided for convenience.)
	void StartTiming(void);

	// Takes ownership of the sock. sock is added to the internal set of connected
	// sockets. The caller object is now effectively a "socket set", i.e. a collection
	// of connected sockets. Method calls are routed to the internal sockets based on
	// the value of the SelectorIndex property. For example, if SelectorIndex equals 2,
	// then a call to SendBytes is actually a call to SendBytes on the 3rd socket in
	// the set. (Indexing begins at 0.) Likewise, getting and setting properties are
	// also routed to the contained socket based on SelectorIndex. It is possible to
	// wait on a set of sockets for data to arrive on any of them by calling
	// SelectForReading. See the example link below.
	bool TakeSocket(CkSocket &sock);

	// Unlocks the component allowing for the full functionality to be used. An
	// arbitrary string can be passed to initiate a fully-functional 30-day trial.
	bool UnlockComponent(const char *unlockCode);

	// Convenience method for building a simple HTTP GET request from a URL.
	bool BuildHttpGetRequest(const char *url, CkString &outStr);
	// Convenience method for building a simple HTTP GET request from a URL.
	const char *buildHttpGetRequest(const char *url);

	// Clears the Chilkat-wide in-memory hostname-to-IP address DNS cache. Chilkat
	// automatically maintains this in-memory cache to prevent redundant DNS lookups.
	// If the TTL on the DNS A records being accessed are short and/or these DNS
	// records change frequently, then this method can be called clear the internal
	// cache. Note: The DNS cache is used/shared among all Chilkat objects in a
	// program, and clearing the cache affects all Chilkat objects.
	void DnsCacheClear(void);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
