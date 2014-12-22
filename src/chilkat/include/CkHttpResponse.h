// CkHttpResponse.h: interface for the CkHttpResponse class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkHttpResponse_H
#define _CkHttpResponse_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkMultiByteBase.h"

class CkByteData;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkHttpResponse
class CK_VISIBLE_PUBLIC CkHttpResponse  : public CkMultiByteBase
{
    private:
	

	// Don't allow assignment or copying these objects.
	CkHttpResponse(const CkHttpResponse &);
	CkHttpResponse &operator=(const CkHttpResponse &);

    public:
	CkHttpResponse(void);
	virtual ~CkHttpResponse(void);

	static CkHttpResponse *createNew(void);
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	

	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------
	// The response body returned as a byte array.
	void get_Body(CkByteData &outBytes);

	// The same as the Body property, but returned as a quoted-printable encoded
	// string.
	void get_BodyQP(CkString &str);
	// The same as the Body property, but returned as a quoted-printable encoded
	// string.
	const char *bodyQP(void);

	// The response body returned as a string.
	void get_BodyStr(CkString &str);
	// The response body returned as a string.
	const char *bodyStr(void);

	// The response charset, such as "iso-8859-1", if applicable. Obviously, responses
	// for GIF and JPG files will not have a charset.
	void get_Charset(CkString &str);
	// The response charset, such as "iso-8859-1", if applicable. Obviously, responses
	// for GIF and JPG files will not have a charset.
	const char *charset(void);

	// The content length of the response, in bytes.
	unsigned long get_ContentLength(void);

	// The content length of the response, in number of bytes, returned as a 64-bit
	// integer.
	__int64 get_ContentLength64(void);

	// The Date response header field, returned in a date/time data type.
	void get_Date(SYSTEMTIME &outSysTime);

	// Returns the content of the Date response header field in RFC822 date/time string
	// format.
	void get_DateStr(CkString &str);
	// Returns the content of the Date response header field in RFC822 date/time string
	// format.
	const char *dateStr(void);

	// The domain of the HTTP server that created this response.
	void get_Domain(CkString &str);
	// The domain of the HTTP server that created this response.
	const char *domain(void);

	// Returns the full MIME (header + body) of the HTTP response.
	void get_FullMime(CkString &str);
	// Returns the full MIME (header + body) of the HTTP response.
	const char *fullMime(void);

	// The full text of the response header.
	void get_Header(CkString &str);
	// The full text of the response header.
	const char *header(void);

	// The number of cookies included in the response.
	int get_NumCookies(void);

	// The number of response header fields.
	int get_NumHeaderFields(void);

	// The status code (as an integer) from the first line of an HTTP response. If the
	// StatusLine = "HTTP/1.0 200 OK", the response status code returned is 200.
	int get_StatusCode(void);

	// The first line of an HTTP response, such as "HTTP/1.0 200 OK".
	void get_StatusLine(CkString &str);
	// The first line of an HTTP response, such as "HTTP/1.0 200 OK".
	const char *statusLine(void);



	// ----------------------
	// Methods
	// ----------------------
	// Returns the domain of the Nth cookie in the response. Indexing begins at 0. The
	// number of response cookies is specified in the NumCookies property.
	bool GetCookieDomain(int index, CkString &outStr);
	// Returns the domain of the Nth cookie in the response. Indexing begins at 0. The
	// number of response cookies is specified in the NumCookies property.
	const char *getCookieDomain(int index);
	// Returns the domain of the Nth cookie in the response. Indexing begins at 0. The
	// number of response cookies is specified in the NumCookies property.
	const char *cookieDomain(int index);

	// Returns the expiration date/time of the Nth cookie in the response. Indexing
	// begins at 0. The number of response cookies is specified in the NumCookies
	// property.
	bool GetCookieExpires(int index, SYSTEMTIME &outSysTime);

	// Returns the expiration date/time of the Nth cookie in the response. Indexing
	// begins at 0. The number of response cookies is specified in the NumCookies
	// property.
	bool GetCookieExpiresStr(int index, CkString &outStr);
	// Returns the expiration date/time of the Nth cookie in the response. Indexing
	// begins at 0. The number of response cookies is specified in the NumCookies
	// property.
	const char *getCookieExpiresStr(int index);
	// Returns the expiration date/time of the Nth cookie in the response. Indexing
	// begins at 0. The number of response cookies is specified in the NumCookies
	// property.
	const char *cookieExpiresStr(int index);

