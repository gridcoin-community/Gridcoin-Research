// CkEmailW.h: interface for the CkEmailW class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkEmailW_H
#define _CkEmailW_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkWideCharBase.h"

class CkByteData;
class CkCertW;
class CkStringArrayW;
class CkDateTimeW;
class CkCspW;
class CkPrivateKeyW;
class CkXmlCertVaultW;
class CkCertChainW;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkEmailW
class CK_VISIBLE_PUBLIC CkEmailW  : public CkWideCharBase
{
    private:
	

	// Don't allow assignment or copying these objects.
	CkEmailW(const CkEmailW &);
	CkEmailW &operator=(const CkEmailW &);

    public:
	CkEmailW(void);
	virtual ~CkEmailW(void);

	static CkEmailW *createNew(void);
	

	
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	

	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------
	// The body of the email. If the email has both HTML and plain-text bodies, this
	// property returns the HTML body. The GetHtmlBody and GetPlainTextBody methods can
	// be used to access a specific body. The HasHtmlBody and HasPlainTextBody methods
	// can be used to determine the presence of a body.
	void get_Body(CkString &str);
	// The body of the email. If the email has both HTML and plain-text bodies, this
	// property returns the HTML body. The GetHtmlBody and GetPlainTextBody methods can
	// be used to access a specific body. The HasHtmlBody and HasPlainTextBody methods
	// can be used to determine the presence of a body.
	const wchar_t *body(void);
	// The body of the email. If the email has both HTML and plain-text bodies, this
	// property returns the HTML body. The GetHtmlBody and GetPlainTextBody methods can
	// be used to access a specific body. The HasHtmlBody and HasPlainTextBody methods
	// can be used to determine the presence of a body.
	void put_Body(const wchar_t *newVal);

	// The "return-path" address of the email to be used when the email is sent.
	// Bounces (i.e. delivery status notifications, or DSN's) will go to this address.
	// 
	// Note: This is not the content of the "return-path" header for emails that are
	// downloaded from a POP3 or IMAP server. The BounceAddress is the email address to
	// be used in the process of sending the email via SMTP. (See the "SMTP Protocol in
	// a Nutshell" link below.) The BounceAddress is the email address passed in the
	// "MAIL FROM" SMTP command which becomes the "return-path" header in the email
	// when received.
	// 
	// Note: The Sender and BounceAddress properties are identical and perform the same
	// function. Setting the Sender property also sets the BounceAddress property, and
	// vice-versa. The reason for the duplication is that BounceAddress existed first,
	// and developers typically searched for a "Sender" property without realizing that
	// the BounceAddress property served this function.
	// 
	void get_BounceAddress(CkString &str);
	// The "return-path" address of the email to be used when the email is sent.
	// Bounces (i.e. delivery status notifications, or DSN's) will go to this address.
	// 
	// Note: This is not the content of the "return-path" header for emails that are
	// downloaded from a POP3 or IMAP server. The BounceAddress is the email address to
	// be used in the process of sending the email via SMTP. (See the "SMTP Protocol in
	// a Nutshell" link below.) The BounceAddress is the email address passed in the
	// "MAIL FROM" SMTP command which becomes the "return-path" header in the email
	// when received.
	// 
	// Note: The Sender and BounceAddress properties are identical and perform the same
	// function. Setting the Sender property also sets the BounceAddress property, and
	// vice-versa. The reason for the duplication is that BounceAddress existed first,
	// and developers typically searched for a "Sender" property without realizing that
	// the BounceAddress property served this function.
	// 
	const wchar_t *bounceAddress(void);
	// The "return-path" address of the email to be used when the email is sent.
	// Bounces (i.e. delivery status notifications, or DSN's) will go to this address.
	// 
	// Note: This is not the content of the "return-path" header for emails that are
	// downloaded from a POP3 or IMAP server. The BounceAddress is the email address to
	// be used in the process of sending the email via SMTP. (See the "SMTP Protocol in
	// a Nutshell" link below.) The BounceAddress is the email address passed in the
	// "MAIL FROM" SMTP command which becomes the "return-path" header in the email
	// when received.
	// 
	// Note: The Sender and BounceAddress properties are identical and perform the same
	// function. Setting the Sender property also sets the BounceAddress property, and
	// vice-versa. The reason for the duplication is that BounceAddress existed first,
	// and developers typically searched for a "Sender" property without realizing that
	// the BounceAddress property served this function.
	// 
	void put_BounceAddress(const wchar_t *newVal);

	// Sets the charset for the entire email. The header fields and plain-text/HTML
	// bodies will be converted and sent in this charset. (This includes parsing and
	// updating the HTML with the appropriate META tag specifying the charset.) All
	// formatting and encoding of the email MIME is handled automatically by the
	// Chilkat Mail component. If your application wants to send a Shift_JIS email, you
	// simply set the Charset property to "Shift_JIS". Note: If a charset property is
	// not explicitly set, the Chilkat component automatically detects the charset and
	// chooses the appropriate charset. If all characters are 7bit (i.e. us-ascii) the
	// charset is left blank. If the email contain a mix of languages such that no one
	// charset can be chosen, or if the language cannot be determined without
	// ambiguity, then the "utf-8" charset will be chosen.
	void get_Charset(CkString &str);
	// Sets the charset for the entire email. The header fields and plain-text/HTML
	// bodies will be converted and sent in this charset. (This includes parsing and
	// updating the HTML with the appropriate META tag specifying the charset.) All
	// formatting and encoding of the email MIME is handled automatically by the
	// Chilkat Mail component. If your application wants to send a Shift_JIS email, you
	// simply set the Charset property to "Shift_JIS". Note: If a charset property is
	// not explicitly set, the Chilkat component automatically detects the charset and
	// chooses the appropriate charset. If all characters are 7bit (i.e. us-ascii) the
	// charset is left blank. If the email contain a mix of languages such that no one
	// charset can be chosen, or if the language cannot be determined without
	// ambiguity, then the "utf-8" charset will be chosen.
	const wchar_t *charset(void);
	// Sets the charset for the entire email. The header fields and plain-text/HTML
	// bodies will be converted and sent in this charset. (This includes parsing and
	// updating the HTML with the appropriate META tag specifying the charset.) All
	// formatting and encoding of the email MIME is handled automatically by the
	// Chilkat Mail component. If your application wants to send a Shift_JIS email, you
	// simply set the Charset property to "Shift_JIS". Note: If a charset property is
	// not explicitly set, the Chilkat component automatically detects the charset and
	// chooses the appropriate charset. If all characters are 7bit (i.e. us-ascii) the
	// charset is left blank. If the email contain a mix of languages such that no one
	// charset can be chosen, or if the language cannot be determined without
	// ambiguity, then the "utf-8" charset will be chosen.
	void put_Charset(const wchar_t *newVal);

	// true if the email arrived encrypted and was successfully decrypted, otherwise
	// false.
	bool get_Decrypted(void);

	// The date/time from the "Date" header in UTC/GMT standard time. Use the LocalDate
	// property to get the local date and time.
	void get_EmailDate(SYSTEMTIME &outSysTime);
	// The date/time from the "Date" header in UTC/GMT standard time. Use the LocalDate
	// property to get the local date and time.
	void put_EmailDate(const SYSTEMTIME &sysTime);

	// The date/time from the "Date" header in the UTC/GMT timezone in RFC822 string
	// form.
	void get_EmailDateStr(CkString &str);
	// The date/time from the "Date" header in the UTC/GMT timezone in RFC822 string
	// form.
	const wchar_t *emailDateStr(void);
	// The date/time from the "Date" header in the UTC/GMT timezone in RFC822 string
	// form.
	void put_EmailDateStr(const wchar_t *newVal);

	// If the email was received encrypted, this contains the details of the
	// certificate used for encryption.
	void get_EncryptedBy(CkString &str);
	// If the email was received encrypted, this contains the details of the
	// certificate used for encryption.
	const wchar_t *encryptedBy(void);

	// Set this property to send an email to a list of recipients stored in a plain
	// text file. The file format is simple: one recipient per line, no comments
	// allowed, blank lines are ignored.Setting this property is equivalent to adding a
	// "CKX-FileDistList"header field to the email. Chilkat Mail treats header fields
	// beginning with "CKX-"specially in that these fields are never transmitted with
	// the email when sent. However, CKX fields are saved and restored when saving to
	// XML or loading from XML (or MIME). When sending an email containing a
	// "CKX-FileDistList"header field, Chilkat Mail will read the distribution list
	// file and send the email to each recipient. Emails can be sent individually, or
	// with BCC, 100 recipients at a time. (see the MailMan.SendIndividual property).
	void get_FileDistList(CkString &str);
	// Set this property to send an email to a list of recipients stored in a plain
	// text file. The file format is simple: one recipient per line, no comments
	// allowed, blank lines are ignored.Setting this property is equivalent to adding a
	// "CKX-FileDistList"header field to the email. Chilkat Mail treats header fields
	// beginning with "CKX-"specially in that these fields are never transmitted with
	// the email when sent. However, CKX fields are saved and restored when saving to
	// XML or loading from XML (or MIME). When sending an email containing a
	// "CKX-FileDistList"header field, Chilkat Mail will read the distribution list
	// file and send the email to each recipient. Emails can be sent individually, or
	// with BCC, 100 recipients at a time. (see the MailMan.SendIndividual property).
	const wchar_t *fileDistList(void);
	// Set this property to send an email to a list of recipients stored in a plain
	// text file. The file format is simple: one recipient per line, no comments
	// allowed, blank lines are ignored.Setting this property is equivalent to adding a
	// "CKX-FileDistList"header field to the email. Chilkat Mail treats header fields
	// beginning with "CKX-"specially in that these fields are never transmitted with
	// the email when sent. However, CKX fields are saved and restored when saving to
	// XML or loading from XML (or MIME). When sending an email containing a
	// "CKX-FileDistList"header field, Chilkat Mail will read the distribution list
	// file and send the email to each recipient. Emails can be sent individually, or
	// with BCC, 100 recipients at a time. (see the MailMan.SendIndividual property).
	void put_FileDistList(const wchar_t *newVal);

	// The combined name and email address of the sender, such as "John Smith" . This
	// is the content that will be placed in the From: header field. If the actual
	// sender is to be different, then set the Sender property to a different email
	// address.
	void get_From(CkString &str);
	// The combined name and email address of the sender, such as "John Smith" . This
	// is the content that will be placed in the From: header field. If the actual
	// sender is to be different, then set the Sender property to a different email
	// address.
	const wchar_t *ck_from(void);
	// The combined name and email address of the sender, such as "John Smith" . This
	// is the content that will be placed in the From: header field. If the actual
	// sender is to be different, then set the Sender property to a different email
	// address.
	void put_From(const wchar_t *newVal);

	// The email address of the sender.
	void get_FromAddress(CkString &str);
	// The email address of the sender.
	const wchar_t *fromAddress(void);
	// The email address of the sender.
	void put_FromAddress(const wchar_t *newVal);

