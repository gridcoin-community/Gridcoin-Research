// CkMailManW.h: interface for the CkMailManW class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkMailManW_H
#define _CkMailManW_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkWideCharBase.h"

class CkByteData;
class CkEmailBundleW;
class CkEmailW;
class CkStringArrayW;
class CkCertW;
class CkCspW;
class CkPrivateKeyW;
class CkSshKeyW;
class CkXmlCertVaultW;
class CkMailManProgressW;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkMailManW
class CK_VISIBLE_PUBLIC CkMailManW  : public CkWideCharBase
{
    private:
	bool m_cbOwned;
	CkMailManProgressW *m_callback;

	// Don't allow assignment or copying these objects.
	CkMailManW(const CkMailManW &);
	CkMailManW &operator=(const CkMailManW &);

    public:
	CkMailManW(void);
	virtual ~CkMailManW(void);

	static CkMailManW *createNew(void);
	

	CkMailManW(bool bCallbackOwned);
	static CkMailManW *createNew(bool bCallbackOwned);

	
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	CkMailManProgressW *get_EventCallbackObject(void) const;
	void put_EventCallbackObject(CkMailManProgressW *progress);


	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------
	// Prevents sending any email if any of the addresses in the recipient list are
	// rejected by the SMTP server. The default value is false, which indicates that
	// the mail sending should continue even if some email addresses are invalid.
	// (Note: Not all SMTP servers check the validity of email addresses, and even for
	// those that do, it is not 100% accurate.)
	// 
	// Note: An SMTP server only knows the validity of email addresses within the
	// domain it controls.
	// 
	bool get_AllOrNone(void);
	// Prevents sending any email if any of the addresses in the recipient list are
	// rejected by the SMTP server. The default value is false, which indicates that
	// the mail sending should continue even if some email addresses are invalid.
	// (Note: Not all SMTP servers check the validity of email addresses, and even for
	// those that do, it is not 100% accurate.)
	// 
	// Note: An SMTP server only knows the validity of email addresses within the
	// domain it controls.
	// 
	void put_AllOrNone(bool newVal);

	// If true, then the following will occur when a connection is made to an SMTP or
	// POP3 server:
	// 
	// 1) If the SmtpPort property = 465, then sets StartTLS = false and SmtpSsl =
	// true
	// 2) If the SmtpPort property = 25, sets SmtpSsl = false
	// 3) If the MailPort property = 995, sets PopSsl = true
	// 4) If the MailPort property = 110, sets PopSsl = false
	// 
	// The default value of this property is true.
	// 
	bool get_AutoFix(void);
	// If true, then the following will occur when a connection is made to an SMTP or
	// POP3 server:
	// 
	// 1) If the SmtpPort property = 465, then sets StartTLS = false and SmtpSsl =
	// true
	// 2) If the SmtpPort property = 25, sets SmtpSsl = false
	// 3) If the MailPort property = 995, sets PopSsl = true
	// 4) If the MailPort property = 110, sets PopSsl = false
	// 
	// The default value of this property is true.
	// 
	void put_AutoFix(bool newVal);

	// Controls whether a unique Message-ID header is auto-generated for each email
	// sent.
	// 
	// The Message-ID header field should contain a unique message ID for each email
	// that is sent. The default behavior is to auto-generate this header field at the
	// time the message is sent. This makes it easier for the same email object to be
	// re-used. If the message ID is not unique, the SMTP server may consider the
	// message to be a duplicate of one that has already been sent, and may discard it
	// without sending. This property controls whether message IDs are automatically
	// generated. If auto-generation is turned on (true), the value returned by
	// GetHeaderField("Message-ID") will not reflect the actual message ID that gets
	// sent with the email.
	// 
	// To turn off automatic Message-ID generation, set this property to false.
	// 
	bool get_AutoGenMessageId(void);
	// Controls whether a unique Message-ID header is auto-generated for each email
	// sent.
	// 
	// The Message-ID header field should contain a unique message ID for each email
	// that is sent. The default behavior is to auto-generate this header field at the
	// time the message is sent. This makes it easier for the same email object to be
	// re-used. If the message ID is not unique, the SMTP server may consider the
	// message to be a duplicate of one that has already been sent, and may discard it
	// without sending. This property controls whether message IDs are automatically
	// generated. If auto-generation is turned on (true), the value returned by
	// GetHeaderField("Message-ID") will not reflect the actual message ID that gets
	// sent with the email.
	// 
	// To turn off automatic Message-ID generation, set this property to false.
	// 
	void put_AutoGenMessageId(bool newVal);

	// If true, then the SMTP "RSET" command is automatically sent to ensure that the
	// SMTP connection is in a valid state when a new email is about to be sent on an
	// already established connection. The default value is true.
	// 
	// Important: This property only applies when an email is sent on an already-open
	// SMTP connection.
	// 
	bool get_AutoSmtpRset(void);
	// If true, then the SMTP "RSET" command is automatically sent to ensure that the
	// SMTP connection is in a valid state when a new email is about to be sent on an
	// already established connection. The default value is true.
	// 
	// Important: This property only applies when an email is sent on an already-open
	// SMTP connection.
	// 
	void put_AutoSmtpRset(bool newVal);

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

	// The time (in seconds) to wait before while trying to connect to a mail server
	// (POP3 or SMTP). The default value is 30.
	int get_ConnectTimeout(void);
	// The time (in seconds) to wait before while trying to connect to a mail server
	// (POP3 or SMTP). The default value is 30.
	void put_ConnectTimeout(int newVal);

	// (An SMTP DSN service extension feature) An arbitrary string that will be used as
	// the ENVID property when sending email. See RFC 3461 for more details.
	void get_DsnEnvid(CkString &str);
	// (An SMTP DSN service extension feature) An arbitrary string that will be used as
	// the ENVID property when sending email. See RFC 3461 for more details.
	const wchar_t *dsnEnvid(void);
	// (An SMTP DSN service extension feature) An arbitrary string that will be used as
	// the ENVID property when sending email. See RFC 3461 for more details.
	void put_DsnEnvid(const wchar_t *newVal);

	// (An SMTP DSN service extension feature) A string that will be used as the NOTIFY
	// parameter when sending email. (See RFC 3461 for more details. ) This string can
	// be left blank, or can be set to "NEVER", or any combination of a comma-separated
	// list of "SUCCESS", "FAILURE", or "NOTIFY".
	void get_DsnNotify(CkString &str);
	// (An SMTP DSN service extension feature) A string that will be used as the NOTIFY
	// parameter when sending email. (See RFC 3461 for more details. ) This string can
	// be left blank, or can be set to "NEVER", or any combination of a comma-separated
	// list of "SUCCESS", "FAILURE", or "NOTIFY".
	const wchar_t *dsnNotify(void);
	// (An SMTP DSN service extension feature) A string that will be used as the NOTIFY
	// parameter when sending email. (See RFC 3461 for more details. ) This string can
	// be left blank, or can be set to "NEVER", or any combination of a comma-separated
	// list of "SUCCESS", "FAILURE", or "NOTIFY".
	void put_DsnNotify(const wchar_t *newVal);

	// (An SMTP DSN service extension feature) A string that will be used as the RET
	// parameter when sending email. (See RFC 3461 for more details. ) This string can
	// be left blank, or can be set to "FULL" to receive entire-message DSN
	// notifications, or "HDRS" to receive header-only DSN notifications.
	void get_DsnRet(CkString &str);
	// (An SMTP DSN service extension feature) A string that will be used as the RET
	// parameter when sending email. (See RFC 3461 for more details. ) This string can
	// be left blank, or can be set to "FULL" to receive entire-message DSN
	// notifications, or "HDRS" to receive header-only DSN notifications.
	const wchar_t *dsnRet(void);
	// (An SMTP DSN service extension feature) A string that will be used as the RET
	// parameter when sending email. (See RFC 3461 for more details. ) This string can
	// be left blank, or can be set to "FULL" to receive entire-message DSN
	// notifications, or "HDRS" to receive header-only DSN notifications.
	void put_DsnRet(const wchar_t *newVal);

	// If true, causes the digital certificate chain to be embedded in signed emails.
	// The certificates in the chain of authentication are embedded up to but not
	// including the root certificate. If the IncludeRootCert property is also true,
	// then the root CA certificate is also included in the S/MIME signature.
	bool get_EmbedCertChain(void);
	// If true, causes the digital certificate chain to be embedded in signed emails.
	// The certificates in the chain of authentication are embedded up to but not
	// including the root certificate. If the IncludeRootCert property is also true,
	// then the root CA certificate is also included in the S/MIME signature.
	void put_EmbedCertChain(bool newVal);

