// CkUploadW.h: interface for the CkUploadW class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkUploadW_H
#define _CkUploadW_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkWideCharBase.h"

class CkByteData;
class CkBaseProgressW;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkUploadW
class CK_VISIBLE_PUBLIC CkUploadW  : public CkWideCharBase
{
    private:
	bool m_cbOwned;
	CkBaseProgressW *m_callback;

	// Don't allow assignment or copying these objects.
	CkUploadW(const CkUploadW &);
	CkUploadW &operator=(const CkUploadW &);

    public:
	CkUploadW(void);
	virtual ~CkUploadW(void);

	static CkUploadW *createNew(void);
	

	CkUploadW(bool bCallbackOwned);
	static CkUploadW *createNew(bool bCallbackOwned);

	
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
	// The chunk size (in bytes) used by the underlying TCP/IP sockets for uploading
	// files. The default value is 65535.
	int get_ChunkSize(void);
	// The chunk size (in bytes) used by the underlying TCP/IP sockets for uploading
	// files. The default value is 65535.
	void put_ChunkSize(int newVal);

	// When true, the request header will included an "Expect: 100-continue" header
	// field. This indicates that the server should respond with an intermediate
	// response of "100 Continue" or "417 Expectation Failed" response based on the
	// information available in the request header. This helps avoid situations such as
	// limits on upload sizes. It allows the server to reject the upload, and then the
	// client can abort prior to uploading the data.
	// 
	// The default value of this property is true.
	// 
	bool get_Expect100Continue(void);
	// When true, the request header will included an "Expect: 100-continue" header
	// field. This indicates that the server should respond with an intermediate
	// response of "100 Continue" or "417 Expectation Failed" response based on the
	// information available in the request header. This helps avoid situations such as
	// limits on upload sizes. It allows the server to reject the upload, and then the
	// client can abort prior to uploading the data.
	// 
	// The default value of this property is true.
	// 
	void put_Expect100Continue(bool newVal);

	// The number of milliseconds between each AbortCheck event callback. The
	// AbortCheck callback allows an application to abort any FTP operation prior to
	// completion. If HeartbeatMs is 0, no AbortCheck event callbacks will occur. Also,
	// AbortCheck callbacks do not occur when doing asynchronous uploads.
	int get_HeartbeatMs(void);
	// The number of milliseconds between each AbortCheck event callback. The
	// AbortCheck callback allows an application to abort any FTP operation prior to
	// completion. If HeartbeatMs is 0, no AbortCheck event callbacks will occur. Also,
	// AbortCheck callbacks do not occur when doing asynchronous uploads.
	void put_HeartbeatMs(int newVal);

	// The hostname of the HTTP server that is the target of the upload. Do not include
	// "http://" in the hostname. It can be a hostname, such as "www.chilkatsoft.com",
	// or an IP address, such as "168.144.70.227".
	void get_Hostname(CkString &str);
	// The hostname of the HTTP server that is the target of the upload. Do not include
	// "http://" in the hostname. It can be a hostname, such as "www.chilkatsoft.com",
	// or an IP address, such as "168.144.70.227".
	const wchar_t *hostname(void);
	// The hostname of the HTTP server that is the target of the upload. Do not include
	// "http://" in the hostname. It can be a hostname, such as "www.chilkatsoft.com",
	// or an IP address, such as "168.144.70.227".
	void put_Hostname(const wchar_t *newVal);

	// A timeout in milliseconds. The default value is 30000. If the upload hangs (i.e.
	// progress halts) for more than this time, the component will abort the upload.
	// (It will timeout.)
	int get_IdleTimeoutMs(void);
	// A timeout in milliseconds. The default value is 30000. If the upload hangs (i.e.
	// progress halts) for more than this time, the component will abort the upload.
	// (It will timeout.)
	void put_IdleTimeoutMs(int newVal);

	// The HTTP login for sites requiring authentication. Chilkat Upload supports Basic
	// HTTP authentication.
	void get_Login(CkString &str);
	// The HTTP login for sites requiring authentication. Chilkat Upload supports Basic
	// HTTP authentication.
	const wchar_t *login(void);
	// The HTTP login for sites requiring authentication. Chilkat Upload supports Basic
	// HTTP authentication.
	void put_Login(const wchar_t *newVal);