	// The name of the sender.
	void get_FromName(CkString &str);
	// The name of the sender.
	const wchar_t *fromName(void);
	// The name of the sender.
	void put_FromName(const wchar_t *newVal);

	// The complete MIME header of the email.
	void get_Header(CkString &str);
	// The complete MIME header of the email.
	const wchar_t *header(void);

	// A read-only property that identifies the primary language group for the email.
	// Possible values are:
	// 
	//         "latin1" (for English and all Western European languages)
	//         "central" (for Central European languages such as Polish, Czech,
	//         Hungarian, etc.)
	//         "russian" (for Cyrillic languages)
	//         "greek"
	//         "turkish"
	//         "hebrew"
	//         "arabic"
	//         "thai"
	//         "vietnamese"
	//         "chinese"
	//         "japanese"
	//         "korean"
	//         "devanagari"
	//         "bengali"
	//         "gurmukhi"
	//         "gujarati"
	//         "oriya"
	//         "tamil"
	//         "telugu"
	//         "kannada"
	//         "malayalam"
	//         "sinhala"
	//         "lao"
	//         "tibetan"
	//         "myanmar"
	//         "georgian"
	//         "unknown"
	// 
	// The language group determination is made soley on the subject and
	// plain-text/HTML email bodies. Characters in the FROM, TO, CC, and other header
	// fields are not considered.
	// 
	// The primary determining factor is the characters found in the Subject header
	// field. For example, if an email contains Japanese in the Subject, but the body
	// contains Russian characters, it will be considered "japanese".
	// 
	// The language is determined by where the Unicode chars fall in various blocks in
	// the Unicode Basic Multilingual Plane. For more information, see the book
	// "Unicode Demystified" by Richard Gillam.
	// 
	void get_Language(CkString &str);
	// A read-only property that identifies the primary language group for the email.
	// Possible values are:
	// 
	//         "latin1" (for English and all Western European languages)
	//         "central" (for Central European languages such as Polish, Czech,
	//         Hungarian, etc.)
	//         "russian" (for Cyrillic languages)
	//         "greek"
	//         "turkish"
	//         "hebrew"
	//         "arabic"
	//         "thai"
	//         "vietnamese"
	//         "chinese"
	//         "japanese"
	//         "korean"
	//         "devanagari"
	//         "bengali"
	//         "gurmukhi"
	//         "gujarati"
	//         "oriya"
	//         "tamil"
	//         "telugu"
	//         "kannada"
	//         "malayalam"
	//         "sinhala"
	//         "lao"
	//         "tibetan"
	//         "myanmar"
	//         "georgian"
	//         "unknown"
	// 
	// The language group determination is made soley on the subject and
	// plain-text/HTML email bodies. Characters in the FROM, TO, CC, and other header
	// fields are not considered.
	// 
	// The primary determining factor is the characters found in the Subject header
	// field. For example, if an email contains Japanese in the Subject, but the body
	// contains Russian characters, it will be considered "japanese".
	// 
	// The language is determined by where the Unicode chars fall in various blocks in
	// the Unicode Basic Multilingual Plane. For more information, see the book
	// "Unicode Demystified" by Richard Gillam.
	// 
	const wchar_t *language(void);

	// The date/time found in the "Date" header field returned in the local timezone.
	void get_LocalDate(SYSTEMTIME &outSysTime);
	// The date/time found in the "Date" header field returned in the local timezone.
	void put_LocalDate(const SYSTEMTIME &sysTime);

	// The date/time found in the "Date" header field returned in the local timezone in
	// RFC822 string form.
	void get_LocalDateStr(CkString &str);
	// The date/time found in the "Date" header field returned in the local timezone in
	// RFC822 string form.
	const wchar_t *localDateStr(void);
	// The date/time found in the "Date" header field returned in the local timezone in
	// RFC822 string form.
	void put_LocalDateStr(const wchar_t *newVal);

	// Identifies the email software that sent the email.
	void get_Mailer(CkString &str);
	// Identifies the email software that sent the email.
	const wchar_t *mailer(void);
	// Identifies the email software that sent the email.
	void put_Mailer(const wchar_t *newVal);

	// The number of alternative bodies present in the email. An email that is not
	// "multipart/alternative"will return 0 alternatives. An email that is
	// "multipart/alternative" will return a number greater than or equal to 1.
	int get_NumAlternatives(void);

	// Returns the number of embedded emails. Some mail clients will embed an email
	// that is to be forwarded into a new email as a "message/rfc822" subpart of the
	// MIME message structure. This property tells how many emails have been embedded.
	// The original email can be retrieved by calling GetAttachedMessage.
	int get_NumAttachedMessages(void);

	// The number of attachments contained in the email.
	// 
	// Note: If an email is downloaded from an IMAP server without attachments, then
	// the number of attachments should be obtained by calling the IMAP object's
	// GetMailNumAttach method. This property indicates the actual number of
	// attachments already present within the email object.
	// 
	int get_NumAttachments(void);

	// The number of blind carbon-copy email recipients.
	int get_NumBcc(void);

	// The number of carbon-copy email recipients.
	int get_NumCC(void);

	// Returns the number of days old from the current system date/time. The email's
	// date is obtained from the "Date" header field. If the Date header field is
	// missing, or invalid, then -9999 is returned. A negative number may be returned
	// if the Date header field contains a future date/time. (However, -9999 represents
	// an error condition.)
	int get_NumDaysOld(void);

	// The number of header fields.
	int get_NumHeaderFields(void);

	// The number of related items present in this email. Related items are typically
	// image files (JPEGs or GIFs) or style sheets (CSS files) that are included with
	// HTML formatted messages with internal "CID"hyperlinks.
	int get_NumRelatedItems(void);

	// Returns the number of replacement patterns previously set by calling the
	// SetReplacePattern method 1 or more times. If replacement patterns are set, the
	// email bodies and header fields are modified by applying the search/replacement
	// strings during the message sending process.
	int get_NumReplacePatterns(void);

	// (For multipart/report emails that have sub-parts with Content-Types such as
	// message/feedback-report.) Any MIME sub-part within the email that has a
	// Content-Type of "message/*", but is not a "message/rfc822", is considered to be
	// a "report" and is included in this count. (A "message/rfc822" is considered an
	// attached message and is handled by the NumAttachedMessages property and the
	// GetAttachedMessage method.) Any MIME sub-part having a Content-Type equal to
	// "text/rfc822-headers" is also considered to be a "report". The GetReport method
	// may be called to get the body content of each "report" contained within a
	// multipart/report email.
	int get_NumReports(void);

	// The number of direct email recipients.
	int get_NumTo(void);

	// When true (the default) the methods to save email attachments and related
	// items will overwrite files if they already exist. If false, then the methods
	// that save email attachments and related items will append a string of 4
	// characters to create a unique filename if a file already exists. The filename of
	// the attachment (or related item) within the email object is updated and can be
	// retrieved by the program to determine the actual file(s) created.
	bool get_OverwriteExisting(void);
	// When true (the default) the methods to save email attachments and related
	// items will overwrite files if they already exist. If false, then the methods
	// that save email attachments and related items will append a string of 4
	// characters to create a unique filename if a file already exists. The filename of
	// the attachment (or related item) within the email object is updated and can be
	// retrieved by the program to determine the actual file(s) created.
	void put_OverwriteExisting(bool newVal);

	// When an email is sent encrypted (using PKCS7 public-key encryption), this
	// selects the underlying symmetric encryption algorithm. Possible values are:
	// "aes", "des", "3des", and "rc2".
	void get_Pkcs7CryptAlg(CkString &str);
	// When an email is sent encrypted (using PKCS7 public-key encryption), this
	// selects the underlying symmetric encryption algorithm. Possible values are:
	// "aes", "des", "3des", and "rc2".
	const wchar_t *pkcs7CryptAlg(void);
	// When an email is sent encrypted (using PKCS7 public-key encryption), this
	// selects the underlying symmetric encryption algorithm. Possible values are:
	// "aes", "des", "3des", and "rc2".
	void put_Pkcs7CryptAlg(const wchar_t *newVal);

	// When the email is sent encrypted (using PKCS7 public-key encryption), this
	// selects the key length of the underlying symmetric encryption algorithm. The
	// possible values allowed depend on the Pkcs7CryptAlg property. For "aes", the key
	// length may be 128, 192, or 256. For "3des" the key length must be 192. For "des"
	// the key length must be 40. For "rc2" the key length can be 40, 56, 64, or 128.
	int get_Pkcs7KeyLength(void);
	// When the email is sent encrypted (using PKCS7 public-key encryption), this
	// selects the key length of the underlying symmetric encryption algorithm. The
	// possible values allowed depend on the Pkcs7CryptAlg property. For "aes", the key
	// length may be 128, 192, or 256. For "3des" the key length must be 192. For "des"
	// the key length must be 40. For "rc2" the key length can be 40, 56, 64, or 128.
	void put_Pkcs7KeyLength(int newVal);

	// Only applies when building an email with non-English characters where the
	// charset is not explicitly set. The Chilkat email component will automatically
	// choose a charset based on the languages found within an email (if the charset is
	// not already specified within the MIME or explicitly specified by setting the
	// Charset property). The default charset chosen for each language is:
	// 
	// Chinese: gb2312
	// Japanese: shift_JIS
	// Korean: ks_c_5601-1987
	// Thai: windows-874
	// All others: iso-8859-*
	// 
	// This allows for charsets such as iso-2022-jp to be chosen instead of the
	// default. If the preferred charset does not apply to the situation, it is not
	// used. For example, if the preferred charset is iso-2022-jp, but the email
	// contains Greek characters, then the preferred charset is ignored.
	// 
	void get_PreferredCharset(CkString &str);
	// Only applies when building an email with non-English characters where the
	// charset is not explicitly set. The Chilkat email component will automatically
	// choose a charset based on the languages found within an email (if the charset is
	// not already specified within the MIME or explicitly specified by setting the
	// Charset property). The default charset chosen for each language is:
	// 
	// Chinese: gb2312
	// Japanese: shift_JIS
	// Korean: ks_c_5601-1987
	// Thai: windows-874
	// All others: iso-8859-*
	// 
	// This allows for charsets such as iso-2022-jp to be chosen instead of the
	// default. If the preferred charset does not apply to the situation, it is not
	// used. For example, if the preferred charset is iso-2022-jp, but the email
	// contains Greek characters, then the preferred charset is ignored.
	// 
	const wchar_t *preferredCharset(void);
	// Only applies when building an email with non-English characters where the
	// charset is not explicitly set. The Chilkat email component will automatically
	// choose a charset based on the languages found within an email (if the charset is
	// not already specified within the MIME or explicitly specified by setting the
	// Charset property). The default charset chosen for each language is:
	// 
	// Chinese: gb2312
	// Japanese: shift_JIS
	// Korean: ks_c_5601-1987
	// Thai: windows-874
	// All others: iso-8859-*
	// 
	// This allows for charsets such as iso-2022-jp to be chosen instead of the
	// default. If the preferred charset does not apply to the situation, it is not
	// used. For example, if the preferred charset is iso-2022-jp, but the email
	// contains Greek characters, then the preferred charset is ignored.
	// 
	void put_PreferredCharset(const wchar_t *newVal);

