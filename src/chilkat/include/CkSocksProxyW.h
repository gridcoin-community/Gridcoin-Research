// CkSocksProxyW.h: interface for the CkSocksProxyW class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkSocksProxyW_H
#define _CkSocksProxyW_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkWideCharBase.h"

class CkBaseProgressW;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkSocksProxyW
class CK_VISIBLE_PUBLIC CkSocksProxyW  : public CkWideCharBase
{
    private:
	bool m_cbOwned;
	CkBaseProgressW *m_callback;

	// Don't allow assignment or copying these objects.
	CkSocksProxyW(const CkSocksProxyW &);
	CkSocksProxyW &operator=(const CkSocksProxyW &);

    public:
	CkSocksProxyW(void);
	virtual ~CkSocksProxyW(void);

	static CkSocksProxyW *createNew(void);
	

	CkSocksProxyW(bool bCallbackOwned);
	static CkSocksProxyW *createNew(bool bCallbackOwned);

	
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
	// If true, unauthenticated SOCKS5 connections are allowed. If false, SOCKS5
	// connections must provide a login/password. The default value is true.
	bool get_AllowUnauthenticatedSocks5(void);
	// If true, unauthenticated SOCKS5 connections are allowed. If false, SOCKS5
	// connections must provide a login/password. The default value is true.
	void put_AllowUnauthenticatedSocks5(bool newVal);

	// true if the current pending connection is a SOCKS5 connection that provides a
	// login/password. In this case, the Login and Password properties may be checked
	// to determine if the connection should be allowed or rejected. If allowed, the
	// ProceedSocks5 method should be called. Otherwise the RejectConnection method
	// should be called.
	// 
	// A value of false indicates that the current pending connection is either
	// SOCKS4 or unauthenticated SOCKS5.
	// 
	bool get_AuthenticatedSocks5(void);

	// The connecting client's IP address for the current pending connect request. For
	// authenticated SOCKS5 connections, the client IP is not available until after the
	// ProceedSocks5 method is called.
	void get_ClientIp(CkString &str);
	// The connecting client's IP address for the current pending connect request. For
	// authenticated SOCKS5 connections, the client IP is not available until after the
	// ProceedSocks5 method is called.
	const wchar_t *clientIp(void);

	// The connecting client's bound port number for the current pending connect
	// request. For authenticated SOCKS5 connections, this information is not available
	// until after the ProceedSocks5 method is called.
	int get_ClientPort(void);

	// true if a connect request is pending after calling WaitForConnection.
	// Otherwise false.
	bool get_ConnectionPending(void);

	// The IP address to use for computers with multiple network interfaces or IP
	// addresses. For computers with a single network interface (i.e. most computers),
	// this property should not be set. For multihoming computers, the default IP
	// address is automatically used if this property is not set.
	// 
	// The IP address is a string such as in dotted notation using numbers, not domain
	// names, such as "165.164.55.124".
	// 
	// Note: The listen port is specified as an argument in the Initialize method.
	// 
	void get_ListenBindIpAddress(CkString &str);
	// The IP address to use for computers with multiple network interfaces or IP
	// addresses. For computers with a single network interface (i.e. most computers),
	// this property should not be set. For multihoming computers, the default IP
	// address is automatically used if this property is not set.
	// 
	// The IP address is a string such as in dotted notation using numbers, not domain
	// names, such as "165.164.55.124".
	// 
	// Note: The listen port is specified as an argument in the Initialize method.
	// 
	const wchar_t *listenBindIpAddress(void);
	// The IP address to use for computers with multiple network interfaces or IP
	// addresses. For computers with a single network interface (i.e. most computers),
	// this property should not be set. For multihoming computers, the default IP
	// address is automatically used if this property is not set.
	// 
	// The IP address is a string such as in dotted notation using numbers, not domain
	// names, such as "165.164.55.124".
	// 
	// Note: The listen port is specified as an argument in the Initialize method.
	// 
	void put_ListenBindIpAddress(const wchar_t *newVal);

	// The login (username) provided by a connecting client when a connect request is
	// in progress. Unauthenticated SOCKS5 connect requests do not provide a login or
	// password. SOCKS4 connections provide a login (i.e. username), but no password.
	void get_Login(CkString &str);
	// The login (username) provided by a connecting client when a connect request is
	// in progress. Unauthenticated SOCKS5 connect requests do not provide a login or
	// password. SOCKS4 connections provide a login (i.e. username), but no password.
	const wchar_t *login(void);

	// The local bind IP address to use for outbound connections to a destination
	// server. This property is for computers with multiple network interfaces or IP
	// addresses. For computers with a single network interface (i.e. most computers),
	// this property should not be set. For multihoming computers, the default IP
	// address is automatically used if this property is not set.
	// 
	// The IP address is a string such as in dotted notation using numbers, not domain
	// names, such as "165.164.55.124".
	// 
	void get_OutboundBindIpAddress(CkString &str);
	// The local bind IP address to use for outbound connections to a destination
	// server. This property is for computers with multiple network interfaces or IP
	// addresses. For computers with a single network interface (i.e. most computers),
	// this property should not be set. For multihoming computers, the default IP
	// address is automatically used if this property is not set.
	// 
	// The IP address is a string such as in dotted notation using numbers, not domain
	// names, such as "165.164.55.124".
	// 
	const wchar_t *outboundBindIpAddress(void);
	// The local bind IP address to use for outbound connections to a destination
	// server. This property is for computers with multiple network interfaces or IP
	// addresses. For computers with a single network interface (i.e. most computers),
	// this property should not be set. For multihoming computers, the default IP
	// address is automatically used if this property is not set.
	// 
	// The IP address is a string such as in dotted notation using numbers, not domain
	// names, such as "165.164.55.124".
	// 
	void put_OutboundBindIpAddress(const wchar_t *newVal);

