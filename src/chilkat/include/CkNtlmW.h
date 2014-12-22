// CkNtlmW.h: interface for the CkNtlmW class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkNtlmW_H
#define _CkNtlmW_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkWideCharBase.h"




#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkNtlmW
class CK_VISIBLE_PUBLIC CkNtlmW  : public CkWideCharBase
{
    private:
	

	// Don't allow assignment or copying these objects.
	CkNtlmW(const CkNtlmW &);
	CkNtlmW &operator=(const CkNtlmW &);

    public:
	CkNtlmW(void);
	virtual ~CkNtlmW(void);

	static CkNtlmW *createNew(void);
	

	
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	

	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------
	// The ClientChallenge is passed in the Type 3 message from the client to the
	// server. It must contain exactly 8 bytes. Because this is a string property, the
	// bytes are get/set in encoded form (such as hex or base64) based on the value of
	// the EncodingMode property. For example, if the EncodingMode property = "hex",
	// then a hex representation of 8 bytes should be used to set the ClientChallenge.
	// 
	// Note: Setting the ClientChallenge is optional. If the ClientChallenge remains
	// unset, it will be automatically set to 8 random bytes when the GenType3 method
	// is called.
	// 
	void get_ClientChallenge(CkString &str);
	// The ClientChallenge is passed in the Type 3 message from the client to the
	// server. It must contain exactly 8 bytes. Because this is a string property, the
	// bytes are get/set in encoded form (such as hex or base64) based on the value of
	// the EncodingMode property. For example, if the EncodingMode property = "hex",
	// then a hex representation of 8 bytes should be used to set the ClientChallenge.
	// 
	// Note: Setting the ClientChallenge is optional. If the ClientChallenge remains
	// unset, it will be automatically set to 8 random bytes when the GenType3 method
	// is called.
	// 
	const wchar_t *clientChallenge(void);
	// The ClientChallenge is passed in the Type 3 message from the client to the
	// server. It must contain exactly 8 bytes. Because this is a string property, the
	// bytes are get/set in encoded form (such as hex or base64) based on the value of
	// the EncodingMode property. For example, if the EncodingMode property = "hex",
	// then a hex representation of 8 bytes should be used to set the ClientChallenge.
	// 
	// Note: Setting the ClientChallenge is optional. If the ClientChallenge remains
	// unset, it will be automatically set to 8 random bytes when the GenType3 method
	// is called.
	// 
	void put_ClientChallenge(const wchar_t *newVal);

	// Optional. This is information that would be set by the server for inclusion in
	// the "Target Info" internal portion of the Type 2 message. Note: If any optional
	// "Target Info" fields are provided, then both NetBiosComputerName and
	// NetBiosDomainName must be provided.
	void get_DnsComputerName(CkString &str);
	// Optional. This is information that would be set by the server for inclusion in
	// the "Target Info" internal portion of the Type 2 message. Note: If any optional
	// "Target Info" fields are provided, then both NetBiosComputerName and
	// NetBiosDomainName must be provided.
	const wchar_t *dnsComputerName(void);
	// Optional. This is information that would be set by the server for inclusion in
	// the "Target Info" internal portion of the Type 2 message. Note: If any optional
	// "Target Info" fields are provided, then both NetBiosComputerName and
	// NetBiosDomainName must be provided.
	void put_DnsComputerName(const wchar_t *newVal);

	// Optional. This is information that would be set by the server for inclusion in
	// the "Target Info" internal portion of the Type 2 message. Note: If any optional
	// "Target Info" fields are provided, then both NetBiosComputerName and
	// NetBiosDomainName must be provided.
	void get_DnsDomainName(CkString &str);
	// Optional. This is information that would be set by the server for inclusion in
	// the "Target Info" internal portion of the Type 2 message. Note: If any optional
	// "Target Info" fields are provided, then both NetBiosComputerName and
	// NetBiosDomainName must be provided.
	const wchar_t *dnsDomainName(void);
	// Optional. This is information that would be set by the server for inclusion in
	// the "Target Info" internal portion of the Type 2 message. Note: If any optional
	// "Target Info" fields are provided, then both NetBiosComputerName and
	// NetBiosDomainName must be provided.
	void put_DnsDomainName(const wchar_t *newVal);