	// If true, then header fields added via the AddHeaderField or AddHeaderField2
	// methods are prepended to the top of the header as opposed to appended to the
	// bottom. The default value is false.
	bool get_PrependHeaders(void);
	// If true, then header fields added via the AddHeaderField or AddHeaderField2
	// methods are prepended to the top of the header as opposed to appended to the
	// bottom. The default value is false.
	void put_PrependHeaders(bool newVal);

	// true if this email was originally received with encryption, otherwise false.
	bool get_ReceivedEncrypted(void);

	// true if this email was originally received with a digital signature, otherwise
	// false.
	bool get_ReceivedSigned(void);

	// The email address to be used when a recipient replies.
	void get_ReplyTo(CkString &str);
	// The email address to be used when a recipient replies.
	const wchar_t *replyTo(void);
	// The email address to be used when a recipient replies.
	void put_ReplyTo(const wchar_t *newVal);

	// Set to true if you want the email to request a return-receipt when received by
	// the recipient. The default value is false.
	bool get_ReturnReceipt(void);
	// Set to true if you want the email to request a return-receipt when received by
	// the recipient. The default value is false.
	void put_ReturnReceipt(bool newVal);

	// Set to true if this email should be sent encrypted.
	bool get_SendEncrypted(void);
	// Set to true if this email should be sent encrypted.
	void put_SendEncrypted(bool newVal);

	// Set to true if this email should be sent with a digital signature.
	bool get_SendSigned(void);
	// Set to true if this email should be sent with a digital signature.
	void put_SendSigned(bool newVal);

	// true if the email was received with one or more digital signatures, and if all
	// the signatures were validated indicating that the email was not altered.
	// Otherwise this property is set to false.
	bool get_SignaturesValid(void);

	// If the email was received digitally signed, this property contains the fields of
	// the cert's SubjectDN.
	// 
	// For example: US, 60187, Illinois, Wheaton, 1719 E Forest Ave, "Chilkat Software,
	// Inc.", "Chilkat Software, Inc."
	// 
	// It is like the DN (Distinguished Name), but without the "AttrName=" before each
	// attribute.
	// 
	void get_SignedBy(CkString &str);
	// If the email was received digitally signed, this property contains the fields of
	// the cert's SubjectDN.
	// 
	// For example: US, 60187, Illinois, Wheaton, 1719 E Forest Ave, "Chilkat Software,
	// Inc.", "Chilkat Software, Inc."
	// 
	// It is like the DN (Distinguished Name), but without the "AttrName=" before each
	// attribute.
	// 
	const wchar_t *signedBy(void);

	// Selects the underlying hash algorithm used when sending signed (PKCS7) email.
	// Possible values are "sha1", "sha256", "sha384", "sha512", "md5", and "md2".
	void get_SigningHashAlg(CkString &str);
	// Selects the underlying hash algorithm used when sending signed (PKCS7) email.
	// Possible values are "sha1", "sha256", "sha384", "sha512", "md5", and "md2".
	const wchar_t *signingHashAlg(void);
	// Selects the underlying hash algorithm used when sending signed (PKCS7) email.
	// Possible values are "sha1", "sha256", "sha384", "sha512", "md5", and "md2".
	void put_SigningHashAlg(const wchar_t *newVal);

	// The size in bytes of the email, including all parts and attachments.
	int get_Size(void);

	// The email subject.
	void get_Subject(CkString &str);
	// The email subject.
	const wchar_t *subject(void);
	// The email subject.
	void put_Subject(const wchar_t *newVal);

	// This is the unique ID assigned by the POP3 server. Emails can be retrieved or
	// deleted from the POP3 server via the UIDL. The header field for this property is
	// "X-UIDL".
	// 
	// Important: Emails downloaded via the IMAP protocol do not have UIDL's. UIDL's
	// are specific to the POP3 protocol. IMAP servers use UID's (notice the spelling
	// difference -- "UIDL" vs. "UID"). An email downloaded via the Chilkat IMAP
	// component will contain a "ckx-imap-uid" header field that contains the UID. This
	// may be accessed via the GetHeaderField method.
	// 
	void get_Uidl(CkString &str);
	// This is the unique ID assigned by the POP3 server. Emails can be retrieved or
	// deleted from the POP3 server via the UIDL. The header field for this property is
	// "X-UIDL".
	// 
	// Important: Emails downloaded via the IMAP protocol do not have UIDL's. UIDL's
	// are specific to the POP3 protocol. IMAP servers use UID's (notice the spelling
	// difference -- "UIDL" vs. "UID"). An email downloaded via the Chilkat IMAP
	// component will contain a "ckx-imap-uid" header field that contains the UID. This
	// may be accessed via the GetHeaderField method.
	// 
	const wchar_t *uidl(void);

	// Applies to the UnpackHtml method. If true, then relative paths are used within
	// the HTML for the links to the related files (images and style sheets) that were
	// unpacked to the filesystem. Otherwise absolute paths are used. The default value
	// is true.
	bool get_UnpackUseRelPaths(void);
	// Applies to the UnpackHtml method. If true, then relative paths are used within
	// the HTML for the links to the related files (images and style sheets) that were
	// unpacked to the filesystem. Otherwise absolute paths are used. The default value
	// is true.
	void put_UnpackUseRelPaths(bool newVal);

	// The sender's address for this email message.
	// 
	// This is the address of the actual sender acting on behalf of the author listed
	// in the From: field.
	// 
	// Note: The Sender and BounceAddress properties are identical and perform the same
	// function. Setting the Sender property also sets the BounceAddress property, and
	// vice-versa. The reason for the duplication is that BounceAddress existed first,
	// and developers typically searched for a "Sender" property without realizing that
	// the BounceAddress property served this function.
	// 
	void get_Sender(CkString &str);
	// The sender's address for this email message.
	// 
	// This is the address of the actual sender acting on behalf of the author listed
	// in the From: field.
	// 
	// Note: The Sender and BounceAddress properties are identical and perform the same
	// function. Setting the Sender property also sets the BounceAddress property, and
	// vice-versa. The reason for the duplication is that BounceAddress existed first,
	// and developers typically searched for a "Sender" property without realizing that
	// the BounceAddress property served this function.
	// 
	const wchar_t *sender(void);
	// The sender's address for this email message.
	// 
	// This is the address of the actual sender acting on behalf of the author listed
	// in the From: field.
	// 
	// Note: The Sender and BounceAddress properties are identical and perform the same
	// function. Setting the Sender property also sets the BounceAddress property, and
	// vice-versa. The reason for the duplication is that BounceAddress existed first,
	// and developers typically searched for a "Sender" property without realizing that
	// the BounceAddress property served this function.
	// 
	void put_Sender(const wchar_t *newVal);



	// ----------------------
	// Methods
	// ----------------------
	// Adds or replaces a MIME header field in one of the email attachments. If the
	// header field does not exist, it is added. Otherwise it is replaced.
	void AddAttachmentHeader(int index, const wchar_t *fieldName, const wchar_t *fieldValue);

	// Adds a recipient to the blind carbon-copy list. address is required, but name
	// may be empty.
	bool AddBcc(const wchar_t *friendlyName, const wchar_t *emailAddress);

	// Adds a recipient to the carbon-copy list. address is required, but name may be
	// empty.
	bool AddCC(const wchar_t *friendlyName, const wchar_t *emailAddress);

	// Adds an attachment directly from data in memory to the email.
	bool AddDataAttachment(const wchar_t *filePath, const CkByteData &content);

	// Adds an attachment to an email from in-memory data. Same as AddDataAttachment
	// but allows the content-type to be specified.
	bool AddDataAttachment2(const wchar_t *path, const CkByteData &content, const wchar_t *contentType);

	// Allows for certificates to be explicitly specified for sending encrypted email
	// to one or more recipients. Call this method once per certificate to be used. The
	// ClearEncryptCerts method may be called to clear the list of explicitly-specified
	// certificates.
	// 
	// Note: It is possible to send encrypted email without explicitly specifying the
	// certificates. The Chilkat email component will automatically search the
	// registry-based Current-User and Local-Machine certificate stores for certs
	// matching each of the recipients (To, CC, and BCC recipients).
	// 
	// Note: The SentEncryptCert method is equivalent to calling ClearEncryptCerts
	// followed by AddEncryptCert.
	// 
	bool AddEncryptCert(CkCertW &cert);

	// Adds a file as an attachment to the email. Returns the MIME content-type of the
	// attachment, which is inferred based on the filename extension.
	bool AddFileAttachment(const wchar_t *path, CkString &outStrContentType);
	// Adds a file as an attachment to the email. Returns the MIME content-type of the
	// attachment, which is inferred based on the filename extension.
	const wchar_t *addFileAttachment(const wchar_t *path);

	// Same as AddFileAttachment, but the content type can be explicitly specified.
	bool AddFileAttachment2(const wchar_t *path, const wchar_t *contentType);

	// Any standard or non-standard (custom) header field can be added to the email
	// with this method. One interesting feature is that all header fields whose name
	// begins with "CKX-" will not be included in the header when an email is sent.
	// These fields will be included when saved to or loaded from XML. This makes it
	// easy to include persistent meta-data with an email which your programs can use
	// in any way it chooses.
	// 
	// Important: This method will replace an already-existing header field. To allow
	// for adding duplicate header fields, call AddHeaderField2 (see below).
	// 
	void AddHeaderField(const wchar_t *fieldName, const wchar_t *fieldValue);

	// This method is the same as AddHeaderField, except that if the header field
	// already exists, it is not replaced. A duplicate header will be added.
	void AddHeaderField2(const wchar_t *fieldName, const wchar_t *fieldValue);

	// Sets the HTML body of the email. Use this method if there will be multiple
	// versions of the body, but in different formats, such as HTML and plain text.
	// Otherwise, set the body by calling the SetHtmlBody method.
	bool AddHtmlAlternativeBody(const wchar_t *body);

	// Adds multiple recipients to the blind carbon-copy list. The parameter is a
	// string containing a comma separated list of full email addresses. Returns True
	// if successful.
	bool AddMultipleBcc(const wchar_t *commaSeparatedAddresses);

	// Adds multiple recipients to the carbon-copy list. The parameter is a string
	// containing a comma separated list of full email addresses. Returns True if
	// successful.
	bool AddMultipleCC(const wchar_t *commaSeparatedAddresses);

	// Adds multiple recipients to the "to" list. The parameter is a string containing
	// a comma separated list of full email addresses. Returns True if successful.
	bool AddMultipleTo(const wchar_t *commaSeparatedAddresses);

	// Adds a PFX to the object's internal list of sources to be searched for
	// certificates and private keys when decrypting. Multiple PFX sources can be added
	// by calling this method once for each. (On the Windows operating system, the
	// registry-based certificate stores are also automatically searched, so it is
	// commonly not required to explicitly add PFX sources.)
	// 
	// The pfxBytes contains the bytes of a PFX file (also known as PKCS12 or .p12).
	// 
	bool AddPfxSourceData(const CkByteData &pfxData, const wchar_t *password);

