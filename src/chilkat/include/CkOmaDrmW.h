// CkOmaDrmW.h: interface for the CkOmaDrmW class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkOmaDrmW_H
#define _CkOmaDrmW_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkWideCharBase.h"

class CkByteData;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkOmaDrmW
class CK_VISIBLE_PUBLIC CkOmaDrmW  : public CkWideCharBase
{
    private:
	

	// Don't allow assignment or copying these objects.
	CkOmaDrmW(const CkOmaDrmW &);
	CkOmaDrmW &operator=(const CkOmaDrmW &);

    public:
	CkOmaDrmW(void);
	virtual ~CkOmaDrmW(void);

	static CkOmaDrmW *createNew(void);
	

	
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	

	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------
	// The 16-byte secret key for encryption/decryption in Base64 encoded form.
	void get_Base64Key(CkString &str);
	// The 16-byte secret key for encryption/decryption in Base64 encoded form.
	const wchar_t *base64Key(void);
	// The 16-byte secret key for encryption/decryption in Base64 encoded form.
	void put_Base64Key(const wchar_t *newVal);

	// The MIME media type of the DRM protected content.
	void get_ContentType(CkString &str);
	// The MIME media type of the DRM protected content.
	const wchar_t *contentType(void);
	// The MIME media type of the DRM protected content.
	void put_ContentType(const wchar_t *newVal);

	// The unique URI for the DRM protected content.
	void get_ContentUri(CkString &str);
	// The unique URI for the DRM protected content.
	const wchar_t *contentUri(void);
	// The unique URI for the DRM protected content.
	void put_ContentUri(const wchar_t *newVal);

	// Decrypts and returns the DRM protected content. The secret key must be set
	// correctly beforehand. The the secret key is incorrect, gibberish is returned.
	void get_DecryptedData(CkByteData &outBytes);

	// The version of the DRM content format specification used to produce this content
	// object.
	int get_DrmContentVersion(void);

	// This property provides access to the encrypted data contained within the DRM
	// content.
	void get_EncryptedData(CkByteData &outBytes);

	// Header fields separated by CRLF line endings containing additional meta data
	// about the DRM content.
	void get_Headers(CkString &str);
	// Header fields separated by CRLF line endings containing additional meta data
	// about the DRM content.
	const wchar_t *headers(void);
	// Header fields separated by CRLF line endings containing additional meta data
	// about the DRM content.
	void put_Headers(const wchar_t *newVal);

	// The initialization vector to be used when encrypting the DRM protected content.
	void get_IV(CkByteData &outBytes);
	// The initialization vector to be used when encrypting the DRM protected content.
	void put_IV(const CkByteData &inBytes);



	// ----------------------
	// Methods
	// ----------------------
	// Writes a .dcf file using the property settings and data contained within the
	// calling object.
	bool CreateDcfFile(const wchar_t *filename);

	// Returns a header field value by field name.
	bool GetHeaderField(const wchar_t *fieldName, CkString &outVal);
	// Returns a header field value by field name.
	const wchar_t *getHeaderField(const wchar_t *fieldName);
	// Returns a header field value by field name.
	const wchar_t *headerField(const wchar_t *fieldName);

	// Loads a .dcf file from in-memory data.
	bool LoadDcfData(const CkByteData &data);

	// Loads a .dcf file into the calling object. Once loaded, the headers and data can
	// be accessed via the OMA DRM object's properties and methods.
	bool LoadDcfFile(const wchar_t *filename);

	// Loads data into the OMA DRM object. This is the data that will be written to the
	// .dcf file when the CreateDcfFile method is called
	void LoadUnencryptedData(const CkByteData &data);

	// Loads file data into the OMA DRM object. This is the data that will be written
	// to the .dcf file when the CreateDcfFile method is called
	bool LoadUnencryptedFile(const wchar_t *filename);

	// Decrypts the data contained within the OMA DRM object and saves the result to an
	// output file.
	bool SaveDecrypted(const wchar_t *filename);

	// Sets the initialization vector via an encoded string. The encoding may be
	// "base64", "hex", "url", or "quoted-printable".
	void SetEncodedIV(const wchar_t *encodedIv, const wchar_t *encoding);

	// Unlocks the OMA DRM component at runtime. This must be called once at the
	// beginning of your application. Passing an arbitrary value initiates a
	// fully-functional 30-day trial. A permanent unlock code is required to use the
	// component beyond 30 days. Because the OMA DRM component is included with the
	// Chilkat Crypt license, any valid Chilkat Crypt unlock code will also unlock this
	// component.
	bool UnlockComponent(const wchar_t *b1);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