	// Optional. May be set by the client for inclusion in the Type 1 message.
	void get_Domain(CkString &str);
	// Optional. May be set by the client for inclusion in the Type 1 message.
	const wchar_t *domain(void);
	// Optional. May be set by the client for inclusion in the Type 1 message.
	void put_Domain(const wchar_t *newVal);

	// Determines the encoding mode used for getting/setting various properties, such
	// as ClientChallenge. The valid case-insensitive modes are "Base64", "modBase64",
	// "Base32", "UU", "QP" (for quoted-printable), "URL" (for url-encoding), "Hex",
	// "Q", "B", "url_oath", "url_rfc1738", "url_rfc2396", and "url_rfc3986".
	void get_EncodingMode(CkString &str);
	// Determines the encoding mode used for getting/setting various properties, such
	// as ClientChallenge. The valid case-insensitive modes are "Base64", "modBase64",
	// "Base32", "UU", "QP" (for quoted-printable), "URL" (for url-encoding), "Hex",
	// "Q", "B", "url_oath", "url_rfc1738", "url_rfc2396", and "url_rfc3986".
	const wchar_t *encodingMode(void);
	// Determines the encoding mode used for getting/setting various properties, such
	// as ClientChallenge. The valid case-insensitive modes are "Base64", "modBase64",
	// "Base32", "UU", "QP" (for quoted-printable), "URL" (for url-encoding), "Hex",
	// "Q", "B", "url_oath", "url_rfc1738", "url_rfc2396", and "url_rfc3986".
	void put_EncodingMode(const wchar_t *newVal);