	// Adds a PFX file to the object's internal list of sources to be searched for
	// certificates and private keys when decrypting. Multiple PFX files can be added
	// by calling this method once for each. (On the Windows operating system, the
	// registry-based certificate stores are also automatically searched, so it is
	// commonly not required to explicitly add PFX sources.)
	// 
	// The pfxFilePath contains the bytes of a PFX file (also known as PKCS12 or .p12).
	// 
	bool AddPfxSourceFile(const wchar_t *pfxFilePath, const wchar_t *password);

	// Sets the plain-text body of the email. Use this method if there will be multiple
	// versions of the body, but in different formats, such as HTML and plain text.
	// Otherwise, simply set the Body property.
	bool AddPlainTextAlternativeBody(const wchar_t *body);

	// Adds the memory data as a related item to the email and returns the Content-ID.
	// Emails formatted in HTML can include images with this call and internally
	// reference the image through a "cid"hyperlink. (Chilkat Email.NET fully supports
	// the MHTML standard.)
	bool AddRelatedData(const wchar_t *path, const CkByteData &inData, CkString &outStr);
	// Adds the memory data as a related item to the email and returns the Content-ID.
	// Emails formatted in HTML can include images with this call and internally
	// reference the image through a "cid"hyperlink. (Chilkat Email.NET fully supports
	// the MHTML standard.)
	const wchar_t *addRelatedData(const wchar_t *path, const CkByteData &inData);

	// Adds a related item to the email from in-memory byte data. Related items are
	// things such as images and style sheets that are embedded within an HTML email.
	// They are not considered attachments because their sole purpose is to participate
	// in the display of the HTML. This method differs from AddRelatedData in that it
	// does not use or return a Content-ID. The filename argument should be set to the
	// filename used in the HTML img tag's src attribute (if it's an image), or the URL
	// referenced in an HTML link tag for a stylesheet.
	void AddRelatedData2(const CkByteData &inData, const wchar_t *fileNameInHtml);

#if !defined(CHILKAT_MONO)
	// The same as AddRelatedData2, except the data is passed in as a "const unsigned
	// char *" with the byte count in  szBytes.
	void AddRelatedData2P(const unsigned char *pByteData, unsigned long szByteData, const wchar_t *fileNameInHtml);
#endif

#if !defined(CHILKAT_MONO)
	// The same as AddRelatedData, except the data is passed in as a "const unsigned
	// char *" with the byte count in  szBytes. The Content-ID assigned to the related item
	// is returned (in  outStrContentId for the upper-case alternative for this method).
	bool AddRelatedDataP(const wchar_t *nameInHtml, const unsigned char *pByteData, unsigned long szByteData, CkString &outStrContentId);
	// The same as AddRelatedData, except the data is passed in as a "const unsigned
	// char *" with the byte count in  szBytes. The Content-ID assigned to the related item
	// is returned (in  outStrContentId for the upper-case alternative for this method).
	const wchar_t *addRelatedDataP(const wchar_t *nameInHtml, const unsigned char *pByteData, unsigned long szByteData);
#endif

	// Adds the contents of a file to the email and returns the Content-ID. Emails
	// formatted in HTML can include images with this call and internally reference the
	// image through a "cid" hyperlink. (Chilkat Email.NET fully supports the MHTML
	// standard.)
	bool AddRelatedFile(const wchar_t *path, CkString &outStrContentID);
	// Adds the contents of a file to the email and returns the Content-ID. Emails
	// formatted in HTML can include images with this call and internally reference the
	// image through a "cid" hyperlink. (Chilkat Email.NET fully supports the MHTML
	// standard.)
	const wchar_t *addRelatedFile(const wchar_t *path);

	// Adds a related item to the email from a file. Related items are things such as
	// images and style sheets that are embedded within an HTML email. They are not
	// considered attachments because their sole purpose is to participate in the
	// display of the HTML. This method differs from AddRelatedFile in that it does not
	// use or return a Content-ID. The filenameInHtml argument should be set to the
	// filename used in the HTML img tag's src attribute (if it's an image), or the URL
	// referenced in an HTML link tag for a stylesheet.
	bool AddRelatedFile2(const wchar_t *filenameOnDisk, const wchar_t *filenameInHtml);

	// Adds or replaces a MIME header field in one of the email's related items. If the
	// header field does not exist, it is added. Otherwise it is replaced.
	void AddRelatedHeader(int index, const wchar_t *fieldName, const wchar_t *fieldValue);

	// Adds a related item to the email. A related item is typically an image or style
	// sheet referenced by an HTML tag within the HTML email body. The contents of the
	// related item are passed  str. nameInHtml specifies the filename that should be used
	// within the HTML, and not an actual filename on the local filesystem.  charset
	// specifies the charset that should be used for the text content of the related
	// item. Returns the content-ID generated for the added item.
	bool AddRelatedString(const wchar_t *nameInHtml, const wchar_t *str, const wchar_t *charset, CkString &outCid);
	// Adds a related item to the email. A related item is typically an image or style
	// sheet referenced by an HTML tag within the HTML email body. The contents of the
	// related item are passed  str. nameInHtml specifies the filename that should be used
	// within the HTML, and not an actual filename on the local filesystem.  charset
	// specifies the charset that should be used for the text content of the related
	// item. Returns the content-ID generated for the added item.
	const wchar_t *addRelatedString(const wchar_t *nameInHtml, const wchar_t *str, const wchar_t *charset);

	// Adds a related item to the email from an in-memory string. Related items are
	// things such as images and style sheets that are embedded within an HTML email.
	// They are not considered attachments because their sole purpose is to participate
	// in the display of the HTML. The filenameInHtml argument should be set to the
	// filename used in the HTML img tag's src attribute (if it's an image), or the URL
	// referenced in an HTML link tag for a stylesheet. The charset argument indicates
	// that the content should first be converted to the specified charset prior to
	// adding to the email. It should hava a value such as "iso-8859-1", "utf-8",
	// "Shift_JIS", etc.
	void AddRelatedString2(const wchar_t *content, const wchar_t *charset, const wchar_t *fileNameInHtml);

	// Adds an attachment directly from a string in memory to the email.
	bool AddStringAttachment(const wchar_t *path, const wchar_t *content);

	// Adds an attachment to an email. The filename argument specifies the filename to
	// be used for the attachment and is not an actual filename existing on the local
	// filesystem. The "str" argument contains the text data for the attachment. The
	// string will be converted to the charset specified in the last argument before
	// being added to the email.
	bool AddStringAttachment2(const wchar_t *path, const wchar_t *content, const wchar_t *charset);

	// Adds a recipient to the "to" list. address is required, but name may be empty.
	// Emails that have no "To" recipients will be sent to
	// _LT_undisclosed-recipients_GT_.
	bool AddTo(const wchar_t *friendlyName, const wchar_t *emailAddress);

	// Adds an iCalendar (text/calendar) alternative body to the email. The icalContent
	// contains the content of the iCalendar data. A sample is shown here:
	// BEGIN:VCALENDAR
	// VERSION:2.0
	// PRODID:-//hacksw/handcal//NONSGML v1.0//EN
	// BEGIN:VEVENT
	// UID:uid1@example.com
	// DTSTAMP:19970714T170000Z
	// ORGANIZER;CN=John Doe:MAILTO:john.doe@example.com
	// DTSTART:19970714T170000Z
	// DTEND:19970715T035959Z
	// SUMMARY:Bastille Day Party
	// END:VEVENT
	// END:VCALENDAR
	// The  methodName is the "method" attribute used in the Content-Type header field in the
	// alternative body. For example, if set to "REQUEST", then the alternative body's
	// header would look like this:
	// Content-Type: text/calendar; method=REQUEST
	// Content-Transfer-Encoding: base64
	bool AddiCalendarAlternativeBody(const wchar_t *body, const wchar_t *methodName);

	// Decrypts and restores an email message that was previously encrypted using
	// AesEncrypt. The password must match the password used for encryption.
	bool AesDecrypt(const wchar_t *password);

	// Encrypts the email body, all alternative bodies, all message sub-parts and
	// attachments using 128-bit AES (Rijndael, CBC mode) encryption. To decrypt, you
	// must use the AesDecrypt method with the same password. The AesEncrypt/Decrypt
	// methods use symmetric password-based greatly simplify sending and receiving
	// encrypted emails because certificates and public/private key issues do not have
	// to be dealt with. However, the sending and receiving applications must both be
	// using Chilkat Email .NET or ActiveX components.
	bool AesEncrypt(const wchar_t *password);

	// Appends a string to the plain-text body.
	void AppendToBody(const wchar_t *str);

	// Please see the examples at the following pages for detailed information:
	bool AspUnpack(const wchar_t *prefix, const wchar_t *saveDir, const wchar_t *urlPath, bool cleanFiles);

	// Please see the examples at the following pages for detailed information:
	bool AspUnpack2(const wchar_t *prefix, const wchar_t *saveDir, const wchar_t *urlPath, bool cleanFiles, CkByteData &outHtml);

	// Attaches a MIME message to the email object. The attached MIME will be
	// encapsulated in an message/rfc822 sub-part. To attach one email object to
	// another, pass the output of GetMimeBinary to the input of this method.
	bool AttachMessage(const CkByteData &mimeBytes);

	// Takes a byte array of multibyte (non-Unicode) data and returns a Unicode
	// B-Encoded string.
	bool BEncodeBytes(const CkByteData &inData, const wchar_t *charset, CkString &outEncodedStr);
	// Takes a byte array of multibyte (non-Unicode) data and returns a Unicode
	// B-Encoded string.
	const wchar_t *bEncodeBytes(const CkByteData &inData, const wchar_t *charset);

	// Takes a Unicode string, converts it to the charset specified in the 2nd
	// parameter, B-Encodes the converted multibyte data, and returns the encoded
	// Unicode string.
	bool BEncodeString(const wchar_t *str, const wchar_t *charset, CkString &outEncodedStr);
	// Takes a Unicode string, converts it to the charset specified in the 2nd
	// parameter, B-Encodes the converted multibyte data, and returns the encoded
	// Unicode string.
	const wchar_t *bEncodeString(const wchar_t *str, const wchar_t *charset);

	// Clears the list of blind carbon-copy recipients.
	void ClearBcc(void);

	// Clears the list of carbon-copy recipients.
	void ClearCC(void);

	// Clears the internal list of explicitly specified certificates to be used for
	// this encrypted email.
	void ClearEncryptCerts(void);

	// Clears the list of "to" recipients.
	void ClearTo(void);

	// Creates and returns an identical copy of the Email object.
	// The caller is responsible for deleting the object returned by this method.
	CkEmailW *Clone(void);

