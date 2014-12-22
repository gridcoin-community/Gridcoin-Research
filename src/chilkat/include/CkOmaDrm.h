// CkOmaDrm.h: interface for the CkOmaDrm class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkOmaDrm_H
#define _CkOmaDrm_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkMultiByteBase.h"

class CkByteData;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkOmaDrm
class CK_VISIBLE_PUBLIC CkOmaDrm  : public CkMultiByteBase
{
    private:
	

	// Don't allow assignment or copying these objects.
	CkOmaDrm(const CkOmaDrm &);
	CkOmaDrm &operator=(const CkOmaDrm &);

    public:
	CkOmaDrm(void);
	virtual ~CkOmaDrm(void);

	static CkOmaDrm *createNew(void);
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
	const char *base64Key(void);
	// The 16-byte secret key for encryption/decryption in Base64 encoded form.
	void put_Base64Key(const char *newVal);

	// The MIME media type of the DRM protected content.
	void get_ContentType(CkString &str);
	// The MIME media type of the DRM protected content.
	const char *contentType(void);
	// The MIME media type of the DRM protected content.
	void put_ContentType(const char *newVal);

	// The unique URI for the DRM protected content.
	void get_ContentUri(CkString &str);
	// The unique URI for the DRM protected content.
	const char *contentUri(void);
	// The unique URI for the DRM protected content.
	void put_ContentUri(const char *newVal);

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
	const char *headers(void);
	// Header fields separated by CRLF line endings containing additional meta data
	// about the DRM content.
	void put_Headers(const char *newVal);

	// The initialization vector to be used when encrypting the DRM protected content.
	void get_IV(CkByteData &outBytes);
	// The initialization vector to be used when encrypting the DRM protected content.
	void put_IV(const CkByteData &inBytes);



	// ----------------------
	// Methods
	// ----------------------
	// Writes a .dcf file using the property settings and data contained within the
	// calling object.
	bool CreateDcfFile(const char *filename);

	// Returns a header field value by field name.
	bool GetHeaderField(const char *fieldName, CkString &outVal);
	// Returns a header field value by field name.
	const char *getHeaderField(const char *fieldName);
	// Returns a header field value by field name.
	const char *headerField(const char *fieldName);

	// Loads a .dcf file from in-memory data.
	bool LoadDcfData(const CkByteData &data);

	// Loads a .dcf file into the calling object. Once loaded, the headers and data can
	// be accessed via the OMA DRM object's properties and methods.
	bool LoadDcfFile(const char *filename);

	// Loads data into the OMA DRM object. This is the data that will be written to the
	// .dcf file when the CreateDcfFile method is called
	void LoadUnencryptedData(const CkByteData &data);

	// Loads file data into the OMA DRM object. This is the data that will be written
	// to the .dcf file when the CreateDcfFile method is called
	bool LoadUnencryptedFile(const char *filename);

	// Decrypts the data contained within the OMA DRM object and saves the result to an
	// output file.
	bool SaveDecrypted(const char *filename);

	// Sets the initialization vector via an encoded string. The encoding may be
	// "base64", "hex", "url", or "quoted-printable".
	void SetEncodedIV(const char *encodedIv, const char *encoding);

	// Unlocks the OMA DRM component at runtime. This must be called once at the
	// beginning of your application. Passing an arbitrary value initiates a
	// fully-functional 30-day trial. A permanent unlock code is required to use the
	// component beyond 30 days. Because the OMA DRM component is included with the
	// Chilkat Crypt license, any valid Chilkat Crypt unlock code will also unlock this
	// component.
	bool UnlockComponent(const char *b1);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
