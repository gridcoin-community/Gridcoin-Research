// CkHttpRequest.h: interface for the CkHttpRequest class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkHttpRequest_H
#define _CkHttpRequest_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkMultiByteBase.h"

class CkByteData;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkHttpRequest
class CK_VISIBLE_PUBLIC CkHttpRequest  : public CkMultiByteBase
{
    private:
	

	// Don't allow assignment or copying these objects.
	CkHttpRequest(const CkHttpRequest &);
	CkHttpRequest &operator=(const CkHttpRequest &);

    public:
	CkHttpRequest(void);
	virtual ~CkHttpRequest(void);

	static CkHttpRequest *createNew(void);
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	

	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------
	// Controls the character encoding used for HTTP request parameters for POST
	// requests. The default value is the ANSI charset of the computer. The charset
	// should match the charset expected by the form target.
	// 
	// The "charset" attribute is only included in the Content-Type header of the
	// request if the SendCharset property is set to true.
	// 
	void get_Charset(CkString &str);
	// Controls the character encoding used for HTTP request parameters for POST
	// requests. The default value is the ANSI charset of the computer. The charset
	// should match the charset expected by the form target.
	// 
	// The "charset" attribute is only included in the Content-Type header of the
	// request if the SendCharset property is set to true.
	// 
	const char *charset(void);
	// Controls the character encoding used for HTTP request parameters for POST
	// requests. The default value is the ANSI charset of the computer. The charset
	// should match the charset expected by the form target.
	// 
	// The "charset" attribute is only included in the Content-Type header of the
	// request if the SendCharset property is set to true.
	// 
	void put_Charset(const char *newVal);

	// The ContentType property sets the "Content-Type" header field, and identifies
	// the content-type of the HTTP request body. Common values are:
	// 
	//         
	// application/x-www-form-urlencoded    
	// multipart/form-data    
	// text/xml    
	// application/jsonrequest    
	//     
	// 
	// If ContentType is set equal to the empty string, then no Content-Type header is
	// included in the HTTP request.
	void get_ContentType(CkString &str);
	// The ContentType property sets the "Content-Type" header field, and identifies
	// the content-type of the HTTP request body. Common values are:
	// 
	//         
	// application/x-www-form-urlencoded    
	// multipart/form-data    
	// text/xml    
	// application/jsonrequest    
	//     
	// 
	// If ContentType is set equal to the empty string, then no Content-Type header is
	// included in the HTTP request.
	const char *contentType(void);
	// The ContentType property sets the "Content-Type" header field, and identifies
	// the content-type of the HTTP request body. Common values are:
	// 
	//         
	// application/x-www-form-urlencoded    
	// multipart/form-data    
	// text/xml    
	// application/jsonrequest    
	//     
	// 
	// If ContentType is set equal to the empty string, then no Content-Type header is
	// included in the HTTP request.
	void put_ContentType(const char *newVal);

	// Composes and returns the entire MIME header of the HTTP request.
	void get_EntireHeader(CkString &str);
	// Composes and returns the entire MIME header of the HTTP request.
	const char *entireHeader(void);
	// Composes and returns the entire MIME header of the HTTP request.
	void put_EntireHeader(const char *newVal);

	// The HttpVerb property should be set to the name of the HTTP method that appears
	// on the "start line" of an HTTP request, such as GET, POST, PUT, DELETE, etc. It
	// is also possible to use the various WebDav verbs such as PROPFIND, PROPPATCH,
	// MKCOL, COPY, MOVE, LOCK, UNLOCK, etc. In general, the HttpVerb may be set to
	// anything, even custom verbs recognized by a custom server-side app.
	void get_HttpVerb(CkString &str);
	// The HttpVerb property should be set to the name of the HTTP method that appears
	// on the "start line" of an HTTP request, such as GET, POST, PUT, DELETE, etc. It
	// is also possible to use the various WebDav verbs such as PROPFIND, PROPPATCH,
	// MKCOL, COPY, MOVE, LOCK, UNLOCK, etc. In general, the HttpVerb may be set to
	// anything, even custom verbs recognized by a custom server-side app.
	const char *httpVerb(void);
	// The HttpVerb property should be set to the name of the HTTP method that appears
	// on the "start line" of an HTTP request, such as GET, POST, PUT, DELETE, etc. It
	// is also possible to use the various WebDav verbs such as PROPFIND, PROPPATCH,
	// MKCOL, COPY, MOVE, LOCK, UNLOCK, etc. In general, the HttpVerb may be set to
	// anything, even custom verbs recognized by a custom server-side app.
	void put_HttpVerb(const char *newVal);