	// Computes a global unique key for the email that may be used as a key for a
	// relational database table (or anything else). The key is created by a digest-MD5
	// hash of the concatenation of the following header fields: Message-ID, Subject,
	// From, Date, To. (The header fields are Q/B decoded if necessary, converted to
	// the utf-8 encoding, concatenated, and hashed using MD5.) The 16-byte MD5 hash is
	// returned as an encoded string. The encoding determines the encoding: base64, hex,
	// url, etc. If  bFold is true, then the 16-byte MD5 hash is folded to 8 bytes with
	// an XOR to produce a shorter key.
	bool ComputeGlobalKey(const wchar_t *encoding, bool bFold, CkString &outStr);
	// Computes a global unique key for the email that may be used as a key for a
	// relational database table (or anything else). The key is created by a digest-MD5
	// hash of the concatenation of the following header fields: Message-ID, Subject,
	// From, Date, To. (The header fields are Q/B decoded if necessary, converted to
	// the utf-8 encoding, concatenated, and hashed using MD5.) The 16-byte MD5 hash is
	// returned as an encoded string. The encoding determines the encoding: base64, hex,
	// url, etc. If  bFold is true, then the 16-byte MD5 hash is folded to 8 bytes with
	// an XOR to produce a shorter key.
	const wchar_t *computeGlobalKey(const wchar_t *encoding, bool bFold);

	// Creates a new DSN (Delivery Status Notification) email having the format as
	// specified in RFC 3464. See the example (below) for more detailed information.
	// The caller is responsible for deleting the object returned by this method.
	CkEmailW *CreateDsn(const wchar_t *explanation, const wchar_t *xmlDeliveryStatus, bool bHeaderOnly);

	// Returns a copy of the Email object with the body and header fields changed so
	// that the newly created email can be forwarded. After calling CreateForward,
	// simply add new recipients to the created email, and call MailMan.SendEmail.
	// The caller is responsible for deleting the object returned by this method.
	CkEmailW *CreateForward(void);

	// Creates a new MDN (Message Disposition Notification) email having the format as
	// specified in RFC 3798. See the example (below) for more detailed information.
	// The caller is responsible for deleting the object returned by this method.
	CkEmailW *CreateMdn(const wchar_t *explanation, const wchar_t *xmlMdnFields, bool bHeaderOnly);

	// Returns a copy of the Email object with the body and header fields changed so
	// that the newly created email can be sent as a reply. After calling CreateReply,
	// simply prepend additional information to the body, and call MailMan.SendEmail.
	// The caller is responsible for deleting the object returned by this method.
	CkEmailW *CreateReply(void);

	// Saves the email to a temporary MHT file so that a WebBrowser control can
	// navigate to it and display it. If fileName is empty, a temporary filename is
	// generated and returned. If fileName is non-empty, then it will be created or
	// overwritten, and the input filename is simply returned.The MHT file that is
	// created will not contain any of the email's attachments, if any existed. Also,
	// if the email was plain-text, the MHT file will be saved such that the plain-text
	// is converted to HTML using pre-formatted text ("pre" HTML tags) allowing it to
	// be displayed correctly in a WebBrowser.
	bool CreateTempMht(const wchar_t *inFilename, CkString &outPath);
	// Saves the email to a temporary MHT file so that a WebBrowser control can
	// navigate to it and display it. If fileName is empty, a temporary filename is
	// generated and returned. If fileName is non-empty, then it will be created or
	// overwritten, and the input filename is simply returned.The MHT file that is
	// created will not contain any of the email's attachments, if any existed. Also,
	// if the email was plain-text, the MHT file will be saved such that the plain-text
	// is converted to HTML using pre-formatted text ("pre" HTML tags) allowing it to
	// be displayed correctly in a WebBrowser.
	const wchar_t *createTempMht(const wchar_t *inFilename);

	// Removes all attachments from the email.
	void DropAttachments(void);

	// A related item is typically an embedded image referenced from the HTML in the
	// email via a "CID" hyperlink. This method removes the Nth embedded image from the
	// email. Note: If the HTML tries to reference the removed image, it will be
	// displayed as a broken image link.
	void DropRelatedItem(int index);

	// A related item is typically an embedded image referenced from the HTML in the
	// email via a "CID" hyperlink. This method removes all the embedded images from
	// the email.
	void DropRelatedItems(void);

	// Drops a single attachment from the email. Returns True if successful.
	bool DropSingleAttachment(int index);

	// Digitally signed and/or encrypted emails are automatically "unwrapped" when
	// received from a POP3 or IMAP server, or when loaded from any source such as a
	// MIME string, in-memory byte data, or a .eml file. The results of the signature
	// verification / decryption are stored in the properties such as ReceivedSigned,
	// ReceivedEncrypted, SignaturesValid, etc. The signing certificate can be obtained
	// via the GetSigningCert function. If the signature contained more certificates in
	// the chain of authentication, this method provides a means to access them.
	// 
	// During signature verification, the email object collects the certs found in the
	// signature and holds onto them internally. To get the issuing certificate of the
	// signing certificate, call this method passing the cert returned by
	// GetSigningCert. If the issuing cert is available, it is returned. Otherwise
	// _NULL_ is returned. If the cert passed in is the root (i.e. a self-signed
	// certificate), then the cert object returned is a copy of the cert passed in.
	// 
	// To traverse the chain to the root, one would write a loop that on first
	// iteration passes the cert returned by GetSignedByCert (not GetSignerCert), and
	// then on each subsequent iteration passes the cert from the previous iteration.
	// The loop would exit when a cert is returned that has the same SubjectDN and
	// SerialNumber as what was passed in (or when FindIssuer returns _NULL_).
	// 
	// The caller is responsible for deleting the object returned by this method.
	CkCertW *FindIssuer(CkCertW &cert);

	// Generates a unique filename for this email. The filename will be different each
	// time the method is called.
	bool GenerateFilename(CkString &outStrFilename);
	// Generates a unique filename for this email. The filename will be different each
	// time the method is called.
	const wchar_t *generateFilename(void);

	// Returns the value of a header field within the Nth alternative body's MIME
	// sub-part.
	bool GetAltHeaderField(int index, const wchar_t *fieldName, CkString &outStr);
	// Returns the value of a header field within the Nth alternative body's MIME
	// sub-part.
	const wchar_t *getAltHeaderField(int index, const wchar_t *fieldName);
	// Returns the value of a header field within the Nth alternative body's MIME
	// sub-part.
	const wchar_t *altHeaderField(int index, const wchar_t *fieldName);

	// Returns the Nth alternative body. The NumAlternatives property tells the number
	// of alternative bodies present. Use the GetHtmlBody and GetPlainTextBody methods
	// to easily get the HTML or plain text alternative bodies.
	bool GetAlternativeBody(int index, CkString &outStrBody);
	// Returns the Nth alternative body. The NumAlternatives property tells the number
	// of alternative bodies present. Use the GetHtmlBody and GetPlainTextBody methods
	// to easily get the HTML or plain text alternative bodies.
	const wchar_t *getAlternativeBody(int index);
	// Returns the Nth alternative body. The NumAlternatives property tells the number
	// of alternative bodies present. Use the GetHtmlBody and GetPlainTextBody methods
	// to easily get the HTML or plain text alternative bodies.
	const wchar_t *alternativeBody(int index);

	// Returns the alternative body by content-type, such as "text/plain", "text/html",
	// "text/xml", etc.
	bool GetAlternativeBodyByContentType(const wchar_t *contentType, CkString &outStr);
	// Returns the alternative body by content-type, such as "text/plain", "text/html",
	// "text/xml", etc.
	const wchar_t *getAlternativeBodyByContentType(const wchar_t *contentType);
	// Returns the alternative body by content-type, such as "text/plain", "text/html",
	// "text/xml", etc.
	const wchar_t *alternativeBodyByContentType(const wchar_t *contentType);

	// Returns the content type of the Nth alternative body. The NumAlternatives
	// property tells the number of alternative bodies present.
	bool GetAlternativeContentType(int index, CkString &outStrContentType);
	// Returns the content type of the Nth alternative body. The NumAlternatives
	// property tells the number of alternative bodies present.
	const wchar_t *getAlternativeContentType(int index);
	// Returns the content type of the Nth alternative body. The NumAlternatives
	// property tells the number of alternative bodies present.
	const wchar_t *alternativeContentType(int index);

	// Returns an embedded "message/rfc822" subpart as an email object. (Emails are
	// embedded as "message/rfc822" subparts by some mail clients when forwarding an
	// email.) This method allows the original email to be accessed.
	// The caller is responsible for deleting the object returned by this method.
	CkEmailW *GetAttachedMessage(int index);

	// Returns the filename of the Nth attached (embedded) email. The filename is the
	// "filename" attribute of the content-disposition header field found within the
	// Nth message/rfc822 sub-part of the calling email object.
	bool GetAttachedMessageFilename(int index, CkString &outStr);
	// Returns the filename of the Nth attached (embedded) email. The filename is the
	// "filename" attribute of the content-disposition header field found within the
	// Nth message/rfc822 sub-part of the calling email object.
	const wchar_t *getAttachedMessageFilename(int index);
	// Returns the filename of the Nth attached (embedded) email. The filename is the
	// "filename" attribute of the content-disposition header field found within the
	// Nth message/rfc822 sub-part of the calling email object.
	const wchar_t *attachedMessageFilename(int index);

	// Returns the ContentID header field for the Nth attachment. The first attachment
	// is at index 0.
	bool GetAttachmentContentID(int index, CkString &outStrContentID);
	// Returns the ContentID header field for the Nth attachment. The first attachment
	// is at index 0.
	const wchar_t *getAttachmentContentID(int index);
	// Returns the ContentID header field for the Nth attachment. The first attachment
	// is at index 0.
	const wchar_t *attachmentContentID(int index);

	// Returns the Content-Type header field for the Nth attachment. Indexing of
	// attachments begins at 0.
	bool GetAttachmentContentType(int index, CkString &outStrContentType);
	// Returns the Content-Type header field for the Nth attachment. Indexing of
	// attachments begins at 0.
	const wchar_t *getAttachmentContentType(int index);
	// Returns the Content-Type header field for the Nth attachment. Indexing of
	// attachments begins at 0.
	const wchar_t *attachmentContentType(int index);

	// Retrieves an attachment's binary data for in-memory access.
	bool GetAttachmentData(int index, CkByteData &outData);

	// Retrieves an attachment's filename.
	bool GetAttachmentFilename(int index, CkString &outStrFilename);
	// Retrieves an attachment's filename.
	const wchar_t *getAttachmentFilename(int index);
	// Retrieves an attachment's filename.
	const wchar_t *attachmentFilename(int index);

	// Returns the value of a header field (by name) of an attachment.
	bool GetAttachmentHeader(int attachIndex, const wchar_t *fieldName, CkString &outFieldValue);
	// Returns the value of a header field (by name) of an attachment.
	const wchar_t *getAttachmentHeader(int attachIndex, const wchar_t *fieldName);
	// Returns the value of a header field (by name) of an attachment.
	const wchar_t *attachmentHeader(int attachIndex, const wchar_t *fieldName);