	// The negotiate flags that are set in the Type 1 message generated by the client
	// and sent to the server. These flags have a default value and should ONLY be set
	// by a programmer that is an expert in the NTLM protocol and knows what they mean.
	// In general, this property should be left at it's default value.
	// 
	// The flags are represented as a string of letters, where each letter represents a
	// bit. The full set of possible flags (bit values) are shown below:
	// NegotiateUnicode               0x00000001
	// NegotiateOEM                   0x00000002
	// RequestTarget                  0x00000004
	// NegotiateSign                  0x00000010
	// NegotiateSeal                  0x00000020
	// NegotiateDatagramStyle         0x00000040
	// NegotiateLanManagerKey         0x00000080
	// NegotiateNetware               0x00000100
	// NegotiateNTLMKey               0x00000200
	// NegotiateDomainSupplied        0x00001000
	// NegotiateWorkstationSupplied   0x00002000
	// NegotiateLocalCall             0x00004000
	// NegotiateAlwaysSign            0x00008000
	// TargetTypeDomain               0x00010000
	// TargetTypeServer               0x00020000
	// TargetTypeShare                0x00040000
	// NegotiateNTLM2Key              0x00080000
	// RequestInitResponse            0x00100000
	// RequestAcceptResponse          0x00200000
	// RequestNonNTSessionKey         0x00400000
	// NegotiateTargetInfo            0x00800000
	// Negotiate128                   0x20000000
	// NegotiateKeyExchange           0x40000000
	// Negotiate56                    0x80000000
	// The mapping of letters to bit values are as follows:
	// 0x01 - "A"
	// 0x02 - "B"
	// 0x04 - "C"
	// 0x10 - "D"
	// 0x20 - "E"
	// 0x40 - "F"
	// 0x80 - "G"
	// 0x200 - "H"
	// 0x400 - "I"
	// 0x800 - "J"
	// 0x1000 - "K"
	// 0x2000 - "L"
	// 0x8000 - "M"
	// 0x10000 - "N"
	// 0x20000 - "O"
	// 0x40000 - "P"
	// 0x80000 - "Q"
	// 0x100000 - "R"
	// 0x400000 - "S"
	// 0x800000 - "T"
	// 0x2000000 - "U"
	// 0x20000000 - "V"
	// 0x40000000 - "W"
	// 0x80000000 - "X"
	// The default Flags value has the following flags set: NegotiateUnicode,
	// NegotiateOEM, RequestTarget, NegotiateNTLMKey, NegotiateAlwaysSign,
	// NegotiateNTLM2Key. The corresponds to the string "ABCHMQ".
	// 
	void get_Flags(CkString &str);
	// The negotiate flags that are set in the Type 1 message generated by the client
	// and sent to the server. These flags have a default value and should ONLY be set
	// by a programmer that is an expert in the NTLM protocol and knows what they mean.
	// In general, this property should be left at it's default value.
	// 
	// The flags are represented as a string of letters, where each letter represents a
	// bit. The full set of possible flags (bit values) are shown below:
	// NegotiateUnicode               0x00000001
	// NegotiateOEM                   0x00000002
	// RequestTarget                  0x00000004
	// NegotiateSign                  0x00000010
	// NegotiateSeal                  0x00000020
	// NegotiateDatagramStyle         0x00000040
	// NegotiateLanManagerKey         0x00000080
	// NegotiateNetware               0x00000100
	// NegotiateNTLMKey               0x00000200
	// NegotiateDomainSupplied        0x00001000
	// NegotiateWorkstationSupplied   0x00002000
	// NegotiateLocalCall             0x00004000
	// NegotiateAlwaysSign            0x00008000
	// TargetTypeDomain               0x00010000
	// TargetTypeServer               0x00020000
	// TargetTypeShare                0x00040000
	// NegotiateNTLM2Key              0x00080000
	// RequestInitResponse            0x00100000
	// RequestAcceptResponse          0x00200000
	// RequestNonNTSessionKey         0x00400000
	// NegotiateTargetInfo            0x00800000
	// Negotiate128                   0x20000000
	// NegotiateKeyExchange           0x40000000
	// Negotiate56                    0x80000000
	// The mapping of letters to bit values are as follows:
	// 0x01 - "A"
	// 0x02 - "B"
	// 0x04 - "C"
	// 0x10 - "D"
	// 0x20 - "E"
	// 0x40 - "F"
	// 0x80 - "G"
	// 0x200 - "H"
	// 0x400 - "I"
	// 0x800 - "J"
	// 0x1000 - "K"
	// 0x2000 - "L"
	// 0x8000 - "M"
	// 0x10000 - "N"
	// 0x20000 - "O"
	// 0x40000 - "P"
	// 0x80000 - "Q"
	// 0x100000 - "R"
	// 0x400000 - "S"
	// 0x800000 - "T"
	// 0x2000000 - "U"
	// 0x20000000 - "V"
	// 0x40000000 - "W"
	// 0x80000000 - "X"
	// The default Flags value has the following flags set: NegotiateUnicode,
	// NegotiateOEM, RequestTarget, NegotiateNTLMKey, NegotiateAlwaysSign,
	// NegotiateNTLM2Key. The corresponds to the string "ABCHMQ".
	// 
	const wchar_t *flags(void);
	// The negotiate flags that are set in the Type 1 message generated by the client
	// and sent to the server. These flags have a default value and should ONLY be set
	// by a programmer that is an expert in the NTLM protocol and knows what they mean.
	// In general, this property should be left at it's default value.
	// 
	// The flags are represented as a string of letters, where each letter represents a
	// bit. The full set of possible flags (bit values) are shown below:
	// NegotiateUnicode               0x00000001
	// NegotiateOEM                   0x00000002
	// RequestTarget                  0x00000004
	// NegotiateSign                  0x00000010
	// NegotiateSeal                  0x00000020
	// NegotiateDatagramStyle         0x00000040
	// NegotiateLanManagerKey         0x00000080
	// NegotiateNetware               0x00000100
	// NegotiateNTLMKey               0x00000200
	// NegotiateDomainSupplied        0x00001000
	// NegotiateWorkstationSupplied   0x00002000
	// NegotiateLocalCall             0x00004000
	// NegotiateAlwaysSign            0x00008000
	// TargetTypeDomain               0x00010000
	// TargetTypeServer               0x00020000
	// TargetTypeShare                0x00040000
	// NegotiateNTLM2Key              0x00080000
	// RequestInitResponse            0x00100000
	// RequestAcceptResponse          0x00200000
	// RequestNonNTSessionKey         0x00400000
	// NegotiateTargetInfo            0x00800000
	// Negotiate128                   0x20000000
	// NegotiateKeyExchange           0x40000000
	// Negotiate56                    0x80000000
	// The mapping of letters to bit values are as follows:
	// 0x01 - "A"
	// 0x02 - "B"
	// 0x04 - "C"
	// 0x10 - "D"
	// 0x20 - "E"
	// 0x40 - "F"
	// 0x80 - "G"
	// 0x200 - "H"
	// 0x400 - "I"
	// 0x800 - "J"
	// 0x1000 - "K"
	// 0x2000 - "L"
	// 0x8000 - "M"
	// 0x10000 - "N"
	// 0x20000 - "O"
	// 0x40000 - "P"
	// 0x80000 - "Q"
	// 0x100000 - "R"
	// 0x400000 - "S"
	// 0x800000 - "T"
	// 0x2000000 - "U"
	// 0x20000000 - "V"
	// 0x40000000 - "W"
	// 0x80000000 - "X"
	// The default Flags value has the following flags set: NegotiateUnicode,
	// NegotiateOEM, RequestTarget, NegotiateNTLMKey, NegotiateAlwaysSign,
	// NegotiateNTLM2Key. The corresponds to the string "ABCHMQ".
	// 
	void put_Flags(const wchar_t *newVal);

