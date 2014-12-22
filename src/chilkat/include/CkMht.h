// CkMht.h: interface for the CkMht class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkMht_H
#define _CkMht_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkMultiByteBase.h"

class CkBaseProgress;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkMht
class CK_VISIBLE_PUBLIC CkMht  : public CkMultiByteBase
{
    private:
	CkBaseProgress *m_callback;

	// Don't allow assignment or copying these objects.
	CkMht(const CkMht &);
	CkMht &operator=(const CkMht &);

    public:
	CkMht(void);
	virtual ~CkMht(void);

	static CkMht *createNew(void);
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
	// When processing an HTML file or string (not a website URL), this defines the
	// base URL to be used when converting relative HREFs to absolute HREFs.
	void get_BaseUrl(CkString &str);
	// When processing an HTML file or string (not a website URL), this defines the
	// base URL to be used when converting relative HREFs to absolute HREFs.
	const char *baseUrl(void);
	// When processing an HTML file or string (not a website URL), this defines the
	// base URL to be used when converting relative HREFs to absolute HREFs.
	void put_BaseUrl(const char *newVal);

	// The amount of time in seconds to wait before timing out when connecting to an
	// HTTP server. The default value is 10 seconds.
	int get_ConnectTimeout(void);
	// The amount of time in seconds to wait before timing out when connecting to an
	// HTTP server. The default value is 10 seconds.
	void put_ConnectTimeout(int newVal);

	// A filename to save the result HTML when converting a URL, file, or HTML string.
	// If problems are experienced, the before/after HTML can be analyzed to help
	// determine the cause.
	void get_DebugHtmlAfter(CkString &str);
	// A filename to save the result HTML when converting a URL, file, or HTML string.
	// If problems are experienced, the before/after HTML can be analyzed to help
	// determine the cause.
	const char *debugHtmlAfter(void);
	// A filename to save the result HTML when converting a URL, file, or HTML string.
	// If problems are experienced, the before/after HTML can be analyzed to help
	// determine the cause.
	void put_DebugHtmlAfter(const char *newVal);

	// A filename to save the input HTML when converting a URL, file, or HTML string.
	// If problems are experienced, the before/after HTML can be analyzed to help
	// determine the cause.
	void get_DebugHtmlBefore(CkString &str);
	// A filename to save the input HTML when converting a URL, file, or HTML string.
	// If problems are experienced, the before/after HTML can be analyzed to help
	// determine the cause.
	const char *debugHtmlBefore(void);
	// A filename to save the input HTML when converting a URL, file, or HTML string.
	// If problems are experienced, the before/after HTML can be analyzed to help
	// determine the cause.
	void put_DebugHtmlBefore(const char *newVal);

	// When true causes the Mht class to be much more verbose in its logging. The
	// default is false.
	bool get_DebugTagCleaning(void);
	// When true causes the Mht class to be much more verbose in its logging. The
	// default is false.
	void put_DebugTagCleaning(bool newVal);

	// Controls whether images are embedded in the MHT/EML, or whether the IMG SRC
	// attributes are left as external URL references. If false, the IMG SRC tags are
	// converted to absolute URLs (if necessary) and the images are not embedded within
	// the MHT/EML.
	bool get_EmbedImages(void);
	// Controls whether images are embedded in the MHT/EML, or whether the IMG SRC
	// attributes are left as external URL references. If false, the IMG SRC tags are
	// converted to absolute URLs (if necessary) and the images are not embedded within
	// the MHT/EML.
	void put_EmbedImages(bool newVal);

	// If true, only images found on the local filesystem (i.e. links to files) will
	// be embedded within the MHT.
	bool get_EmbedLocalOnly(void);
	// If true, only images found on the local filesystem (i.e. links to files) will
	// be embedded within the MHT.
	void put_EmbedLocalOnly(bool newVal);

	// If true, page parts such as images, style sheets, etc. will be fetched from
	// the disk cache if possible. The disk cache root may be defined by calling
	// AddCacheRoot. The default value is false.
	bool get_FetchFromCache(void);
	// If true, page parts such as images, style sheets, etc. will be fetched from
	// the disk cache if possible. The disk cache root may be defined by calling
	// AddCacheRoot. The default value is false.
	void put_FetchFromCache(bool newVal);