	// The HTTP version in the request header. Defaults to "1.1".
	void get_HttpVersion(CkString &str);
	// The HTTP version in the request header. Defaults to "1.1".
	const char *httpVersion(void);
	// The HTTP version in the request header. Defaults to "1.1".
	void put_HttpVersion(const char *newVal);

	// Returns the number of request header fields.
	int get_NumHeaderFields(void);

	// Returns the number of query parameters.
	int get_NumParams(void);

	// The path of the resource requested. A path of "/" indicates the default document
	// for a domain.
	void get_Path(CkString &str);
	// The path of the resource requested. A path of "/" indicates the default document
	// for a domain.
	const char *path(void);
	// The path of the resource requested. A path of "/" indicates the default document
	// for a domain.
	void put_Path(const char *newVal);

	// Controls whether the charset is explicitly included in the content-type header
	// field of the HTTP POST request. The default value of this property is false.
	bool get_SendCharset(void);
	// Controls whether the charset is explicitly included in the content-type header
	// field of the HTTP POST request. The default value of this property is false.
	void put_SendCharset(bool newVal);



	// ----------------------
	// Methods
	// ----------------------
	// Adds a file to an upload request where the contents of the file come from an
	// in-memory byte array. To create a file upload request, call UseUpload and then
	// call AddBytesForUpload, AddStringForUpload, or AddFileForUpload for each file to
	// be uploaded.
	// 
	// name is an arbitrary name. (In HTML, it is the form field name of the input
	// tag.)
	//  remoteFileName is the name of the file to be created on the HTTP server.
	//  byteData contains the contents (bytes) to be uploaded.
	// 
	bool AddBytesForUpload(const char *name, const char *filename, const CkByteData &byteData);

	// Same as AddBytesForUpload, but allows the Content-Type header field to be
	// directly specified. (Otherwise, the Content-Type header is automatically
	// determined based on the  remoteFileName's file extension.)
	bool AddBytesForUpload2(const char *name, const char *filename, const CkByteData &byteData, const char *contentType);

	// Adds a file to an upload request. To create a file upload request, call
	// UseUpload and then call AddFileForUpload, AddBytesForUpload, or
	// AddStringForUpload for each file to be uploaded. This method does not read the
	// file into memory. When the upload occurs, the data is streamed directly from the
	// file, thus allowing for very large files to be uploaded without consuming large
	// amounts of memory.
	// 
	// name is an arbitrary name. (In HTML, it is the form field name of the input
	// tag.)
	//  filePath is the path to an existing file in the local filesystem.
	// 
	bool AddFileForUpload(const char *name, const char *filename);

	// Same as AddFileForUpload, but allows the Content-Type header field to be
	// directly specified. (Otherwise, the Content-Type header is automatically
	// determined based on the file extension.)
	// 
	// name is an arbitrary name. (In HTML, it is the form field name of the input
	// tag.)
	//  filePath is the path to an existing file in the local filesystem.
	// 
	bool AddFileForUpload2(const char *name, const char *filename, const char *contentType);

	// Adds a request header to the HTTP request. If a header having the same field
	// name is already present, this method replaces it.
	void AddHeader(const char *name, const char *value);

	// Adds a request query parameter (name/value pair) to the HTTP request. The name
	// and value strings passed to this method should not be URL encoded.
	void AddParam(const char *name, const char *value);

	// Same as AddFileForUpload, but the upload data comes from an in-memory string
	// instead of a file.
	bool AddStringForUpload(const char *name, const char *filename, const char *strData, const char *charset);

	// Same as AddStringForUpload, but allows the Content-Type header field to be
	// directly specified. (Otherwise, the Content-Type header is automatically
	// determined based on the ARG2's file extension.)
	bool AddStringForUpload2(const char *name, const char *filename, const char *strData, const char *charset, const char *contentType);

	// Returns the request text that would be sent if Http.SynchronousRequest was
	// called.
	bool GenerateRequestText(CkString &outStr);
	// Returns the request text that would be sent if Http.SynchronousRequest was
	// called.
	const char *generateRequestText(void);