	// Returns the size (in bytes) of the Nth attachment. The 1st attachment is at
	// index 0. Returns -1 if there is no attachment at the specified index.
	int GetAttachmentSize(int index);

	// Retrieves an attachment's data as a String. All CRLF sequences will be
	// translated to single newline characters.
	bool GetAttachmentString(int index, const wchar_t *charset, CkString &outStr);
	// Retrieves an attachment's data as a String. All CRLF sequences will be
	// translated to single newline characters.
	const wchar_t *getAttachmentString(int index, const wchar_t *charset);
	// Retrieves an attachment's data as a String. All CRLF sequences will be
	// translated to single newline characters.
	const wchar_t *attachmentString(int index, const wchar_t *charset);

	// Retrieves an attachment's data as a String. All end-of-lines will be translated
	// to CRLF sequences.
	bool GetAttachmentStringCrLf(int index, const wchar_t *charset, CkString &outStrData);
	// Retrieves an attachment's data as a String. All end-of-lines will be translated
	// to CRLF sequences.
	const wchar_t *getAttachmentStringCrLf(int index, const wchar_t *charset);
	// Retrieves an attachment's data as a String. All end-of-lines will be translated
	// to CRLF sequences.
	const wchar_t *attachmentStringCrLf(int index, const wchar_t *charset);

	// Returns a blind carbon-copy recipient's full email address.
	bool GetBcc(int index, CkString &outStr);
	// Returns a blind carbon-copy recipient's full email address.
	const wchar_t *getBcc(int index);
	// Returns a blind carbon-copy recipient's full email address.
	const wchar_t *bcc(int index);

	// Returns the Nth BCC address (only the address part, not the friendly-name part).
	bool GetBccAddr(int index, CkString &outStr);
	// Returns the Nth BCC address (only the address part, not the friendly-name part).
	const wchar_t *getBccAddr(int index);
	// Returns the Nth BCC address (only the address part, not the friendly-name part).
	const wchar_t *bccAddr(int index);

	// Returns the Nth BCC name (only the friendly-name part, not the address part).
	bool GetBccName(int index, CkString &outStr);
	// Returns the Nth BCC name (only the friendly-name part, not the address part).
	const wchar_t *getBccName(int index);
	// Returns the Nth BCC name (only the friendly-name part, not the address part).
	const wchar_t *bccName(int index);

	// Returns a carbon-copy recipient's full email address.
	bool GetCC(int index, CkString &outStr);
	// Returns a carbon-copy recipient's full email address.
	const wchar_t *getCC(int index);
	// Returns a carbon-copy recipient's full email address.
	const wchar_t *cC(int index);

	// Returns the Nth CC address (only the address part, not the friendly-name part).
	bool GetCcAddr(int index, CkString &outStr);
	// Returns the Nth CC address (only the address part, not the friendly-name part).
	const wchar_t *getCcAddr(int index);
	// Returns the Nth CC address (only the address part, not the friendly-name part).
	const wchar_t *ccAddr(int index);

	// Returns the Nth CC name (only the friendly-name part, not the address part).
	bool GetCcName(int index, CkString &outStr);
	// Returns the Nth CC name (only the friendly-name part, not the address part).
	const wchar_t *getCcName(int index);
	// Returns the Nth CC name (only the friendly-name part, not the address part).
	const wchar_t *ccName(int index);

	// If the email is a multipart/report, then it is a delivery status notification.
	// This method can be used to get individual pieces of information from the
	// message/delivery-status part of the email. This method should only be called if
	// the IsMultipartReport method returns true.
	// 
	// The fieldName should be set a string such as "Final-Recipient", "Status", "Action",
	// "Reporting-MTA", etc.
	// Reporting-MTA: dns; XYZ.abc.nl
	// 
	// Final-recipient: RFC822; someEmailAddr@doesnotexist123.nl
	// Action: failed
	// Status: 5.4.4
	// X-Supplementary-Info: 
	// 
	bool GetDeliveryStatusInfo(const wchar_t *fieldName, CkString &outFieldValue);
	// If the email is a multipart/report, then it is a delivery status notification.
	// This method can be used to get individual pieces of information from the
	// message/delivery-status part of the email. This method should only be called if
	// the IsMultipartReport method returns true.
	// 
	// The fieldName should be set a string such as "Final-Recipient", "Status", "Action",
	// "Reporting-MTA", etc.
	// Reporting-MTA: dns; XYZ.abc.nl
	// 
	// Final-recipient: RFC822; someEmailAddr@doesnotexist123.nl
	// Action: failed
	// Status: 5.4.4
	// X-Supplementary-Info: 
	// 
	const wchar_t *getDeliveryStatusInfo(const wchar_t *fieldName);
	// If the email is a multipart/report, then it is a delivery status notification.
	// This method can be used to get individual pieces of information from the
	// message/delivery-status part of the email. This method should only be called if
	// the IsMultipartReport method returns true.
	// 
	// The fieldName should be set a string such as "Final-Recipient", "Status", "Action",
	// "Reporting-MTA", etc.
	// Reporting-MTA: dns; XYZ.abc.nl
	// 
	// Final-recipient: RFC822; someEmailAddr@doesnotexist123.nl
	// Action: failed
	// Status: 5.4.4
	// X-Supplementary-Info: 
	// 
	const wchar_t *deliveryStatusInfo(const wchar_t *fieldName);

	// If the email is a multipart/report, then it is a delivery status notification.
	// This method can be used to get Final-Recipient values from the
	// message/delivery-status part of the email. This method should only be called if
	// the IsMultipartReport method returns true.
	// The caller is responsible for deleting the object returned by this method.
	CkStringArrayW *GetDsnFinalRecipients(void);

	// Returns the date/time found in the "Date" header field as a date/time object.
	// The caller is responsible for deleting the object returned by this method.
	CkDateTimeW *GetDt(void);

	// Returns the certificate that was previously set by SetEncryptCert.
	// The caller is responsible for deleting the object returned by this method.
	CkCertW *GetEncryptCert(void);

	// Returns the certificate associated with a received encrypted email.
	// The caller is responsible for deleting the object returned by this method.
	CkCertW *GetEncryptedByCert(void);

	// Reads a file and returns the contents as a String. This is here purely for
	// convenience.
	bool GetFileContent(const wchar_t *path, CkByteData &outData);

	// Returns the value of a header field.
	bool GetHeaderField(const wchar_t *fieldName, CkString &outStrFieldData);
	// Returns the value of a header field.
	const wchar_t *getHeaderField(const wchar_t *fieldName);
	// Returns the value of a header field.
	const wchar_t *headerField(const wchar_t *fieldName);

	// Return the name of the Nth header field. The NumHeaderFields() method can be
	// used to get the number of header fields. The GetHeaderField() method can be used
	// to get the value of the field given the field name.
	bool GetHeaderFieldName(int index, CkString &outStrFieldName);
	// Return the name of the Nth header field. The NumHeaderFields() method can be
	// used to get the number of header fields. The GetHeaderField() method can be used
	// to get the value of the field given the field name.
	const wchar_t *getHeaderFieldName(int index);
	// Return the name of the Nth header field. The NumHeaderFields() method can be
	// used to get the number of header fields. The GetHeaderField() method can be used
	// to get the value of the field given the field name.
	const wchar_t *headerFieldName(int index);

	// Returns the value of the Nth header field. (Indexing begins at 0) The number of
	// header fields can be obtained from the NumHeaderFields property.
	bool GetHeaderFieldValue(int index, CkString &outStrFieldValue);
	// Returns the value of the Nth header field. (Indexing begins at 0) The number of
	// header fields can be obtained from the NumHeaderFields property.
	const wchar_t *getHeaderFieldValue(int index);
	// Returns the value of the Nth header field. (Indexing begins at 0) The number of
	// header fields can be obtained from the NumHeaderFields property.
	const wchar_t *headerFieldValue(int index);

	// Returns the body having the "text/html" content type.
	bool GetHtmlBody(CkString &outStrBody);
	// Returns the body having the "text/html" content type.
	const wchar_t *getHtmlBody(void);
	// Returns the body having the "text/html" content type.
	const wchar_t *htmlBody(void);

	// When email headers are downloaded from an IMAP server (using Chilkat IMAP), a
	// "ckx-imap-uid" header field is added. The content of this header is the UID or
	// sequence number of the email on the IMAP server. In addition, a "ckx-imap-isUid"
	// header field is added, and this will have the value YES or NO. If the value is
	// YES, then ckx-imap-uid contains a UID, if the value is NO, then ckx-imap-uid
	// contains the sequence number. This method returns the UID if ckx-imap-uid exists
	// and contains a UID, otherwise it returns -1.
	// 
	// An application that wishes to download the full email would use this UID and
	// then call the Chilkat IMAP object's FetchSingle or FetchSingleAsMime methods.
	// 
	// Note:If an email was downloaded from the IMAP server in a way such that the UID
	// is not received, then there will be no "ckx-imap-uid" header field and this
	// method would return -1. For example, if emails are downloaded by sequence
	// numbers via the Imap.FetchSequence method, then UIDs are not used and therefore
	// the email object will not contain this information.
	// 
	int GetImapUid(void);

	// Parses an HTML email and returns the set of domain names that occur in
	// hyperlinks within the HTML body.
	// The caller is responsible for deleting the object returned by this method.
	CkStringArrayW *GetLinkedDomains(void);

	// Returns a header field's data in a byte array. If the field was Q or B encoded,
	// this is automatically decoded, and the raw bytes of the field are returned. Call
	// GetHeaderField to retrieve the header field as a Unicode string.
	bool GetMbHeaderField(const wchar_t *fieldName, const wchar_t *charset, CkByteData &outBytes);

	// Returns the HTML body converted to a specified charset. If no HTML body exists,
	// the returned byte array is empty. The returned data will be such that not only
	// is the character data converted (if necessary) to the convertToCharset, but the
	// HTML is edited to add or modify the META tag that specifies the charset within
	// the HTML.
	bool GetMbHtmlBody(const wchar_t *charset, CkByteData &outData);

	// Returns the plain-text body converted to a specified charset. The return value
	// is a byte array containing multibyte character data.
	bool GetMbPlainTextBody(const wchar_t *charset, CkByteData &outData);

	// Return the email as MIME text containing the email header, body (or bodies),
	// related items (if any), and all attachments
	bool GetMime(CkString &outStrMime);
	// Return the email as MIME text containing the email header, body (or bodies),
	// related items (if any), and all attachments
	const wchar_t *getMime(void);
	// Return the email as MIME text containing the email header, body (or bodies),
	// related items (if any), and all attachments
	const wchar_t *mime(void);

	// Returns the full MIME of an email.
	bool GetMimeBinary(CkByteData &outBytes);

	// Returns the email body having the "text/plain" content type.
	bool GetPlainTextBody(CkString &outStrBody);
	// Returns the email body having the "text/plain" content type.
	const wchar_t *getPlainTextBody(void);
	// Returns the email body having the "text/plain" content type.
	const wchar_t *plainTextBody(void);

