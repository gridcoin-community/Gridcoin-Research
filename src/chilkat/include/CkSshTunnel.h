// CkSshTunnel.h: interface for the CkSshTunnel class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkSshTunnel_H
#define _CkSshTunnel_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkMultiByteBase.h"

class CkSshKey;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkSshTunnel
class CK_VISIBLE_PUBLIC CkSshTunnel  : public CkMultiByteBase
{
    private:
	

	// Don't allow assignment or copying these objects.
	CkSshTunnel(const CkSshTunnel &);
	CkSshTunnel &operator=(const CkSshTunnel &);

    public:
	CkSshTunnel(void);
	virtual ~CkSshTunnel(void);

	static CkSshTunnel *createNew(void);
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	

	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------
	// May be set to the path of a log file that the SshTunnel will create and log
	// activity regarding connections accepted.
	void get_AcceptThreadSessionLogPath(CkString &str);
	// May be set to the path of a log file that the SshTunnel will create and log
	// activity regarding connections accepted.
	const char *acceptThreadSessionLogPath(void);
	// May be set to the path of a log file that the SshTunnel will create and log
	// activity regarding connections accepted.
	void put_AcceptThreadSessionLogPath(const char *newVal);

	// Contains log text detailing the establishment of each SSH server connection.
	// This log will continue to grow as new connections are accepted. This property
	// may be cleared by setting it to an empty string.
	void get_ConnectLog(CkString &str);
	// Contains log text detailing the establishment of each SSH server connection.
	// This log will continue to grow as new connections are accepted. This property
	// may be cleared by setting it to an empty string.
	const char *connectLog(void);
	// Contains log text detailing the establishment of each SSH server connection.
	// This log will continue to grow as new connections are accepted. This property
	// may be cleared by setting it to an empty string.
	void put_ConnectLog(const char *newVal);

	// Maximum number of milliseconds to wait when connecting to an SSH server. The
	// default value is 10000 (i.e. 10 seconds).
	int get_ConnectTimeoutMs(void);
	// Maximum number of milliseconds to wait when connecting to an SSH server. The
	// default value is 10000 (i.e. 10 seconds).
	void put_ConnectTimeoutMs(int newVal);

	// The destination hostname or IP address (in dotted decimal notation) of the
	// service (such as a database server). Data sent through the SSH tunnel is
	// forwarded by the SSH server to this destination. Data received from the
	// destination (by the SSH server) is forwarded back to the client through the SSH
	// tunnel.
	void get_DestHostname(CkString &str);
	// The destination hostname or IP address (in dotted decimal notation) of the
	// service (such as a database server). Data sent through the SSH tunnel is
	// forwarded by the SSH server to this destination. Data received from the
	// destination (by the SSH server) is forwarded back to the client through the SSH
	// tunnel.
	const char *destHostname(void);
	// The destination hostname or IP address (in dotted decimal notation) of the
	// service (such as a database server). Data sent through the SSH tunnel is
	// forwarded by the SSH server to this destination. Data received from the
	// destination (by the SSH server) is forwarded back to the client through the SSH
	// tunnel.
	void put_DestHostname(const char *newVal);

	// The destination port of the service (such as a database server).
	int get_DestPort(void);
	// The destination port of the service (such as a database server).
	void put_DestPort(int newVal);

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

	// A tunnel will fail when progress for sending or receiving data halts for more
	// than this number of milliseconds. The default value is 10,000 (which is 10
	// seconds). A timeout of 0 indicates an infinite wait time (i.e. no timeout).
	int get_IdleTimeoutMs(void);
	// A tunnel will fail when progress for sending or receiving data halts for more
	// than this number of milliseconds. The default value is 10,000 (which is 10
	// seconds). A timeout of 0 indicates an infinite wait time (i.e. no timeout).
	void put_IdleTimeoutMs(int newVal);

	// true if a background thread is running and accepting connections.
	bool get_IsAccepting(void);

	// If true, then an in-memory log of connections is kept in the ConnectLog
	// property. The default value is true.
	bool get_KeepConnectLog(void);
	// If true, then an in-memory log of connections is kept in the ConnectLog
	// property. The default value is true.
	void put_KeepConnectLog(bool newVal);