	// An expression that is applied to any of the following method calls when present:
	// LoadXmlFile, LoadXmlString, LoadMbx, CopyMail, and TransferMail. For these
	// methods, only the emails that match the filter's expression are returned in the
	// email bundle. In the case of TransferMail, only the matching emails are removed
	// from the mail server. The filter allows any header field, or the body, to be
	// checked.
	// Here are some examples of expressions:
	// 
	// Body like "mortgage rates*". 
	// Subject contains "update" and From contains "chilkat" 
	// To = "info@chilkatsoft.com" 
	// 
	// Here are the general rules for forming filter expressions:
	// 
	// Any MIME header field name can be used, case is insensitive. 
	// Literal strings are double-quoted, and case is insensitive. 
	// The "*" wildcard matches 0 or more occurances of any character. 
	// Parentheses can be used to control precedence. 
	// The logical operators are: AND, OR, NOT (case insensitive) 
	// Comparison operators are: =, , =, String comparison operators are: CONTAINS, LIKE (case insensitive)
	void get_Filter(CkString &str);
	// An expression that is applied to any of the following method calls when present:
	// LoadXmlFile, LoadXmlString, LoadMbx, CopyMail, and TransferMail. For these
	// methods, only the emails that match the filter's expression are returned in the
	// email bundle. In the case of TransferMail, only the matching emails are removed
	// from the mail server. The filter allows any header field, or the body, to be
	// checked.
	// Here are some examples of expressions:
	// 
	// Body like "mortgage rates*". 
	// Subject contains "update" and From contains "chilkat" 
	// To = "info@chilkatsoft.com" 
	// 
	// Here are the general rules for forming filter expressions:
	// 
	// Any MIME header field name can be used, case is insensitive. 
	// Literal strings are double-quoted, and case is insensitive. 
	// The "*" wildcard matches 0 or more occurances of any character. 
	// Parentheses can be used to control precedence. 
	// The logical operators are: AND, OR, NOT (case insensitive) 
	// Comparison operators are: =, , =, String comparison operators are: CONTAINS, LIKE (case insensitive)
	const wchar_t *filter(void);
	// An expression that is applied to any of the following method calls when present:
	// LoadXmlFile, LoadXmlString, LoadMbx, CopyMail, and TransferMail. For these
	// methods, only the emails that match the filter's expression are returned in the
	// email bundle. In the case of TransferMail, only the matching emails are removed
	// from the mail server. The filter allows any header field, or the body, to be
	// checked.
	// Here are some examples of expressions:
	// 
	// Body like "mortgage rates*". 
	// Subject contains "update" and From contains "chilkat" 
	// To = "info@chilkatsoft.com" 
	// 
	// Here are the general rules for forming filter expressions:
	// 
	// Any MIME header field name can be used, case is insensitive. 
	// Literal strings are double-quoted, and case is insensitive. 
	// The "*" wildcard matches 0 or more occurances of any character. 
	// Parentheses can be used to control precedence. 
	// The logical operators are: AND, OR, NOT (case insensitive) 
	// Comparison operators are: =, , =, String comparison operators are: CONTAINS, LIKE (case insensitive)
	void put_Filter(const wchar_t *newVal);

	// The time interval, in milliseconds, between AbortCheck event callbacks. The
	// heartbeat provides a means for an application to monitor a mail-sending and/or
	// mail-reading method call, and to abort it while in progress.
	int get_HeartbeatMs(void);
	// The time interval, in milliseconds, between AbortCheck event callbacks. The
	// heartbeat provides a means for an application to monitor a mail-sending and/or
	// mail-reading method call, and to abort it while in progress.
	void put_HeartbeatMs(int newVal);

	// Specifies the hostname to be used for the EHLO/HELO command sent to an SMTP
	// server. By default, this property is an empty string which causes the local
	// hostname to be used.
	void get_HeloHostname(CkString &str);
	// Specifies the hostname to be used for the EHLO/HELO command sent to an SMTP
	// server. By default, this property is an empty string which causes the local
	// hostname to be used.
	const wchar_t *heloHostname(void);
	// Specifies the hostname to be used for the EHLO/HELO command sent to an SMTP
	// server. By default, this property is an empty string which causes the local
	// hostname to be used.
	void put_HeloHostname(const wchar_t *newVal);

	// If an HTTP proxy requiring authentication is to be used, set this property to
	// the HTTP proxy authentication method name. Valid choices are "LOGIN" or "NTLM".
	void get_HttpProxyAuthMethod(CkString &str);
	// If an HTTP proxy requiring authentication is to be used, set this property to
	// the HTTP proxy authentication method name. Valid choices are "LOGIN" or "NTLM".
	const wchar_t *httpProxyAuthMethod(void);
	// If an HTTP proxy requiring authentication is to be used, set this property to
	// the HTTP proxy authentication method name. Valid choices are "LOGIN" or "NTLM".
	void put_HttpProxyAuthMethod(const wchar_t *newVal);

	// The NTLM authentication domain (optional) if NTLM authentication is used.
	void get_HttpProxyDomain(CkString &str);
	// The NTLM authentication domain (optional) if NTLM authentication is used.
	const wchar_t *httpProxyDomain(void);
	// The NTLM authentication domain (optional) if NTLM authentication is used.
	void put_HttpProxyDomain(const wchar_t *newVal);

	// If an HTTP proxy is to be used, set this property to the HTTP proxy hostname or
	// IPv4 address (in dotted decimal notation).
	void get_HttpProxyHostname(CkString &str);
	// If an HTTP proxy is to be used, set this property to the HTTP proxy hostname or
	// IPv4 address (in dotted decimal notation).
	const wchar_t *httpProxyHostname(void);
	// If an HTTP proxy is to be used, set this property to the HTTP proxy hostname or
	// IPv4 address (in dotted decimal notation).
	void put_HttpProxyHostname(const wchar_t *newVal);

	// If an HTTP proxy requiring authentication is to be used, set this property to
	// the HTTP proxy password.
	void get_HttpProxyPassword(CkString &str);
	// If an HTTP proxy requiring authentication is to be used, set this property to
	// the HTTP proxy password.
	const wchar_t *httpProxyPassword(void);
	// If an HTTP proxy requiring authentication is to be used, set this property to
	// the HTTP proxy password.
	void put_HttpProxyPassword(const wchar_t *newVal);

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
	const wchar_t *httpProxyUsername(void);
	// If an HTTP proxy requiring authentication is to be used, set this property to
	// the HTTP proxy login name.
	void put_HttpProxyUsername(const wchar_t *newVal);

	// If true (the default) then any method that deletes an email from the POP3
	// server will also issue a QUIT command to close the session to ensure the message
	// is deleted immediately.
	// 
	// The POP3 protocol is such that the DELE command marks a message for deletion. It
	// is not actually deleted until the QUIT command is sent and the session is
	// closed. If ImmediateDelete is true, then any Chilkat MailMan method that marks
	// a message (or messages) for deletion will also followup with a QUIT command and
	// close the session. If your program sets ImmediateDelete to false, it must make
	// sure to call Pop3EndSession to ensure that messages marked for deletion are
	// actually deleted.
	// 
	bool get_ImmediateDelete(void);
	// If true (the default) then any method that deletes an email from the POP3
	// server will also issue a QUIT command to close the session to ensure the message
	// is deleted immediately.
	// 
	// The POP3 protocol is such that the DELE command marks a message for deletion. It
	// is not actually deleted until the QUIT command is sent and the session is
	// closed. If ImmediateDelete is true, then any Chilkat MailMan method that marks
	// a message (or messages) for deletion will also followup with a QUIT command and
	// close the session. If your program sets ImmediateDelete to false, it must make
	// sure to call Pop3EndSession to ensure that messages marked for deletion are
	// actually deleted.
	// 
	void put_ImmediateDelete(bool newVal);

	// Controls whether the root certificate in the chain of authentication (i.e. the
	// CA root certificate) is included within the S/MIME signature of a signed email.
	// Note: This property only applies if the EmbedCertChain property is also true.
	bool get_IncludeRootCert(void);
	// Controls whether the root certificate in the chain of authentication (i.e. the
	// CA root certificate) is included within the S/MIME signature of a signed email.
	// Note: This property only applies if the EmbedCertChain property is also true.
	void put_IncludeRootCert(bool newVal);

	// Returns true if still connected to the SMTP server. Otherwise returns false
	// (if there was never a connection in the first place, or if the connection was
	// lost in a previous method call).
	// 
	// Note: Accessing this property does not trigger any communication with the SMTP
	// server. A connection to the SMTP server is established by explicitly calling
	// OpenSmtpConnection, or it is implicitly established as needed by any method that
	// requires communication. A lost connection will only be detected when attempting
	// to communicate with the server. To truly determine if a connection to the SMTP
	// server is open and valid, one might call the SmtpNoop method instead. This
	// property can return true if the server has disconnected, but the client has
	// not attempted to communicate with the server since the disconnect.
	// 
	bool get_IsSmtpConnected(void);

	// The name of the file created in the SMTPQ's queue directory for the last email
	// sent via SendQ, SendQ2, or SendMimeQ.
	void get_LastSendQFilename(CkString &str);
	// The name of the file created in the SMTPQ's queue directory for the last email
	// sent via SendQ, SendQ2, or SendMimeQ.
	const wchar_t *lastSendQFilename(void);

	// Returns the last SMTP diagnostic status code. This can be checked after sending
	// an email. SMTP reply codes are defined by RFC 821 - Simple Mail Transfer
	// Protocol.
	int get_LastSmtpStatus(void);

	// A log filename where the MailMan will log each message in the exact form it was
	// received from a POP3 server. This property is provided for help in debugging.
	void get_LogMailReceivedFilename(CkString &str);
	// A log filename where the MailMan will log each message in the exact form it was
	// received from a POP3 server. This property is provided for help in debugging.
	const wchar_t *logMailReceivedFilename(void);
	// A log filename where the MailMan will log each message in the exact form it was
	// received from a POP3 server. This property is provided for help in debugging.
	void put_LogMailReceivedFilename(const wchar_t *newVal);

	// A log filename where the MailMan will log the exact message sent to the SMTP
	// server. This property is helpful in debugging.
	void get_LogMailSentFilename(CkString &str);
	// A log filename where the MailMan will log the exact message sent to the SMTP
	// server. This property is helpful in debugging.
	const wchar_t *logMailSentFilename(void);
	// A log filename where the MailMan will log the exact message sent to the SMTP
	// server. This property is helpful in debugging.
	void put_LogMailSentFilename(const wchar_t *newVal);