	// After an upload has completed, this property contains the number of bytes sent.
	// During asynchronous uploads, this property contains the current number of bytes
	// sent while the upload is in progress.
	unsigned long get_NumBytesSent(void);

	// The HTTP password for sites requiring authentication. Chilkat Upload supports
	// Basic HTTP authentication.
	void get_Password(CkString &str);
	// The HTTP password for sites requiring authentication. Chilkat Upload supports
	// Basic HTTP authentication.
	const wchar_t *password(void);
	// The HTTP password for sites requiring authentication. Chilkat Upload supports
	// Basic HTTP authentication.
	void put_Password(const wchar_t *newVal);

	// The path part of the upload URL. Some examples:
	// 
	// If the upload target (i.e. consumer) URL is:
	// http://www.freeaspupload.net/freeaspupload/testUpload.asp, then
	// 
	//     Hostname = "www.freeaspupload.net" Path = "/freeaspupload/testUpload.asp"
	// 
	// If the upload target URL is
	// http://www.chilkatsoft.com/cgi-bin/ConsumeUpload.exe, then
	// 
	//     Hostname = "www.chilkatsoft.com" Path = "/cgi-bin/ConsumeUpload.exe"
	// 
	void get_Path(CkString &str);
	// The path part of the upload URL. Some examples:
	// 
	// If the upload target (i.e. consumer) URL is:
	// http://www.freeaspupload.net/freeaspupload/testUpload.asp, then
	// 
	//     Hostname = "www.freeaspupload.net" Path = "/freeaspupload/testUpload.asp"
	// 
	// If the upload target URL is
	// http://www.chilkatsoft.com/cgi-bin/ConsumeUpload.exe, then
	// 
	//     Hostname = "www.chilkatsoft.com" Path = "/cgi-bin/ConsumeUpload.exe"
	// 
	const wchar_t *path(void);
	// The path part of the upload URL. Some examples:
	// 
	// If the upload target (i.e. consumer) URL is:
	// http://www.freeaspupload.net/freeaspupload/testUpload.asp, then
	// 
	//     Hostname = "www.freeaspupload.net" Path = "/freeaspupload/testUpload.asp"
	// 
	// If the upload target URL is
	// http://www.chilkatsoft.com/cgi-bin/ConsumeUpload.exe, then
	// 
	//     Hostname = "www.chilkatsoft.com" Path = "/cgi-bin/ConsumeUpload.exe"
	// 
	void put_Path(const wchar_t *newVal);

	// Contains the current percentage completion (0 to 100) while an asynchronous
	// upload is in progress.
	unsigned long get_PercentUploaded(void);

	// The port number of the upload target (i.e. consumer) URL. The default value is
	// 80. If SSL is used, this should be set to 443 (typically).
	int get_Port(void);
	// The port number of the upload target (i.e. consumer) URL. The default value is
	// 80. If SSL is used, this should be set to 443 (typically).
	void put_Port(int newVal);

	// The domain name of a proxy host if an HTTP proxy is used. Do not include the
	// "http://". The domain name may be a hostname, such as "www.chilkatsoft.com", or
	// an IP address, such as "168.144.70.227".
	void get_ProxyDomain(CkString &str);
	// The domain name of a proxy host if an HTTP proxy is used. Do not include the
	// "http://". The domain name may be a hostname, such as "www.chilkatsoft.com", or
	// an IP address, such as "168.144.70.227".
	const wchar_t *proxyDomain(void);
	// The domain name of a proxy host if an HTTP proxy is used. Do not include the
	// "http://". The domain name may be a hostname, such as "www.chilkatsoft.com", or
	// an IP address, such as "168.144.70.227".
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

	// An HTTP upload is nothing more than an HTTP POST that contains the content of
	// the files being uploaded. Just as with any HTTP POST or GET, the server should
	// send an HTTP response that consists of header and body.
	// 
	// This property contains the body part of the HTTP response.
	// 
	void get_ResponseBody(CkByteData &outBytes);

	// An HTTP upload is nothing more than an HTTP POST that contains the content of
	// the files being uploaded. Just as with any HTTP POST or GET, the server should
	// send an HTTP response that consists of header and body.
	// 
	// This property contains the header part of the HTTP response.
	// 
	void get_ResponseHeader(CkString &str);
	// An HTTP upload is nothing more than an HTTP POST that contains the content of
	// the files being uploaded. Just as with any HTTP POST or GET, the server should
	// send an HTTP response that consists of header and body.
	// 
	// This property contains the header part of the HTTP response.
	// 
	const wchar_t *responseHeader(void);

