// CkHttpW.h: interface for the CkHttpW class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkHttpW_H
#define _CkHttpW_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkWideCharBase.h"

class CkByteData;
class CkHttpResponseW;
class CkCertW;
class CkHttpRequestW;
class CkPrivateKeyW;
class CkHttpProgressW;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkHttpW
class CK_VISIBLE_PUBLIC CkHttpW  : public CkWideCharBase
{
    private:
	bool m_cbOwned;
	CkHttpProgressW *m_callback;

	// Don't allow assignment or copying these objects.
	CkHttpW(const CkHttpW &);
	CkHttpW &operator=(const CkHttpW &);

    public:
	CkHttpW(void);
	virtual ~CkHttpW(void);

	static CkHttpW *createNew(void);
	

	CkHttpW(bool bCallbackOwned);
	static CkHttpW *createNew(bool bCallbackOwned);

	
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	CkHttpProgressW *get_EventCallbackObject(void) const;
	void put_EventCallbackObject(CkHttpProgressW *progress);


	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------
	// The Accept header field to be automatically included with GET requests issued by
	// QuickGet or QuickGetStr. The default value is "*/*".
	void get_Accept(CkString &str);
	// The Accept header field to be automatically included with GET requests issued by
	// QuickGet or QuickGetStr. The default value is "*/*".
	const wchar_t *ck_accept(void);
	// The Accept header field to be automatically included with GET requests issued by
	// QuickGet or QuickGetStr. The default value is "*/*".
	void put_Accept(const wchar_t *newVal);

	// The AcceptCharset header field to be automatically included with GET requests
	// issued by QuickGet or QuickGetStr. The default value is
	// "ISO-8859-1,utf-8;q=0.7,*;q=0.7".
	void get_AcceptCharset(CkString &str);
	// The AcceptCharset header field to be automatically included with GET requests
	// issued by QuickGet or QuickGetStr. The default value is
	// "ISO-8859-1,utf-8;q=0.7,*;q=0.7".
	const wchar_t *acceptCharset(void);
	// The AcceptCharset header field to be automatically included with GET requests
	// issued by QuickGet or QuickGetStr. The default value is
	// "ISO-8859-1,utf-8;q=0.7,*;q=0.7".
	void put_AcceptCharset(const wchar_t *newVal);

	// The AcceptLanguage header field to be automatically included with GET requests
	// issued by QuickGet or QuickGetStr. The default value is "en-us,en;q=0.5".
	void get_AcceptLanguage(CkString &str);
	// The AcceptLanguage header field to be automatically included with GET requests
	// issued by QuickGet or QuickGetStr. The default value is "en-us,en;q=0.5".
	const wchar_t *acceptLanguage(void);
	// The AcceptLanguage header field to be automatically included with GET requests
	// issued by QuickGet or QuickGetStr. The default value is "en-us,en;q=0.5".
	void put_AcceptLanguage(const wchar_t *newVal);

	// Controls whether the "Accept-Encoding: gzip" header is added to HTTP requests
	// sent via any method that sends an HTTP request without using the HttpRequest
	// object (such as QuickGetStr). If false, then the empty Accept-Encoding header
	// is added which means the server response should contain the uncompressed
	// content. The default value is true, which means the server, if it chooses, may
	// send a gzipped response.
	bool get_AllowGzip(void);
	// Controls whether the "Accept-Encoding: gzip" header is added to HTTP requests
	// sent via any method that sends an HTTP request without using the HttpRequest
	// object (such as QuickGetStr). If false, then the empty Accept-Encoding header
	// is added which means the server response should contain the uncompressed
	// content. The default value is true, which means the server, if it chooses, may
	// send a gzipped response.
	void put_AllowGzip(bool newVal);

	// If set to true, the "Host" header field will automatically be added to the
	// request header for any QuickGet or QuickGetStr method calls. The value of the
	// Host header field is taken from the hostname part of the URL passed to
	// QuickGet/QuickGetStr.
	bool get_AutoAddHostHeader(void);
	// If set to true, the "Host" header field will automatically be added to the
	// request header for any QuickGet or QuickGetStr method calls. The value of the
	// Host header field is taken from the hostname part of the URL passed to
	// QuickGet/QuickGetStr.
	void put_AutoAddHostHeader(bool newVal);

	// The AWS Access Key to be used with the Amazon S3 methods listed below.
	void get_AwsAccessKey(CkString &str);
	// The AWS Access Key to be used with the Amazon S3 methods listed below.
	const wchar_t *awsAccessKey(void);
	// The AWS Access Key to be used with the Amazon S3 methods listed below.
	void put_AwsAccessKey(const wchar_t *newVal);

	// The regional endpoint (domain) to be used for Amazon S3 method calls. The
	// default value is "s3.amazonaws.com". This can be set to any valid Amazon S3
	// endpoint, such as "s3-eu-west-1.amazonaws.com", or the endpoints for S3-API
	// compatible services from other different providers.
	void get_AwsEndpoint(CkString &str);
	// The regional endpoint (domain) to be used for Amazon S3 method calls. The
	// default value is "s3.amazonaws.com". This can be set to any valid Amazon S3
	// endpoint, such as "s3-eu-west-1.amazonaws.com", or the endpoints for S3-API
	// compatible services from other different providers.
	const wchar_t *awsEndpoint(void);
	// The regional endpoint (domain) to be used for Amazon S3 method calls. The
	// default value is "s3.amazonaws.com". This can be set to any valid Amazon S3
	// endpoint, such as "s3-eu-west-1.amazonaws.com", or the endpoints for S3-API
	// compatible services from other different providers.
	void put_AwsEndpoint(const wchar_t *newVal);

	// The AWS Secret Key to be used with the Amazon S3 methods listed below.
	void get_AwsSecretKey(CkString &str);
	// The AWS Secret Key to be used with the Amazon S3 methods listed below.
	const wchar_t *awsSecretKey(void);
	// The AWS Secret Key to be used with the Amazon S3 methods listed below.
	void put_AwsSecretKey(const wchar_t *newVal);

	// The AWS sub-resources to be used with the Amazon S3 methods listed below.
	// 
	// If the S3 request needs to address a sub-resource, like ?versioning, ?policy,
	// ?location, ?acl, or ?torrent, or ?versionid append the sub-resource and its
	// value if it has one. Note that in case of multiple sub-resources, sub-resources
	// must be lexicographically sorted by sub-resource name and separated by '&'. e.g.
	// "acl&versionId=value"
	// 
	// The list of sub-resources that can be included are: acl, location, logging,
	// notification, partNumber, policy, requestPayment, torrent, uploadId, uploads,
	// versionId, versioning, versions and website.
	// 
	void get_AwsSubResources(CkString &str);
	// The AWS sub-resources to be used with the Amazon S3 methods listed below.
	// 
	// If the S3 request needs to address a sub-resource, like ?versioning, ?policy,
	// ?location, ?acl, or ?torrent, or ?versionid append the sub-resource and its
	// value if it has one. Note that in case of multiple sub-resources, sub-resources
	// must be lexicographically sorted by sub-resource name and separated by '&'. e.g.
	// "acl&versionId=value"
	// 
	// The list of sub-resources that can be included are: acl, location, logging,
	// notification, partNumber, policy, requestPayment, torrent, uploadId, uploads,
	// versionId, versioning, versions and website.
	// 
	const wchar_t *awsSubResources(void);
	// The AWS sub-resources to be used with the Amazon S3 methods listed below.
	// 
	// If the S3 request needs to address a sub-resource, like ?versioning, ?policy,
	// ?location, ?acl, or ?torrent, or ?versionid append the sub-resource and its
	// value if it has one. Note that in case of multiple sub-resources, sub-resources
	// must be lexicographically sorted by sub-resource name and separated by '&'. e.g.
	// "acl&versionId=value"
	// 
	// The list of sub-resources that can be included are: acl, location, logging,
	// notification, partNumber, policy, requestPayment, torrent, uploadId, uploads,
	// versionId, versioning, versions and website.
	// 
	void put_AwsSubResources(const wchar_t *newVal);

	// If HTTP basic authentication is needed, this property must be set to true. The
	// HTTP protocol allows for several different types of authentication schemes, such
	// as NTLM, Digest, OAuth1, etc. A given server will support (or allow) certain
	// authentication schemes (also known as authentication methods). Except for the
	// "Basic" authentication method, the other forms of authentication do not involve
	// sending the login and password in plain unencrypted text over the connection.
	// The Basic authentication method is insecure in that it sends the login/password
	// for all to see. If the connection is SSL/TLS, then this might be considered OK.
	// Chilkat takes the safe approach and will not allow Basic authentication unless
	// this property has been explicitly set to true. The default value of this
	// property is false.
	// 
	// Note: It is not required to know the authentication methods accepted by the
	// server beforehand (except for the case of Basic authentication). When
	// authentication is required, Chilkat will first send the request without the
	// Authorization header, receive back the 401 Authorization Required response along
	// with information about what authentication methods are accepted, and then
	// re-send with an accepted authentication method. If the authentication method is
	// known in advance, then an application may set the appropriate property, such as
	// NtlmAuth to true so that the extra (internal) round-trip is not required.
	// 
	bool get_BasicAuth(void);
	// If HTTP basic authentication is needed, this property must be set to true. The
	// HTTP protocol allows for several different types of authentication schemes, such
	// as NTLM, Digest, OAuth1, etc. A given server will support (or allow) certain
	// authentication schemes (also known as authentication methods). Except for the
	// "Basic" authentication method, the other forms of authentication do not involve
	// sending the login and password in plain unencrypted text over the connection.
	// The Basic authentication method is insecure in that it sends the login/password
	// for all to see. If the connection is SSL/TLS, then this might be considered OK.
	// Chilkat takes the safe approach and will not allow Basic authentication unless
	// this property has been explicitly set to true. The default value of this
	// property is false.
	// 
	// Note: It is not required to know the authentication methods accepted by the
	// server beforehand (except for the case of Basic authentication). When
	// authentication is required, Chilkat will first send the request without the
	// Authorization header, receive back the 401 Authorization Required response along
	// with information about what authentication methods are accepted, and then
	// re-send with an accepted authentication method. If the authentication method is
	// known in advance, then an application may set the appropriate property, such as
	// NtlmAuth to true so that the extra (internal) round-trip is not required.
	// 
	void put_BasicAuth(bool newVal);

