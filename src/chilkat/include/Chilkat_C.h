
#ifndef _CHILKAT_C
#define _CHILKAT_C

#if !defined(BOOL_IS_TYPEDEF) && !defined(OBJC_BOOL_DEFINED)
#ifndef BOOL
#define BOOL int
#endif
#endif
	
#ifndef TRUE
#define TRUE 1
#endif
	
#ifndef FALSE
#define FALSE 0
#endif	
	
#if !defined(WIN32) && !defined(WINCE)
#include "SystemTime.h"              
#include "FileTime.h"                
#endif                  

#include "ck_inttypes.h"
	
// Use typedefs so we can explicitly see the kind of object pointed
// to by "void *"
	
typedef void *HCkByteData;
typedef void *HCkString;
typedef void *HCkCert;
typedef void *HCkEmail;
typedef void *HCkEmailBundle;
typedef void *HCkMailMan;
typedef void *HCkMailProgress;
typedef void *HCkPrivateKey;
typedef void *HCkPublicKey;
typedef void *HCkCsp;
typedef void *HCkMime;
typedef void *HCkKeyContainer;
typedef void *HCkCertStore;
typedef void *HCkCreateCS;
typedef void *HCkBounce;
typedef void *HCkCharset;
typedef void *HCkCrypt2;
typedef void *HCkCrypt2Progress;
typedef void *HCkFtp2;
typedef void *HCkFtpProgress;
typedef void *HCkHtmlToXml;
typedef void *HCkHtmlToText;
typedef void *HCkHttp;
typedef void *HCkHttpProgress;
typedef void *HCkHttpRequest;
typedef void *HCkHttpResponse;
typedef void *HCkImap;
typedef void *HCkImapProgress;
typedef void *HCkMailboxes;
typedef void *HCkMessageSet;
typedef void *HCkMht;
typedef void *HCkMhtProgress;
typedef void *HCkRar;
typedef void *HCkRarEntry;
typedef void *HCkRsa;
typedef void *HCkSocket;
typedef void *HCkSocketProgress;
typedef void *HCkSpider;
typedef void *HCkSpiderProgress;
typedef void *HCkUpload;
typedef void *HCkCgi;
typedef void *HCkSettings;
typedef void *HCkStringArray;
typedef void *HCkXml;
typedef void *HCkAtom;
typedef void *HCkAtomProgress;
typedef void *HCkRss;
typedef void *HCkRssProgress;
typedef void *HCkZip;
typedef void *HCkZipProgress;
typedef void *HCkZipEntry;
typedef void *HCkZipCrc;
typedef void *HCkCompression;
typedef void *HCkGzip;
typedef void *HCkUnixCompress;
typedef void *HCkSsh;
typedef void *HCkSshProgress;
typedef void *HCkSFtp;
typedef void *HCkSFtpProgress;
typedef void *HCkSFtpDir;
typedef void *HCkSFtpFile;
typedef void *HCkSshKey;
typedef void *HCkTar;
typedef void *HCkTarProgress;
typedef void *HCkBz2;
typedef void *HCkBz2Progress;
typedef void *HCkDh;
typedef void *HCkDhProgress;
typedef void *HCkDsa;
typedef void *HCkDsaProgress;
typedef void *HCkXmp;
typedef void *HCkCache;
typedef void *HCkDkim;
typedef void *HCkDkimProgress;
typedef void *HCkFileAccess;
typedef void *HCkSocksProxy;
typedef void *HCkSocksProxyProgress;
typedef void *HCkDateTime;
typedef void *HCkCsv;
typedef void *HCkSshTunnel;
typedef void *HCkOmaDrm;
typedef void *HCkNtlm;
typedef void *HCkDirTree;
typedef void *HCkDtObj;
typedef void *HCkTrustedRoots;
typedef void *HCkCertChain;
typedef void *HCkPfx;
typedef void *HCkXmlCertVault;


typedef void *HCkByteDataW;
typedef void *HCkStringW;
typedef void *HCkCertW;
typedef void *HCkEmailW;
typedef void *HCkEmailBundleW;
typedef void *HCkMailManW;
typedef void *HCkMailProgressW;
typedef void *HCkPrivateKeyW;
typedef void *HCkPublicKeyW;
typedef void *HCkCspW;
typedef void *HCkMimeW;
typedef void *HCkKeyContainerW;
typedef void *HCkCertStoreW;
typedef void *HCkCreateCSW;
typedef void *HCkBounceW;
typedef void *HCkCharsetW;
typedef void *HCkCrypt2W;
typedef void *HCkCrypt2ProgressW;
typedef void *HCkFtp2W;
typedef void *HCkFtpProgressW;
typedef void *HCkHtmlToXmlW;
typedef void *HCkHtmlToTextW;
typedef void *HCkHttpW;
typedef void *HCkHttpProgressW;
typedef void *HCkHttpRequestW;
typedef void *HCkHttpResponseW;
typedef void *HCkImapW;
typedef void *HCkImapProgressW;
typedef void *HCkMailboxesW;
typedef void *HCkMessageSetW;
typedef void *HCkMhtW;
typedef void *HCkMhtProgressW;
typedef void *HCkRarW;
typedef void *HCkRarEntryW;
typedef void *HCkRsaW;
typedef void *HCkSocketW;
typedef void *HCkSocketProgressW;
typedef void *HCkSpiderW;
typedef void *HCkSpiderProgressW;
typedef void *HCkUploadW;
typedef void *HCkCgiW;
typedef void *HCkSettingsW;
typedef void *HCkStringArrayW;
typedef void *HCkXmlW;
typedef void *HCkAtomW;
typedef void *HCkAtomProgressW;
typedef void *HCkRssW;
typedef void *HCkRssProgressW;
typedef void *HCkZipW;
typedef void *HCkZipProgressW;
typedef void *HCkZipEntryW;
typedef void *HCkZipCrcW;
typedef void *HCkCompressionW;
typedef void *HCkGzipW;
typedef void *HCkUnixCompressW;
typedef void *HCkSshW;
typedef void *HCkSFtpW;
typedef void *HCkSshProgressW;
typedef void *HCkSFtpProgressW;
typedef void *HCkSFtpDirW;
typedef void *HCkSFtpFileW;
typedef void *HCkSshKeyW;
typedef void *HCkTarW;
typedef void *HCkBz2W;
typedef void *HCkDhW;
typedef void *HCkDsaW;
typedef void *HCkTarProgressW;
typedef void *HCkBz2ProgressW;
typedef void *HCkDhProgressW;
typedef void *HCkDsaProgressW;
typedef void *HCkXmpW;
typedef void *HCkCacheW;
typedef void *HCkDkimW;
typedef void *HCkDkimProgressW;
typedef void *HCkFileAccessW;
typedef void *HCkSocksProxyW;
typedef void *HCkSocksProxyProgressW;
typedef void *HCkDateTimeW;
typedef void *HCkCsvW;
typedef void *HCkSshTunnelW;
typedef void *HCkOmaDrmW;
typedef void *HCkNtlmW;
typedef void *HCkDirTreeW;
typedef void *HCkDtObjW;
typedef void *HCkTrustedRootsW;
typedef void *HCkCertChainW;
typedef void *HCkPfxW;
typedef void *HCkXmlCertVaultW;

#endif