	// The domain name of the POP3 server. Do not include "http://" in the domain name.
	// This property may also be set to an IP address string, such as "168.144.70.227".
	// Both IPv4 and IPv6 address formats are supported.
	void get_MailHost(CkString &str);
	// The domain name of the POP3 server. Do not include "http://" in the domain name.
	// This property may also be set to an IP address string, such as "168.144.70.227".
	// Both IPv4 and IPv6 address formats are supported.
	const wchar_t *mailHost(void);
	// The domain name of the POP3 server. Do not include "http://" in the domain name.
	// This property may also be set to an IP address string, such as "168.144.70.227".
	// Both IPv4 and IPv6 address formats are supported.
	void put_MailHost(const wchar_t *newVal);

	// The port number of the POP3 server. Only needs to be set if the POP3 server is
	// running on a non-standard port. The default value is 110. (If SSL/TLS is used by
	// setting the PopSsl property = true, then this property should probably be set
	// to 995, which is the standard SSL/TLS port for POP3.)
	int get_MailPort(void);
	// The port number of the POP3 server. Only needs to be set if the POP3 server is
	// running on a non-standard port. The default value is 110. (If SSL/TLS is used by
	// setting the PopSsl property = true, then this property should probably be set
	// to 995, which is the standard SSL/TLS port for POP3.)
	void put_MailPort(int newVal);

	// Limits the number of messages the MailMan will try to retrieve from the POP3
	// server in a single method call. If you are trying to read a large mailbox, you
	// might set this to a value such as 100 to download 100 emails at a time.
	int get_MaxCount(void);
	// Limits the number of messages the MailMan will try to retrieve from the POP3
	// server in a single method call. If you are trying to read a large mailbox, you
	// might set this to a value such as 100 to download 100 emails at a time.
	void put_MaxCount(int newVal);

	// When set to true, signed emails are sent using opaque signing. The default is
	// to send clear-text (multipart/signed) emails.
	bool get_OpaqueSigning(void);
	// When set to true, signed emails are sent using opaque signing. The default is
	// to send clear-text (multipart/signed) emails.
	void put_OpaqueSigning(bool newVal);

	// Controls whether SPA authentication for POP3 is used or not. To use SPA
	// authentication, set this propoerty = true. No other programming changes are
	// required. The default value is false.
	bool get_Pop3SPA(void);
	// Controls whether SPA authentication for POP3 is used or not. To use SPA
	// authentication, set this propoerty = true. No other programming changes are
	// required. The default value is false.
	void put_Pop3SPA(bool newVal);

	// 0 if no POP3 session is active. Otherwise a positive integer that is incremented
	// with each new POP3 session. It may be used to determine if a new POP3 session
	// has been established.
	int get_Pop3SessionId(void);

	// This string property accumulates the raw commands sent to the POP3 server, and
	// the raw responses received from the POP3 server. This property is read-only, but
	// it may be cleared by calling ClearPop3SessionLog.
	void get_Pop3SessionLog(CkString &str);
	// This string property accumulates the raw commands sent to the POP3 server, and
	// the raw responses received from the POP3 server. This property is read-only, but
	// it may be cleared by calling ClearPop3SessionLog.
	const wchar_t *pop3SessionLog(void);

	// When connecting via SSL, this property is true if the POP3 server's SSL
	// certificate was verified. Otherwise it is set to false.
	bool get_Pop3SslServerCertVerified(void);

	// If true, then an unencrypted connection (typically on port 110) is
	// automatically converted to a secure TLS connection via the STLS command (see RFC
	// 2595) when connecting. This should only be used with POP3 servers that support
	// the STLS capability. If this property is set to true, then the PopSsl property
	// should be set to false. (The PopSsl property controls whether the connection
	// is SSL/TLS from the beginning. Setting the Pop3Stls property = true indicates
	// that the POP3 client will initially connect unencrypted and then convert to
	// TLS.)
	bool get_Pop3Stls(void);
	// If true, then an unencrypted connection (typically on port 110) is
	// automatically converted to a secure TLS connection via the STLS command (see RFC
	// 2595) when connecting. This should only be used with POP3 servers that support
	// the STLS capability. If this property is set to true, then the PopSsl property
	// should be set to false. (The PopSsl property controls whether the connection
	// is SSL/TLS from the beginning. Setting the Pop3Stls property = true indicates
	// that the POP3 client will initially connect unencrypted and then convert to
	// TLS.)
	void put_Pop3Stls(bool newVal);

	// The POP3 password.
	// 
	// If the Pop3SPA property is set, the PopUsername and PopPassword properties may
	// be set to the string "default" to cause the component to use the current
	// logged-on credentials (of the calling process) for authentication.
	// 
	void get_PopPassword(CkString &str);
	// The POP3 password.
	// 
	// If the Pop3SPA property is set, the PopUsername and PopPassword properties may
	// be set to the string "default" to cause the component to use the current
	// logged-on credentials (of the calling process) for authentication.
	// 
	const wchar_t *popPassword(void);
	// The POP3 password.
	// 
	// If the Pop3SPA property is set, the PopUsername and PopPassword properties may
	// be set to the string "default" to cause the component to use the current
	// logged-on credentials (of the calling process) for authentication.
	// 
	void put_PopPassword(const wchar_t *newVal);

	// Provides a way to specify the POP3 password from a Base64-encoded string.
	void get_PopPasswordBase64(CkString &str);
	// Provides a way to specify the POP3 password from a Base64-encoded string.
	const wchar_t *popPasswordBase64(void);
	// Provides a way to specify the POP3 password from a Base64-encoded string.
	void put_PopPasswordBase64(const wchar_t *newVal);

	// Controls whether TLS/SSL is used when reading email from a POP3 server. Note:
	// Check first to determine if your POP3 server can accept TLS/SSL connections.
	// Also, be sure to set the MailPort property to the TLS/SSL POP3 port number,
	// which is typically 995.
	bool get_PopSsl(void);
	// Controls whether TLS/SSL is used when reading email from a POP3 server. Note:
	// Check first to determine if your POP3 server can accept TLS/SSL connections.
	// Also, be sure to set the MailPort property to the TLS/SSL POP3 port number,
	// which is typically 995.
	void put_PopSsl(bool newVal);

	// The POP3 login name.
	// 
	// If the Pop3SPA property is set, the PopUsername and PopPassword properties may
	// be set to the string "default" to cause the component to use the current
	// logged-on credentials (of the calling process) for authentication.
	// 
	void get_PopUsername(CkString &str);
	// The POP3 login name.
	// 
	// If the Pop3SPA property is set, the PopUsername and PopPassword properties may
	// be set to the string "default" to cause the component to use the current
	// logged-on credentials (of the calling process) for authentication.
	// 
	const wchar_t *popUsername(void);
	// The POP3 login name.
	// 
	// If the Pop3SPA property is set, the PopUsername and PopPassword properties may
	// be set to the string "default" to cause the component to use the current
	// logged-on credentials (of the calling process) for authentication.
	// 
	void put_PopUsername(const wchar_t *newVal);

	// The maximum time to wait, in seconds, if the POP3 or SMTP server stops
	// responding. The default value is 30 seconds.
	int get_ReadTimeout(void);
	// The maximum time to wait, in seconds, if the POP3 or SMTP server stops
	// responding. The default value is 30 seconds.
	void put_ReadTimeout(int newVal);

	// If true, then the mailman will verify the SMTP or POP3 server's SSL
	// certificate when connecting. The certificate is expired, or if the cert's
	// signature is invalid, the connection is not allowed. The default value of this
	// property is false. (Obviously, this only applies to SSL/TLS connections.)
	bool get_RequireSslCertVerify(void);
	// If true, then the mailman will verify the SMTP or POP3 server's SSL
	// certificate when connecting. The certificate is expired, or if the cert's
	// signature is invalid, the connection is not allowed. The default value of this
	// property is false. (Obviously, this only applies to SSL/TLS connections.)
	void put_RequireSslCertVerify(bool newVal);

	// Controls whether the Date header field is reset to the current date/time when an
	// email is loaded from LoadMbx, LoadEml, LoadMime, LoadXml, or LoadXmlString. The
	// default is false (to not reset the date). To automatically reset the date, set
	// this property equal to true.
	bool get_ResetDateOnLoad(void);
	// Controls whether the Date header field is reset to the current date/time when an
	// email is loaded from LoadMbx, LoadEml, LoadMime, LoadXml, or LoadXmlString. The
	// default is false (to not reset the date). To automatically reset the date, set
	// this property equal to true.
	void put_ResetDateOnLoad(bool newVal);

	// The buffer size to be used with the underlying TCP/IP socket for sending. The
	// default value is 32767.
	int get_SendBufferSize(void);
	// The buffer size to be used with the underlying TCP/IP socket for sending. The
	// default value is 32767.
	void put_SendBufferSize(int newVal);

	// Determines how emails are sent to distribution lists. If true, emails are sent
	// to each recipient in the list one at a time, with the "To"header field
	// containing the email address of the recipient. If false, emails will contain
	// in the "To"header field, and are sent to 100 BCC recipients at a time. As an
	// example, if your distribution list contained 350 email addresses, 4 emails would
	// be sent, the first 3 having 100 BCC recipients, and the last email with 50 BCC
	// recipients.The default value of this property is true.
	bool get_SendIndividual(void);
	// Determines how emails are sent to distribution lists. If true, emails are sent
	// to each recipient in the list one at a time, with the "To"header field
	// containing the email address of the recipient. If false, emails will contain
	// in the "To"header field, and are sent to 100 BCC recipients at a time. As an
	// example, if your distribution list contained 350 email addresses, 4 emails would
	// be sent, the first 3 having 100 BCC recipients, and the last email with 50 BCC
	// recipients.The default value of this property is true.
	void put_SendIndividual(bool newVal);