	// The HTTP response status code of the HTTP response. A list of HTTP status codes
	// can be found here: HTTP Response Status Codes
	// <http://en.wikipedia.org/wiki/List_of_HTTP_status_codes> .
	int get_ResponseStatus(void);

	// Set this to true if the upload is to HTTPS. For example, if the target of the
	// upload is:
	// 
	//     https://www.myuploadtarget.com/consumeUpload.asp
	// 
	// then set:
	// 
	//     Ssl = true
	//     Hostname = "www.myuploadtarget.com"
	//     Path = "/consumeupload.asp"
	//     Port = 443
	//     
	// 
	bool get_Ssl(void);
	// Set this to true if the upload is to HTTPS. For example, if the target of the
	// upload is:
	// 
	//     https://www.myuploadtarget.com/consumeUpload.asp
	// 
	// then set:
	// 
	//     Ssl = true
	//     Hostname = "www.myuploadtarget.com"
	//     Path = "/consumeupload.asp"
	//     Port = 443
	//     
	// 
	void put_Ssl(bool newVal);

	// The total size of the upload (in bytes). This property will become set at the
	// beginning of an asynchronous upload. A program may monitor asynchronous uploads
	// by tracking both NumBytesSent and PercentUploaded.
	// 
	// This property is also set during synchronous uploads.
	// 
	unsigned long get_TotalUploadSize(void);

	// Set to true when an asynchronous upload is started. When the asynchronous
	// upload is complete, this property becomes equal to false. A program will
	// typically begin an asynchronous upload by calling BeginUpload, and then
	// periodically checking the value of this property to determine when the upload is
	// complete.
	bool get_UploadInProgress(void);

	// Set to true (success) or false (failed) after an asynchronous upload
	// completes or aborts due to failure. When a program does an asynchronous upload,
	// it will wait until UploadInProgress becomes false. It will then check the
	// value of this property to determine if the upload was successful or not.
	bool get_UploadSuccess(void);

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
#ifndef SINGLE_THREADED
	// May be called during an asynchronous upload to abort.
	void AbortUpload(void);
#endif

	// Adds a custom HTTP header to the HTTP upload.
	void AddCustomHeader(const wchar_t *name, const wchar_t *value);

	// Adds a file to the list of files to be uploaded in the next call to
	// BlockingUpload, BeginUpload, or UploadToMemory. To upload more than one file,
	// call this method once for each file to be uploaded.
	void AddFileReference(const wchar_t *name, const wchar_t *filename);

	// Adds a custom HTTP request parameter to the upload.
	void AddParam(const wchar_t *name, const wchar_t *value);

#ifndef SINGLE_THREADED
	// Starts an asynchronous upload. Only one asynchronous upload may be in progress
	// at a time. To achieve multiple asynchronous uploads, use multiple instances of
	// the Chilkat Upload object. Each object instance is capable of managing a single
	// asynchronous upload.
	// 
	// When this method is called, a background thread is started and the asynchronous
	// upload runs in the background. The upload may be aborted at any time by calling
	// AbortUpload. The upload is completed (or failed) when UploadInProgress becomes
	// false. At that point, the UploadSuccess property may be checked to determine
	// success (true) or failure (false).
	// 
	bool BeginUpload(void);
#endif

	// Uploads files in a synchronous manner. This method does not return until all
	// files have been uploaded, or the upload fails. The files to be uploaded are
	// indicated by calling AddFileReference once for each file (prior to calling this
	// method).
	bool BlockingUpload(void);

	// Clears the internal list of files created by calls to AddFileReference.
	void ClearFileReferences(void);

	// Clears the internal list of params created by calls to AddParam.
	void ClearParams(void);

	// A convenience method for putting the calling process to sleep for N
	// milliseconds. It is provided here because some programming languages do not
	// provide this capability, and sleeping for short increments of time is helpful
	// when doing asynchronous uploads.
	void SleepMs(int millisec);

	// Writes the complete HTTP POST to memory. The POST contains the HTTP header, any
	// custom params added by calling AddParam, and each file to be uploaded. This is
	// helpful in debugging. It allows you to see the exact HTTP POST sent to the
	// server if BlockingUpload or BeginUpload is called.
	bool UploadToMemory(CkByteData &outData);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