	// When a background-enabled method is run asynchronously in a background thread,
	// the last-error information is saved here and not in the LastErrorText property.
	// If the background method fails, this will contain information about what
	// transpired. (This property also contains information when the background method
	// succeeds.)
	void get_BgLastErrorText(CkString &str);
	// When a background-enabled method is run asynchronously in a background thread,
	// the last-error information is saved here and not in the LastErrorText property.
	// If the background method fails, this will contain information about what
	// transpired. (This property also contains information when the background method
	// succeeds.)
	const wchar_t *bgLastErrorText(void);

	// The integer percent completed for a background HTTP method call. The value will
	// be between 0 and 100 while a background method call is in progress. Otherwise,
	// the value is meaningless. The BgPercentDone only applies in cases where it is
	// possible to track completion by a percentage. If an HTTP response is chunked,
	// then there is no way of knowing how much response data is forthcoming, and
	// therefore it is not possible to track the percentage completed.
	int get_BgPercentDone(void);

	// If a backgrounded method returns a byte array, the returned data is found here.
	void get_BgResultData(CkByteData &outBytes);

	// If a backgrounded method returns an integer, the return value is found here.
	int get_BgResultInt(void);

	// If a backgrounded method returns a string, the return value is found here.
	void get_BgResultString(CkString &str);
	// If a backgrounded method returns a string, the return value is found here.
	const wchar_t *bgResultString(void);

	// Becomes true when the background method completes. Your application would
	// periodically check for this condition.
	bool get_BgTaskFinished(void);

	// If true then the object instance already has a backgrounded method running.
	// Another backgrounded method cannot be started until the 1st completes. (Multiple
	// simultaneous background methods may run by using multiple object instances.)
	// 
	// If false, then no method is currently running in a background thread.
	// 
	bool get_BgTaskRunning(void);

	// This property's value is only meaningful (true or false) after a
	// backgrounded method completes.
	bool get_BgTaskSuccess(void);

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

	// The amount of time in seconds to wait before timing out when connecting to an
	// HTTP server.
	int get_ConnectTimeout(void);
	// The amount of time in seconds to wait before timing out when connecting to an
	// HTTP server.
	void put_ConnectTimeout(int newVal);

	// The Connection header field to be automatically included with GET requests
	// issued by QuickGet or QuickGetStr. The default value is "Keep-Alive". To prevent
	// the Connection header from being added to the HTTP header, set this property to
	// the empty string.
	void get_Connection(CkString &str);
	// The Connection header field to be automatically included with GET requests
	// issued by QuickGet or QuickGetStr. The default value is "Keep-Alive". To prevent
	// the Connection header from being added to the HTTP header, set this property to
	// the empty string.
	const wchar_t *connection(void);
	// The Connection header field to be automatically included with GET requests
	// issued by QuickGet or QuickGetStr. The default value is "Keep-Alive". To prevent
	// the Connection header from being added to the HTTP header, set this property to
	// the empty string.
	void put_Connection(const wchar_t *newVal);

	// Specifies a directory where cookies are automatically persisted if the
	// Http.SaveCookies property is turned on. Cookies are stored in XML formatted
	// files, one per domain, to main it easy for other programs to understand and
	// parse. May be set to the string "memory" to cache cookies in memory.
	void get_CookieDir(CkString &str);
	// Specifies a directory where cookies are automatically persisted if the
	// Http.SaveCookies property is turned on. Cookies are stored in XML formatted
	// files, one per domain, to main it easy for other programs to understand and
	// parse. May be set to the string "memory" to cache cookies in memory.
	const wchar_t *cookieDir(void);
	// Specifies a directory where cookies are automatically persisted if the
	// Http.SaveCookies property is turned on. Cookies are stored in XML formatted
	// files, one per domain, to main it easy for other programs to understand and
	// parse. May be set to the string "memory" to cache cookies in memory.
	void put_CookieDir(const wchar_t *newVal);

	// The default freshness period (in minutes) for cached documents when the
	// FreshnessAlgorithm property is set to 0. The default value is 10080 (1 week).
	int get_DefaultFreshPeriod(void);
	// The default freshness period (in minutes) for cached documents when the
	// FreshnessAlgorithm property is set to 0. The default value is 10080 (1 week).
	void put_DefaultFreshPeriod(int newVal);

	// Setting this property to true causes the HTTP component to use digest
	// authentication. The default value is false.
	bool get_DigestAuth(void);
	// Setting this property to true causes the HTTP component to use digest
	// authentication. The default value is false.
	void put_DigestAuth(bool newVal);

	// If the KeepEventLog property is set to true, then this property will contain
	// the number of events that have accumulated in the in-memory event log. The
	// events are indexed from 0 to EventLogCount-1. The ClearEventLog method may be
	// called to clear the event log. The name and value of each event can be retrieved
	// via the EventLogName and EventLogValue methods.
	int get_EventLogCount(void);

	// Set to true if pages should be fetched from cache when possible. Only HTTP GET
	// requests are cached. HTTP responses that include Set-Cookie headers are not
	// cached. A page is fetched from the disk cache if it is present and it is "fresh"
	// according to the FreshnessAlgorithm property. If a page exists in cache but is
	// not fresh, the HTTP component will issue a revalidate request and update the
	// cache appropriately according to the response.
	bool get_FetchFromCache(void);
	// Set to true if pages should be fetched from cache when possible. Only HTTP GET
	// requests are cached. HTTP responses that include Set-Cookie headers are not
	// cached. A page is fetched from the disk cache if it is present and it is "fresh"
	// according to the FreshnessAlgorithm property. If a page exists in cache but is
	// not fresh, the HTTP component will issue a revalidate request and update the
	// cache appropriately according to the response.
	void put_FetchFromCache(bool newVal);

	// If an HTTP GET was redirected (as indicated by the WasRedirected property), this
	// property will contain the final redirect URL, assuming the FollowRedirects
	// property is true.
	void get_FinalRedirectUrl(CkString &str);
	// If an HTTP GET was redirected (as indicated by the WasRedirected property), this
	// property will contain the final redirect URL, assuming the FollowRedirects
	// property is true.
	const wchar_t *finalRedirectUrl(void);

	// If true, then 301 and 302 redirects are automatically followed when calling
	// QuickGet and QuickGetStr. FollowRedirects is true by default.
	bool get_FollowRedirects(void);
	// If true, then 301 and 302 redirects are automatically followed when calling
	// QuickGet and QuickGetStr. FollowRedirects is true by default.
	void put_FollowRedirects(bool newVal);

	// The freshness algorithm to use when determining the freshness of a cached HTTP
	// GET response. A value of 1 causes an LM-factor algorithm to be used. This is the
	// default. The LMFactor property is a value between 1 and 100 indicating the
	// percentage of time based on the last-modified date of the HTML page. For
	// example, if the LMFactor is 50, and an HTML page was modified 10 days ago, then
	// the page will expire (i.e. no longer be fresh) in 5 days (50% of 10 days). This
	// only applies to HTTP responses that do not have page expiration information. If
	// the FreshnessAlgorithm = 0, then a constant expire time period determined by the
	// DefaultFreshPeriod property is used.
	int get_FreshnessAlgorithm(void);
	// The freshness algorithm to use when determining the freshness of a cached HTTP
	// GET response. A value of 1 causes an LM-factor algorithm to be used. This is the
	// default. The LMFactor property is a value between 1 and 100 indicating the
	// percentage of time based on the last-modified date of the HTML page. For
	// example, if the LMFactor is 50, and an HTML page was modified 10 days ago, then
	// the page will expire (i.e. no longer be fresh) in 5 days (50% of 10 days). This
	// only applies to HTTP responses that do not have page expiration information. If
	// the FreshnessAlgorithm = 0, then a constant expire time period determined by the
	// DefaultFreshPeriod property is used.
	void put_FreshnessAlgorithm(int newVal);

	// When set to a non-zero value, it specifies the time interval in milliseconds
	// between AbortCheck events. Any HTTP operation can be aborted via the AbortCheck
	// event. Event callbacks are supported for ActiveX, .NET, and C++ implementations
	// of this API.
	int get_HeartbeatMs(void);
	// When set to a non-zero value, it specifies the time interval in milliseconds
	// between AbortCheck events. Any HTTP operation can be aborted via the AbortCheck
	// event. Event callbacks are supported for ActiveX, .NET, and C++ implementations
	// of this API.
	void put_HeartbeatMs(int newVal);

	// Some HTTP responses contain a "Cache-Control: must-revalidate" header. If this
	// is present, the server is requesting that the client always issue a revalidate
	// HTTP request instead of serving the page directly from cache. If
	// IgnoreMustRevalidate is set to true, then Chilkat HTTP will serve the page
	// directly from cache without revalidating until the page is no longer fresh.
	// 
	// The default value of this property is false.
	// 
	bool get_IgnoreMustRevalidate(void);
	// Some HTTP responses contain a "Cache-Control: must-revalidate" header. If this
	// is present, the server is requesting that the client always issue a revalidate
	// HTTP request instead of serving the page directly from cache. If
	// IgnoreMustRevalidate is set to true, then Chilkat HTTP will serve the page
	// directly from cache without revalidating until the page is no longer fresh.
	// 
	// The default value of this property is false.
	// 
	void put_IgnoreMustRevalidate(bool newVal);

	// Some HTTP responses contain headers of various types that indicate that the page
	// should not be cached. Chilkat HTTP will adhere to this unless this property is
	// set to true.
	// 
	// The default value of this property is false.
	// 
	bool get_IgnoreNoCache(void);
	// Some HTTP responses contain headers of various types that indicate that the page
	// should not be cached. Chilkat HTTP will adhere to this unless this property is
	// set to true.
	// 
	// The default value of this property is false.
	// 
	void put_IgnoreNoCache(bool newVal);