	// The local bind port to use for outbound connections to a destination server. In
	// most cases this property should never be set. Do not set this property unless
	// you know exactly what you are doing and you know that it is necessary.
	int get_OutboundBindPort(void);
	// The local bind port to use for outbound connections to a destination server. In
	// most cases this property should never be set. Do not set this property unless
	// you know exactly what you are doing and you know that it is necessary.
	void put_OutboundBindPort(int newVal);

	// The password sent by a connecting client during an authenticated SOCKS5
	// connection.
	void get_Password(CkString &str);
	// The password sent by a connecting client during an authenticated SOCKS5
	// connection.
	const wchar_t *password(void);

	// The destination server's IP address for the current pending connect request. For
	// authenticated SOCKS5 connections, this information is not available until after
	// the ProceedSocks5 method is called.
	void get_ServerIp(CkString &str);
	// The destination server's IP address for the current pending connect request. For
	// authenticated SOCKS5 connections, this information is not available until after
	// the ProceedSocks5 method is called.
	const wchar_t *serverIp(void);

	// The destination server's port for the current pending connect request. For
	// authenticated SOCKS5 connections, this information is not available until after
	// the ProceedSocks5 method is called.
	int get_ServerPort(void);

	// The SOCKS version of the current pending connect request. This property will
	// have a value of 4 for SOCKS4 connections, and 5 for SOCKS5 connections.
	int get_SocksVersion(void);



	// ----------------------
	// Methods
	// ----------------------
	// This is the final step in establishing a SOCKS5 tunnel. The SOCKS "handshake" is
	// completed and the bi-directional SOCKS tunnel is managed in a background thread.
	// All data arriving from the client is automatically forwarded to the server, and
	// all data arriving from the server is automatically forwarded to the client.
	// Regardless of how many SOCKS tunnels are active, they are all managed in a
	// single background thread.
	bool AllowConnection(void);


	bool GetTunnelsXml(CkString &outStr);

	const wchar_t *getTunnelsXml(void);

	const wchar_t *tunnelsXml(void);

	// Initializes the SOCKS server to listen on a port. Both SOCKS4 and SOCKS5
	// connections are handled by this component/class.
	bool Initialize(int port);

	// This method should be called after receiving an authenticated SOCKS5 connection
	// and verifying the Login and Password. It sends an "authentication accepted"
	// message to the SOCKS5 client and proceeds to the next step in the SOCKS5
	// "handshake". If ProceedSocks5 returns true, the following properties will
	// contain valid data: ClientIp, ClientPort, ServerIp, ServerPort. If ProceedSocks5
	// returns false it means that the connection with the client has ended.
	bool ProceedSocks5(void);

	// Rejects a client connection and closes the connection with the client.
	bool RejectConnection(void);


	bool StopAllTunnels(int maxWaitMs);

	// Unlocks the component allowing for the full functionality to be used. An
	// arbitrary string can be passed to initiate a fully-functional 30-day trial.
	bool UnlockComponent(const wchar_t *unlockCode);

	// Waits for an incoming SOCKS4 or SOCKS5 connection. The maximum number of
	// milliseconds to way is specified by maxWaitMs. Passing a value of 0 for maxWaitMs causes
	// the method to wait indefinitely. Returns true if a connection was received or
	// if a timeout occurred. To distinguish between a timeout and a pending connect
	// request, check the ConnectionPending property. If ConnectionPending is true,
	// then a client connection is waiting to be accepted/rejected. Otherwise it was a
	// timeout. A return value of false indicates a more serious problem where it's
	// best to examine the contents of the LastErrorText property.
	// 
	// If a connection is pending, the next step is to examine the various properties
	// about the connect request to determine if it should be accepted or rejected. The
	// SocksVersion property tells whether it's a SOCKS4 or SOCKS5 connection. If a
	// SOCKS4 connection is received, the following properties are set: ClientIp,
	// ClientPort, ServerIp, ServerPort, Login. Your program should proceed by calling
	// AllowConnection or RejectConnection.
	// 
	// If a SOCKS5 connection is received, check the AuthenticatedSocks5 property to
	// determine if the client is providing a login/password. If so, then the Login and
	// Password properties have this information. If valid, your application should
	// continue the SOCKS5 connection by calling ProceedSocks5. Otherwise the
	// connection should be rejected immediately by calling RejectConnection.
	// 
	// For SOCKS5, the ClientIp, ClientPort, ServerIp, and ServerPort properties are
	// set immediately after WaitForConnection returns for unauthenticated SOCKS5
	// connections, or after ProceedSocks5 returns for authenticated SOCKS5. These
	// properties may be examined to accept or reject the connection. Call
	// AllowConnection to allow, or RejectConnection to reject.
	// 
	// Note: If the AllowUnauthenticatedSocks5 property is set to false, then
	// unauthenicated SOCKS5 connections are automatically rejected. (In other words,
	// WaitForConnection will never return an unauthenticated connection attempt and
	// will instead automatically reject it and continue waiting for the next incoming
	// connection.)
	// 
	bool WaitForConnection(int maxWaitMs);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