	// The MailMan will not try to retrieve mail messages from a POP3 server that are
	// greater than this size limit. The default value is 0 indicating no size limit.
	// The SizeLimit is specified in number of bytes.
	int get_SizeLimit(void);
	// The MailMan will not try to retrieve mail messages from a POP3 server that are
	// greater than this size limit. The default value is 0 indicating no size limit.
	// The SizeLimit is specified in number of bytes.
	void put_SizeLimit(int newVal);

	// This property should usually be left empty. The MailMan will by default choose
	// the most secure login method available to prevent unencrypted username and
	// passwords from being transmitted if possible. However, some SMTP servers may not
	// advertise the acceptable authorization methods, and therefore it is not possible
	// to automatically determine the best authorization method. To force a particular
	// auth method, or to prevent any authorization from being used, set this property
	// to one of the following values: "NONE", "LOGIN", "PLAIN", "CRAM-MD5", or "NTLM".
	void get_SmtpAuthMethod(CkString &str);
	// This property should usually be left empty. The MailMan will by default choose
	// the most secure login method available to prevent unencrypted username and
	// passwords from being transmitted if possible. However, some SMTP servers may not
	// advertise the acceptable authorization methods, and therefore it is not possible
	// to automatically determine the best authorization method. To force a particular
	// auth method, or to prevent any authorization from being used, set this property
	// to one of the following values: "NONE", "LOGIN", "PLAIN", "CRAM-MD5", or "NTLM".
	const wchar_t *smtpAuthMethod(void);
	// This property should usually be left empty. The MailMan will by default choose
	// the most secure login method available to prevent unencrypted username and
	// passwords from being transmitted if possible. However, some SMTP servers may not
	// advertise the acceptable authorization methods, and therefore it is not possible
	// to automatically determine the best authorization method. To force a particular
	// auth method, or to prevent any authorization from being used, set this property
	// to one of the following values: "NONE", "LOGIN", "PLAIN", "CRAM-MD5", or "NTLM".
	void put_SmtpAuthMethod(const wchar_t *newVal);

	// The domain name of the SMTP server. Do not include "http://" in the domain name.
	// This property may also be set to an IP address string, such as "168.144.70.227".
	// Both IPv4 and IPv6 address formats are supported.
	void get_SmtpHost(CkString &str);
	// The domain name of the SMTP server. Do not include "http://" in the domain name.
	// This property may also be set to an IP address string, such as "168.144.70.227".
	// Both IPv4 and IPv6 address formats are supported.
	const wchar_t *smtpHost(void);
	// The domain name of the SMTP server. Do not include "http://" in the domain name.
	// This property may also be set to an IP address string, such as "168.144.70.227".
	// Both IPv4 and IPv6 address formats are supported.
	void put_SmtpHost(const wchar_t *newVal);

	// The Windows domain for logging into the SMTP server. Use this only if your SMTP
	// server requires NTLM authentication, which means your SMTP server uses
	// Integrated Windows Authentication. If there is no domain, this can be left
	// empty.
	void get_SmtpLoginDomain(CkString &str);
	// The Windows domain for logging into the SMTP server. Use this only if your SMTP
	// server requires NTLM authentication, which means your SMTP server uses
	// Integrated Windows Authentication. If there is no domain, this can be left
	// empty.
	const wchar_t *smtpLoginDomain(void);
	// The Windows domain for logging into the SMTP server. Use this only if your SMTP
	// server requires NTLM authentication, which means your SMTP server uses
	// Integrated Windows Authentication. If there is no domain, this can be left
	// empty.
	void put_SmtpLoginDomain(const wchar_t *newVal);

	// The password for logging into the SMTP server. Use this only if your SMTP server
	// requires authentication. Chilkat Email.NET supports the LOGIN, PLAIN, CRAM-MD5,
	// and NTLM login methods, and it will automatically choose the most secure method
	// available. Additional login methods will be available in the future.
	// 
	// If NTLM (Windows-Integrated) authentication is used, the SmtpUsername and
	// SmtpPassword properties may be set to the string "default" to cause the
	// component to use the current logged-on credentials (of the calling process) for
	// authentication.
	// 
	void get_SmtpPassword(CkString &str);
	// The password for logging into the SMTP server. Use this only if your SMTP server
	// requires authentication. Chilkat Email.NET supports the LOGIN, PLAIN, CRAM-MD5,
	// and NTLM login methods, and it will automatically choose the most secure method
	// available. Additional login methods will be available in the future.
	// 
	// If NTLM (Windows-Integrated) authentication is used, the SmtpUsername and
	// SmtpPassword properties may be set to the string "default" to cause the
	// component to use the current logged-on credentials (of the calling process) for
	// authentication.
	// 
	const wchar_t *smtpPassword(void);
	// The password for logging into the SMTP server. Use this only if your SMTP server
	// requires authentication. Chilkat Email.NET supports the LOGIN, PLAIN, CRAM-MD5,
	// and NTLM login methods, and it will automatically choose the most secure method
	// available. Additional login methods will be available in the future.
	// 
	// If NTLM (Windows-Integrated) authentication is used, the SmtpUsername and
	// SmtpPassword properties may be set to the string "default" to cause the
	// component to use the current logged-on credentials (of the calling process) for
	// authentication.
	// 
	void put_SmtpPassword(const wchar_t *newVal);

	// The port number of the SMTP server used to send email. Only needs to be set if
	// the SMTP server is running on a non-standard port. The default value is 25. If
	// SmtpSsl is set to true, this property should be set to 465. (TCP port 465 is
	// reserved by common industry practice for secure SMTP communication using the SSL
	// protocol.)
	int get_SmtpPort(void);
	// The port number of the SMTP server used to send email. Only needs to be set if
	// the SMTP server is running on a non-standard port. The default value is 25. If
	// SmtpSsl is set to true, this property should be set to 465. (TCP port 465 is
	// reserved by common industry practice for secure SMTP communication using the SSL
	// protocol.)
	void put_SmtpPort(int newVal);

	// This string property accumulates the raw commands sent to the SMTP server, and
	// the raw responses received from the SMTP server. This property is read-only, but
	// it may be cleared by calling ClearSmtpSessionLog.
	void get_SmtpSessionLog(CkString &str);
	// This string property accumulates the raw commands sent to the SMTP server, and
	// the raw responses received from the SMTP server. This property is read-only, but
	// it may be cleared by calling ClearSmtpSessionLog.
	const wchar_t *smtpSessionLog(void);

	// When set to true, causes the mailman to connect to the SMTP server via the
	// TLS/SSL protocol.
	bool get_SmtpSsl(void);
	// When set to true, causes the mailman to connect to the SMTP server via the
	// TLS/SSL protocol.
	void put_SmtpSsl(bool newVal);

	// If using SSL, this property will be set to true if the SMTP server's SSL
	// certificate was verified when establishing the connection. Otherwise it is set
	// to false.
	bool get_SmtpSslServerCertVerified(void);

	// The login for logging into the SMTP server. Use this only if your SMTP server
	// requires authentication.
	// 
	// Note: In many cases, an SMTP server will not require authentication when sending
	// to an email address local to it's domain. However, when sending email to an
	// external domain, authentication is required (i.e. the SMTP server is being used
	// as a relay).
	// 
	// If the Pop3SPA property is set, the PopUsername and PopPassword properties may
	// be set to the string "default" to cause the component to use the current
	// logged-on credentials (of the calling process) for authentication.
	// 
	void get_SmtpUsername(CkString &str);
	// The login for logging into the SMTP server. Use this only if your SMTP server
	// requires authentication.
	// 
	// Note: In many cases, an SMTP server will not require authentication when sending
	// to an email address local to it's domain. However, when sending email to an
	// external domain, authentication is required (i.e. the SMTP server is being used
	// as a relay).
	// 
	// If the Pop3SPA property is set, the PopUsername and PopPassword properties may
	// be set to the string "default" to cause the component to use the current
	// logged-on credentials (of the calling process) for authentication.
	// 
	const wchar_t *smtpUsername(void);
	// The login for logging into the SMTP server. Use this only if your SMTP server
	// requires authentication.
	// 
	// Note: In many cases, an SMTP server will not require authentication when sending
	// to an email address local to it's domain. However, when sending email to an
	// external domain, authentication is required (i.e. the SMTP server is being used
	// as a relay).
	// 
	// If the Pop3SPA property is set, the PopUsername and PopPassword properties may
	// be set to the string "default" to cause the component to use the current
	// logged-on credentials (of the calling process) for authentication.
	// 
	void put_SmtpUsername(const wchar_t *newVal);

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
	// Note: This property only applies to FTP data connections. The control connection
	// (for sending commands and getting responses) is not typically a performance
	// issue.
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
	// Note: This property only applies to FTP data connections. The control connection
	// (for sending commands and getting responses) is not typically a performance
	// issue.
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

	// May be set to one of the following integer values:
	// 
	// 0 - No SOCKS proxy is used. This is the default.
	// 4 - Connect via a SOCKS4 proxy.
	// 5 - Connect via a SOCKS5 proxy.
	// 
	int get_SocksVersion(void);
	// May be set to one of the following integer values:
	// 
	// 0 - No SOCKS proxy is used. This is the default.
	// 4 - Connect via a SOCKS4 proxy.
	// 5 - Connect via a SOCKS5 proxy.
	// 
	void put_SocksVersion(int newVal);