	// The time interval, in milliseconds, between AbortCheck event callbacks. The
	// heartbeat/AbortCheck provides a means for an application to abort any MHT method
	// before completion.
	// 
	// The default value is 0, which means that no AbortCheck events will be fired.
	// 
	int get_HeartbeatMs(void);
	// The time interval, in milliseconds, between AbortCheck event callbacks. The
	// heartbeat/AbortCheck provides a means for an application to abort any MHT method
	// before completion.
	// 
	// The default value is 0, which means that no AbortCheck events will be fired.
	// 
	void put_HeartbeatMs(int newVal);

	// Some HTTP responses contain a "Cache-Control: must-revalidate" header. If this
	// is present, the server is requesting that the client always issue a revalidate
	// HTTP request instead of serving the page directly from cache. If
	// IgnoreMustRevalidate is set to true, then Chilkat MHT will serve the page
	// directly from cache without revalidating until the page is no longer fresh.
	// (assuming that FetchFromCache is set to true)
	// 
	// The default value of this property is false.
	// 
	bool get_IgnoreMustRevalidate(void);
	// Some HTTP responses contain a "Cache-Control: must-revalidate" header. If this
	// is present, the server is requesting that the client always issue a revalidate
	// HTTP request instead of serving the page directly from cache. If
	// IgnoreMustRevalidate is set to true, then Chilkat MHT will serve the page
	// directly from cache without revalidating until the page is no longer fresh.
	// (assuming that FetchFromCache is set to true)
	// 
	// The default value of this property is false.
	// 
	void put_IgnoreMustRevalidate(bool newVal);

	// Some HTTP responses contain headers of various types that indicate that the page
	// should not be cached. Chilkat MHT will adhere to this unless this property is
	// set to true.
	// 
	// The default value of this property is false.
	// 
	bool get_IgnoreNoCache(void);
	// Some HTTP responses contain headers of various types that indicate that the page
	// should not be cached. Chilkat MHT will adhere to this unless this property is
	// set to true.
	// 
	// The default value of this property is false.
	// 
	void put_IgnoreNoCache(bool newVal);

	// Only applies when creating MHT files. Scripts are always removed when creating
	// EML or emails from HTML. If set to true, then all scripts are removed, if set
	// to false (the default) then scripts are not removed.
	bool get_NoScripts(void);
	// Only applies when creating MHT files. Scripts are always removed when creating
	// EML or emails from HTML. If set to true, then all scripts are removed, if set
	// to false (the default) then scripts are not removed.
	void put_NoScripts(bool newVal);

	// Setting this property to true causes the MHT component to use NTLM
	// authentication (also known as IWA -- or Integrated Windows Authentication) when
	// authentication with an HTTP server.
	// 
	// The default value of this property is false.
	// 
	bool get_NtlmAuth(void);
	// Setting this property to true causes the MHT component to use NTLM
	// authentication (also known as IWA -- or Integrated Windows Authentication) when
	// authentication with an HTTP server.
	// 
	// The default value of this property is false.
	// 
	void put_NtlmAuth(bool newVal);

	// The number of directory levels to be used under each cache root. The default is
	// 0, meaning that each cached item is stored in a cache root directory. A value of
	// 1 causes each cached page to be stored in one of 255 subdirectories named
	// "0","1", "2", ..."255" under a cache root. A value of 2 causes two levels of
	// subdirectories ("0..255/0..255") under each cache root. The MHT control
	// automatically creates subdirectories as needed. The reason for mutliple levels
	// is to alleviate problems that may arise when huge numbers of files are stored in
	// a single directory. For example, Windows Explorer does not behave well when
	// trying to display the contents of directories with thousands of files.
	int get_NumCacheLevels(void);
	// The number of directory levels to be used under each cache root. The default is
	// 0, meaning that each cached item is stored in a cache root directory. A value of
	// 1 causes each cached page to be stored in one of 255 subdirectories named
	// "0","1", "2", ..."255" under a cache root. A value of 2 causes two levels of
	// subdirectories ("0..255/0..255") under each cache root. The MHT control
	// automatically creates subdirectories as needed. The reason for mutliple levels
	// is to alleviate problems that may arise when huge numbers of files are stored in
	// a single directory. For example, Windows Explorer does not behave well when
	// trying to display the contents of directories with thousands of files.
	void put_NumCacheLevels(int newVal);

	// The number of cache roots to be used for the disk cache. This allows the disk
	// cache spread out over multiple disk drives. Each cache root is a string
	// indicating the drive letter and directory path. For example, "E:\Cache". To
	// create a cache with four roots, call AddCacheRoot once for each directory root.
	int get_NumCacheRoots(void);