	// Returns the content ID of a related item contained with the email. Related items
	// are typically images and style-sheets embedded within HTML emails.
	bool GetRelatedContentID(int index, CkString &outStrContentID);
	// Returns the content ID of a related item contained with the email. Related items
	// are typically images and style-sheets embedded within HTML emails.
	const wchar_t *getRelatedContentID(int index);
	// Returns the content ID of a related item contained with the email. Related items
	// are typically images and style-sheets embedded within HTML emails.
	const wchar_t *relatedContentID(int index);

	// Returns the Content-Location of a related item contained with the email. Related
	// items are typically images and style-sheets embedded within HTML emails.
	bool GetRelatedContentLocation(int index, CkString &outStr);
	// Returns the Content-Location of a related item contained with the email. Related
	// items are typically images and style-sheets embedded within HTML emails.
	const wchar_t *getRelatedContentLocation(int index);
	// Returns the Content-Location of a related item contained with the email. Related
	// items are typically images and style-sheets embedded within HTML emails.
	const wchar_t *relatedContentLocation(int index);

	// Returns the content-type of the Nth related content item in an email message.
	bool GetRelatedContentType(int index, CkString &outStrContentType);
	// Returns the content-type of the Nth related content item in an email message.
	const wchar_t *getRelatedContentType(int index);
	// Returns the content-type of the Nth related content item in an email message.
	const wchar_t *relatedContentType(int index);

	// Returns the content of a related item contained with the email. Related items
	// are typically images and style-sheets embedded within HTML emails.
	bool GetRelatedData(int index, CkByteData &outBuffer);

	// Returns the filename of a related item contained with the email. Related items
	// are typically images and style-sheets embedded within HTML emails.
	bool GetRelatedFilename(int index, CkString &outStrFilename);
	// Returns the filename of a related item contained with the email. Related items
	// are typically images and style-sheets embedded within HTML emails.
	const wchar_t *getRelatedFilename(int index);
	// Returns the filename of a related item contained with the email. Related items
	// are typically images and style-sheets embedded within HTML emails.
	const wchar_t *relatedFilename(int index);

	// Returns the text with CR line-endings of a related item contained with the
	// email. Related items are typically images and style-sheets embedded within HTML
	// emails.
	bool GetRelatedString(int index, const wchar_t *charset, CkString &outStrData);
	// Returns the text with CR line-endings of a related item contained with the
	// email. Related items are typically images and style-sheets embedded within HTML
	// emails.
	const wchar_t *getRelatedString(int index, const wchar_t *charset);
	// Returns the text with CR line-endings of a related item contained with the
	// email. Related items are typically images and style-sheets embedded within HTML
	// emails.
	const wchar_t *relatedString(int index, const wchar_t *charset);

	// Returns the text with CRLF line-endings of a related item contained with the
	// email. Related items are typically images and style-sheets embedded within HTML
	// emails.
	bool GetRelatedStringCrLf(int index, const wchar_t *charset, CkString &outStr);
	// Returns the text with CRLF line-endings of a related item contained with the
	// email. Related items are typically images and style-sheets embedded within HTML
	// emails.
	const wchar_t *getRelatedStringCrLf(int index, const wchar_t *charset);
	// Returns the text with CRLF line-endings of a related item contained with the
	// email. Related items are typically images and style-sheets embedded within HTML
	// emails.
	const wchar_t *relatedStringCrLf(int index, const wchar_t *charset);

	// Returns a replacement pattern previously defined for mail-merge operations.
	bool GetReplacePattern(int index, CkString &outStrPattern);
	// Returns a replacement pattern previously defined for mail-merge operations.
	const wchar_t *getReplacePattern(int index);
	// Returns a replacement pattern previously defined for mail-merge operations.
	const wchar_t *replacePattern(int index);

	// Returns a replacement string for a previously defined pattern/replacement string
	// pair. (This is a mail-merge feature.)
	bool GetReplaceString(int index, CkString &outStr);
	// Returns a replacement string for a previously defined pattern/replacement string
	// pair. (This is a mail-merge feature.)
	const wchar_t *getReplaceString(int index);
	// Returns a replacement string for a previously defined pattern/replacement string
	// pair. (This is a mail-merge feature.)
	const wchar_t *replaceString(int index);

	// Returns a replacement string for a previously defined pattern/replacement string
	// pair. (This is a mail-merge feature.)
	bool GetReplaceString2(const wchar_t *pattern, CkString &outStr);
	// Returns a replacement string for a previously defined pattern/replacement string
	// pair. (This is a mail-merge feature.)
	const wchar_t *getReplaceString2(const wchar_t *pattern);
	// Returns a replacement string for a previously defined pattern/replacement string
	// pair. (This is a mail-merge feature.)
	const wchar_t *replaceString2(const wchar_t *pattern);

	// (See the NumReports property.) Returns the body content of the Nth report within
	// a multipart/report email.
	// 
	// Multipart/report is a message type that contains data formatted for a mail
	// server to read. It is split between a text/plain (or some other content/type
	// easily readable) and a message/delivery-status, which contains the data
	// formatted for the mail server to read.
	// 
	// It is defined in RFC 3462
	// 
	bool GetReport(int index, CkString &outStr);
	// (See the NumReports property.) Returns the body content of the Nth report within
	// a multipart/report email.
	// 
	// Multipart/report is a message type that contains data formatted for a mail
	// server to read. It is split between a text/plain (or some other content/type
	// easily readable) and a message/delivery-status, which contains the data
	// formatted for the mail server to read.
	// 
	// It is defined in RFC 3462
	// 
	const wchar_t *getReport(int index);
	// (See the NumReports property.) Returns the body content of the Nth report within
	// a multipart/report email.
	// 
	// Multipart/report is a message type that contains data formatted for a mail
	// server to read. It is split between a text/plain (or some other content/type
	// easily readable) and a message/delivery-status, which contains the data
	// formatted for the mail server to read.
	// 
	// It is defined in RFC 3462
	// 
	const wchar_t *report(int index);

	// Return the certificate used to digitally sign this email.
	// The caller is responsible for deleting the object returned by this method.
	CkCertW *GetSignedByCert(void);

	// Return the certificate that will be used to digitally sign this email. This is
	// the cerficate that was previously set by calling the SetSigningCert method.
	// The caller is responsible for deleting the object returned by this method.
	CkCertW *GetSigningCert(void);

	// Returns a "to" recipient's full email address.
	bool GetTo(int index, CkString &outStr);
	// Returns a "to" recipient's full email address.
	const wchar_t *getTo(int index);
	// Returns a "to" recipient's full email address.
	const wchar_t *to(int index);

	// Returns the Nth To address (only the address part, not the friendly-name part).
	bool GetToAddr(int index, CkString &outStr);
	// Returns the Nth To address (only the address part, not the friendly-name part).
	const wchar_t *getToAddr(int index);
	// Returns the Nth To address (only the address part, not the friendly-name part).
	const wchar_t *toAddr(int index);

	// Returns the Nth To name (only the friendly-name part, not the address part).
	bool GetToName(int index, CkString &outStr);
	// Returns the Nth To name (only the friendly-name part, not the address part).
	const wchar_t *getToName(int index);
	// Returns the Nth To name (only the friendly-name part, not the address part).
	const wchar_t *toName(int index);

	// Convert the email object to an XML document in memory
	bool GetXml(CkString &outStrXml);
	// Convert the email object to an XML document in memory
	const wchar_t *getXml(void);
	// Convert the email object to an XML document in memory
	const wchar_t *xml(void);

	// Returns true if the email has a header field with the specified fieldName with a
	// value matching  valuePattern. Case sensitivity is controlled by  caseSensitive. The  valuePattern may
	// include 0 or more asterisk (wildcard) characters which match 0 or more of any
	// character.
	bool HasHeaderMatching(const wchar_t *fieldName, const wchar_t *valuePattern, bool caseInsensitive);

	// Returns true if the email has an HTML body.
	bool HasHtmlBody(void);

	// Returns true if the email has a plain-text body.
	bool HasPlainTextBody(void);

	// Returns true if the email is a multipart/report email.
	bool IsMultipartReport(void);

	// Loads a complete email from a .EML file. (EML files are simply RFC822 MIME text
	// files.)
	bool LoadEml(const wchar_t *mimePath);

	// Loads an email with the contents of an XML email file.
	bool LoadXml(const wchar_t *xmlPath);

	// Loads an email from an XML string (previously obtained by calling the GetXml
	// method). The contents of the calling email object are erased and replaced with
	// the email contained within the XML string.
	bool LoadXmlString(const wchar_t *xmlStr);

	// Takes a byte array of multibyte (non-Unicode) data and returns a Unicode
	// Q-Encoded string.
	bool QEncodeBytes(const CkByteData &inData, const wchar_t *charset, CkString &outEncodedStr);
	// Takes a byte array of multibyte (non-Unicode) data and returns a Unicode
	// Q-Encoded string.
	const wchar_t *qEncodeBytes(const CkByteData &inData, const wchar_t *charset);

	// Takes a Unicode string, converts it to the charset specified in the 2nd
	// parameter, Q-Encodes the converted multibyte data, and returns the encoded
	// Unicode string.
	bool QEncodeString(const wchar_t *str, const wchar_t *charset, CkString &outEncodedStr);
	// Takes a Unicode string, converts it to the charset specified in the 2nd
	// parameter, Q-Encodes the converted multibyte data, and returns the encoded
	// Unicode string.
	const wchar_t *qEncodeString(const wchar_t *str, const wchar_t *charset);

	// Removes the Nth message/rfc822 sub-part of the email. Indexing begins at 0.
	void RemoveAttachedMessage(int idx);

	// Removes all message/rfc822 sub-parts of the email object.
	void RemoveAttachedMessages(void);

	// Removes path information from all attachment filenames.
	void RemoveAttachmentPaths(void);

	// Removes by name all occurances of a header field.
	void RemoveHeaderField(const wchar_t *fieldName);

	// Removes the HTML body from the email (if an HTML body exists).
	void RemoveHtmlAlternative(void);

	// Removes the plain-text body from the email (if a plain-text body exists).
	void RemovePlainTextAlternative(void);

	// Save all the attachments of an email to files in a directory specified by dirPath.
	// The OverwriteExisting property controls whether existing files are allowed to be
	// overwritten.
	bool SaveAllAttachments(const wchar_t *directory);

	// Saves the Nth email attachment to the directory specified by  dirPath. The 1st
	// attachment is at index 0. The OverwriteExisting property controls whether
	// existing files are allowed to be overwritten.
	bool SaveAttachedFile(int index, const wchar_t *directory);

	// Convert this email object to EML and save it to a file.
	bool SaveEml(const wchar_t *path);

	// Saves the Nth related item to the directory specified by  dirPath. (The 1st related
	// item is at index 0) Related content items are typically image or style-sheets
	// embedded within an HTML email. The OverwriteExisting property controls whether
	// existing files are allowed to be overwritten.
	bool SaveRelatedItem(int index, const wchar_t *directory);