	// When set to true, causes the mailman to issue a STARTTLS command to switch
	// over to a secure SSL/TLS connection prior to authenticating and sending email.
	// The default value is false.
	bool get_StartTLS(void);
	// When set to true, causes the mailman to issue a STARTTLS command to switch
	// over to a secure SSL/TLS connection prior to authenticating and sending email.
	// The default value is false.
	void put_StartTLS(bool newVal);

	// If true, will automatically use APOP authentication if the POP3 server
	// supports it. The default value of this property is false.
	bool get_UseApop(void);
	// If true, will automatically use APOP authentication if the POP3 server
	// supports it. The default value of this property is false.
	void put_UseApop(bool newVal);

	// The filename attribute to be used in the Content-Disposition header field when
	// sending a PCKS7 encrypted email. The default value is "smime.p7m".
	void get_P7mEncryptAttachFilename(CkString &str);
	// The filename attribute to be used in the Content-Disposition header field when
	// sending a PCKS7 encrypted email. The default value is "smime.p7m".
	const wchar_t *p7mEncryptAttachFilename(void);
	// The filename attribute to be used in the Content-Disposition header field when
	// sending a PCKS7 encrypted email. The default value is "smime.p7m".
	void put_P7mEncryptAttachFilename(const wchar_t *newVal);

	// The filename attribute to be used in the Content-Disposition header field when
	// sending a PCKS7 opaque signed email. The default value is "smime.p7m".
	void get_P7mSigAttachFilename(CkString &str);
	// The filename attribute to be used in the Content-Disposition header field when
	// sending a PCKS7 opaque signed email. The default value is "smime.p7m".
	const wchar_t *p7mSigAttachFilename(void);
	// The filename attribute to be used in the Content-Disposition header field when
	// sending a PCKS7 opaque signed email. The default value is "smime.p7m".
	void put_P7mSigAttachFilename(const wchar_t *newVal);

	// The filename attribute to be used in the Content-Disposition header field when
	// sending a signed email with a detached PKCS7 signature. The default value is
	// "smime.p7s".
	void get_P7sSigAttachFilename(CkString &str);
	// The filename attribute to be used in the Content-Disposition header field when
	// sending a signed email with a detached PKCS7 signature. The default value is
	// "smime.p7s".
	const wchar_t *p7sSigAttachFilename(void);
	// The filename attribute to be used in the Content-Disposition header field when
	// sending a signed email with a detached PKCS7 signature. The default value is
	// "smime.p7s".
	void put_P7sSigAttachFilename(const wchar_t *newVal);

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
	// Adds a PFX to the object's internal list of sources to be searched for
	// certificates and private keys when decrypting or when creating signed email for
	// sending. Multiple PFX sources can be added by calling this method once for each.
	// (On the Windows operating system, the registry-based certificate stores are also
	// automatically searched, so it is commonly not required to explicitly add PFX
	// sources.)
	// 
	// The ARG1 contains the bytes of a PFX file (also known as PKCS12 or .p12).
	// 
	bool AddPfxSourceData(const CkByteData &pfxData, const wchar_t *password);

	// Adds a PFX file to the object's internal list of sources to be searched for
	// certificates and private keys when decrypting or when sending signed email.
	// Multiple PFX files can be added by calling this method once for each. (On the
	// Windows operating system, the registry-based certificate stores are also
	// automatically searched, so it is commonly not required to explicitly add PFX
	// sources.)
	// 
	// The ARG1 contains the bytes of a PFX file (also known as PKCS12 or .p12).
	// 
	bool AddPfxSourceFile(const wchar_t *pfxFilePath, const wchar_t *password);

	// Returns the number of emails available on the POP3 server. Returns -1 on error.
	// 
	// The VerifyPopConnection method can be called to verify basic TCP/IP connectivity
	// with the POP3 server. The VerifyPopLogin method can be called to verify the POP3
	// login. The Verify* methods are intended to be called as a way of diagnosing the
	// failure when a POP3 method returns an error status.
	// 
	int CheckMail(void);

	// Clears the list of bad email addresses stored within the Mailman object. When an
	// email-sending method is called, any email addresses rejected by the SMTP server
	// will be cached within the Mailman object. These can be accessed by calling the
	// GetBadEmailAddresses method. This method clears the Mailman's in-memory cache of
	// bad addresses.
	void ClearBadEmailAddresses(void);

	// Clears the contents of the Pop3SessionLog property.
	void ClearPop3SessionLog(void);

	// Clears the contents of the SmtpSessionLog property.
	void ClearSmtpSessionLog(void);

	// The mailman object automatically opens an SMTP connection (if necessary)
	// whenever an email-sending method is called. The connection is kept open until
	// explicitly closed by this method. Calling this method is entirely optional. The
	// SMTP connection is also automatically closed when the mailman object is
	// destructed. Thus, if an application calls SendEmail 10 times to send 10 emails,
	// the 1st call will open the SMTP connection, while the subsequent 9 will send
	// over the existing connection (unless a property such as username, login,
	// hostname, etc. is changed, which would force the connection to become closed and
	// re-established with the next mail-sending method call).
	// 
	// Note: This method sends a QUIT command to the SMTP server prior to closing the
	// connection.
	// 
	bool CloseSmtpConnection(void);

	// Copy the email from a POP3 server into a EmailBundle. This does not remove the
	// email from the POP3 server.
	// The caller is responsible for deleting the object returned by this method.
	CkEmailBundleW *CopyMail(void);

	// Marks multiple emails on the POP3 server for deletion. (Each email in emailBundle that
	// is also present on the server is marked for deletion.) To complete the deletion
	// of the emails, a "QUIT" message must be sent and the POP3 session ended. This
	// will happen automatically when the ImmediateDelete property equals true, which
	// is the default. If ImmediateDelete equals false, then the Pop3EndSession
	// method can be called to send the "QUIT" and end the session (i.e. disconnect.)
	// 
	// Note: When making multiple calls to a Delete* method, it's best to turn off
	// ImmediateDelete, and then manually call Pop3EndSession to finalize the
	// deletions.
	// 
	// Also, any method call requiring communication with the POP3 server will
	// automatically re-establish a session based on the current property settings.
	// 
	bool DeleteBundle(const CkEmailBundleW &bundle);

	// Marks an email for deletion by message number. WARNING: Be very careful if
	// calling this method. Message numbers are specific to a POP3 session. If a
	// maildrop has (for example) 10 messages, the message numbers will be 1, 2, 3, ...
	// 10. If message number 1 is deleted and a new POP3 session is established, there
	// will be 9 messages numbered 1, 2, 3, ... 9.
	// 
	// IMPORTANT: A POP3 must first be established by either calling Pop3BeginSession
	// explicitly, or implicitly by calling some other method that automatically
	// establishes the session. This method will not automatically establish a new POP3
	// session (because if it did, the message numbers would potentially be different
	// than what the application expects).
	// 
	// This method only marks an email for deletion. It is not actually removed from
	// the maildrop until the POP3 session is explicitly ended by calling
	// Pop3EndSession.
	// 
	bool DeleteByMsgnum(int msgnum);

	// Marks an email on the POP3 server for deletion. To complete the deletion of an
	// email, a "QUIT" message must be sent and the POP3 session ended. This will
	// happen automatically when the ImmediateDelete property equals true, which is
	// the default. If ImmediateDelete equals false, then the Pop3EndSession method
	// can be called to send the "QUIT" and end the session (i.e. disconnect.)
	// 
	// Note: When making multiple calls to a Delete* method, it's best to turn off
	// ImmediateDelete, and then manually call Pop3EndSession to finalize the
	// deletions.
	// 
	// Also, any method call requiring communication with the POP3 server will
	// automatically re-establish a session based on the current property settings.
	// 
	bool DeleteByUidl(const wchar_t *uidl);

	// Marks an email on the POP3 server for deletion. To complete the deletion of an
	// email, a "QUIT" message must be sent and the POP3 session ended. This will
	// happen automatically when the ImmediateDelete property equals true, which is
	// the default. If ImmediateDelete equals false, then the Pop3EndSession method
	// can be called to send the "QUIT" and end the session (i.e. disconnect.)
	// 
	// Note: When making multiple calls to a Delete* method, it's best to turn off
	// ImmediateDelete, and then manually call Pop3EndSession to finalize the
	// deletions.
	// 
	// Also, any method call requiring communication with the POP3 server will
	// automatically re-establish a session based on the current property settings.
	// 
	bool DeleteEmail(const CkEmailW &email);

	// Marks multiple emails on the POP3 server for deletion. (Any email on the server
	// having a UIDL equal to a UIDL found in uidlArray is marked for deletion.) To complete
	// the deletion of the emails, a "QUIT" message must be sent and the POP3 session
	// ended. This will happen automatically when the ImmediateDelete property equals
	// true, which is the default. If ImmediateDelete equals false, then the
	// Pop3EndSession method can be called to send the "QUIT" and end the session (i.e.
	// disconnect.)
	// 
	// Note: When making multiple calls to a Delete* method, it's best to turn off
	// ImmediateDelete, and then manually call Pop3EndSession to finalize the
	// deletions.
	// 
	// Also, any method call requiring communication with the POP3 server will
	// automatically re-establish a session based on the current property settings.
	// 
	bool DeleteMultiple(const CkStringArrayW &uidlArray);