	// In most cases, this property does not need to be set. It is provided for cases
	// where it is required to bind the listen socket to a specific IP address (usually
	// for computers with multiple network interfaces or IP addresses). For computers
	// with a single network interface (i.e. most computers), this property should not
	// be set. For multihoming computers, the default IP address is automatically used
	// if this property is not set.
	void get_ListenBindIpAddress(CkString &str);
	// In most cases, this property does not need to be set. It is provided for cases
	// where it is required to bind the listen socket to a specific IP address (usually
	// for computers with multiple network interfaces or IP addresses). For computers
	// with a single network interface (i.e. most computers), this property should not
	// be set. For multihoming computers, the default IP address is automatically used
	// if this property is not set.
	const char *listenBindIpAddress(void);
	// In most cases, this property does not need to be set. It is provided for cases
	// where it is required to bind the listen socket to a specific IP address (usually
	// for computers with multiple network interfaces or IP addresses). For computers
	// with a single network interface (i.e. most computers), this property should not
	// be set. For multihoming computers, the default IP address is automatically used
	// if this property is not set.
	void put_ListenBindIpAddress(const char *newVal);

	// If a port number equal to 0 is passed to BeginAccepting, then this property will
	// contain the actual allocated port number used. Otherwise it is equal to the port
	// number passed to BeginAccepting, or 0 if BeginAccepting was never called.
	int get_ListenPort(void);

	// The maximum packet length to be used in the SSH transport protocol. The default
	// value is 32768.
	int get_MaxPacketSize(void);
	// The maximum packet length to be used in the SSH transport protocol. The default
	// value is 32768.
	void put_MaxPacketSize(int newVal);

	// In most cases, this property does not need to be set. It is provided for cases
	// where it is required to bind the socket that is to connect to the SSH server (in
	// the background thread) to a specific IP address (usually for computers with
	// multiple network interfaces or IP addresses). For computers with a single
	// network interface (i.e. most computers), this property should not be set. For
	// multihoming computers, the default IP address is automatically used if this
	// property is not set.
	void get_OutboundBindIpAddress(CkString &str);
	// In most cases, this property does not need to be set. It is provided for cases
	// where it is required to bind the socket that is to connect to the SSH server (in
	// the background thread) to a specific IP address (usually for computers with
	// multiple network interfaces or IP addresses). For computers with a single
	// network interface (i.e. most computers), this property should not be set. For
	// multihoming computers, the default IP address is automatically used if this
	// property is not set.
	const char *outboundBindIpAddress(void);
	// In most cases, this property does not need to be set. It is provided for cases
	// where it is required to bind the socket that is to connect to the SSH server (in
	// the background thread) to a specific IP address (usually for computers with
	// multiple network interfaces or IP addresses). For computers with a single
	// network interface (i.e. most computers), this property should not be set. For
	// multihoming computers, the default IP address is automatically used if this
	// property is not set.
	void put_OutboundBindIpAddress(const char *newVal);

	// Unless there is a specific requirement for binding to a specific port, leave
	// this unset (at the default value of 0). (99.9% of all users should never need to
	// set this property.)
	int get_OutboundBindPort(void);
	// Unless there is a specific requirement for binding to a specific port, leave
	// this unset (at the default value of 0). (99.9% of all users should never need to
	// set this property.)
	void put_OutboundBindPort(int newVal);

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

	// The SSH server's hostname or IP address (in dotted-decimal notation).
	void get_SshHostname(CkString &str);
	// The SSH server's hostname or IP address (in dotted-decimal notation).
	const char *sshHostname(void);
	// The SSH server's hostname or IP address (in dotted-decimal notation).
	void put_SshHostname(const char *newVal);

	// The SSH server login. Authentication is typically required to establish the SSH
	// tunnel in the background thread.
	void get_SshLogin(CkString &str);
	// The SSH server login. Authentication is typically required to establish the SSH
	// tunnel in the background thread.
	const char *sshLogin(void);
	// The SSH server login. Authentication is typically required to establish the SSH
	// tunnel in the background thread.
	void put_SshLogin(const char *newVal);

	// The SSH server password.
	void get_SshPassword(CkString &str);
	// The SSH server password.
	const char *sshPassword(void);
	// The SSH server password.
	void put_SshPassword(const char *newVal);

	// The SSH server's port. The default is 22, and this typically won't change.
	int get_SshPort(void);
	// The SSH server's port. The default is 22, and this typically won't change.
	void put_SshPort(int newVal);

	// Controls whether the TCP_NODELAY socket option is used for the underlying TCP/IP
	// socket. The default value is false. Setting this property equal to true
	// disables the Nagle algorithm and allows for better performance when small
	// amounts of data are sent.
	bool get_TcpNoDelay(void);
	// Controls whether the TCP_NODELAY socket option is used for the underlying TCP/IP
	// socket. The default value is false. Setting this property equal to true
	// disables the Nagle algorithm and allows for better performance when small
	// amounts of data are sent.
	void put_TcpNoDelay(bool newVal);

	// Contains the accumulated errors, if any, of the background thread. Call
	// ClearTunnelErrors to clear this in-memory log.
	void get_TunnelErrors(CkString &str);
	// Contains the accumulated errors, if any, of the background thread. Call
	// ClearTunnelErrors to clear this in-memory log.
	const char *tunnelErrors(void);