	// Optional. This is information that would be set by the server for inclusion in
	// the "Target Info" internal portion of the Type 2 message. Note: If any optional
	// "Target Info" fields are provided, then both NetBiosComputerName and
	// NetBiosDomainName must be provided.
	void get_NetBiosComputerName(CkString &str);
	// Optional. This is information that would be set by the server for inclusion in
	// the "Target Info" internal portion of the Type 2 message. Note: If any optional
	// "Target Info" fields are provided, then both NetBiosComputerName and
	// NetBiosDomainName must be provided.
	const wchar_t *netBiosComputerName(void);
	// Optional. This is information that would be set by the server for inclusion in
	// the "Target Info" internal portion of the Type 2 message. Note: If any optional
	// "Target Info" fields are provided, then both NetBiosComputerName and
	// NetBiosDomainName must be provided.
	void put_NetBiosComputerName(const wchar_t *newVal);

	// Optional. This is information that would be set by the server for inclusion in
	// the "Target Info" internal portion of the Type 2 message. Note: If any optional
	// "Target Info" fields are provided, then both NetBiosComputerName and
	// NetBiosDomainName must be provided.
	void get_NetBiosDomainName(CkString &str);
	// Optional. This is information that would be set by the server for inclusion in
	// the "Target Info" internal portion of the Type 2 message. Note: If any optional
	// "Target Info" fields are provided, then both NetBiosComputerName and
	// NetBiosDomainName must be provided.
	const wchar_t *netBiosDomainName(void);
	// Optional. This is information that would be set by the server for inclusion in
	// the "Target Info" internal portion of the Type 2 message. Note: If any optional
	// "Target Info" fields are provided, then both NetBiosComputerName and
	// NetBiosDomainName must be provided.
	void put_NetBiosDomainName(const wchar_t *newVal);

	// The version of the NTLM protocol to be used. Must be set to either 1 or 2. The
	// default value is 1 (for NTLMv1). Setting this property equal to 2 selects
	// NTLMv2.
	int get_NtlmVersion(void);
	// The version of the NTLM protocol to be used. Must be set to either 1 or 2. The
	// default value is 1 (for NTLMv1). Setting this property equal to 2 selects
	// NTLMv2.
	void put_NtlmVersion(int newVal);

	// If the "A" flag is unset, then Unicode strings are not used internally in the
	// NTLM messages. Strings are instead represented using the OEM code page (i.e.
	// charset, or character encoding) as specified here. In general, given that the
	// Flags property should rarely be modified, and given that the "A" flag is set by
	// default (meaning that Unicode is used), the OemCodePage property will not apply.
	// The default value is the default OEM code page of the local computer.
	int get_OemCodePage(void);
	// If the "A" flag is unset, then Unicode strings are not used internally in the
	// NTLM messages. Strings are instead represented using the OEM code page (i.e.
	// charset, or character encoding) as specified here. In general, given that the
	// Flags property should rarely be modified, and given that the "A" flag is set by
	// default (meaning that Unicode is used), the OemCodePage property will not apply.
	// The default value is the default OEM code page of the local computer.
	void put_OemCodePage(int newVal);