	// Fetches an email by message number. WARNING: Be very careful if calling this
	// method. Message numbers are specific to a POP3 session. If a maildrop has (for
	// example) 10 messages, the message numbers will be 1, 2, 3, ... 10. If message
	// number 1 is deleted and a new POP3 session is established, there will be 9
	// messages numbered 1, 2, 3, ... 9.
	// 
	// IMPORTANT: A POP3 connection must first be established by either calling
	// Pop3BeginSession explicitly, or implicitly by calling some other method that
	// automatically establishes the session. This method will not automatically
	// establish a new POP3 session (because if it did, the message numbers would
	// potentially be different than what the application expects).
	// 
	// The caller is responsible for deleting the object returned by this method.
	CkEmailW *FetchByMsgnum(int msgnum);

	// Fetches an email from the POP3 mail server given its UIDL. Calling this method
	// does not remove the email from the server. A typical program might get the email
	// headers from the POP3 server by calling GetAllHeaders or GetHeaders, and then
	// fetch individual emails by UIDL.
	// 
	// Returns a null reference on failure.
	// 
	// The caller is responsible for deleting the object returned by this method.
	CkEmailW *FetchEmail(const wchar_t *uidl);

	// Fetches an email by UIDL and returns the MIME source of the email in a byte
	// array.
	bool FetchMime(const wchar_t *uidl, CkByteData &outData);

	// Fetches an email by message number and returns the MIME source of the email in a
	// byte array. WARNING: Message sequend numbers are specific to a POP3 session. If
	// a maildrop has (for example) 10 messages, the message numbers will be 1, 2, 3,
	// ... 10. If message number 1 is deleted and a new POP3 session is established,
	// there will be 9 messages numbered 1, 2, 3, ... 9.
	// 
	// IMPORTANT: A POP3 connection must first be established by either calling
	// Pop3BeginSession explicitly, or implicitly by calling some other method that
	// automatically establishes the session. This method will not automatically
	// establish a new POP3 session (because if it did, the message numbers would
	// potentially be different than what the application expects).
	// 
	bool FetchMimeByMsgnum(int msgnum, CkByteData &outBytes);

	// Given an array of UIDL strings, fetchs all the emails from the POP3 server whose
	// UIDL is present in the array, and returns the emails in a bundle.
	// The caller is responsible for deleting the object returned by this method.
	CkEmailBundleW *FetchMultiple(const CkStringArrayW &uidlArray);

	// Given an array of UIDL strings, fetchs all the email headers from the POP3
	// server whose UIDL is present in the array.
	// 
	// Note: The email objects returned in the bundle contain only headers. The
	// attachments will be missing, and the bodies will be mostly missing (only the 1st
	//  numBodyLines lines of either the plain-text or HTML body will be present).
	// 
	// The caller is responsible for deleting the object returned by this method.
	CkEmailBundleW *FetchMultipleHeaders(const CkStringArrayW &uidlArray, int numBodyLines);

	// Given an array of UIDL strings, fetchs all the emails from the POP3 server whose
	// UIDL is present in the array, and returns the MIME source of each email in an
	// "stringarray" -- an object containing a collection of strings. Returns a null
	// reference on failure.
	// The caller is responsible for deleting the object returned by this method.
	CkStringArrayW *FetchMultipleMime(const CkStringArrayW &uidlArray);

	// Fetches a single header by message number. Returns an email object on success,
	// or a null reference on failure.
	// 
	// Note: The email objects returned in the bundle contain only headers. The
	// attachments will be missing, and the bodies will be mostly missing (only the 1st
	//  messageNumber lines of either the plain-text or HTML body will be present).
	// 
	// Also Important:Message numbers are specific to a POP3 session (whereas UIDLs are
	// valid across sessions). Be very careful when using this method.
	// 
	// The caller is responsible for deleting the object returned by this method.
	CkEmailW *FetchSingleHeader(int numBodyLines, int index);

	// Fetches a single header by UIDL. Returns an email object on success, or a null
	// reference on failure.
	// 
	// Note: The email objects returned in the bundle contain only headers. The
	// attachments will be missing, and the bodies will be mostly missing (only the 1st
	// ARG2 lines of either the plain-text or HTML body will be present).
	// 
	// The caller is responsible for deleting the object returned by this method.
	CkEmailW *FetchSingleHeaderByUidl(int numBodyLines, const wchar_t *uidl);

	// Returns all the emails from the POP3 server, but only the first numBodyLines lines of
	// the body. Attachments are not returned. The emails returned in the bundle are
	// valid email objects, the only difference is that the body is truncated to
	// include only the top numBodyLines lines, and the attachments will be missing.
	// The caller is responsible for deleting the object returned by this method.
	CkEmailBundleW *GetAllHeaders(int numBodyLines);

	// Returns a string array object containing a list of failed and invalid email
	// addresses that have accumulated during SMTP sends. The list will not contain
	// duplicates. Also, this only works with some SMTP servers -- not all SMTP servers
	// check the validity of each email address.
	// 
	// Note: An SMTP server can only validate the email addresses within it's own
	// domain. External email address are not verifiable at the time of sending.
	// 
	// The caller is responsible for deleting the object returned by this method.
	CkStringArrayW *GetBadEmailAddrs(void);

	// If a partial email was obtained using GetHeaders or GetAllHeaders, this method
	// will take the partial email as an argument, and download the full email from the
	// server. A new email object (separate from the partial email) is returned. A null
	// reference is returned on failure.
	// The caller is responsible for deleting the object returned by this method.
	CkEmailW *GetFullEmail(const CkEmailW &email);

	// The same as the GetAllHeaders method, except only the emails from  fromIndex to  toIndex
	// on the POP3 server are returned. The first email on the server is at index 0.
	// The caller is responsible for deleting the object returned by this method.
	CkEmailBundleW *GetHeaders(int numBodyLines, int fromIndex, int toIndex);

	// Returns the number of emails on the POP3 server, or -1 for failure.
	// 
	// This method is identical to CheckEmail. It was added for clarity.
	// 
	int GetMailboxCount(void);

	// Returns an XML document with information about the emails in a POP3 mailbox. The
	// XML contains the UIDL and size (in bytes) of each email in the mailbox.
	bool GetMailboxInfoXml(CkString &outXml);
	// Returns an XML document with information about the emails in a POP3 mailbox. The
	// XML contains the UIDL and size (in bytes) of each email in the mailbox.
	const wchar_t *getMailboxInfoXml(void);
	// Returns an XML document with information about the emails in a POP3 mailbox. The
	// XML contains the UIDL and size (in bytes) of each email in the mailbox.
	const wchar_t *mailboxInfoXml(void);

	// Returns the total combined size in bytes of all the emails in the POP3 mailbox.
	// This is also known as the "mail drop" size. Returns -1 on failure.
	unsigned long GetMailboxSize(void);

	// Returns the POP3 server's SSL certificate. This is available after connecting
	// via SSL to a POP3 server. (To use POP3 SSL, set the PopSsl property = true.)
	// 
	// Returns a null reference if no POP3 SSL certificate is available.
	// 
	// The caller is responsible for deleting the object returned by this method.
	CkCertW *GetPop3SslServerCert(void);

	// Returns the list of successful email addresses in the last call to a mail
	// sending method, such as SendEmail.
	// 
	// When an email is sent, the email addresses that were flagged invalid by the SMTP
	// server are saved in a "bad email addresses" list within the mailman object, and
	// the acceptable email addresses are saved in a "good email addresses" list
	// (within the mailman object). These internal lists are automatically reset at the
	// start of the next mail-sending method call. This allows for a program to know
	// which email addresses were accepted and which were not.
	// 
	// Note: The AllOrNone property controls whether the mail-sending method, such as
	// SendEmail, returns false (to indicate failure) if any single email address is
	// rejected.
	// 
	// Note: An SMTP server can only be aware of invalid email addresses that are of
	// the same domain. For example, the comcast.net mail servers would only be aware
	// of what comcast.net email addresses are valid. All external email addresses are
	// implicitly accepted because the server is simply forwarding the email towards
	// the mail server controlling that domain.
	// 
	// The caller is responsible for deleting the object returned by this method.
	CkStringArrayW *GetSentToEmailAddrs(void);

	// Returns the size of an email (including attachments) given the UIDL of the email
	// on the POP3 server. Returns -1 for failure.
	int GetSizeByUidl(const wchar_t *uidl);

	// If using SSL/TLS, this method returns the SMTP server's digital certificate used
	// with the secure connection.
	// The caller is responsible for deleting the object returned by this method.
	CkCertW *GetSmtpSslServerCert(void);

	// Returns the UIDLs of the emails currently stored on the POP3 server.
	// The caller is responsible for deleting the object returned by this method.
	CkStringArrayW *GetUidls(void);

	// Contacts the SMTP server and determines if it supports the DSN (Delivery Status
	// Notification) features specified by RFC 3461 and supported by the DsnEnvid,
	// DsnNotify, and DsnRet properties. Returns true if the SMTP server supports
	// DSN, otherwise returns false.
	bool IsSmtpDsnCapable(void);

	// Returns true if the mailman is already unlocked, otherwise returns false.
	bool IsUnlocked(void);

	// Loads an email from a .eml file. (EML files contain the MIME source of an
	// email.) Returns a null reference on failure.
	// 
	// Note: MHT files can be loaded into an email object by calling this method.
	// 
	// The caller is responsible for deleting the object returned by this method.
	CkEmailW *LoadEml(const wchar_t *emlFilename);

	// Loads a .mbx file containing emails and returns an email bundle. If a Filter is
	// present, only emails matching the filter are returned.
	// The caller is responsible for deleting the object returned by this method.
	CkEmailBundleW *LoadMbx(const wchar_t *mbxFileName);

	// Creates and loads an email from a MIME string. Returns a null reference on
	// failure.
	// The caller is responsible for deleting the object returned by this method.
	CkEmailW *LoadMime(const wchar_t *mimeText);

