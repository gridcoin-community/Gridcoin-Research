// CkHttpProgress.h: interface for the CkHttpProgress class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _CKHTTPPROGRESSW_H
#define _CKHTTPPROGRESSW_H

#include "CkBaseProgressW.h"

#ifndef __sun__
#pragma pack (push, 8)
#endif
 

/*

    ProgressInfo events:


    SocketConnect - hostname:port, called when initiating a connection.
    SocketConnected - hostname:port, called after successfully connected.
    HttpProxyConnect - hostname:port
    SslHandshake - "Starting"/"Finished"
    HttpGetBegin - URL
    HttpCacheHit - "Returning page from cache."
    HttpInfo - various conditions...
	"Begin reading response"    -- called when beginning to read the response.
	"Finished reading response"
	"Existing connection with HTTP server no longer open, restarting GET with new connection."
	"Reading chunked response."
	"UnGzipping response data"
	"Connection:close header is present"
    GetRequest - the full HTTP GET request to be sent to the server.
    ResponseHeader - the header of the HTTP response.
    HttpStatusCode - HTTP response status code (integer)
    ChunkSize - Size (in bytes) of next chunk in response.
    ResponseContentLength - Non-chunked response size in bytes.
    UnGzippedLength -  If the response was gzip compressed, this is the uncompressed size.
    HostnameResolve - hostname, Called when starting to resolve a hostname (to an IP address)
    ResolvedToIp - dotted IP address, called after hostname is resolved.
    HttpAuth - one of the following strings:
	    "Starting Negotiate Authentication"
	    "Starting NTLM Authentication"
	    "Adding Basic Authentication Header"
	    "Adding Proxy Authentication Header"
	    "Starting Proxy NTLM Authentication"
	    "Starting Digest Authentication"
    CookieToSend - Value of a Set-Cookie header field to be added to the outgoing request.
    SavingCookie - XML of cookie being persisted.
    HttpRedirect - Redirect URL
    Socks4Connect - domain:port
    Socks5Connect - domain:port

    HttpRequestBegin - Verb (such as POST, GET, PUT), domain:port/path
    RequestHeader - The full HTTP request header to be sent.
    StartSendingRequest - Size of entire request, including header, in number of bytes. (Not called for QuickGet)
	- for uploads, this is the size of the entire upload (headers and all files combined)
    SubPartHeader - The header for one of the parts within a multipart request.
    UploadFilename - The file about to be uploaded (streamed from file to socket..)

  */

// When creating an application class that inherits the CkHttpProgressW base class, use the CK_HTTPPROGRESSW_API 
// definition to declare the overrides in the class header.  This has the effect that if for
// some unforeseen and unlikely reason the Chilkat event callback API changes, or if new
// callback methods are added in a future version, then you'll discover them at compile time
// after updating to the new Chilkat version.  
// For example:
/*
    class MyProgress : public CkHttpProgressW
    {
	public:
	    CK_HTTPPROGRESSW_API

	...
    };
*/
#define CK_HTTPPROGRESSW_API \
	void HttpRedirect(const wchar_t *originalUrl, const wchar_t *redirectUrl, bool *abort);\
	void HttpChunked(void);\
	void HttpBeginReceive(void);\
	void HttpEndReceive(bool success);\
	void HttpBeginSend(void);\
	void HttpEndSend(bool success);\
	void ReceiveRate(__int64 byteCount, unsigned long bytesPerSec);\
	void SendRate(__int64 byteCount, unsigned long bytesPerSec);

class CkHttpProgressW : public CkBaseProgressW
{
    public:
	CkHttpProgressW() { }
	virtual ~CkHttpProgressW() { }

	// These event callbacks are now defined in CkBaseProgressW.
	//virtual void PercentDone(int pctDone, bool *abort) { }
	//virtual void AbortCheck(bool *abort) { }
	//virtual void ProgressInfo(const wchar_t *name, const wchar_t *value) { }

	virtual void HttpRedirect(const wchar_t *originalUrl, const wchar_t *redirectUrl, bool *abort) { }

	// Called just before a chunked response is about to be received.
	// With chunked responses, it is not possible to get PercentDone callbacks because
	// the entire size of the HTTP response is not known as it is being received.
	virtual void HttpChunked(void) { }

	virtual void HttpBeginReceive(void) { }
	virtual void HttpEndReceive(bool success) { }
	virtual void HttpBeginSend(void) { }
	virtual void HttpEndSend(bool success) { }

	virtual void ReceiveRate(__int64 byteCount, unsigned long bytesPerSec) { }
	virtual void SendRate(__int64 byteCount, unsigned long bytesPerSec) { }

};
#ifndef __sun__
#pragma pack (pop)
#endif


#endif