	// If true, an in-memory event log is kept for any method that communicates with
	// an HTTP server (such as Download, PostUrlEncoded, QuickGetStr,
	// SynchronousRequest, etc.). When HTTP methods are called asynchronously, the
	// event log can be checked while the HTTP operation is in in progress. This is
	// done by examining the EventLogCount property and then fetching each event's name
	// and value via the EventLogName and EventLogValue methods. See this example:
	// Asynchronous HTTP
	// <http://www.cknotes.com/?p=271> .
	// 
	// The ClearBgEventLog method may be called to clear the in-memory event log.
	// 
	// Important: If event logging is enabled, make sure to clear the event log after
	// each HTTP method call. Otherwise the log will continue to grow without bounds.
	// 
	// The default value of this property is false.
	// 
	// The following items may be found in the event log:
	// Name
	// Value
	// 
	// SocketConnect
	// hostname:port, called when initiating a connection.
	// 
	// SocketConnected
	// hostname:port, called after successfully connected.
	// 
	// HttpProxyConnect
	// hostname:port
	// 
	// SslHandshake
	// "Starting"/"Finished"
	// 
	// HttpGetBegin
	// URL
	// 
	// HttpCacheHit
	// "Returning page from cache."
	// 
	// HttpInfo
	// various conditions...
	// "Begin reading response" -- called when beginning to read the response.
	// "Finished reading response"
	// "Existing connection with HTTP server no longer open, restarting GET with new
	// connection."
	// "Reading chunked response."
	// "UnGzipping response data"
	// "Connection:close header is present"
	// 
	// GetRequest
	// the full HTTP GET request to be sent to the server.
	// 
	// ResponseHeader
	// the header of the HTTP response.
	// 
	// HttpStatusCode
	// HTTP response status code (integer)
	// 
	// ChunkSize
	// Size (in bytes) of next chunk in response.
	// 
	// ResponseContentLength
	// Non-chunked response size in bytes.
	// 
	// UnGzippedLength
	// If the response was gzip compressed, this is the uncompressed size.
	// 
	// HostnameResolve
	// hostname, Called when starting to resolve a hostname (to an IP address)
	// 
	// ResolvedToIp
	// dotted IP address, called after hostname is resolved.
	// 
	// HttpAuth
	// one of the following strings:
	// "Starting Negotiate Authentication"
	// "Starting NTLM Authentication"
	// "Adding Basic Authentication Header"
	// "Adding Proxy Authentication Header"
	// "Starting Proxy NTLM Authentication"
	// "Starting Digest Authentication"
	// 
	// CookieToSend
	// Value of a Set-Cookie header field to be added to the outgoing request.
	// 
	// SavingCookie
	// XML of cookie being persisted.
	// 
	// HttpRedirect
	// Redirect URL
	// 
	// Socks4Connect
	// domain:port
	// 
	// Socks5Connect
	// domain:port
	// 
	// HttpRequestBegin
	// Verb (such as POST, GET, PUT), domain:port/path
	// 
	// RequestHeader
	// The full HTTP request header to be sent.
	// 
	// StartSendingRequest
	// Size of entire request, including header, in number of bytes. (Not called for
	// QuickGet) For uploads, this is the size of the entire upload (headers and all
	// files combined)
	// 
	// SubPartHeader
	// The header for one of the parts within a multipart request.
	// 
	// UploadFilename
	// The file about to be uploaded (streamed from file to socket..)
	// 
	bool get_KeepEventLog(void);
	// If true, an in-memory event log is kept for any method that communicates with
	// an HTTP server (such as Download, PostUrlEncoded, QuickGetStr,
	// SynchronousRequest, etc.). When HTTP methods are called asynchronously, the
	// event log can be checked while the HTTP operation is in in progress. This is
	// done by examining the EventLogCount property and then fetching each event's name
	// and value via the EventLogName and EventLogValue methods. See this example:
	// Asynchronous HTTP
	// <http://www.cknotes.com/?p=271> .
	// 
	// The ClearBgEventLog method may be called to clear the in-memory event log.
	// 
	// Important: If event logging is enabled, make sure to clear the event log after
	// each HTTP method call. Otherwise the log will continue to grow without bounds.
	// 
	// The default value of this property is false.
	// 
	// The following items may be found in the event log:
	// Name
	// Value
	// 
	// SocketConnect
	// hostname:port, called when initiating a connection.
	// 
	// SocketConnected
	// hostname:port, called after successfully connected.
	// 
	// HttpProxyConnect
	// hostname:port
	// 
	// SslHandshake
	// "Starting"/"Finished"
	// 
	// HttpGetBegin
	// URL
	// 
	// HttpCacheHit
	// "Returning page from cache."
	// 
	// HttpInfo
	// various conditions...
	// "Begin reading response" -- called when beginning to read the response.
	// "Finished reading response"
	// "Existing connection with HTTP server no longer open, restarting GET with new
	// connection."
	// "Reading chunked response."
	// "UnGzipping response data"
	// "Connection:close header is present"
	// 
	// GetRequest
	// the full HTTP GET request to be sent to the server.
	// 
	// ResponseHeader
	// the header of the HTTP response.
	// 
	// HttpStatusCode
	// HTTP response status code (integer)
	// 
	// ChunkSize
	// Size (in bytes) of next chunk in response.
	// 
	// ResponseContentLength
	// Non-chunked response size in bytes.
	// 
	// UnGzippedLength
	// If the response was gzip compressed, this is the uncompressed size.
	// 
	// HostnameResolve
	// hostname, Called when starting to resolve a hostname (to an IP address)
	// 
	// ResolvedToIp
	// dotted IP address, called after hostname is resolved.
	// 
	// HttpAuth
	// one of the following strings:
	// "Starting Negotiate Authentication"
	// "Starting NTLM Authentication"
	// "Adding Basic Authentication Header"
	// "Adding Proxy Authentication Header"
	// "Starting Proxy NTLM Authentication"
	// "Starting Digest Authentication"
	// 
	// CookieToSend
	// Value of a Set-Cookie header field to be added to the outgoing request.
	// 
	// SavingCookie
	// XML of cookie being persisted.
	// 
	// HttpRedirect
	// Redirect URL
	// 
	// Socks4Connect
	// domain:port
	// 
	// Socks5Connect
	// domain:port
	// 
	// HttpRequestBegin
	// Verb (such as POST, GET, PUT), domain:port/path
	// 
	// RequestHeader
	// The full HTTP request header to be sent.
	// 
	// StartSendingRequest
	// Size of entire request, including header, in number of bytes. (Not called for
	// QuickGet) For uploads, this is the size of the entire upload (headers and all
	// files combined)
	// 
	// SubPartHeader
	// The header for one of the parts within a multipart request.
	// 
	// UploadFilename
	// The file about to be uploaded (streamed from file to socket..)
	// 
	void put_KeepEventLog(bool newVal);

	// An integer between 1 and 100 that indicates the percentage of time from the HTTP
	// page's last-modified date that will be used for the freshness period. The
	// default value is 25. For example, if a page is fetched with a last-modified date
	// of 4 weeks ago, and the LMFactor = 25, then the page will be considered fresh in
	// the cache for 1 week (25% of 4 weeks).
	int get_LMFactor(void);
	// An integer between 1 and 100 that indicates the percentage of time from the HTTP
	// page's last-modified date that will be used for the freshness period. The
	// default value is 25. For example, if a page is fetched with a last-modified date
	// of 4 weeks ago, and the LMFactor = 25, then the page will be considered fresh in
	// the cache for 1 week (25% of 4 weeks).
	void put_LMFactor(int newVal);

	// The content-type of the last HTTP response received by the HTTP component.
	void get_LastContentType(CkString &str);
	// The content-type of the last HTTP response received by the HTTP component.
	const wchar_t *lastContentType(void);

	// The text of the last HTTP header sent by any of this class's methods. The
	// purpose of this property is to allow the developer to examine the exact HTTP
	// header for debugging purposes.
	void get_LastHeader(CkString &str);
	// The text of the last HTTP header sent by any of this class's methods. The
	// purpose of this property is to allow the developer to examine the exact HTTP
	// header for debugging purposes.
	const wchar_t *lastHeader(void);

	// The value of the Last-Modified header in the last HTTP response received by the
	// HTTP component.
	void get_LastModDate(CkString &str);
	// The value of the Last-Modified header in the last HTTP response received by the
	// HTTP component.
	const wchar_t *lastModDate(void);

	// The entire last response header for the last HTTP response received by the HTTP
	// component.
	void get_LastResponseHeader(CkString &str);
	// The entire last response header for the last HTTP response received by the HTTP
	// component.
	const wchar_t *lastResponseHeader(void);

	// The last HTTP status value received by the HTTP component. This only applies to
	// methods that do not return an HTTP response object. For methods that return an
	// HTTP response object, such as SynchronousRequest, the status code is found in
	// the StatusCode property of the response object.
	int get_LastStatus(void);

	// The HTTP login for pages requiring a login/password. Chilkat HTTP can do both
	// Basic and NTLM HTTP authentication. (NTLM is also known as SPA (or Windows
	// Integrated Authentication). To use NTLM, set the NtlmAuth property = true.
	void get_Login(CkString &str);
	// The HTTP login for pages requiring a login/password. Chilkat HTTP can do both
	// Basic and NTLM HTTP authentication. (NTLM is also known as SPA (or Windows
	// Integrated Authentication). To use NTLM, set the NtlmAuth property = true.
	const wchar_t *login(void);
	// The HTTP login for pages requiring a login/password. Chilkat HTTP can do both
	// Basic and NTLM HTTP authentication. (NTLM is also known as SPA (or Windows
	// Integrated Authentication). To use NTLM, set the NtlmAuth property = true.
	void put_Login(const wchar_t *newVal);

	// The optional domain name to be used with NTLM / Kerberos / Negotiate
	// authentication.
	void get_LoginDomain(CkString &str);
	// The optional domain name to be used with NTLM / Kerberos / Negotiate
	// authentication.
	const wchar_t *loginDomain(void);
	// The optional domain name to be used with NTLM / Kerberos / Negotiate
	// authentication.
	void put_LoginDomain(const wchar_t *newVal);

	// The maximum number of simultaneous open HTTP connections managed by the HTTP
	// component. The Chilkat HTTP component automatically manages HTTP connections. If
	// the number of open HTTP connections is about to be exceeded, the connection with
	// the least recent activity is automatically closed.
	int get_MaxConnections(void);
	// The maximum number of simultaneous open HTTP connections managed by the HTTP
	// component. The Chilkat HTTP component automatically manages HTTP connections. If
	// the number of open HTTP connections is about to be exceeded, the connection with
	// the least recent activity is automatically closed.
	void put_MaxConnections(int newVal);

	// Limits the amount of time a document can be kept "fresh" in the cache. The
	// MaxFreshPeriod is specified in minutes, and the default value is 525600 which is
	// equal to 1 year.
	int get_MaxFreshPeriod(void);
	// Limits the amount of time a document can be kept "fresh" in the cache. The
	// MaxFreshPeriod is specified in minutes, and the default value is 525600 which is
	// equal to 1 year.
	void put_MaxFreshPeriod(int newVal);

	// The maximum HTTP response size to be accepted by the calling program. A value of
	// 0 (the default) indicates that there is no maximum value.
	unsigned long get_MaxResponseSize(void);
	// The maximum HTTP response size to be accepted by the calling program. A value of
	// 0 (the default) indicates that there is no maximum value.
	void put_MaxResponseSize(unsigned long newVal);

	// The Http class will automatically fail any URL longer than this length. The
	// default MaxUrlLen is 800 characters.
	int get_MaxUrlLen(void);
	// The Http class will automatically fail any URL longer than this length. The
	// default MaxUrlLen is 800 characters.
	void put_MaxUrlLen(int newVal);