	// Loads an XML file containing a single email and returns an email object. Returns
	// a null reference on failure.
	// The caller is responsible for deleting the object returned by this method.
	CkEmailW *LoadXmlEmail(const wchar_t *filename);

	// Loads an XML string containing a single email and returns an email object.
	// Returns a null reference on failure.
	// The caller is responsible for deleting the object returned by this method.
	CkEmailW *LoadXmlEmailString(const wchar_t *xmlString);

	// Loads an XML file containing one or more emails and returns an email bundle. If
	// a Filter is present, only emails matching the filter are returned. Returns a
	// null reference on failure.
	// The caller is responsible for deleting the object returned by this method.
	CkEmailBundleW *LoadXmlFile(const wchar_t *filename);

	// Loads from an XML string containing emails and returns an email bundle. If a
	// Filter is present, only emails matching the filter are returned.
	// The caller is responsible for deleting the object returned by this method.
	CkEmailBundleW *LoadXmlString(const wchar_t *xmlString);

#if defined(CK_MX_INCLUDED)
	// Performs a DNS MX lookup to return the mail server hostname based on an email
	// address.
	bool MxLookup(const wchar_t *emailAddress, CkString &outStrHostname);
	// Performs a DNS MX lookup to return the mail server hostname based on an email
	// address.
	const wchar_t *mxLookup(const wchar_t *emailAddress);
#endif

#if defined(CK_MX_INCLUDED)
	// Performs a DNS MX lookup to return the list of mail server hostnames based on an
	// email address. The primary server is at index 0. In most cases, there is only
	// one mail server for a given email address.
	// The caller is responsible for deleting the object returned by this method.
	CkStringArrayW *MxLookupAll(const wchar_t *emailAddress);
#endif

	// Explicitly opens a connection to the SMTP server and authenticates (if a
	// username/password was specified). Calling this method is optional because the
	// SendEmail method and other mail-sending methods will automatically open the
	// connection to the SMTP server if one is not already established.
	bool OpenSmtpConnection(void);

	// Call to explicitly begin a POP3 session. It is not necessary to call this method
	// because any method requiring an established POP3 session will automatically
	// connect and login if a session is not already open.
	bool Pop3BeginSession(void);

	// Call to explicitly end a POP3 session. If the ImmediateDelete property is set to
	// false, and emails marked for deletion will be deleted at this time.
	bool Pop3EndSession(void);

	// This method is identical to Pop3EndSession, but no "QUIT" command is sent. The
	// client simply disconnects from the POP3 server.
	// 
	// This method should always return true.
	// 
	bool Pop3EndSessionNoQuit(void);

	// Sends a NOOP command to the POP3 server. This may be a useful method to call
	// periodically to keep a connection open, or to verify that the POP3 connection
	// (session) is open and functioning.
	bool Pop3Noop(void);

	// Sends a RSET command to the POP3 server. If any messages have been marked as
	// deleted by the POP3 server, they are unmarked. Calling Pop3Reset resets the POP3
	// session to a valid, known starting point.
	bool Pop3Reset(void);

	// Sends a raw command to the POP3 server and returns the POP3 server's response.
	// If non-us-ascii characters are included in command, then  charset indicates the charset
	// to be used in sending the command (such as "utf-8", "ansi", "iso-8859-1",
	// "Shift_JIS", etc.)
	bool Pop3SendRawCommand(const wchar_t *command, const wchar_t *charset, CkString &outStr);
	// Sends a raw command to the POP3 server and returns the POP3 server's response.
	// If non-us-ascii characters are included in command, then  charset indicates the charset
	// to be used in sending the command (such as "utf-8", "ansi", "iso-8859-1",
	// "Shift_JIS", etc.)
	const wchar_t *pop3SendRawCommand(const wchar_t *command, const wchar_t *charset);

	// A quick way to send an email to a single recipient without having to explicitly
	// create an email object.
	bool QuickSend(const wchar_t *fromAddr, const wchar_t *toAddr, const wchar_t *subject, const wchar_t *body, const wchar_t *smtpServer);

	// When an email is sent by calling SendEmail, it is first "rendered" according to
	// the values of the email properties and contents. It may be digitally signed,
	// encrypted, values substituted for replacement patterns, and header fields "Q"or
	// "B" encoded as needed based on the email. The RenderToMime method performs the
	// rendering, but without the actual sending. The MIME text produced is exactly
	// what would be sent to the SMTP server had SendEmail been called. (The SendEmail
	// method is effectively the same as calling RenderToMime followed by a call to
	// SendRendered.)
	// 
	// The rendered MIME string is returned on success.
	// 
	bool RenderToMime(const CkEmailW &email, CkString &outStr);
	// When an email is sent by calling SendEmail, it is first "rendered" according to
	// the values of the email properties and contents. It may be digitally signed,
	// encrypted, values substituted for replacement patterns, and header fields "Q"or
	// "B" encoded as needed based on the email. The RenderToMime method performs the
	// rendering, but without the actual sending. The MIME text produced is exactly
	// what would be sent to the SMTP server had SendEmail been called. (The SendEmail
	// method is effectively the same as calling RenderToMime followed by a call to
	// SendRendered.)
	// 
	// The rendered MIME string is returned on success.
	// 
	const wchar_t *renderToMime(const CkEmailW &email);

	// This method is the same as RenderToMime, but the MIME is returned in a byte
	// array. If an email uses an 8bit or binary MIME encoding, then calling
	// RenderToMime may introduce errors because it is not possible to return non-text
	// binary data as a string. Therefore, calling RenderToMimeBytes is recommended
	// over RenderToMime, unless it is assured that the email (MIME) does not use a
	// binary encoding for non-text data.
	bool RenderToMimeBytes(CkEmailW &email, CkByteData &outBytes);

	// Sends a bundle of emails. This is identical to calling SendEmail for each email
	// in the bundle.
	// 
	// If an error occurs when sending one of the emails in the bundle, it will
	// continue with each subsequent email until each email in the bundle has been
	// attempted (unless a fatal error occurs, in which case the send is aborted).
	// 
	// Because it is difficult or impossible to programmatically identify which emails
	// in the bundle failed and which succeeded, it is best to write a loop that sends
	// each email separately (via the SendEmail method).
	// 
	bool SendBundle(const CkEmailBundleW &bundle);

	// Sends a single email.
	// 
	// Important: Some SMTP servers do not actually send the email until the connection
	// is closed. In these cases, it is necessary to call CloseSmtpConnection for the
	// mail to be sent. Most SMTP servers send the email immediately, and it is not
	// required to close the connection.
	// 
	bool SendEmail(const CkEmailW &email);

	// Provides complete control over the email that is sent. The MIME text passed in
	//  mimeSource (the MIME source of an email) is passed exactly as-is to the SMTP server.
	// The  recipients of the email are passed as a string of comma separated email addresses
	// (without friendly names). The fromAddr is the reverse-path email address. This is
	// where bounced email will be delivered. It may be different than the From header
	// field in the  mimeSource.
	// 
	// To understand how the fromAddr and  recipients relate to the email addresses found in the
	// MIME headers (FROM, TO, CC), see the link below entitled "SMTP Protocol in a
	// Nutshell". The fromAddr is what is passed to the SMTP server in the "MAIL FROM"
	// command. The  recipients are the email addresses passed in "RCPT TO" commands. These
	// are usually the same email addresses found in the MIME headers, but need not be
	// (unless the SMTP server enforces policies that require them to be the same).
	// 
	bool SendMime(const wchar_t *from, const wchar_t *recipients, const wchar_t *mimeText);

	// This method is the same as SendMime, except the MIME is passed in a byte array.
	// This can be important if the MIME uses a binary encoding, or if a DKIM/DomainKey
	// signature is included.
	// 
	// To understand how the fromAddr and  recipients relate to the email addresses found in the
	// MIME headers (FROM, TO, CC), see the link below entitled "SMTP Protocol in a
	// Nutshell". The fromAddr is what is passed to the SMTP server in the "MAIL FROM"
	// command. The  recipients are the email addresses passed in "RCPT TO" commands. These
	// are usually the same email addresses found in the MIME headers, but need not be
	// (unless the SMTP server enforces policies that require them to be the same).
	// 
	bool SendMimeBytes(const wchar_t *from, const wchar_t *recipients, const CkByteData &mimeData);

#if defined(CK_SMTPQ_INCLUDED)
	// This method is the samem as SendMimeQ, except the MIME is passed in a byte array
	// argument instead of a string argument.
	bool SendMimeBytesQ(const wchar_t *from, const wchar_t *recipients, const CkByteData &mimeData);
#endif

#if defined(CK_SMTPQ_INCLUDED)
	// Same as SendMime, except the email is written to the Chilkat SMTPQ's queue
	// directory for background sending from the SMTPQ service.
	bool SendMimeQ(const wchar_t *from, const wchar_t *recipients, const wchar_t *mimeText);
#endif