	// The password corresponding to the username of the account to be authenticated.
	// This must be set by the client prior to generating and sending the Type 3
	// message.
	void get_Password(CkString &str);
	// The password corresponding to the username of the account to be authenticated.
	// This must be set by the client prior to generating and sending the Type 3
	// message.
	const wchar_t *password(void);
	// The password corresponding to the username of the account to be authenticated.
	// This must be set by the client prior to generating and sending the Type 3
	// message.
	void put_Password(const wchar_t *newVal);

	// This is similar to the ClientChallenge in that it must contain 8 bytes.
	// 
	// The ServerChallenge is passed in the Type 2 message from the server to the
	// client. Because this is a string property, the bytes are get/set in encoded form
	// (such as hex or base64) based on the value of the EncodingMode property. For
	// example, if the EncodingMode property = "hex", then a hex representation of 8
	// bytes should be used to set the ServerChallenge.
	// 
	// Note: Setting the ServerChallenge is optional. If the ServerChallenge remains
	// unset, it will be automatically set to 8 random bytes when the GenType2 method
	// is called.
	// 
	void get_ServerChallenge(CkString &str);
	// This is similar to the ClientChallenge in that it must contain 8 bytes.
	// 
	// The ServerChallenge is passed in the Type 2 message from the server to the
	// client. Because this is a string property, the bytes are get/set in encoded form
	// (such as hex or base64) based on the value of the EncodingMode property. For
	// example, if the EncodingMode property = "hex", then a hex representation of 8
	// bytes should be used to set the ServerChallenge.
	// 
	// Note: Setting the ServerChallenge is optional. If the ServerChallenge remains
	// unset, it will be automatically set to 8 random bytes when the GenType2 method
	// is called.
	// 
	const wchar_t *serverChallenge(void);
	// This is similar to the ClientChallenge in that it must contain 8 bytes.
	// 
	// The ServerChallenge is passed in the Type 2 message from the server to the
	// client. Because this is a string property, the bytes are get/set in encoded form
	// (such as hex or base64) based on the value of the EncodingMode property. For
	// example, if the EncodingMode property = "hex", then a hex representation of 8
	// bytes should be used to set the ServerChallenge.
	// 
	// Note: Setting the ServerChallenge is optional. If the ServerChallenge remains
	// unset, it will be automatically set to 8 random bytes when the GenType2 method
	// is called.
	// 
	void put_ServerChallenge(const wchar_t *newVal);

	// The authentication realm in which the authenticating account has membership,
	// such as a domain for domain accounts, or a server name for local machine
	// accounts. The TargetName is used in the Type2 message sent from the server to
	// the client.
	void get_TargetName(CkString &str);
	// The authentication realm in which the authenticating account has membership,
	// such as a domain for domain accounts, or a server name for local machine
	// accounts. The TargetName is used in the Type2 message sent from the server to
	// the client.
	const wchar_t *targetName(void);
	// The authentication realm in which the authenticating account has membership,
	// such as a domain for domain accounts, or a server name for local machine
	// accounts. The TargetName is used in the Type2 message sent from the server to
	// the client.
	void put_TargetName(const wchar_t *newVal);

	// The username of the account to be authenticated. This must be set by the client
	// prior to generating and sending the Type 3 message.
	void get_UserName(CkString &str);
	// The username of the account to be authenticated. This must be set by the client
	// prior to generating and sending the Type 3 message.
	const wchar_t *userName(void);
	// The username of the account to be authenticated. This must be set by the client
	// prior to generating and sending the Type 3 message.
	void put_UserName(const wchar_t *newVal);

	// The value to be used in the optional workstation field in Type 1 message.
	void get_Workstation(CkString &str);
	// The value to be used in the optional workstation field in Type 1 message.
	const wchar_t *workstation(void);
	// The value to be used in the optional workstation field in Type 1 message.
	void put_Workstation(const wchar_t *newVal);