	// If set to true, then the appropriate headers to mimic Mozilla/FireFox are
	// automatically added to requests sent via the QuickGet and QuickGetStr methods.
	bool get_MimicFireFox(void);
	// If set to true, then the appropriate headers to mimic Mozilla/FireFox are
	// automatically added to requests sent via the QuickGet and QuickGetStr methods.
	void put_MimicFireFox(bool newVal);

	// If set to true, then the appropriate headers to mimic Internet Explorer are
	// automatically added to requests sent via the QuickGet and QuickGetStr methods.
	bool get_MimicIE(void);
	// If set to true, then the appropriate headers to mimic Internet Explorer are
	// automatically added to requests sent via the QuickGet and QuickGetStr methods.
	void put_MimicIE(bool newVal);

	// The freshness period for a document in cache will not be less than this value
	// (in minutes). The default value is 30.
	int get_MinFreshPeriod(void);
	// The freshness period for a document in cache will not be less than this value
	// (in minutes). The default value is 30.
	void put_MinFreshPeriod(int newVal);

	// Set this property equal to true for Negotiate authentication. Negotiate
	// authentication will dynamically select Kerberos or NTLM authentication depending
	// on what the server requires.
	// 
	// Note: The NegotiateAuth property is only available for the Microsoft Windows
	// operating system.
	// 
	bool get_NegotiateAuth(void);
	// Set this property equal to true for Negotiate authentication. Negotiate
	// authentication will dynamically select Kerberos or NTLM authentication depending
	// on what the server requires.
	// 
	// Note: The NegotiateAuth property is only available for the Microsoft Windows
	// operating system.
	// 
	void put_NegotiateAuth(bool newVal);

	// Setting this property to true causes the HTTP component to use NTLM
	// authentication (also known as IWA -- or Integrated Windows Authentication) when
	// authentication with an HTTP server. The default value is false.
	bool get_NtlmAuth(void);
	// Setting this property to true causes the HTTP component to use NTLM
	// authentication (also known as IWA -- or Integrated Windows Authentication) when
	// authentication with an HTTP server. The default value is false.
	void put_NtlmAuth(bool newVal);

	// The number of directory levels to be used under each cache root. The default is
	// 0, meaning that each cached HTML page is stored in a cache root directory. A
	// value of 1 causes each cached page to be stored in one of 255 subdirectories
	// named "0","1", "2", ..."255" under a cache root. A value of 2 causes two levels
	// of subdirectories ("0..255/0..255") under each cache root. The HTTP control
	// automatically creates subdirectories as needed. The reason for mutliple levels
	// is to alleviate problems that may arise with unrelated software when huge
	// numbers of files are stored in a single directory. For example, Windows Explorer
	// does not behave well when trying to display the contents of directories with
	// thousands of files.
	int get_NumCacheLevels(void);
	// The number of directory levels to be used under each cache root. The default is
	// 0, meaning that each cached HTML page is stored in a cache root directory. A
	// value of 1 causes each cached page to be stored in one of 255 subdirectories
	// named "0","1", "2", ..."255" under a cache root. A value of 2 causes two levels
	// of subdirectories ("0..255/0..255") under each cache root. The HTTP control
	// automatically creates subdirectories as needed. The reason for mutliple levels
	// is to alleviate problems that may arise with unrelated software when huge
	// numbers of files are stored in a single directory. For example, Windows Explorer
	// does not behave well when trying to display the contents of directories with
	// thousands of files.
	void put_NumCacheLevels(int newVal);

	// The number of cache roots to be used for the HTTP cache. This allows the disk
	// cache spread out over multiple disk drives. Each cache root is a string
	// indicating the drive letter and directory path. For example, "E:\Cache". An
	// example of a very large low-cost cache might be four USB external drives. To
	// create a cache with four roots, call AddCacheRoot once for each directory root.
	int get_NumCacheRoots(void);

	// If true then causes an OAuth Authorization header to be added to any request
	// sent by the HTTP object. For example:
	// Authorization: OAuth realm="http://sp.example.com/",
	//                 oauth_consumer_key="0685bd9184jfhq22",
	//                 oauth_token="ad180jjd733klru7",
	//                 oauth_signature_method="HMAC-SHA1",
	//                 oauth_signature="wOJIO9A2W5mFwDgiDvZbTSMK%2FPY%3D",
	//                 oauth_timestamp="137131200",
	//                 oauth_nonce="4572616e48616d6d65724c61686176",
	//                 oauth_version="1.0"
	// The information used to compute the OAuth Authorization header is obtained from
	// the other OAuth* properties, such as OAuthConsumerKey, OAuthConsumerSecret,
	// OAuthRealm, etc.
	bool get_OAuth1(void);
	// If true then causes an OAuth Authorization header to be added to any request
	// sent by the HTTP object. For example:
	// Authorization: OAuth realm="http://sp.example.com/",
	//                 oauth_consumer_key="0685bd9184jfhq22",
	//                 oauth_token="ad180jjd733klru7",
	//                 oauth_signature_method="HMAC-SHA1",
	//                 oauth_signature="wOJIO9A2W5mFwDgiDvZbTSMK%2FPY%3D",
	//                 oauth_timestamp="137131200",
	//                 oauth_nonce="4572616e48616d6d65724c61686176",
	//                 oauth_version="1.0"
	// The information used to compute the OAuth Authorization header is obtained from
	// the other OAuth* properties, such as OAuthConsumerKey, OAuthConsumerSecret,
	// OAuthRealm, etc.
	void put_OAuth1(bool newVal);

	// The OAuth consumer key to be used in the Authorization header.
	void get_OAuthConsumerKey(CkString &str);
	// The OAuth consumer key to be used in the Authorization header.
	const wchar_t *oAuthConsumerKey(void);
	// The OAuth consumer key to be used in the Authorization header.
	void put_OAuthConsumerKey(const wchar_t *newVal);

	// The OAuth consumer secret to be used in computing the contents of the
	// Authorization header.
	void get_OAuthConsumerSecret(CkString &str);
	// The OAuth consumer secret to be used in computing the contents of the
	// Authorization header.
	const wchar_t *oAuthConsumerSecret(void);
	// The OAuth consumer secret to be used in computing the contents of the
	// Authorization header.
	void put_OAuthConsumerSecret(const wchar_t *newVal);

	// The OAuth realm to be used in the Authorization header.
	void get_OAuthRealm(CkString &str);
	// The OAuth realm to be used in the Authorization header.
	const wchar_t *oAuthRealm(void);
	// The OAuth realm to be used in the Authorization header.
	void put_OAuthRealm(const wchar_t *newVal);

	// The OAuth signature method, such as "HMAC-SHA1" to be used in the Authorization
	// header. The default is "HMAC-SHA1". It is also possible to choose "RSA-SHA1", in
	// which case the RSA private key would need to be provided via the SetOAuthRsaKey
	// method.
	void get_OAuthSigMethod(CkString &str);
	// The OAuth signature method, such as "HMAC-SHA1" to be used in the Authorization
	// header. The default is "HMAC-SHA1". It is also possible to choose "RSA-SHA1", in
	// which case the RSA private key would need to be provided via the SetOAuthRsaKey
	// method.
	const wchar_t *oAuthSigMethod(void);
	// The OAuth signature method, such as "HMAC-SHA1" to be used in the Authorization
	// header. The default is "HMAC-SHA1". It is also possible to choose "RSA-SHA1", in
	// which case the RSA private key would need to be provided via the SetOAuthRsaKey
	// method.
	void put_OAuthSigMethod(const wchar_t *newVal);

	// The OAuth token to be used in the Authorization header.
	void get_OAuthToken(CkString &str);
	// The OAuth token to be used in the Authorization header.
	const wchar_t *oAuthToken(void);
	// The OAuth token to be used in the Authorization header.
	void put_OAuthToken(const wchar_t *newVal);

	// The OAuth token secret to be used in computing the Authorization header.
	void get_OAuthTokenSecret(CkString &str);
	// The OAuth token secret to be used in computing the Authorization header.
	const wchar_t *oAuthTokenSecret(void);
	// The OAuth token secret to be used in computing the Authorization header.
	void put_OAuthTokenSecret(const wchar_t *newVal);

	// The OAuth verifier to be used in the Authorization header.
	void get_OAuthVerifier(CkString &str);
	// The OAuth verifier to be used in the Authorization header.
	const wchar_t *oAuthVerifier(void);
	// The OAuth verifier to be used in the Authorization header.
	void put_OAuthVerifier(const wchar_t *newVal);

	// The HTTP password for pages requiring a login/password. Chilkat HTTP can do both
	// Basic and NTLM HTTP authentication. (NTLM is also known as SPA (or Windows
	// Integrated Authentication). To use NTLM, set the NtlmAuth property = true.
	void get_Password(CkString &str);
	// The HTTP password for pages requiring a login/password. Chilkat HTTP can do both
	// Basic and NTLM HTTP authentication. (NTLM is also known as SPA (or Windows
	// Integrated Authentication). To use NTLM, set the NtlmAuth property = true.
	const wchar_t *password(void);
	// The HTTP password for pages requiring a login/password. Chilkat HTTP can do both
	// Basic and NTLM HTTP authentication. (NTLM is also known as SPA (or Windows
	// Integrated Authentication). To use NTLM, set the NtlmAuth property = true.
	void put_Password(const wchar_t *newVal);

	// Set this to "basic" if you know in advance that Basic authentication is to be
	// used for the HTTP proxy. Otherwise leave this property unset. Note: It is not
	// necessary to set this property. The HTTP component will automatically handle
	// proxy authentication for any of the supported authentication methods: NTLM,
	// Digest, or Basic. Setting this property equal to "basic" prevents the 407
	// response which is automatically handled internal to Chilkat and never seen by
	// your application.
	void get_ProxyAuthMethod(CkString &str);
	// Set this to "basic" if you know in advance that Basic authentication is to be
	// used for the HTTP proxy. Otherwise leave this property unset. Note: It is not
	// necessary to set this property. The HTTP component will automatically handle
	// proxy authentication for any of the supported authentication methods: NTLM,
	// Digest, or Basic. Setting this property equal to "basic" prevents the 407
	// response which is automatically handled internal to Chilkat and never seen by
	// your application.
	const wchar_t *proxyAuthMethod(void);
	// Set this to "basic" if you know in advance that Basic authentication is to be
	// used for the HTTP proxy. Otherwise leave this property unset. Note: It is not
	// necessary to set this property. The HTTP component will automatically handle
	// proxy authentication for any of the supported authentication methods: NTLM,
	// Digest, or Basic. Setting this property equal to "basic" prevents the 407
	// response which is automatically handled internal to Chilkat and never seen by
	// your application.
	void put_ProxyAuthMethod(const wchar_t *newVal);