	// This property provides a means for the noscript option to be selected when
	// possible. If PreferMHTScripts = false, then scripts with noscript alternatives
	// are removed and the noscript content is kept. If true (the default), then
	// scripts are preserved and the noscript options are discarded.
	bool get_PreferMHTScripts(void);
	// This property provides a means for the noscript option to be selected when
	// possible. If PreferMHTScripts = false, then scripts with noscript alternatives
	// are removed and the noscript content is kept. If true (the default), then
	// scripts are preserved and the noscript options are discarded.
	void put_PreferMHTScripts(bool newVal);

	// (Optional) A proxy host:port if a proxy is necessary to access the Internet. The
	// proxy string should be formatted as "hostname:port", such as
	// "www.chilkatsoft.com:100".
	void get_Proxy(CkString &str);
	// (Optional) A proxy host:port if a proxy is necessary to access the Internet. The
	// proxy string should be formatted as "hostname:port", such as
	// "www.chilkatsoft.com:100".
	const char *proxy(void);
	// (Optional) A proxy host:port if a proxy is necessary to access the Internet. The
	// proxy string should be formatted as "hostname:port", such as
	// "www.chilkatsoft.com:100".
	void put_Proxy(const char *newVal);

	// If an HTTP proxy is used and it requires authentication, this property specifies
	// the HTTP proxy login.
	void get_ProxyLogin(CkString &str);
	// If an HTTP proxy is used and it requires authentication, this property specifies
	// the HTTP proxy login.
	const char *proxyLogin(void);
	// If an HTTP proxy is used and it requires authentication, this property specifies
	// the HTTP proxy login.
	void put_ProxyLogin(const char *newVal);

	// If an HTTP proxy is used and it requires authentication, this property specifies
	// the HTTP proxy password.
	void get_ProxyPassword(CkString &str);
	// If an HTTP proxy is used and it requires authentication, this property specifies
	// the HTTP proxy password.
	const char *proxyPassword(void);
	// If an HTTP proxy is used and it requires authentication, this property specifies
	// the HTTP proxy password.
	void put_ProxyPassword(const char *newVal);

	// The amount of time in seconds to wait before timing out when reading from an
	// HTTP server. The ReadTimeout is the amount of time that needs to elapse while no
	// additional data is forthcoming. During a long data transfer, if the data stream
	// halts for more than this amount, it will timeout. Otherwise, there is no limit
	// on the length of time for the entire data transfer.
	// 
	// The default value is 20 seconds.
	// 
	int get_ReadTimeout(void);
	// The amount of time in seconds to wait before timing out when reading from an
	// HTTP server. The ReadTimeout is the amount of time that needs to elapse while no
	// additional data is forthcoming. During a long data transfer, if the data stream
	// halts for more than this amount, it will timeout. Otherwise, there is no limit
	// on the length of time for the entire data transfer.
	// 
	// The default value is 20 seconds.
	// 
	void put_ReadTimeout(int newVal);

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

	// Controls whether absolute or relative paths are used when referencing images in
	// the unpacked HTML. The default value is true indicating that relative paths
	// will be used. To use absolute paths, set this property value equal to false.
	bool get_UnpackUseRelPaths(void);
	// Controls whether absolute or relative paths are used when referencing images in
	// the unpacked HTML. The default value is true indicating that relative paths
	// will be used. To use absolute paths, set this property value equal to false.
	void put_UnpackUseRelPaths(bool newVal);

	// Controls whether the cache is automatically updated with the responses from HTTP
	// GET requests. If true, the disk cache is updated, if false (the default),
	// the cache is not updated.
	bool get_UpdateCache(void);
	// Controls whether the cache is automatically updated with the responses from HTTP
	// GET requests. If true, the disk cache is updated, if false (the default),
	// the cache is not updated.
	void put_UpdateCache(bool newVal);

	// Controls whether CID URLs are used for embedded references when generating MHT
	// or EML documents. If UseCids is false, then URLs are left unchanged and the
	// embedded items will contain "content-location" headers that match the URLs in
	// the HTML. If true, CIDs are generated and the URLs within the HTML are
	// replaced with "CID:" links.
	// 
	// The default value of this property is true.
	// 
	bool get_UseCids(void);
	// Controls whether CID URLs are used for embedded references when generating MHT
	// or EML documents. If UseCids is false, then URLs are left unchanged and the
	// embedded items will contain "content-location" headers that match the URLs in
	// the HTML. If true, CIDs are generated and the URLs within the HTML are
	// replaced with "CID:" links.
	// 
	// The default value of this property is true.
	// 
	void put_UseCids(bool newVal);