	// Returns the value of a request header field.
	bool GetHeaderField(const char *name, CkString &outStr);
	// Returns the value of a request header field.
	const char *getHeaderField(const char *name);
	// Returns the value of a request header field.
	const char *headerField(const char *name);

	// Returns the Nth request header field name. Indexing begins at 0, and the number
	// of request header fields is specified by the NumHeaderFields property.
	bool GetHeaderName(int index, CkString &outStr);
	// Returns the Nth request header field name. Indexing begins at 0, and the number
	// of request header fields is specified by the NumHeaderFields property.
	const char *getHeaderName(int index);
	// Returns the Nth request header field name. Indexing begins at 0, and the number
	// of request header fields is specified by the NumHeaderFields property.
	const char *headerName(int index);

	// Returns the Nth request header field value. Indexing begins at 0, and the number
	// of request header fields is specified by the NumHeaderFields property.
	bool GetHeaderValue(int index, CkString &outStr);
	// Returns the Nth request header field value. Indexing begins at 0, and the number
	// of request header fields is specified by the NumHeaderFields property.
	const char *getHeaderValue(int index);
	// Returns the Nth request header field value. Indexing begins at 0, and the number
	// of request header fields is specified by the NumHeaderFields property.
	const char *headerValue(int index);

	// Returns a request query parameter value by name.
	bool GetParam(const char *name, CkString &outStr);
	// Returns a request query parameter value by name.
	const char *getParam(const char *name);
	// Returns a request query parameter value by name.
	const char *param(const char *name);

	// Returns the Nth request query parameter field name. Indexing begins at 0, and
	// the number of request query parameter fields is specified by the NumParams
	// property.
	bool GetParamName(int index, CkString &outStr);
	// Returns the Nth request query parameter field name. Indexing begins at 0, and
	// the number of request query parameter fields is specified by the NumParams
	// property.
	const char *getParamName(int index);
	// Returns the Nth request query parameter field name. Indexing begins at 0, and
	// the number of request query parameter fields is specified by the NumParams
	// property.
	const char *paramName(int index);

	// Returns the Nth request query parameter field value. Indexing begins at 0, and
	// the number of request query parameter fields is specified by the NumParams
	// property.
	bool GetParamValue(int index, CkString &outStr);
	// Returns the Nth request query parameter field value. Indexing begins at 0, and
	// the number of request query parameter fields is specified by the NumParams
	// property.
	const char *getParamValue(int index);
	// Returns the Nth request query parameter field value. Indexing begins at 0, and
	// the number of request query parameter fields is specified by the NumParams
	// property.
	const char *paramValue(int index);

	// Returns the request parameters in URL encoded form (i.e. in the exact form that
	// would be sent if the ContentType property was
	// application/x-www-form-urlencoded). For example, if a request has two params:
	// param1="abc 123" and param2="abc-123", then GetUrlEncodedParams would return
	// "abc+123<param2=abc%2D123"
	bool GetUrlEncodedParams(CkString &outStr);
	// Returns the request parameters in URL encoded form (i.e. in the exact form that
	// would be sent if the ContentType property was
	// application/x-www-form-urlencoded). For example, if a request has two params:
	// param1="abc 123" and param2="abc-123", then GetUrlEncodedParams would return
	// "abc+123<param2=abc%2D123"
	const char *getUrlEncodedParams(void);
	// Returns the request parameters in URL encoded form (i.e. in the exact form that
	// would be sent if the ContentType property was
	// application/x-www-form-urlencoded). For example, if a request has two params:
	// param1="abc 123" and param2="abc-123", then GetUrlEncodedParams would return
	// "abc+123<param2=abc%2D123"
	const char *urlEncodedParams(void);

	// The HTTP protocol is such that all HTTP requests are MIME. For non-multipart
	// requests, this method may be called to set the MIME body of the HTTP request to
	// the exact contents of the byteData.
	// Note: A non-multipart HTTP request consists of (1) the HTTP start line, (2) MIME
	// header fields, and (3) the MIME body. This method sets the MIME body.
	bool LoadBodyFromBytes(const CkByteData &binaryData);