	// May be set to the path of a log file that the SshTunnel will create and log
	// information regarding tunnel activity.
	void get_TunnelThreadSessionLogPath(CkString &str);
	// May be set to the path of a log file that the SshTunnel will create and log
	// information regarding tunnel activity.
	const char *tunnelThreadSessionLogPath(void);
	// May be set to the path of a log file that the SshTunnel will create and log
	// information regarding tunnel activity.
	void put_TunnelThreadSessionLogPath(const char *newVal);



	// ----------------------
	// Methods
	// ----------------------
	// Clears the TunnelErrors property (i.e. sets it to the empty string).
	void ClearTunnelErrors(void);

	// Returns information about the current set of running SSH tunnels. This is a
	// snapshot of the tunnels at a single point in time. The XML has this format:
	// _LT_tunnels>
	// 	_LT_t>
	// 		_LT_uniqueId>..._LT_/uniqueId>
	// 		_LT_clientIp>..._LT_/clientIp>
	// 		_LT_clientPort>..._LT_/clientPort>
	// 		_LT_serverIp>..._LT_/serverIp>
	// 		_LT_serverPort>..._LT_/serverPort>
	// 		_LT_tunnelType>..._LT_/tunnelType>
	// 		_LT_login>..._LT_/login>
	// 		_LT_password>..._LT_/password>
	// 	_LT_/t>
	// 	_LT_t>
	// 	...
	// 	_LT_/t>
	// 	...
	// _LT_/tunnels>
	bool GetTunnelsXml(CkString &outStr);
	// Returns information about the current set of running SSH tunnels. This is a
	// snapshot of the tunnels at a single point in time. The XML has this format:
	// _LT_tunnels>
	// 	_LT_t>
	// 		_LT_uniqueId>..._LT_/uniqueId>
	// 		_LT_clientIp>..._LT_/clientIp>
	// 		_LT_clientPort>..._LT_/clientPort>
	// 		_LT_serverIp>..._LT_/serverIp>
	// 		_LT_serverPort>..._LT_/serverPort>
	// 		_LT_tunnelType>..._LT_/tunnelType>
	// 		_LT_login>..._LT_/login>
	// 		_LT_password>..._LT_/password>
	// 	_LT_/t>
	// 	_LT_t>
	// 	...
	// 	_LT_/t>
	// 	...
	// _LT_/tunnels>
	const char *getTunnelsXml(void);
	// Returns information about the current set of running SSH tunnels. This is a
	// snapshot of the tunnels at a single point in time. The XML has this format:
	// _LT_tunnels>
	// 	_LT_t>
	// 		_LT_uniqueId>..._LT_/uniqueId>
	// 		_LT_clientIp>..._LT_/clientIp>
	// 		_LT_clientPort>..._LT_/clientPort>
	// 		_LT_serverIp>..._LT_/serverIp>
	// 		_LT_serverPort>..._LT_/serverPort>
	// 		_LT_tunnelType>..._LT_/tunnelType>
	// 		_LT_login>..._LT_/login>
	// 		_LT_password>..._LT_/password>
	// 	_LT_/t>
	// 	_LT_t>
	// 	...
	// 	_LT_/t>
	// 	...
	// _LT_/tunnels>
	const char *tunnelsXml(void);

	// Sets the key to be used for public-key SSH authentication. NOTE: The private key
	// is required for authentication. The public-part of the key is installed on the
	// server, and the client must present the private key.
	bool SetSshAuthenticationKey(CkSshKey &key);

	// Stops the listen background thread. It is possible to continue accepting
	// connections by re-calling BeginAccepting.
	bool StopAccepting(void);

	// Stops all currently running tunnels in the SSH tunnel pool background thread.
	bool StopAllTunnels(int maxWaitMs);

	// Unlocks the component. This must be called once prior to calling any other
	// method. A fully-functional 30-day trial is automatically started when an
	// arbitrary string is passed to this method. For example, passing "Hello", or
	// "abc123" will unlock the component for the 1st thirty days after the initial
	// install.
	bool UnlockComponent(const char *unlockCode);

	// Starts a background thread for listening on listenPort. A new SSH tunnel is created
	// and managed for each accepted connection. SSH tunnels are managed in a 2nd
	// background thread: the SSH tunnel pool thread.
	// 
	// BeginAccepting starts a background thread that creates a socket, binds to the
	// port, and begins listening. If the bind fails (meaning something else may have
	// already bound to the same port), then the background thread exits. You may check
	// to see if BeginAccepting succeeds by waiting a short time (perhaps 50 millisec)
	// and then examine the IsAccepting property. If it is false, then BeginAccepting
	// failed.
	// 
	bool BeginAccepting(int listenPort);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