	// The domain name of a proxy host if an HTTP proxy is used.
	void get_ProxyDomain(CkString &str);
	// The domain name of a proxy host if an HTTP proxy is used.
	const wchar_t *proxyDomain(void);
	// The domain name of a proxy host if an HTTP proxy is used.
	void put_ProxyDomain(const wchar_t *newVal);

	// If an HTTP proxy is used and it requires authentication, this property specifies
	// the HTTP proxy login.
	void get_ProxyLogin(CkString &str);
	// If an HTTP proxy is used and it requires authentication, this property specifies
	// the HTTP proxy login.
	const wchar_t *proxyLogin(void);
	// If an HTTP proxy is used and it requires authentication, this property specifies
	// the HTTP proxy login.
	void put_ProxyLogin(const wchar_t *newVal);

	// The NTLM authentication domain (optional) if NTLM authentication is used.
	void get_ProxyLoginDomain(CkString &str);
	// The NTLM authentication domain (optional) if NTLM authentication is used.
	const wchar_t *proxyLoginDomain(void);
	// The NTLM authentication domain (optional) if NTLM authentication is used.
	void put_ProxyLoginDomain(const wchar_t *newVal);

	// If an HTTP proxy is used and it requires authentication, this property specifies
	// the HTTP proxy password.
	void get_ProxyPassword(CkString &str);
	// If an HTTP proxy is used and it requires authentication, this property specifies
	// the HTTP proxy password.
	const wchar_t *proxyPassword(void);
	// If an HTTP proxy is used and it requires authentication, this property specifies
	// the HTTP proxy password.
	void put_ProxyPassword(const wchar_t *newVal);

	// The port number of a proxy server if an HTTP proxy is used.
	int get_ProxyPort(void);
	// The port number of a proxy server if an HTTP proxy is used.
	void put_ProxyPort(int newVal);

	// The amount of time in seconds to wait before timing out when reading from an
	// HTTP server. The ReadTimeout is the amount of time that needs to elapse while no
	// additional data is forthcoming. During a long download, if the data stream halts
	// for more than this amount, it will timeout. Otherwise, there is no limit on the
	// length of time for the entire download.
	int get_ReadTimeout(void);
	// The amount of time in seconds to wait before timing out when reading from an
	// HTTP server. The ReadTimeout is the amount of time that needs to elapse while no
	// additional data is forthcoming. During a long download, if the data stream halts
	// for more than this amount, it will timeout. Otherwise, there is no limit on the
	// length of time for the entire download.
	void put_ReadTimeout(int newVal);

	// Indicates the HTTP verb, such as GET, POST, PUT, etc. to be used for a redirect
	// when the FollowRedirects property is set to true. The default value is an
	// empty string, indicating that the same verb as the original HTTP request should
	// be used.
	void get_RedirectVerb(CkString &str);
	// Indicates the HTTP verb, such as GET, POST, PUT, etc. to be used for a redirect
	// when the FollowRedirects property is set to true. The default value is an
	// empty string, indicating that the same verb as the original HTTP request should
	// be used.
	const wchar_t *redirectVerb(void);
	// Indicates the HTTP verb, such as GET, POST, PUT, etc. to be used for a redirect
	// when the FollowRedirects property is set to true. The default value is an
	// empty string, indicating that the same verb as the original HTTP request should
	// be used.
	void put_RedirectVerb(const wchar_t *newVal);

	// The Referer header field to be automatically included with GET requests issued
	// by QuickGet or QuickGetStr. The default value is the empty string which causes
	// the Referer field to be omitted from the request header.
	void get_Referer(CkString &str);
	// The Referer header field to be automatically included with GET requests issued
	// by QuickGet or QuickGetStr. The default value is the empty string which causes
	// the Referer field to be omitted from the request header.
	const wchar_t *referer(void);
	// The Referer header field to be automatically included with GET requests issued
	// by QuickGet or QuickGetStr. The default value is the empty string which causes
	// the Referer field to be omitted from the request header.
	void put_Referer(const wchar_t *newVal);

	// If set, then any HTTP response to any POST or GET, including downloads, will be
	// rejected if the content-type in the response header does not match this setting.
	// If the content-type does not match, only the header of the HTTP response is
	// read, the connection to the HTTP server is closed, and the remainder of the
	// response is never read.
	// 
	// This property is empty (zero-length string) by default.
	// 
	// Some typical content-types are "text/html", "text/xml", "image/gif",
	// "image/jpeg", "application/zip", "application/msword", "application/pdf", etc.
	// 
	void get_RequiredContentType(CkString &str);
	// If set, then any HTTP response to any POST or GET, including downloads, will be
	// rejected if the content-type in the response header does not match this setting.
	// If the content-type does not match, only the header of the HTTP response is
	// read, the connection to the HTTP server is closed, and the remainder of the
	// response is never read.
	// 
	// This property is empty (zero-length string) by default.
	// 
	// Some typical content-types are "text/html", "text/xml", "image/gif",
	// "image/jpeg", "application/zip", "application/msword", "application/pdf", etc.
	// 
	const wchar_t *requiredContentType(void);
	// If set, then any HTTP response to any POST or GET, including downloads, will be
	// rejected if the content-type in the response header does not match this setting.
	// If the content-type does not match, only the header of the HTTP response is
	// read, the connection to the HTTP server is closed, and the remainder of the
	// response is never read.
	// 
	// This property is empty (zero-length string) by default.
	// 
	// Some typical content-types are "text/html", "text/xml", "image/gif",
	// "image/jpeg", "application/zip", "application/msword", "application/pdf", etc.
	// 
	void put_RequiredContentType(const wchar_t *newVal);

	// If true, then all S3_* methods will use a secure SSL/TLS connection for
	// communications. The default value is false.
	bool get_S3Ssl(void);
	// If true, then all S3_* methods will use a secure SSL/TLS connection for
	// communications. The default value is false.
	void put_S3Ssl(bool newVal);

	// If this property is true, cookies are automatically persisted to XML files in
	// the directory specified by the CookiesDir property (or in memory if CookieDir =
	// "memory"). Both CookiesDir and SaveCookies must be set for cookies to be
	// persisted.
	bool get_SaveCookies(void);
	// If this property is true, cookies are automatically persisted to XML files in
	// the directory specified by the CookiesDir property (or in memory if CookieDir =
	// "memory"). Both CookiesDir and SaveCookies must be set for cookies to be
	// persisted.
	void put_SaveCookies(bool newVal);

	// The buffer size to be used with the underlying TCP/IP socket for sending. The
	// default value is 65535.
	int get_SendBufferSize(void);
	// The buffer size to be used with the underlying TCP/IP socket for sending. The
	// default value is 65535.
	void put_SendBufferSize(int newVal);

	// If true, then cookies previously persisted to the CookiesDir are automatically
	// added to all HTTP requests. Only cookies matching the domain and path are added.
	bool get_SendCookies(void);
	// If true, then cookies previously persisted to the CookiesDir are automatically
	// added to all HTTP requests. Only cookies matching the domain and path are added.
	void put_SendCookies(bool newVal);

	// Enables file-based session logging. If set to a filename (or relative/absolute
	// filepath), then the exact HTTP requests and responses are logged to a file. The
	// file is created if it does not already exist, otherwise it is appended.
	void get_SessionLogFilename(CkString &str);
	// Enables file-based session logging. If set to a filename (or relative/absolute
	// filepath), then the exact HTTP requests and responses are logged to a file. The
	// file is created if it does not already exist, otherwise it is appended.
	const wchar_t *sessionLogFilename(void);
	// Enables file-based session logging. If set to a filename (or relative/absolute
	// filepath), then the exact HTTP requests and responses are logged to a file. The
	// file is created if it does not already exist, otherwise it is appended.
	void put_SessionLogFilename(const wchar_t *newVal);

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
	// Note: This property only applies to FTP data connections. The FTP control
	// connection is not used for uploading or downloading files, and is therefore not
	// performance sensitive.
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
	// Note: This property only applies to FTP data connections. The FTP control
	// connection is not used for uploading or downloading files, and is therefore not
	// performance sensitive.
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

	// Selects the secure protocol to be used for secure (SSL) connections. Possible
	// values are:
	// 
	//     default
	//     TLS 1.0
	//     SSL 3.0
	//     SSL 2.0
	//     PCT 1.0
	//     
	// 
	// The default value is "default", which allows for the protocol to be selected
	// dynamically at runtime based on the requirements of the server.
	void get_SslProtocol(CkString &str);
	// Selects the secure protocol to be used for secure (SSL) connections. Possible
	// values are:
	// 
	//     default
	//     TLS 1.0
	//     SSL 3.0
	//     SSL 2.0
	//     PCT 1.0
	//     
	// 
	// The default value is "default", which allows for the protocol to be selected
	// dynamically at runtime based on the requirements of the server.
	const wchar_t *sslProtocol(void);
	// Selects the secure protocol to be used for secure (SSL) connections. Possible
	// values are:
	// 
	//     default
	//     TLS 1.0
	//     SSL 3.0
	//     SSL 2.0
	//     PCT 1.0
	//     
	// 
	// The default value is "default", which allows for the protocol to be selected
	// dynamically at runtime based on the requirements of the server.
	void put_SslProtocol(const wchar_t *newVal);

	// Controls whether the cache is automatically updated with the responses from HTTP
	// GET requests.
	bool get_UpdateCache(void);
	// Controls whether the cache is automatically updated with the responses from HTTP
	// GET requests.
	void put_UpdateCache(bool newVal);

	// If true, then background-enabled methods will run in a background thread.
	// Normally, a method will return after its work is completed. However, when
	// UseBgThread is true, the method will return immediately and a background thread
	// is started to carry out the method"™s task.
	// 
	// Background-enabled HTTP methods are:
	//     Download
	//     DownloadAppend
	//     GetHead
	//     PostBinary
	//     PostMime
	//     PostUrlEncoded
	//     PostXml
	//     PutText
	//     QuickDeleteStr
	//     QuickGet
	//     QuickGetObj
	//     QuickGetStr
	//     QuickPutStr
	//     ResumeDownload
	//     SynchronousRequest
	//     XmlRpc
	//     XmlRpcPut
	// 
	bool get_UseBgThread(void);
	// If true, then background-enabled methods will run in a background thread.
	// Normally, a method will return after its work is completed. However, when
	// UseBgThread is true, the method will return immediately and a background thread
	// is started to carry out the method"™s task.
	// 
	// Background-enabled HTTP methods are:
	//     Download
	//     DownloadAppend
	//     GetHead
	//     PostBinary
	//     PostMime
	//     PostUrlEncoded
	//     PostXml
	//     PutText
	//     QuickDeleteStr
	//     QuickGet
	//     QuickGetObj
	//     QuickGetStr
	//     QuickPutStr
	//     ResumeDownload
	//     SynchronousRequest
	//     XmlRpc
	//     XmlRpcPut
	// 
	void put_UseBgThread(bool newVal);

