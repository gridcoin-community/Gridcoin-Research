// CkCkByteData.h: interface for the CkCkByteData class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated.

#ifndef _CkCkByteData_H
#define _CkCkByteData_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkMultiByteBase.h"

class CkByteData;
class CkString;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkCkByteData
class CkCkByteData  : public CkMultiByteBase
{
    private:
	

	// Don't allow assignment or copying these objects.
	CkCkByteData(const CkCkByteData &);
	CkCkByteData &operator=(const CkCkByteData &);

    public:
	CkCkByteData(void);
	virtual ~CkCkByteData(void);

	static CkCkByteData *createNew(void);
	void inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	

	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------


	// ----------------------
	// Methods
	// ----------------------
	void append(const CkByteData &db);

	void append2(const void *pByteData, unsigned long szByteData);

	void appendChar(char ch);

	void appendCharN(char ch, int numTimes);

	void appendEncoded(const char *str, const char *encoding);

	void appendEncodedW(const wchar_t *str, const wchar_t *encoding);

	bool appendFile(const char *path);

	bool appendFileW(const wchar_t *path);

	void appendInt(int v, bool littleEndian);

	void appendRandom(int numBytes);

	void appendRange(const CkByteData &byteData, unsigned long index, unsigned long numBytes);

	void appendShort(short v, bool littleEndian);

	void appendStr(const char *str);

	void appendStrW(const wchar_t *str, const wchar_t *charset);

	bool beginsWith(const CkByteData &byteData);

	bool beginsWith2(const void *pByteData, unsigned long szByteData);

	void borrowData(void *pByteData, unsigned long szByteData);

	void byteSwap4321(void);

	void clear(void);

	void encode(const char *encoding, CkString &str);

	void encodeW(const wchar_t *encoding, CkString &str);

	void ensureBuffer(unsigned long numBytes);

	bool equals(const CkByteData &db);

	bool equals2(const void *pByteData, unsigned long szByteData);

	int findBytes(const CkByteData &byteData);

	int findBytes2(const void *pByteData, unsigned long szByteData);

	unsigned char getByte(unsigned long byteIndex);

	const unsigned char *getBytes(void);

	char getChar(unsigned long byteIndex);

	const unsigned char *getData(void);

	const unsigned char *getDataAt(unsigned long byteIndex);

	const wchar_t *getEncodedW(const wchar_t *encoding);

	int getInt(unsigned long byteIndex);

	const unsigned char *getRange(unsigned long byteIndex, unsigned long numBytes);

	short getShort(unsigned long byteIndex);

	unsigned long getSize(void);

	unsigned int getUInt(unsigned long byteIndex);

	unsigned short getUShort(unsigned long byteIndex);

	bool is7bit(void);

	bool loadFile(const char *path);

	bool loadFileW(const wchar_t *path);

	void pad(int blockSize, int paddingScheme);

	bool preAllocate(unsigned long expectedNumBytes);

	void removeChunk(unsigned long index, unsigned long numBytes);

	unsigned char *removeData(void);

	void replaceChar(unsigned char c, unsigned char replacement);

	bool saveFile(const char *path);

	bool saveFileW(const wchar_t *path);

	void shorten(unsigned long numBytes);

	const wchar_t *to_ws(const char *charset);

	void unpad(int blockSize, int paddingScheme);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