	// If true, a "filename" attribute is added to each Content-Disposition MIME
	// header field for each embedded item (image, style sheet, etc.). If false, then
	// no filename attribute is added.
	// 
	// The default value of this property is true.
	// 
	bool get_UseFilename(void);
	// If true, a "filename" attribute is added to each Content-Disposition MIME
	// header field for each embedded item (image, style sheet, etc.). If false, then
	// no filename attribute is added.
	// 
	// The default value of this property is true.
	// 
	void put_UseFilename(bool newVal);

	// If true, the proxy host/port used by Internet Explorer will also be used by
	// Chilkat MHT.
	bool get_UseIEProxy(void);
	// If true, the proxy host/port used by Internet Explorer will also be used by
	// Chilkat MHT.
	void put_UseIEProxy(bool newVal);

	// If true, an "inline" attribute is added to each Content-Disposition MIME
	// header field for each embedded item (image, style sheet, etc.). If false, then
	// no inline attribute is added.
	// 
	// The default value of this property is true.
	// 
	bool get_UseInline(void);
	// If true, an "inline" attribute is added to each Content-Disposition MIME
	// header field for each embedded item (image, style sheet, etc.). If false, then
	// no inline attribute is added.
	// 
	// The default value of this property is true.
	// 
	void put_UseInline(bool newVal);

	// (Optional) Specifies the login if a a Web page is accessed that requires a login
	void get_WebSiteLogin(CkString &str);
	// (Optional) Specifies the login if a a Web page is accessed that requires a login
	const char *webSiteLogin(void);
	// (Optional) Specifies the login if a a Web page is accessed that requires a login
	void put_WebSiteLogin(const char *newVal);

	// The optional domain name to be used with NTLM / Kerberos / Negotiate
	// authentication.
	void get_WebSiteLoginDomain(CkString &str);
	// The optional domain name to be used with NTLM / Kerberos / Negotiate
	// authentication.
	const char *webSiteLoginDomain(void);
	// The optional domain name to be used with NTLM / Kerberos / Negotiate
	// authentication.
	void put_WebSiteLoginDomain(const char *newVal);

	// Optional) Specifies the password if a a Web page is accessed that requires a
	// login and password
	void get_WebSitePassword(CkString &str);
	// Optional) Specifies the password if a a Web page is accessed that requires a
	// login and password
	const char *webSitePassword(void);
	// Optional) Specifies the password if a a Web page is accessed that requires a
	// login and password
	void put_WebSitePassword(const char *newVal);

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
	// If disk caching is used, this must be called once for each cache root. For
	// example, if the cache is spread across D:\cacheRoot, E:\cacheRoot, and
	// F:\cacheRoot, an application would setup the cache object by calling AddRoot
	// three times -- once with "D:\cacheRoot", once with "E:\cacheRoot", and once with
	// "F:\cacheRoot".
	void AddCacheRoot(const char *dir);

	// Adds a custom HTTP header to all HTTP requests sent by the MHT component. To add
	// multiple header fields, call this method once for each custom header.
	void AddCustomHeader(const char *name, const char *value);

	// (This method rarely needs to be called.) Includes an additional style sheet that
	// would not normally be included with the HTML. This method is provided for cases
	// when style sheet names are constructed and dynamically included in Javascript
	// such that MHT .NET cannot know beforehand what stylesheet to embed. MHT .NET by
	// default downloads and embeds all stylesheets externally referenced by the HTML
	void AddExternalStyleSheet(const char *url);

	// Removes all custom headers that may have accumulated from previous calls to
	// AddCustomHeader.
	void ClearCustomHeaders(void);

	// (This method rarely needs to be called.) Tells Chilkat MHT .NET to not embed any
	// images whose URL matches a pattern. Sometimes images can be referenced within
	// style sheets and not actually used when rendering the page. In cases like those,
	// the image will appear as an attachment in the HTML email. This feature allows
	// you to explicitly remove those images from the email so no attachments appear.
	void ExcludeImagesMatching(const char *pattern);