	// If true, the proxy host/port used by Internet Explorer will also be used by
	// Chilkat HTTP.
	bool get_UseIEProxy(void);
	// If true, the proxy host/port used by Internet Explorer will also be used by
	// Chilkat HTTP.
	void put_UseIEProxy(bool newVal);

	// The UserAgent header field to be automatically included with GET requests issued
	// by QuickGet or QuickGetStr. The default value is "Chilkat/1.0.0
	// (+http://www.chilkatsoft.com/ChilkatHttpUA.asp)" which indicates that the
	// software used to issue the HTTP request was the Chilkat HTTP component.
	void get_UserAgent(CkString &str);
	// The UserAgent header field to be automatically included with GET requests issued
	// by QuickGet or QuickGetStr. The default value is "Chilkat/1.0.0
	// (+http://www.chilkatsoft.com/ChilkatHttpUA.asp)" which indicates that the
	// software used to issue the HTTP request was the Chilkat HTTP component.
	const wchar_t *userAgent(void);
	// The UserAgent header field to be automatically included with GET requests issued
	// by QuickGet or QuickGetStr. The default value is "Chilkat/1.0.0
	// (+http://www.chilkatsoft.com/ChilkatHttpUA.asp)" which indicates that the
	// software used to issue the HTTP request was the Chilkat HTTP component.
	void put_UserAgent(const wchar_t *newVal);

	// Indicates whether the last HTTP GET was redirected.
	bool get_WasRedirected(void);

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
	// This method must be called at least once if disk caching is to be used. The file
	// path (including drive letter) such as "E:\MyHttpCache\" is passed to
	// AddCacheRoot to specify the root directory. The cache can be spread across
	// multiple disk drives by calling AddCacheRoot multiple times, each with a
	// directory path on a separate disk drive.
	void AddCacheRoot(const wchar_t *dir);

	// Adds a custom header field to any HTTP request sent by a method that does not
	// use the HTTP request object. These methods include Download, DownloadAppend,
	// GetHead, PostBinary, PostMime, PostXml, PutBinary, PutText, QuickDeleteStr,
	// QuickGet, QuickGetObj, QuickGetStr, QuickPutStr, XmlRpc, and XmlRpcPut.
	// 
	// Cookies may be explictly added by calling this method passing "Cookie" for the
	// headerFieldName.
	// 
	// The RemoveQuickHeader method can be called to remove a custom header.
	// 
	// * Note: This method is deprecated. It is identical to theSetRequestHeader
	// method. The SetRequestHeader method should be called instead because
	// AddQuickHeader will be removed in a future version.
	// 
	bool AddQuickHeader(const wchar_t *name, const wchar_t *value);

	// If a backgrounded method returns an Http response object, it may be retrieved by
	// calling this method.
	// The caller is responsible for deleting the object returned by this method.
	CkHttpResponseW *BgResponseObject(void);

	// Call this to force the currently running backgrounded method to abort.
	void BgTaskAbort(void);

	// Clears the in-memory event log (which is enabled by setting the KeepEventLog
	// property = true).
	void ClearBgEventLog(void);

	// Clears all cookies cached in memory. Calling this only makes sense if the
	// CookieDir property is set to the string "memory".
	void ClearInMemoryCookies(void);

	// Closes all connections still open from previous HTTP requests.
	// 
	// An HTTP object instance will maintain up to 10 connections. If the HTTP server's
	// response does not include a "Connection: Close" header, the connection will
	// remain open and will be re-used if possible for the next HTTP request to the
	// same hostname:port. (It uses the IP address (in string form) or the domain name,
	// whichever is used in the URL provided by the application.) If 10 connections are
	// already open and another is needed, the object will close the least recently
	// used connection.
	// 
	bool CloseAllConnections(void);

	// Retrieves the content at a URL and saves to a file. All content is saved in
	// streaming mode such that the memory footprint is small and steady. HTTPS is
	// fully supported, as it is with all the methods of this class.
	bool Download(const wchar_t *url, const wchar_t *filename);

	// Same as the Download method, but the output file is open for append.
	bool DownloadAppend(const wchar_t *url, const wchar_t *filename);

	// Retrieves the content at a URL and computes and returns a hash of the content.
	// The hash is returned as an encoded string according to the  encoding, which may be
	// "Base64", "modBase64", "Base32", "UU", "QP" (for quoted-printable), "URL" (for
	// url-encoding), "Hex", "Q", "B", "url_oath", "url_rfc1738", "url_rfc2396", and
	// "url_rfc3986". The  hashAlgorithm may be "sha1", "sha256", "sha384", "sha512", "md2",
	// "md5", "haval", "ripemd128", "ripemd160","ripemd256", or "ripemd320".
	bool DownloadHash(const wchar_t *url, const wchar_t *hashAlgorithm, const wchar_t *encoding, CkString &outStr);
	// Retrieves the content at a URL and computes and returns a hash of the content.
	// The hash is returned as an encoded string according to the  encoding, which may be
	// "Base64", "modBase64", "Base32", "UU", "QP" (for quoted-printable), "URL" (for
	// url-encoding), "Hex", "Q", "B", "url_oath", "url_rfc1738", "url_rfc2396", and
	// "url_rfc3986". The  hashAlgorithm may be "sha1", "sha256", "sha384", "sha512", "md2",
	// "md5", "haval", "ripemd128", "ripemd160","ripemd256", or "ripemd320".
	const wchar_t *downloadHash(const wchar_t *url, const wchar_t *hashAlgorithm, const wchar_t *encoding);

	// Returns the name of the Nth event in the in-memory event log. Refer to the
	// documentation for the KeepEventLog property for the full list of event names.
	// Indexing is from 0 to EventLogCount-1.
	bool EventLogName(int index, CkString &outStr);
	// Returns the name of the Nth event in the in-memory event log. Refer to the
	// documentation for the KeepEventLog property for the full list of event names.
	// Indexing is from 0 to EventLogCount-1.
	const wchar_t *eventLogName(int index);

	// Returns the value of the Nth event in the in-memory event log. Indexing is from
	// 0 to EventLogCount-1.
	bool EventLogValue(int index, CkString &outStr);
	// Returns the value of the Nth event in the in-memory event log. Indexing is from
	// 0 to EventLogCount-1.
	const wchar_t *eventLogValue(int index);

	// Convenience method for extracting the META refresh URL from HTML. For example,
	// if the htmlContent contains a META refresh tag, such as:
	// <meta http-equiv="refresh" content="5;URL='http://example.com/'">
	// Then the return value of this method would be "http://example.com/".
	bool ExtractMetaRefreshUrl(const wchar_t *html, CkString &outStr);
	// Convenience method for extracting the META refresh URL from HTML. For example,
	// if the htmlContent contains a META refresh tag, such as:
	// <meta http-equiv="refresh" content="5;URL='http://example.com/'">
	// Then the return value of this method would be "http://example.com/".
	const wchar_t *extractMetaRefreshUrl(const wchar_t *html);

	// Returns the current GMT (also known as UTC) date/time in a string that is
	// compliant with RFC 2616 format.
	bool GenTimeStamp(CkString &outStr);
	// Returns the current GMT (also known as UTC) date/time in a string that is
	// compliant with RFC 2616 format.
	const wchar_t *genTimeStamp(void);

	// Returns the Nth cache root (indexing begins at 0). Cache roots are set by
	// calling AddCacheRoot one or more times.
	bool GetCacheRoot(int index, CkString &outStr);
	// Returns the Nth cache root (indexing begins at 0). Cache roots are set by
	// calling AddCacheRoot one or more times.
	const wchar_t *getCacheRoot(int index);
	// Returns the Nth cache root (indexing begins at 0). Cache roots are set by
	// calling AddCacheRoot one or more times.
	const wchar_t *cacheRoot(int index);

	// Returns the cookies in XML format for a specific domain. Cookies are only
	// persisted if the SaveCookies property is set to true. If the CookieDir
	// property is set to the keyword "memory", then cookies are saved in-memory.
	bool GetCookieXml(const wchar_t *domain, CkString &outStr);
	// Returns the cookies in XML format for a specific domain. Cookies are only
	// persisted if the SaveCookies property is set to true. If the CookieDir
	// property is set to the keyword "memory", then cookies are saved in-memory.
	const wchar_t *getCookieXml(const wchar_t *domain);
	// Returns the cookies in XML format for a specific domain. Cookies are only
	// persisted if the SaveCookies property is set to true. If the CookieDir
	// property is set to the keyword "memory", then cookies are saved in-memory.
	const wchar_t *cookieXml(const wchar_t *domain);

	// Utility method for extracting the domain name from a full URL. For example, if
	// "http://www.chilkatsoft.com/default.asp" is the URL passed in, then
	// "www.chilkatsoft.com" is returned.
	bool GetDomain(const wchar_t *url, CkString &outStr);
	// Utility method for extracting the domain name from a full URL. For example, if
	// "http://www.chilkatsoft.com/default.asp" is the URL passed in, then
	// "www.chilkatsoft.com" is returned.
	const wchar_t *getDomain(const wchar_t *url);
	// Utility method for extracting the domain name from a full URL. For example, if
	// "http://www.chilkatsoft.com/default.asp" is the URL passed in, then
	// "www.chilkatsoft.com" is returned.
	const wchar_t *domain(const wchar_t *url);

	// Sends an HTTP HEAD request for a URL and returns a response object.
	// The caller is responsible for deleting the object returned by this method.
	CkHttpResponseW *GetHead(const wchar_t *url);

	// Returns the value of a header field that has been pre-defined to be sent with
	// all HTTP GET requests issued by the QuickGet and QuickGetStr methods. By
	// default, this includes header fields such as Accept, AcceptCharset,
	// AcceptLanguage, Connection, UserAgent, etc.
	bool GetRequestHeader(const wchar_t *name, CkString &outStr);
	// Returns the value of a header field that has been pre-defined to be sent with
	// all HTTP GET requests issued by the QuickGet and QuickGetStr methods. By
	// default, this includes header fields such as Accept, AcceptCharset,
	// AcceptLanguage, Connection, UserAgent, etc.
	const wchar_t *getRequestHeader(const wchar_t *name);
	// Returns the value of a header field that has been pre-defined to be sent with
	// all HTTP GET requests issued by the QuickGet and QuickGetStr methods. By
	// default, this includes header fields such as Accept, AcceptCharset,
	// AcceptLanguage, Connection, UserAgent, etc.
	const wchar_t *requestHeader(const wchar_t *name);