	// ----------------------
	// Methods
	// ----------------------
	// Compares the internal contents of two Type3 messages to verify that the LM and
	// NTLM response parts match. A server would typically compute the Type3 message by
	// calling GenType3, and then compare it with the Type3 message received from the
	// client. The method returns true if the responses match, and false if they do
	// not.
	bool CompareType3(const wchar_t *msg1, const wchar_t *msg2);

	// Generates the Type 1 message. The Type 1 message is sent from Client to Server
	// and initiates the NTLM authentication exchange.
	bool GenType1(CkString &outStr);
	// Generates the Type 1 message. The Type 1 message is sent from Client to Server
	// and initiates the NTLM authentication exchange.
	const wchar_t *genType1(void);

	// Generates a Type2 message from a received Type1 message. The server-side
	// generates the Type2 message and sends it to the client. This is the 2nd step in
	// the NTLM protocol. The 1st step is the client generating the initial Type1
	// message which is sent to the server.
	bool GenType2(const wchar_t *type1Msg, CkString &outStr);
	// Generates a Type2 message from a received Type1 message. The server-side
	// generates the Type2 message and sends it to the client. This is the 2nd step in
	// the NTLM protocol. The 1st step is the client generating the initial Type1
	// message which is sent to the server.
	const wchar_t *genType2(const wchar_t *type1Msg);

	// Generates the final message in the NTLM authentication exchange. This message is
	// sent from the client to the server. The Type 2 message received from the server
	// is passed to GenType3. The Username and Password properties are finally used
	// here in the generation of the Type 3 message. Note, the Password is never
	// actually sent. It is used to compute a binary response that the server can then
	// check, using the password it has on file, to verify that indeed the client
	// must've used the correct password.
	bool GenType3(const wchar_t *type2Msg, CkString &outStr);
	// Generates the final message in the NTLM authentication exchange. This message is
	// sent from the client to the server. The Type 2 message received from the server
	// is passed to GenType3. The Username and Password properties are finally used
	// here in the generation of the Type 3 message. Note, the Password is never
	// actually sent. It is used to compute a binary response that the server can then
	// check, using the password it has on file, to verify that indeed the client
	// must've used the correct password.
	const wchar_t *genType3(const wchar_t *type2Msg);

	// The server-side should call this method with the Type 3 message received from
	// the client. The LoadType3 method sets the following properties: Username,
	// Domain, Workstation, and ClientChallenge, all of which are embedded within the
	// Type 3 message.
	// 
	// The server-side code may then use the Username to lookup the associated password
	// and then it will itself call the GenType3 method to do the same computation as
	// done by the client. The server then compares it's computed Type 3 message with
	// the Type 3 message received from the client. If the Type 3 messages are exactly
	// the same, then it must be that the client used the correct password, and
	// therefore the client authentication is successful.
	// 
	bool LoadType3(const wchar_t *type3Msg);

	// For informational purposes only. Allows for the server-side to parse a Type 1
	// message to get human-readable information about the contents.
	bool ParseType1(const wchar_t *type1Msg, CkString &outStr);
	// For informational purposes only. Allows for the server-side to parse a Type 1
	// message to get human-readable information about the contents.
	const wchar_t *parseType1(const wchar_t *type1Msg);

	// For informational purposes only. Allows for the client-side to parse a Type 2
	// message to get human-readable information about the contents.
	bool ParseType2(const wchar_t *type2Msg, CkString &outStr);
	// For informational purposes only. Allows for the client-side to parse a Type 2
	// message to get human-readable information about the contents.
	const wchar_t *parseType2(const wchar_t *type2Msg);

	// For informational purposes only. Allows for the server-side to parse a Type 3
	// message to get human-readable information about the contents.
	bool ParseType3(const wchar_t *type3Msg, CkString &outStr);
	// For informational purposes only. Allows for the server-side to parse a Type 3
	// message to get human-readable information about the contents.
	const wchar_t *parseType3(const wchar_t *type3Msg);

	// Sets one of the negotiate flags to be used in the Type 1 message sent by the
	// client. It should normally be unnecessary to modify the default flag settings.
	// For more information about flags, see the description for the Flags property
	// above.
	bool SetFlag(const wchar_t *flagLetter, bool onOrOff);

	// To be documented soon.
	bool UnlockComponent(const wchar_t *unlockCode);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