	// Creates an EML file from a web page or HTML file. All external images and style
	// sheets are downloaded and embedded in the EML file.
	bool GetAndSaveEML(const char *url, const char *emlFilename);

	// Creates an MHT file from a web page or local HTML file. All external images,
	// scripts, and style sheets are downloaded and embedded in the MHT file.
	bool GetAndSaveMHT(const char *url, const char *mhtFilename);

	// Creates an EML file from a web page or HTML file, compresses, and appends to a
	// new or existing Zip file. All external images and style sheets are downloaded
	// and embedded in the EML.
	bool GetAndZipEML(const char *url, const char *zipEntryFilename, const char *zipFilename);

	// Creates an MHT file from a web page or HTML file, compresses, and appends to a
	// new or existing Zip file. All external images and style sheets are downloaded
	// and embedded in the MHT.
	bool GetAndZipMHT(const char *url, const char *zipEntryFilename, const char *zipFilename);

	// Returns the Nth cache root (indexing begins at 0). Cache roots are set by
	// calling AddCacheRoot one or more times.
	bool GetCacheRoot(int index, CkString &outStr);
	// Returns the Nth cache root (indexing begins at 0). Cache roots are set by
	// calling AddCacheRoot one or more times.
	const char *getCacheRoot(int index);
	// Returns the Nth cache root (indexing begins at 0). Cache roots are set by
	// calling AddCacheRoot one or more times.
	const char *cacheRoot(int index);

	// Creates EML from a web page or HTML file, and returns the EML (MIME) message
	// data as a string.
	bool GetEML(const char *url, CkString &outStr);
	// Creates EML from a web page or HTML file, and returns the EML (MIME) message
	// data as a string.
	const char *getEML(const char *url);
	// Creates EML from a web page or HTML file, and returns the EML (MIME) message
	// data as a string.
	const char *eML(const char *url);

	// Creates MHT from a web page or local HTML file, and returns the MHT (MIME)
	// message data as a string
	bool GetMHT(const char *url, CkString &outStr);
	// Creates MHT from a web page or local HTML file, and returns the MHT (MIME)
	// message data as a string
	const char *getMHT(const char *url);
	// Creates MHT from a web page or local HTML file, and returns the MHT (MIME)
	// message data as a string
	const char *mHT(const char *url);

	// Creates an in-memory EML string from an in-memory HTML string. All external
	// images and style sheets are downloaded and embedded in the EML string that is
	// returned.
	bool HtmlToEML(const char *htmlText, CkString &outStr);
	// Creates an in-memory EML string from an in-memory HTML string. All external
	// images and style sheets are downloaded and embedded in the EML string that is
	// returned.
	const char *htmlToEML(const char *htmlText);

	// Creates an EML file from an in-memory HTML string. All external images and style
	// sheets are downloaded and embedded in the EML file.
	bool HtmlToEMLFile(const char *html, const char *emlFilename);

	// Creates an in-memory MHT web archive from an in-memory HTML string. All external
	// images and style sheets are downloaded and embedded in the MHT string.
	bool HtmlToMHT(const char *htmlText, CkString &outStr);
	// Creates an in-memory MHT web archive from an in-memory HTML string. All external
	// images and style sheets are downloaded and embedded in the MHT string.
	const char *htmlToMHT(const char *htmlText);

	// Creates an MHT file from an in-memory HTML string. All external images and style
	// sheets are downloaded and embedded in the MHT file.
	bool HtmlToMHTFile(const char *html, const char *mhtFilename);

	// Returns true if the MHT component is unlocked.
	bool IsUnlocked(void);

	// Removes a custom header by header field name.
	void RemoveCustomHeader(const char *name);

	// Restores the default property settings.
	void RestoreDefaults(void);

	// Unlocks the component allowing for the full functionality to be used. Returns
	// true if the unlock code is valid.
	bool UnlockComponent(const char *unlockCode);

	// Unpacks the contents of a MHT file. The destination directory is specified by
	//  unpackDir. The name of the HTML file created is specified by  outputHtmlFilename, and supporting
	// files (images, javascripts, etc.) are created in  partsSubDir, which is automatically
	// created if it does not already exist.
	bool UnpackMHT(const char *mhtFilename, const char *unpackDir, const char *htmlFilename, const char *partsSubDir);

	// Same as UnpackMHT, except the MHT is passed in as an in-memory string.
	bool UnpackMHTString(const char *mhtString, const char *unpackDir, const char *htmlFilename, const char *partsSubDir);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