	// Establishes an SSL/TLS connection with a web server for the purpose of
	// retrieving the server's SSL certificate (public-key only of course...). Nothing
	// is retrieved from the web server. This method simply makes a connection, gets
	// the certificate information, and closes the connection.
	// The caller is responsible for deleting the object returned by this method.
	CkCertW *GetServerSslCert(const wchar_t *domain, int port);

	// Returns the path part of a URL. The syntax of a URL is :// : @
	// 
	// : / ; ? # . This method returns the "path" part.
	// 
	bool GetUrlPath(const wchar_t *url, CkString &outStr);
	// Returns the path part of a URL. The syntax of a URL is :// : @
	// 
	// : / ; ? # . This method returns the "path" part.
	// 
	const wchar_t *getUrlPath(const wchar_t *url);
	// Returns the path part of a URL. The syntax of a URL is :// : @
	// 
	// : / ; ? # . This method returns the "path" part.
	// 
	const wchar_t *urlPath(const wchar_t *url);

	// Returns true if the specified header field is defined such that it will be sent
	// with all GET requests issued by the QuickGet and QuickGetStr methods.
	bool HasRequestHeader(const wchar_t *name);

	// Returns true if the Http class has been unlocked. It is necessary to call
	// Http.UnlockComponent before calling any other methods. Passing any string to
	// UnlockComponent will automatically activate a 30-day trial period.
	bool IsUnlocked(void);

	// Sends an HTTP POST request to the url. The body of the HTTP request contains
	// the bytes passed in  byteData. The  contentType is a content type such as "image/gif",
	// "application/pdf", etc. If  md5 is true, then a Content-MD5 header is added
	// with the base64 MD5 hash of the  byteData. Servers aware of the Content-MD5 header
	// will perform a message integrity check to ensure that the data has not been
	// corrupted. If  gzip is true, the  byteData is compressed using the gzip algorithm.
	// The HTTP request body will contain the GZIP compressed data, and a
	// "Content-Encoding: gzip" header is automatically added to indicate that the
	// request data needs to be ungzipped when received (at the server).
	bool PostBinary(const wchar_t *url, const CkByteData &byteData, const wchar_t *contentType, bool md5, bool gzip, CkString &outStr);
	// Sends an HTTP POST request to the url. The body of the HTTP request contains
	// the bytes passed in  byteData. The  contentType is a content type such as "image/gif",
	// "application/pdf", etc. If  md5 is true, then a Content-MD5 header is added
	// with the base64 MD5 hash of the  byteData. Servers aware of the Content-MD5 header
	// will perform a message integrity check to ensure that the data has not been
	// corrupted. If  gzip is true, the  byteData is compressed using the gzip algorithm.
	// The HTTP request body will contain the GZIP compressed data, and a
	// "Content-Encoding: gzip" header is automatically added to indicate that the
	// request data needs to be ungzipped when received (at the server).
	const wchar_t *postBinary(const wchar_t *url, const CkByteData &byteData, const wchar_t *contentType, bool md5, bool gzip);

	// A simplified way of sending a JSON POST and receiving the JSON response. The
	// HTTP response is returned in an HTTP response object. The content type of the
	// HTTP request is "applicatoin/jsonrequest". To send a JSON POST using
	// "application/json", call the PostJson2 method where the content type can be
	// explicitly provided.
	// The caller is responsible for deleting the object returned by this method.
	CkHttpResponseW *PostJson(const wchar_t *url, const wchar_t *jsonText);

	// The same as PostJson,except it allows for the content type to be explicitly
	// provided. The PostJson method automatically uses "application/jsonrequest". If
	// the application needs for the content type to be "application/json", or some
	// other content type, then PostJson2 provides the means.
	// The caller is responsible for deleting the object returned by this method.
	CkHttpResponseW *PostJson2(const wchar_t *url, const wchar_t *contentType, const wchar_t *jsonText);

	// Allows the calling application to specify the exact content to be sent in a
	// POST. The HTTP POST is sent and the response is returned as an HTTP response
	// object.
	// The caller is responsible for deleting the object returned by this method.
	CkHttpResponseW *PostMime(const wchar_t *url, const wchar_t *mime);

	// Sends a simple URL encoded POST. The form parameters are sent in the body of the
	// HTTP request in x-www-form-urlencoded format. The content-type is
	// "application/x-www-form-urlencoded".
	// The caller is responsible for deleting the object returned by this method.
	CkHttpResponseW *PostUrlEncoded(const wchar_t *url, CkHttpRequestW &req);

	// A simplified way of posting XML content to a web server. This method is good for
	// making SOAP calls using HTTP POST. The  xmlCharset should match the character encoding
	// used in the  xmlContent, which is typically "utf-8". The HTTP response is returned in
	// an HTTP response object.
	// The caller is responsible for deleting the object returned by this method.
	CkHttpResponseW *PostXml(const wchar_t *url, const wchar_t *xmlDoc, const wchar_t *charset);

	// Sends an HTTP PUT request to the url. The body of the HTTP request is  byteData. The
	//  contentType is a content type such as "image/gif", "application/pdf", etc. If  md5 is
	// true, then a Content-MD5 header is added with the base64 MD5 hash of the  byteData.
	// Servers aware of the Content-MD5 header will perform a message integrity check
	// to ensure that the data has not been corrupted. If  gzip is true, the  byteData is
	// compressed using the gzip algorithm. The HTTP request body will contain the GZIP
	// compressed data, and a "Content-Encoding: gzip" header is automatically added to
	// indicate that the request data needs to be ungzipped when received (at the
	// server).
	bool PutBinary(const wchar_t *url, const CkByteData &byteData, const wchar_t *contentType, bool md5, bool gzip, CkString &outStr);
	// Sends an HTTP PUT request to the url. The body of the HTTP request is  byteData. The
	//  contentType is a content type such as "image/gif", "application/pdf", etc. If  md5 is
	// true, then a Content-MD5 header is added with the base64 MD5 hash of the  byteData.
	// Servers aware of the Content-MD5 header will perform a message integrity check
	// to ensure that the data has not been corrupted. If  gzip is true, the  byteData is
	// compressed using the gzip algorithm. The HTTP request body will contain the GZIP
	// compressed data, and a "Content-Encoding: gzip" header is automatically added to
	// indicate that the request data needs to be ungzipped when received (at the
	// server).
	const wchar_t *putBinary(const wchar_t *url, const CkByteData &byteData, const wchar_t *contentType, bool md5, bool gzip);

	// Sends an HTTP PUT request to the url. The body of the HTTP request is  textData. The
	//  charset should be set to a charset name such as "iso-8859-1", "windows-1252",
	// "Shift_JIS", "utf-8", etc. The string "ansi" may also be used as a charset name.
	// The  contentType is a content type such as "text/plain", "text/xml", etc. If  md5 is
	// true, then a Content-MD5 header is added with the base64 MD5 hash of the  textData.
	// Servers aware of the Content-MD5 header will perform a message integrity check
	// to ensure that the data has not been corrupted. If  gzip is true, the  textData is
	// compressed using the gzip algorithm. The HTTP request body will contain the GZIP
	// compressed data, and a "Content-Encoding: gzip" header is automatically added to
	// indicate that the request data needs to be ungzipped when received (at the
	// server).
	bool PutText(const wchar_t *url, const wchar_t *textData, const wchar_t *charset, const wchar_t *contentType, bool md5, bool gzip, CkString &outStr);
	// Sends an HTTP PUT request to the url. The body of the HTTP request is  textData. The
	//  charset should be set to a charset name such as "iso-8859-1", "windows-1252",
	// "Shift_JIS", "utf-8", etc. The string "ansi" may also be used as a charset name.
	// The  contentType is a content type such as "text/plain", "text/xml", etc. If  md5 is
	// true, then a Content-MD5 header is added with the base64 MD5 hash of the  textData.
	// Servers aware of the Content-MD5 header will perform a message integrity check
	// to ensure that the data has not been corrupted. If  gzip is true, the  textData is
	// compressed using the gzip algorithm. The HTTP request body will contain the GZIP
	// compressed data, and a "Content-Encoding: gzip" header is automatically added to
	// indicate that the request data needs to be ungzipped when received (at the
	// server).
	const wchar_t *putText(const wchar_t *url, const wchar_t *textData, const wchar_t *charset, const wchar_t *contentType, bool md5, bool gzip);

	// Same as QuickGetStr, but uses the HTTP DELETE method instead of the GET method.
	bool QuickDeleteStr(const wchar_t *url, CkString &outStr);
	// Same as QuickGetStr, but uses the HTTP DELETE method instead of the GET method.
	const wchar_t *quickDeleteStr(const wchar_t *url);

	// Sends an HTTP GET request for a URL and returns the response body as a byte
	// array. The URL may contain query parameters. If the SendCookies property is
	// true, matching cookies previously persisted to the CookiesDir are automatically
	// included in the request. If the FetchFromCache property is true, the page may be
	// fetched directly from cache. Because the URL can specify any type of resource
	// (HTML page, GIF image, etc.) the return value is a byte array. If the resource
	// is known to be a string, such as with an HTML page, you may call QuickGetStr
	// instead. If the HTTP request fails, a zero-length byte array is returned and
	// error information can be found in the LastErrorText, LastErrorXml, or
	// LastErrorHtml properties.
	bool QuickGet(const wchar_t *url, CkByteData &outData);

	// Sends an HTTP GET request for a URL and returns the response object. If the
	// SendCookies property is true, matching cookies previously persisted to the
	// CookiesDir are automatically included in the request. If the FetchFromCache
	// property is true, the page could be fetched directly from cache.
	// The caller is responsible for deleting the object returned by this method.
	CkHttpResponseW *QuickGetObj(const wchar_t *url);

	// Sends an HTTP GET request for a URL and returns the response body as a string.
	// The URL may contain query parameters. If the SendCookies property is true,
	// matching cookies previously persisted to the CookiesDir are automatically
	// included in the request. If the FetchFromCache property is true, the page
	// could be fetched directly from cache. If the HTTP request fails, a NULL value is
	// returned and error information can be found in the LastErrorText, LastErrorXml,
	// or LastErrorHtml properties.
	bool QuickGetStr(const wchar_t *url, CkString &outStr);
	// Sends an HTTP GET request for a URL and returns the response body as a string.
	// The URL may contain query parameters. If the SendCookies property is true,
	// matching cookies previously persisted to the CookiesDir are automatically
	// included in the request. If the FetchFromCache property is true, the page
	// could be fetched directly from cache. If the HTTP request fails, a NULL value is
	// returned and error information can be found in the LastErrorText, LastErrorXml,
	// or LastErrorHtml properties.
	const wchar_t *quickGetStr(const wchar_t *url);