	// Returns the name of the Nth cookie returned in the response. Indexing begins at
	// 0. The number of response cookies is specified in the NumCookies property.
	bool GetCookieName(int index, CkString &outStr);
	// Returns the name of the Nth cookie returned in the response. Indexing begins at
	// 0. The number of response cookies is specified in the NumCookies property.
	const char *getCookieName(int index);
	// Returns the name of the Nth cookie returned in the response. Indexing begins at
	// 0. The number of response cookies is specified in the NumCookies property.
	const char *cookieName(int index);

	// Returns the path of the Nth cookie returned in the response. Indexing begins at
	// 0. The number of response cookies is specified in the NumCookies property.
	bool GetCookiePath(int index, CkString &outStr);
	// Returns the path of the Nth cookie returned in the response. Indexing begins at
	// 0. The number of response cookies is specified in the NumCookies property.
	const char *getCookiePath(int index);
	// Returns the path of the Nth cookie returned in the response. Indexing begins at
	// 0. The number of response cookies is specified in the NumCookies property.
	const char *cookiePath(int index);

	// Returns the value of the Nth cookie returned in the response. Indexing begins at
	// 0. The number of response cookies is specified in the NumCookies property.
	bool GetCookieValue(int index, CkString &outStr);
	// Returns the value of the Nth cookie returned in the response. Indexing begins at
	// 0. The number of response cookies is specified in the NumCookies property.
	const char *getCookieValue(int index);
	// Returns the value of the Nth cookie returned in the response. Indexing begins at
	// 0. The number of response cookies is specified in the NumCookies property.
	const char *cookieValue(int index);

	// Returns the value of a response header field accessed by field name.
	bool GetHeaderField(const char *fieldName, CkString &outStr);
	// Returns the value of a response header field accessed by field name.
	const char *getHeaderField(const char *fieldName);
	// Returns the value of a response header field accessed by field name.
	const char *headerField(const char *fieldName);

	// Returns a response header field attribute. As an example, the response charset
	// is simply the GetHeaderFieldAttr("content-type","charset")
	bool GetHeaderFieldAttr(const char *fieldName, const char *attrName, CkString &outStr);
	// Returns a response header field attribute. As an example, the response charset
	// is simply the GetHeaderFieldAttr("content-type","charset")
	const char *getHeaderFieldAttr(const char *fieldName, const char *attrName);
	// Returns a response header field attribute. As an example, the response charset
	// is simply the GetHeaderFieldAttr("content-type","charset")
	const char *headerFieldAttr(const char *fieldName, const char *attrName);

	// Gets the name of the Nth response header field. Indexing begins at 0. The number
	// of response headers is specified by the NumHeaderFields property.
	bool GetHeaderName(int index, CkString &outStr);
	// Gets the name of the Nth response header field. Indexing begins at 0. The number
	// of response headers is specified by the NumHeaderFields property.
	const char *getHeaderName(int index);
	// Gets the name of the Nth response header field. Indexing begins at 0. The number
	// of response headers is specified by the NumHeaderFields property.
	const char *headerName(int index);

	// Gets the value of the Nth response header field. Indexing begins at 0. The
	// number of response headers is specified by the NumHeaderFields property.
	bool GetHeaderValue(int index, CkString &outStr);
	// Gets the value of the Nth response header field. Indexing begins at 0. The
	// number of response headers is specified by the NumHeaderFields property.
	const char *getHeaderValue(int index);
	// Gets the value of the Nth response header field. Indexing begins at 0. The
	// number of response headers is specified by the NumHeaderFields property.
	const char *headerValue(int index);

	// Saves the body of the HTTP response to a file.
	bool SaveBodyBinary(const char *path);

	// Saves the HTTP response body to a file. This method provides control over CRLF
	// vs bare-LF line-endings. If bCrlf is true, then line endings are automatically
	// converted to CRLF if necessary. If bCrlf is false, then line-endings are
	// automatically converted to bare-LF's (Unix style) if necessary.
	// 
	// To save the HTTP response body exactly as-is (with no line-ending manipulation),
	// then call SaveBodyBinary.
	// 
	bool SaveBodyText(bool bCrlf, const char *path);

	// Convenience method for parsing a param"™s value out of a URL-encoded param
	// string. For example, if a caller passes the following string in
	// encodedParamString:oauth_token=ABC&oauth_token_secret=123&oauth_callback_confirmed=true and
	// "oauth_token_secret" in  paramName, then the return value would be "123".
	bool UrlEncParamValue(const char *encodedParams, const char *paramName, CkString &outStr);
	// Convenience method for parsing a param"™s value out of a URL-encoded param
	// string. For example, if a caller passes the following string in
	// encodedParamString:oauth_token=ABC&oauth_token_secret=123&oauth_callback_confirmed=true and
	// "oauth_token_secret" in  paramName, then the return value would be "123".
	const char *urlEncParamValue(const char *encodedParams, const char *paramName);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