	// Same as SendMime, but the recipient list is read from a text file ( distListFilename)
	// containing one email address per line.
	bool SendMimeToList(const wchar_t *from, const wchar_t *distListFile, const wchar_t *mimeText);

#if defined(CK_SMTPQ_INCLUDED)
	// Queues an email to be sent using the Chilkat SMTP queue service. This is the
	// same as SendEmail, except the email is written to the SMTPQ's queue directory.
	// 
	// The email is written as a .eml to the SMTPQ's queue directory. The SMTP server
	// hostname, login, password, and send-time parameters are saved as encrypted
	// headers in the .eml. The SMTPQ service watches the queue directory. When a .eml
	// file appears, it loads the .eml, extracts and removes the encrypted information
	// from the header, and sends the email.
	// 
	// Note: When the Chilkat SMTPQ service is configured, the location of the queue
	// directory is written to the registry. Because Chilkat SMTPQ is a 32-bit service,
	// it is the 32-bit registry that is written. (Microsoft 64-bit systems have two
	// separate registries -- one for 32-bit and one for 64-bit.) Therefore, if your
	// application is a 64-bit app, the registry lookup for the queue directory will
	// fail. You should instead call the SendQ2 method which allows for the queue
	// directory to be explicitly specified.
	// 
	bool SendQ(const CkEmailW &email);
#endif

#if defined(CK_SMTPQ_INCLUDED)
	// Same as SendQ, but the queue directory can be explicitly specified in a method
	// argument.
	bool SendQ2(const CkEmailW &email, const wchar_t *queueDir);
#endif

	// Send the same email to a list of email addresses.
	bool SendToDistributionList(CkEmailW &email, CkStringArrayW &sa);

#if defined(CK_CSP_INCLUDED)
	// (Only applies to the Microsoft Windows OS) Sets the Cryptographic Service
	// Provider (CSP) to be used for encryption or digital signing, or decryption /
	// signature verification.
	// 
	// This is not commonly used becaues the default Microsoft CSP is typically
	// appropriate. One instance where SetCSP is necessary is when using the Crypto-Pro
	// CSP for the GOST R 34.10-2001 and GOST R 34.10-94 providers.
	// 
	bool SetCSP(CkCspW &csp);
#endif

	// Explicitly specifies the certificate and associated private key to be used for
	// decrypting S/MIME encrypted email.
	// 
	// Note: In most cases, it is easier to call AddPfxSourceFile or AddPfxSourceData
	// to provide the required cert and private key. On Windows systems where the
	// certificate + private key has already been installed in the default certificate
	// store, nothing needs to be done -- the mailman will automatically locate and use
	// the required cert + private key.
	// 
	bool SetDecryptCert2(const CkCertW &cert, CkPrivateKeyW &key);

	// Sets the client-side certificate to be used with SSL connections. This is
	// typically not required, as most SSL connections are such that only the server is
	// authenticated while the client remains unauthenticated.
	bool SetSslClientCert(CkCertW &cert);

	// Allows for a client-side certificate to be used for the SSL / TLS connection.
	bool SetSslClientCertPem(const wchar_t *pemDataOrFilename, const wchar_t *pemPassword);

	// Allows for a client-side certificate to be used for the SSL / TLS connection.
	bool SetSslClientCertPfx(const wchar_t *pfxFilename, const wchar_t *pfxPassword);

	// Sends a no-op to the SMTP server. Calling this method is good for testing to see
	// if the connection to the SMTP server is working and valid. The SmtpNoop method
	// will automatically establish the SMTP connection if it does not already exist.
	bool SmtpNoop(void);

	// Sends an RSET command to the SMTP server. This method is rarely needed. The RSET
	// command resets the state of the connection to the SMTP server to the initial
	// state (so that the component can proceed with sending a new email). The
	// SmtpReset method would only be needed if a mail-sending method failed and left
	// the connection with the SMTP server open and in a non-initial state. (A
	// situation that is probably not even possible with the Chilkat mail component.)
	bool SmtpReset(void);

	// Sends a raw command to the SMTP server and returns the SMTP server's response.
	// If non-us-ascii characters are included in command, then  charset indicates the charset
	// to be used in sending the command (such as "utf-8", "ansi", "iso-8859-1",
	// "Shift_JIS", etc.)
	// 
	// If  bEncodeBase64 is true, then the response is returned in Base64-encoded format.
	// Otherwise the raw response is returned.
	// 
	bool SmtpSendRawCommand(const wchar_t *command, const wchar_t *charset, bool bEncodeBase64, CkString &outStr);
	// Sends a raw command to the SMTP server and returns the SMTP server's response.
	// If non-us-ascii characters are included in command, then  charset indicates the charset
	// to be used in sending the command (such as "utf-8", "ansi", "iso-8859-1",
	// "Shift_JIS", etc.)
	// 
	// If  bEncodeBase64 is true, then the response is returned in Base64-encoded format.
	// Otherwise the raw response is returned.
	// 
	const wchar_t *smtpSendRawCommand(const wchar_t *command, const wchar_t *charset, bool bEncodeBase64);

	// Authenticates with the SSH server using public-key authentication. bSmtp should
	// be set to true for SMTP SSH tunneling (port forwarding) or false for POP3
	// SSH tunneling (port forwarding). The corresponding public key must have been
	// installed on the SSH server for the  sshUsername. Authentication will succeed if the
	// matching  sshPrivateKey is provided.
	// 
	// Important: When reporting problems, please send the full contents of the
	// LastErrorText property to support@chilkatsoft.com.
	// 
	bool SshAuthenticatePk(bool bSmtp, const wchar_t *sshLogin, CkSshKeyW &privateKey);

	// Authenticates with the SSH server using a  sshLogin and  sshPassword. bSmtp should be set to
	// true for SMTP SSH tunneling (port forwarding) or false for POP3 SSH
	// tunneling (port forwarding).
	// 
	// An SSH tunneling (port forwarding) session always begins by first calling
	// SshTunnel to connect to the SSH server, then calling either AuthenticatePw or
	// AuthenticatePk to authenticate.
	// 
	// Note: Once the SSH tunnel is setup by calling SshTunnel and SshAuthenticatePw
	// (or SshAuthenticatePk), all underlying communcations with the POP3 or SMTP
	// server use the SSH tunnel. No changes in programming are required other than
	// making two initial calls to setup the tunnel.
	// 
	// Important: When reporting problems, please send the full contents of the
	// LastErrorText property to support@chilkatsoft.com.
	// 
	bool SshAuthenticatePw(bool bSmtp, const wchar_t *sshLogin, const wchar_t *sshPassword);

	// Closes the SSH tunnel for SMTP or POP3. bSmtp should be set to true for SMTP
	// and false for POP3.
	bool SshCloseTunnel(bool bSmtp);

	// Connects to an SSH server and creates a tunnel for SMTP or POP3. If bSmtp is
	// true, then an SSH tunnel is created for SMTP. If bSmtp is false, the SSH
	// tunnel is created for POP3. The  sshServerHostname is the hostname (or IP address) of the SSH
	// server. The  sshPort is typically 22, which is the standard SSH port number.
	// 
	// An SSH tunneling (port forwarding) session always begins by first calling
	// SshTunnel to connect to the SSH server, followed by calling either
	// AuthenticatePw or AuthenticatePk to authenticate.
	// 
	// Note: Once the SSH tunnel is setup by calling SshTunnel and SshAuthenticatePw
	// (or SshAuthenticatePk), all underlying communcations with the SMTP or POP3
	// server use the SSH tunnel. No changes in programming are required other than
	// making two initial calls to setup the tunnel.
	// 
	// Note: Tunnels are setup separately for POP3 and SMTP. The bSmtp indicates whether
	// the tunnel is for SMTP or POP3.
	// 
	// Important: When reporting problems, please send the full contents of the
	// LastErrorText property to support@chilkatsoft.com.
	// 
	bool SshTunnel(bool bSmtp, const wchar_t *sshServerHostname, int sshServerPort);

	// Downloads and removes all email from a POP3 server. A bundle containing the
	// emails is returned. A null reference is returned on failure.
	// The caller is responsible for deleting the object returned by this method.
	CkEmailBundleW *TransferMail(void);

	// Same as FetchMultipleMime except that the downloaded emails are also deleted
	// from the server. Returns a null reference on failure.
	// The caller is responsible for deleting the object returned by this method.
	CkStringArrayW *TransferMultipleMime(const CkStringArrayW &uidlArray);

	// Unlocks the component. This must be called once at the beginning of your program
	// (or ASP / ASP.NET page). It is very fast and has negligible overhead. An
	// arbitrary string, such as "Hello World" may be passed to automatically begin a
	// fully-functional 30-day trial.
	// 
	// A valid permanent unlock code for this object will always included the substring
	// "MAIL".
	// 
	bool UnlockComponent(const wchar_t *code);

	// Return true if a TCP/IP connection can be established with the POP3 server,
	// otherwise returns false.
	bool VerifyPopConnection(void);

	// Return true if a TCP/IP connection and login is successful with the POP3
	// server. Otherwise return false.
	bool VerifyPopLogin(void);

	// Initiates sending an email, but aborts just after passing all recipients (TO,
	// CC, BCC) to the SMTP server. This allows your program to collect email addresses
	// flagged as invalid by the SMTP server.
	// 
	// Important: Please read this blog post before using this method:
	// http://www.cknotes.com/?p=249
	// 
	bool VerifyRecips(const CkEmailW &email, CkStringArrayW &badAddrs);

	// Return true if a TCP/IP connection can be established with the SMTP server,
	// otherwise returns false.
	bool VerifySmtpConnection(void);

	// Return true if a TCP/IP connection and login is successful with the SMTP
	// server. Otherwise returns false.
	bool VerifySmtpLogin(void);

	// Adds an XML certificate vault to the object's internal list of sources to be
	// searched for certificates and private keys when encrypting/decrypting or
	// signing/verifying. Unlike the AddPfxSourceData and AddPfxSourceFile methods,
	// only a single XML certificate vault can be used. If UseCertVault is called
	// multiple times, only the last certificate vault will be used, as each call to
	// UseCertVault will replace the certificate vault provided in previous calls.
	bool UseCertVault(CkXmlCertVaultW &vault);

	// Explicitly specifies the certificate to be used for decrypting encrypted email.
	bool SetDecryptCert(CkCertW &cert);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