	// Same as QuickGetStr, but uses the HTTP PUT method instead of the GET method.
	bool QuickPutStr(const wchar_t *url, CkString &outStr);
	// Same as QuickGetStr, but uses the HTTP PUT method instead of the GET method.
	const wchar_t *quickPutStr(const wchar_t *url);

	// Removes a header from the internal list of custom header field name/value pairs
	// to be automatically added when HTTP requests are sent via methods that do not
	// use the HTTP request object. (The AddQuickHeader method is called to add custom
	// header fields.)
	// 
	// * Note: This method is deprecated. It is identical to the RemoveRequestHeader
	// method. The RemoveRequestHeader method should be called instead because this
	// method will be removed in a future version.
	// 
	bool RemoveQuickHeader(const wchar_t *name);

	// Removes a header from the internal list of custom header field name/value pairs
	// to be automatically added when HTTP requests are sent via methods that do not
	// use the HTTP request object. (The SetRequestHeader method is called to add
	// custom header fields.)
	void RemoveRequestHeader(const wchar_t *name);

	// Same as QuickGet, but does not send the HTTP GET. Instead, it builds the HTTP
	// request that would've been sent and returns it.
	bool RenderGet(const wchar_t *url, CkString &outStr);
	// Same as QuickGet, but does not send the HTTP GET. Instead, it builds the HTTP
	// request that would've been sent and returns it.
	const wchar_t *renderGet(const wchar_t *url);

	// Same as the Download method, except a failed download may be resumed. The  targetFilename
	// is automatically checked and if it exists, the download will resume at the point
	// where it previously failed. ResumeDownload may be called any number of times
	// until the full download is complete.
	bool ResumeDownload(const wchar_t *url, const wchar_t *filename);

	// Creates a new Amazon S3 bucket.
	bool S3_CreateBucket(const wchar_t *bucketName);

	// Deletes an Amazon S3 bucket.
	bool S3_DeleteBucket(const wchar_t *bucketName);

	// Deletes a remote file (object) on the Amazon S3 service.
	bool S3_DeleteObject(const wchar_t *bucketName, const wchar_t *objectName);

	// The same as DownloadFile, except the file data is returned directly in-memory
	// instead of being written to a local file.
	bool S3_DownloadBytes(const wchar_t *bucketName, const wchar_t *objectName, CkByteData &outBytes);

	// Downloads a file from the Amazon S3 service.
	bool S3_DownloadFile(const wchar_t *bucketName, const wchar_t *objectName, const wchar_t *localFilePath);

	// Downloads a text file (object) from the Amazon S3 service directly into a string
	// variable. The  charset specifies the character encoding, such as "utf-8", of the
	// remote text object.
	bool S3_DownloadString(const wchar_t *bucketName, const wchar_t *objectName, const wchar_t *charset, CkString &outStr);
	// Downloads a text file (object) from the Amazon S3 service directly into a string
	// variable. The  charset specifies the character encoding, such as "utf-8", of the
	// remote text object.
	const wchar_t *s3_DownloadString(const wchar_t *bucketName, const wchar_t *objectName, const wchar_t *charset);

	// Determines if a remote object (file) exists. Returns 1 if the file exists, 0 if
	// it does not exist, -1 if there was a failure in checking, or 2 if using in
	// asynchronous mode to indicate that the background task was successfully started.
	int S3_FileExists(const wchar_t *bucketName, const wchar_t *objectName);

	// Retrieves the XML listing of the objects contained within an Amazon S3 bucket.
	// (This is like a directory listing, but in XML format.)
	// 
	// The bucketPath name may be qualified with URL-encoded params. For example, to list the
	// objects in a bucket named "ChilkatABC" with max-keys = 2000 and marker = "xyz",
	// call S3_ListBucketObject passing the following string for bucketPath:
	// "ChilkatABC?max-keys=2000&marker=xyz"
	// 
	// The S3_ListBucketObjects method recognized all params listed in the AWS
	// documentation for listing objects in a bucket: delimiter, marker, max-keys, and
	// prefix. See Amazon"™s AWS online documentation for more information.
	// 
	bool S3_ListBucketObjects(const wchar_t *bucketName, CkString &outStr);
	// Retrieves the XML listing of the objects contained within an Amazon S3 bucket.
	// (This is like a directory listing, but in XML format.)
	// 
	// The bucketPath name may be qualified with URL-encoded params. For example, to list the
	// objects in a bucket named "ChilkatABC" with max-keys = 2000 and marker = "xyz",
	// call S3_ListBucketObject passing the following string for bucketPath:
	// "ChilkatABC?max-keys=2000&marker=xyz"
	// 
	// The S3_ListBucketObjects method recognized all params listed in the AWS
	// documentation for listing objects in a bucket: delimiter, marker, max-keys, and
	// prefix. See Amazon"™s AWS online documentation for more information.
	// 
	const wchar_t *s3_ListBucketObjects(const wchar_t *bucketName);

	// Retrieves the XML listing of the buckets for an Amazon S3 account.
	bool S3_ListBuckets(CkString &outStr);
	// Retrieves the XML listing of the buckets for an Amazon S3 account.
	const wchar_t *s3_ListBuckets(void);

	// The same as S3_UploadFile, except the contents of the file come from contentBytes
	// instead of a local file.
	bool S3_UploadBytes(const CkByteData &objectContent, const wchar_t *contentType, const wchar_t *bucketName, const wchar_t *objectName);

	// Uploads a file to the Amazon S3 service.
	bool S3_UploadFile(const wchar_t *localFilePath, const wchar_t *contentType, const wchar_t *bucketName, const wchar_t *ObjectName);

	// Uploads an in-memory string to the Amazon S3 service. This is the same as
	// UploadFile, except that the file contents are from an in-memory string instead
	// of a local file. Internal to this method, the objectContent is converted to the character
	// encoding specified by  charset prior to uploading.
	bool S3_UploadString(const wchar_t *objectContent, const wchar_t *charset, const wchar_t *contentType, const wchar_t *bucketName, const wchar_t *ObjectName);

	// Restores cookies for a particular domain. It is assumed that the cookie XML was
	// previously retrieved via the GetCookieXml method, and saved to some sort of
	// persistent storage, such as within a database table. It is then possible for an
	// application to restore the cookies by calling this method.
	bool SetCookieXml(const wchar_t *domain, const wchar_t *cookieXml);

	// Adds a custom header field to any HTTP request sent by a method that does not
	// use the HTTP request object. These methods include Download, DownloadAppend,
	// GetHead, PostBinary, PostMime, PostXml, PutBinary, PutText, QuickDeleteStr,
	// QuickGet, QuickGetObj, QuickGetStr, QuickPutStr, XmlRpc, and XmlRpcPut.
	// 
	// Cookies may be explictly added by calling this method passing "Cookie" for the
	// headerFieldName.
	// 
	// The RemoveRequestHeader method can be called to remove a custom header.
	// 
	void SetRequestHeader(const wchar_t *name, const wchar_t *value);

	// Allows for a client-side certificate to be used for an SSL connection.
	bool SetSslClientCert(CkCertW &cert);

	// Allows for a client-side certificate + private key to be used for the SSL / TLS
	// connection (often called 2-way SSL).
	bool SetSslClientCertPem(const wchar_t *pemDataOrFilename, const wchar_t *pemPassword);

	// Allows for a client-side certificate + private key to be used for the SSL / TLS
	// connection (often called 2-way SSL).
	bool SetSslClientCertPfx(const wchar_t *pfxFilename, const wchar_t *pfxPassword);

	// Convenience method to force the calling process to sleep for a number of
	// milliseconds.
	void SleepMs(int millisec);

	// Sends an explicit HttpRequest to an HTTP server and returns an HttpResponse
	// object. The HttpResponse object provides full access to the response including
	// all headers and the response body. This method may be used to send POST
	// requests, as well as GET, HEAD, file uploads, and XMLHTTP.
	// The caller is responsible for deleting the object returned by this method.
	CkHttpResponseW *SynchronousRequest(const wchar_t *domain, int port, bool ssl, const CkHttpRequestW &req);

	// Unlocks the Http class/component. It is necessary to call Http.UnlockComponent
	// before calling any other methods. Passing any string to UnlockComponent will
	// automatically activate a 30-day trial period.
	bool UnlockComponent(const wchar_t *unlockCode);

	// URL decodes a string.
	bool UrlDecode(const wchar_t *str, CkString &outStr);
	// URL decodes a string.
	const wchar_t *urlDecode(const wchar_t *str);

	// URL encodes a string.
	bool UrlEncode(const wchar_t *str, CkString &outStr);
	// URL encodes a string.
	const wchar_t *urlEncode(const wchar_t *str);

	// Makes an XML RPC call to a URL endpoint. The XML string is passed in an HTTP
	// POST, and the XML response is returned.
	bool XmlRpc(const wchar_t *urlEndpoint, const wchar_t *xmlIn, CkString &outStr);
	// Makes an XML RPC call to a URL endpoint. The XML string is passed in an HTTP
	// POST, and the XML response is returned.
	const wchar_t *xmlRpc(const wchar_t *urlEndpoint, const wchar_t *xmlIn);

	// Same as XmlRpc, but uses the HTTP PUT method instead of the POST method.
	bool XmlRpcPut(const wchar_t *urlEndpoint, const wchar_t *xmlIn, CkString &outStr);
	// Same as XmlRpc, but uses the HTTP PUT method instead of the POST method.
	const wchar_t *xmlRpcPut(const wchar_t *urlEndpoint, const wchar_t *xmlIn);

	// Clears the Chilkat-wide in-memory hostname-to-IP address DNS cache. Chilkat
	// automatically maintains this in-memory cache to prevent redundant DNS lookups.
	// If the TTL on the DNS A records being accessed are short and/or these DNS
	// records change frequently, then this method can be called clear the internal
	// cache. Note: The DNS cache is used/shared among all Chilkat objects in a
	// program, and clearing the cache affects all Chilkat objects.
	void DnsCacheClear(void);

	// Sets the RSA key to be used with OAuth authentication when the RSA-SHA1 OAuth
	// signature method is used (see the OAuthSigMethod property).
	bool SetOAuthRsaKey(CkPrivateKeyW &privKey);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