	// The HTTP protocol is such that all HTTP requests are MIME. For non-multipart
	// requests, this method may be called to set the MIME body of the HTTP request to
	// the exact contents of filePath.
	// Note: A non-multipart HTTP request consists of (1) the HTTP start line, (2) MIME
	// header fields, and (3) the MIME body. This method sets the MIME body.
	bool LoadBodyFromFile(const char *filename);

	// The HTTP protocol is such that all HTTP requests are MIME. For non-multipart
	// requests, this method may be called to set the MIME body of the HTTP request to
	// the exact contents of bodyStr.
	// Note: A non-multipart HTTP request consists of (1) the HTTP start line, (2) MIME
	// header fields, and (3) the MIME body. This method sets the MIME body.
	// 
	//  charset indicates the charset, such as "utf-8" or "iso-8859-1", to be used. The
	// HTTP body will contain the bodyStr converted to this character encoding.
	// 
	bool LoadBodyFromString(const char *bodyStr, const char *charset);

	// Removes all request parameters.
	void RemoveAllParams(void);

	// Removes all occurances of a HTTP request header field. Always returns true.
	bool RemoveHeader(const char *name);

	// Removes a single HTTP request parameter by name.
	void RemoveParam(const char *name);

	// Parses a URL and sets the Path and query parameters (NumParams, GetParam,
	// GetParamName, GetParamValue).
	void SetFromUrl(const char *url);

	// Useful for sending HTTP requests where the body is a very large file. For
	// example, to send an XML HttpRequest containing a very large XML document, one
	// would set the HttpVerb = "POST", the ContentType = "text/xml", and then call
	// StreamBodyFromFile to indicate that the XML body of the request is to be
	// streamed directly from a file. When the HTTP request is actually sent, the body
	// is streamed directly from the file, and thus the file never needs to be loaded
	// in its entirety in memory.
	bool StreamBodyFromFile(const char *filename);

	// Makes the HttpRequest a GET request.
	// 
	// Important: This method is deprecated. An application should instead set the
	// HttpVerb property equal to "GET", and the ContentType equal to an empty string
	// (because GET requests have no request body).
	// 
	void UseGet(void);

	// Makes the HttpRequest a HEAD request.
	// 
	// Important: This method is deprecated. An application should instead set the
	// HttpVerb property equal to "HEAD", and the ContentType equal to an empty string
	// (because HEAD requests have no body).
	// 
	void UseHead(void);

	// Makes the HttpRequest a POST request that uses the
	// "application/x-www-form-urlencoded" content type.
	// 
	// Important: This method is deprecated. An application should instead set the
	// HttpVerb property equal to "POST", and the ContentType equal to
	// "application/x-www-form-urlencoded".
	// 
	void UsePost(void);

	// Makes the HttpRequest a POST request that uses the "multipart/form-data" content
	// type.
	// 
	// Important: This method is deprecated. An application should instead set the
	// HttpVerb property equal to "POST", and the ContentType equal to
	// "multipart/form-data".
	// 
	void UsePostMultipartForm(void);

	// Makes the HttpRequest a PUT request.
	// 
	// Important: This method is deprecated. An application should instead set the
	// HttpVerb property equal to "PUT", and the ContentType equal to
	// "application/x-www-form-urlencoded".
	// 
	void UsePut(void);

	// Makes the HttpRequest a POST request that uses the "multipart/form-data" content
	// type. To create a file upload request, call UseUpload and then call
	// AddFileForUpload for each file to be uploaded.
	// 
	// Important: This method is deprecated. An application should instead set the
	// HttpVerb property equal to "POST", and the ContentType equal to
	// "multipart/form-data".
	// 
	void UseUpload(void);

	// Makes the HttpRequest a PUT request that uses the "multipart/form-data" content
	// type. To create a file upload request (using the PUT verb), call UseUploadPut
	// and then call AddFileForUpload for each file to be uploaded.
	// 
	// Important: This method is deprecated. An application should instead set the
	// HttpVerb property equal to "PUT", and the ContentType equal to
	// "multipart/form-data".
	// 
	void UseUploadPut(void);

	// Makes the HttpRequest a POST request using the "application/xml" content type.
	// The request body is set to the XML string passed to this method.
	// 
	// Important: This method is deprecated. An application should instead set the
	// HttpVerb property equal to "POST", the ContentType equal to "text/xml", and the
	// request body should contain the XML document text.
	// 
	void UseXmlHttp(const char *xmlBody);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