	// Convert this email object to XML and save it to a file.
	bool SaveXml(const wchar_t *path);

	// Sets the charset attribute of the content-type header field for a specified
	// attachment. This can be used if the attachment is a text file that contains text
	// in a non us-ascii charset such as Shift_JIS, iso-8859-2, big5, iso-8859-5, etc.
	bool SetAttachmentCharset(int index, const wchar_t *charset);

	// Set's an attachment's disposition. The default disposition of an attachment is
	// "attachment". This method is typically called to change the disposition to
	// "inline". The 1st attachment is at ARG1 0.
	bool SetAttachmentDisposition(int index, const wchar_t *disposition);

	// Renames a email attachment's filename. The 1st attachment is at index 0.
	bool SetAttachmentFilename(int index, const wchar_t *path);

#if defined(CK_CSP_INCLUDED)
	// (Only applies to the Microsoft Windows OS) Sets the Cryptographic Service
	// Provider (CSP) to be used for encryption or digital signing.
	// 
	// This is not commonly used becaues the default Microsoft CSP is typically
	// appropriate. One instance where SetCSP is necessary is when using the Crypto-Pro
	// CSP for the GOST R 34.10-2001 and GOST R 34.10-94 providers.
	// 
	bool SetCSP(const CkCspW &csp);
#endif

	// Allows for a certificate and private key to be explicity specified for
	// decryptoin. When an email object is loaded via any method, such as LoadEml,
	// SetFromMimeText, SetFromMimeBytes, etc., security layers (signatures and
	// encryption) are automatically unwrapped. Decryption requires a private key. On
	// Windows-based systems, the private key is often pre-installed and nothing need
	// be done to provide it because Chilkat will automatically find it and use it.
	// However, if not on a Windows system, or if the private key was not
	// pre-installed, then it can be provided by this method, or via the
	// AddPfxSourceFile / AddPfxSourceData methods.
	bool SetDecryptCert2(const CkCertW &cert, CkPrivateKeyW &key);

	// Sets the "Date" header field of the email to have the value of the date/time
	// object provided.
	bool SetDt(CkDateTimeW &dt);

	// Set the encryption certificate to be used in encryption. Use the CreateCS,
	// CertStore, and Cert classes to create a Cert object by either locating a
	// certificate in a certificate store or loading one from a file.
	bool SetEncryptCert(const CkCertW &cert);

	// Loads the email object with the mimeBytes. If the email object already contained an
	// email, it is entirely replaced. The character encoding (such as "utf-8",
	// "iso-8859-1", etc.) of the bytes is automatically inferred from the content. If
	// for some reason it is not possible to determine the character encoding, the
	// SetFromMimeBytes2 method may be called to explicitly specify the charset.
	bool SetFromMimeBytes(const CkByteData &mimeBytes);

	// Loads the email object with the mimeBytes. If the email object already contained an
	// email, it is entirely replaced.
	// 
	// The  charset specifies the character encoding of the MIME bytes (such as "utf-8",
	// "iso-8859-1", etc.).
	// 
	bool SetFromMimeBytes2(const CkByteData &mimeBytes, const wchar_t *charset);

	// Loads an email with the contents of a .eml (i.e. MIME) contained in a string.
	// The contents of the email object are completely replaced.
	bool SetFromMimeText(const wchar_t *mimeText);

	// Loads an email from an XML string.
	bool SetFromXmlText(const wchar_t *xmlStr);

	// Sets the HTML body of an email.
	void SetHtmlBody(const wchar_t *html);

	// Sets the HTML email body from a byte array containing character data in the
	// specified character set. This method also updates the email "content-type"header
	// to properly reflect the content type of the body.
	bool SetMbHtmlBody(const wchar_t *charset, const CkByteData &inData);

	// Sets the plain-text email body from a byte array containing character data in
	// the specified character set. This method also updates the email
	// "content-type"header to properly reflect the content type of the body.
	bool SetMbPlainTextBody(const wchar_t *charset, const CkByteData &inData);

	// Sets the filename for a related item within the email.
	bool SetRelatedFilename(int index, const wchar_t *path);

	// Create a pattern/replacement-text pair for mail-merge. When the email is sent
	// via the MailMan's SendEmail method, or any other mail-sending method, the
	// patterns are replaced with the replacement strings during the sending process.
	// Do define multiple replacement patterns, simply call SetReplacePattern once per
	// pattern/replacement string. (Note: The MailMan's RenderToMime method will also
	// do pattern replacements. Methods such as SaveEml or GetMime do not replace
	// patterns.)
	// 
	// Note: Replacement patterns may be placed in any header field, and in both HTML
	// and plain-text email bodies.
	// 
	bool SetReplacePattern(const wchar_t *pattern, const wchar_t *replaceString);

	// Set the certificate to be used in creating a digital signature. Use the
	// CreateCS, CertStore, and Cert classes to create a Cert object by either locating
	// a certificate in a certificate store or loading one from a file.
	bool SetSigningCert(const CkCertW &cert);

	// Explicitly sets the certificate and private key to be used for sending digitally
	// signed email. If the certificate's private key is already installed on the
	// computer, then one may simply call SetSigningCert because the Chilkat component
	// will automatically locate and use the corresponding private key (stored in the
	// Windows Protected Store). In most cases, if the digital certificate is already
	// installed w/ private key on the computer, it is not necessary to explicitly set
	// the signing certificate at all. The Chilkat component will automatically locate
	// and use the certificate containing the FROM email address (from the
	// registry-based certificate store where it was installed).
	bool SetSigningCert2(const CkCertW &cert, CkPrivateKeyW &key);

	// Sets the body of the email and also sets the Content-Type header field of the
	//  contentType. If the email is already multipart/alternative, an additional alternative
	// with the indicated Content-Type will be added. If an alternative with the same
	// Content-Type already exists, it is replaced.
	void SetTextBody(const wchar_t *bodyText, const wchar_t *contentType);

	// True if the caller email has a UIDL that equals the email passed in the
	// argument.
	bool UidlEquals(const CkEmailW &e);

	// Unobfuscates emails by undoing what spammers do to obfuscate email. It removes
	// comments from HTML bodies and unobfuscates hyperlinked URLs.
	void UnSpamify(void);

	// Unpacks an HTML email into an HTML file and related files (images and style
	// sheets). The links within the HTML are updated to point to the files unpacked
	// and saved to disk.
	bool UnpackHtml(const wchar_t *unpackDir, const wchar_t *htmlFilename, const wchar_t *partsSubdir);

	// Unzips and replaces any Zip file attachments with the expanded contents. As an
	// example, if an email contained a single Zip file containing 3 GIF image files as
	// an attachment, then after calling this method the email would contain 3 GIF file
	// attachments, and the Zip attachment would be gone.If an email contains multiple
	// Zip file attachments, each Zip is expanded and replaced with the contents.
	bool UnzipAttachments(void);

	// Replaces all the attachments of an email with a single Zip file attachment
	// having the filename specified.
	bool ZipAttachments(const wchar_t *zipFilename);

	// Creates a typical email used to send EDIFACT messages. Does the following:
	//     Sets the email body to the EDIFACT message passed in ARG1.
	//     Sets the Content-Transfer-Encoding to Base64.
	//     Set the Content-Type equal to "application/EDIFACT".
	//     Sets the Content-Type header's name attribute to ARG2.
	//     Sets the Content-Disposition equal to "attachment".
	//     Sets the Content-Disposition's "filename" attribute equal to ARG3.
	//     The EDIFACT message is converted to the charset indicated by ARG4, and
	//     encoded using Base64 in the email body.
	// The email's subject, recipients, FROM address, and other headers are left
	// unmodified.
	void SetEdifactBody(const wchar_t *message, const wchar_t *name, const wchar_t *filename, const wchar_t *charset);

	// Adds an XML certificate vault to the object's internal list of sources to be
	// searched for certificates and private keys when encrypting/decrypting or
	// signing/verifying. Unlike the AddPfxSourceData and AddPfxSourceFile methods,
	// only a single XML certificate vault can be used. If UseCertVault is called
	// multiple times, only the last certificate vault will be used, as each call to
	// UseCertVault will replace the certificate vault provided in previous calls.
	bool UseCertVault(CkXmlCertVaultW &vault);

	// Return the full certificate chain of the certificate used to digitally sign this
	// email.
	// The caller is responsible for deleting the object returned by this method.
	CkCertChainW *GetSignedByCertChain(void);

	// Allows for a certificate to be explicity provided for decryption. When an email
	// object is loaded via any method, such as LoadEml, SetFromMimeText,
	// SetFromMimeBytes, etc., security layers (signatures and encryption) are
	// automatically unwrapped. This method could be called prior to calling a method
	// that loads the email.
	bool SetDecryptCert(CkCertW &cert);

	// Returns a header field attribute value for the Nth attached (embedded) email.
	// For example, to get the value of the "name" attribute in the Content-Type header
	// for the 1st attached message:
	// Content-Type: message/rfc822; name="md75000024149.eml"
	// then the method arguments should contain the values 0, "Content-Type", "name".
	bool GetAttachedMessageAttr(int index, const wchar_t *fieldName, const wchar_t *attrName, CkString &outStr);
	// Returns a header field attribute value for the Nth attached (embedded) email.
	// For example, to get the value of the "name" attribute in the Content-Type header
	// for the 1st attached message:
	// Content-Type: message/rfc822; name="md75000024149.eml"
	// then the method arguments should contain the values 0, "Content-Type", "name".
	const wchar_t *getAttachedMessageAttr(int index, const wchar_t *fieldName, const wchar_t *attrName);
	// Returns a header field attribute value for the Nth attached (embedded) email.
	// For example, to get the value of the "name" attribute in the Content-Type header
	// for the 1st attached message:
	// Content-Type: message/rfc822; name="md75000024149.eml"
	// then the method arguments should contain the values 0, "Content-Type", "name".
	const wchar_t *attachedMessageAttr(int index, const wchar_t *fieldName, const wchar_t *attrName);

	// Returns a header field attribute value from the header field of the Nth related
	// item.
	bool GetRelatedAttr(int index, const wchar_t *fieldName, const wchar_t *attrName, CkString &outStr);
	// Returns a header field attribute value from the header field of the Nth related
	// item.
	const wchar_t *getRelatedAttr(int index, const wchar_t *fieldName, const wchar_t *attrName);
	// Returns a header field attribute value from the header field of the Nth related
	// item.
	const wchar_t *relatedAttr(int index, const wchar_t *fieldName, const wchar_t *attrName);

	// Returns a header field attribute value from the header field of the Nth
	// attachment.
	bool GetAttachmentAttr(int index, const wchar_t *fieldName, const wchar_t *attrName, CkString &outStr);
	// Returns a header field attribute value from the header field of the Nth
	// attachment.
	const wchar_t *getAttachmentAttr(int index, const wchar_t *fieldName, const wchar_t *attrName);
	// Returns a header field attribute value from the header field of the Nth
	// attachment.
	const wchar_t *attachmentAttr(int index, const wchar_t *fieldName, const wchar_t *attrName);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
