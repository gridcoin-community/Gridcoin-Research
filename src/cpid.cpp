/* 

CPID HASHING ALGORITHM - GRIDCOIN - ROB HALFORD - 10-21-2014

*/
 
/* interface header */
/* system implementation headers */


/*



#include "cpid.h"

#include <cstdio>

// Constants for CPID Transform routine.
#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21
 
///////////////////////////////////////////////
 
// F, G, H and I are basic CPID functions.
inline CPID::uint4 CPID::F(uint4 x, uint4 y, uint4 z) {
  return x&y | ~x&z;
}
 
inline CPID::uint4 CPID::G(uint4 x, uint4 y, uint4 z) {
  return x&z | y&~z;
}
 
inline CPID::uint4 CPID::H(uint4 x, uint4 y, uint4 z) {
  return x^y^z;
}
 
inline CPID::uint4 CPID::I(uint4 x, uint4 y, uint4 z) {
  return y ^ (x | ~z);
}
 
// rotate_left rotates x left n bits.
inline CPID::uint4 CPID::rotate_left(uint4 x, int n) 
{
  return (x << n) | (x >> (32-n));
}
 
inline CPID::uint4 CPID::rotate_right(uint4 x, int n)
{
   // In n>>d, first d bits are 0. To put last 3 bits of at first, do bitwise or of n>>d with n <<(INT_BITS - d) 
   return (x >> n)|(x << (32 - n));
}



// rotate_left rotates x left n bits.
inline CPID::uint4 CPID::rotate_left8(int x, int n) 
{
  return (x << n) | (x >> (8-n));
}
 
inline CPID::uint4 CPID::rotate_right8(int x, int n)
{
   // In n>>d, first d bits are 0. To put last 3 bits of at first, do bitwise or of n>>d with n <<(INT_BITS - d) 
   return (x >> n)|(x << (8 - n));
}


// FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.
// Rotation is separate from addition to prevent recomputation.
inline void CPID::FF(uint4 &a, uint4 b, uint4 c, uint4 d, uint4 x, uint4 s, uint4 ac) 
{
  a = rotate_left(a+ F(b,c,d) + x + ac, s) + b;
}
 
inline void CPID::GG(uint4 &a, uint4 b, uint4 c, uint4 d, uint4 x, uint4 s, uint4 ac) {
  a = rotate_left(a + G(b,c,d) + x + ac, s) + b;
}
 
inline void CPID::HH(uint4 &a, uint4 b, uint4 c, uint4 d, uint4 x, uint4 s, uint4 ac) {
  a = rotate_left(a + H(b,c,d) + x + ac, s) + b;
}
 
inline void CPID::II(uint4 &a, uint4 b, uint4 c, uint4 d, uint4 x, uint4 s, uint4 ac) {
  a = rotate_left(a + I(b,c,d) + x + ac, s) + b;
}
 
//////////////////////////////////////////////
 
// default ctor, just initailize
CPID::CPID()
{
  init();
}
 
//////////////////////////////////////////////
 
// constructor to compute CPID for string and finalize it right away
CPID::CPID(const std::string &text)
{
  init();
  update(text.c_str(), text.length());
  finalize();
}

CPID::CPID(const std::string &text,int entropybit,uint256 blockhash)
{
  init();
  entropybit++;
  update5(text, blockhash);
  finalize();
}



CPID::CPID(std::string email, std::string bpk, uint256 block_hash)
{
	init();
	boost::algorithm::to_lower(bpk);
	boost::algorithm::to_lower(email);
	boinc_public_key = bpk;
	email_hash = email;
	blockhash = block_hash;
	std::string cpid_non = bpk+email;
	merged_hash = cpid_non;
	update(cpid_non.c_str(), cpid_non.length());
    finalize();
}
 
//////////////////////////////
 
void CPID::init()
{
  finalized=false;
  
  count[0] = 0;
  count[1] = 0;
 
  // load magic initialization constants.
  state[0] = 0x67452301;
  state[1] = 0xefcdab89;
  state[2] = 0x98badcfe;
  state[3] = 0x10325476;
}
 
//////////////////////////////
 
// decodes input (unsigned char) into output (uint4). Assumes len is a multiple of 4.
void CPID::decode(uint4 output[], const uint1 input[], size_type len)
{
  for (unsigned int i = 0, j = 0; j < len; i++, j += 4)
    output[i] = ((uint4)input[j]) | (((uint4)input[j+1]) << 8) |
      (((uint4)input[j+2]) << 16) | (((uint4)input[j+3]) << 24);
}
 
//////////////////////////////
 
// encodes input (uint4) into output (unsigned char). Assumes len is
// a multiple of 4.
void CPID::encode(uint1 output[], const uint4 input[], size_type len)
{
  for (size_type i = 0, j = 0; j < len; i++, j += 4) {
    output[j] = input[i] & 0xff;
    output[j+1] = (input[i] >> 8) & 0xff;
    output[j+2] = (input[i] >> 16) & 0xff;
    output[j+3] = (input[i] >> 24) & 0xff;
  }
}
 
//////////////////////////////
 
// apply CPID algo on a block
void CPID::transform(const uint1 block[blocksize])
{
  uint4 a = state[0], b = state[1], c = state[2], d = state[3], x[16];
  decode (x, block, blocksize);
 
  // Round 1 
  FF (a, b, c, d, x[ 0], S11, 0xd76aa478); // 1 
  FF (d, a, b, c, x[ 1], S12, 0xe8c7b756); // 2 
  FF (c, d, a, b, x[ 2], S13, 0x242070db); // 3 
  FF (b, c, d, a, x[ 3], S14, 0xc1bdceee); // 4 
  FF (a, b, c, d, x[ 4], S11, 0xf57c0faf); // 5 
  FF (d, a, b, c, x[ 5], S12, 0x4787c62a); // 6 
  FF (c, d, a, b, x[ 6], S13, 0xa8304613); // 7 
  FF (b, c, d, a, x[ 7], S14, 0xfd469501); // 8 
  FF (a, b, c, d, x[ 8], S11, 0x698098d8); // 9 
  FF (d, a, b, c, x[ 9], S12, 0x8b44f7af); // 10 
  FF (c, d, a, b, x[10], S13, 0xffff5bb1); // 11 
  FF (b, c, d, a, x[11], S14, 0x895cd7be); // 12 
  FF (a, b, c, d, x[12], S11, 0x6b901122); // 13 
  FF (d, a, b, c, x[13], S12, 0xfd987193); // 14 
  FF (c, d, a, b, x[14], S13, 0xa679438e); // 15 
  FF (b, c, d, a, x[15], S14, 0x49b40821); // 16 
 
  // Round 2 
  GG (a, b, c, d, x[ 1], S21, 0xf61e2562); // 17 
  GG (d, a, b, c, x[ 6], S22, 0xc040b340); // 18 
  GG (c, d, a, b, x[11], S23, 0x265e5a51); // 19 
  GG (b, c, d, a, x[ 0], S24, 0xe9b6c7aa); // 20 
  GG (a, b, c, d, x[ 5], S21, 0xd62f105d); // 21 
  GG (d, a, b, c, x[10], S22,  0x2441453); // 22 
  GG (c, d, a, b, x[15], S23, 0xd8a1e681); // 23 
  GG (b, c, d, a, x[ 4], S24, 0xe7d3fbc8); // 24 
  GG (a, b, c, d, x[ 9], S21, 0x21e1cde6); // 25 
  GG (d, a, b, c, x[14], S22, 0xc33707d6); // 26 
  GG (c, d, a, b, x[ 3], S23, 0xf4d50d87); // 27 
  GG (b, c, d, a, x[ 8], S24, 0x455a14ed); // 28 
  GG (a, b, c, d, x[13], S21, 0xa9e3e905); // 29 
  GG (d, a, b, c, x[ 2], S22, 0xfcefa3f8); // 30 
  GG (c, d, a, b, x[ 7], S23, 0x676f02d9); // 31 
  GG (b, c, d, a, x[12], S24, 0x8d2a4c8a); // 32 
 
  // Round 3 
  HH (a, b, c, d, x[ 5], S31, 0xfffa3942); // 33 
  HH (d, a, b, c, x[ 8], S32, 0x8771f681); // 34 
  HH (c, d, a, b, x[11], S33, 0x6d9d6122); // 35 
  HH (b, c, d, a, x[14], S34, 0xfde5380c); // 36 
  HH (a, b, c, d, x[ 1], S31, 0xa4beea44); // 37 
  HH (d, a, b, c, x[ 4], S32, 0x4bdecfa9); // 38 
  HH (c, d, a, b, x[ 7], S33, 0xf6bb4b60); // 39 
  HH (b, c, d, a, x[10], S34, 0xbebfbc70); // 40 
  HH (a, b, c, d, x[13], S31, 0x289b7ec6); // 41 
  HH (d, a, b, c, x[ 0], S32, 0xeaa127fa); // 42 
  HH (c, d, a, b, x[ 3], S33, 0xd4ef3085); // 43 
  HH (b, c, d, a, x[ 6], S34,  0x4881d05); // 44 
  HH (a, b, c, d, x[ 9], S31, 0xd9d4d039); // 45 
  HH (d, a, b, c, x[12], S32, 0xe6db99e5); // 46 
  HH (c, d, a, b, x[15], S33, 0x1fa27cf8); // 47 
  HH (b, c, d, a, x[ 2], S34, 0xc4ac5665); // 48 
 
  // Round 4 
  II (a, b, c, d, x[ 0], S41, 0xf4292244); // 49 
  II (d, a, b, c, x[ 7], S42, 0x432aff97); // 50 
  II (c, d, a, b, x[14], S43, 0xab9423a7); // 51 
  II (b, c, d, a, x[ 5], S44, 0xfc93a039); // 52 
  II (a, b, c, d, x[12], S41, 0x655b59c3); // 53 
  II (d, a, b, c, x[ 3], S42, 0x8f0ccc92); // 54 
  II (c, d, a, b, x[10], S43, 0xffeff47d); // 55 
  II (b, c, d, a, x[ 1], S44, 0x85845dd1); // 56 
  II (a, b, c, d, x[ 8], S41, 0x6fa87e4f); // 57 
  II (d, a, b, c, x[15], S42, 0xfe2ce6e0); // 58 
  II (c, d, a, b, x[ 6], S43, 0xa3014314); // 59 
  II (b, c, d, a, x[13], S44, 0x4e0811a1); // 60 
  II (a, b, c, d, x[ 4], S41, 0xf7537e82); // 61 
  II (d, a, b, c, x[11], S42, 0xbd3af235); // 62 
  II (c, d, a, b, x[ 2], S43, 0x2ad7d2bb); // 63 
  II (b, c, d, a, x[ 9], S44, 0xeb86d391); // 64 
 
  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
 
  // Zeroize sensitive information.
  memset(x, 0, sizeof x);
}
 
//////////////////////////////
 
// CPID block update operation. Continues an CPID message-digest
// operation, processing another message block
void CPID::update(const unsigned char input[], size_type length)
{
  // compute number of bytes mod 64
  size_type index = count[0] / 8 % blocksize;
 
  // Update number of bits
  if ((count[0] += (length << 3)) < (length << 3))
    count[1]++;
  count[1] += (length >> 29);
 
  // number of bytes we need to fill in buffer
  size_type firstpart = 64 - index;
 
  size_type i;
 
  // transform as many times as possible.
  if (length >= firstpart)
  {
    // fill buffer first, transform
    memcpy(&buffer[index], input, firstpart);
    transform(buffer);
 
    // transform chunks of blocksize (64 bytes)
    for (i = firstpart; i + blocksize <= length; i += blocksize)
      transform(&input[i]);
 
    index = 0;
  }
  else
    i = 0;
 
  // buffer remaining input
  memcpy(&buffer[index], &input[i], length-i);
}
 
//////////////////////////////
 
// for convenience provide a version with signed char
void CPID::update(const char input[], size_type length)
{
  update((const unsigned char*)input, length);
}




int BitwiseCount(std::string str, int pos)
{
	char ch;
	if (pos < str.length())
	{
		ch = str.at(pos);
		int asc = (int)ch;
		if (asc > 47 && asc < 71) asc=asc-47;
		return asc;
	}
	return 1;
}


int HexToByte(std::string hex)
{
	int x = 0;
	std::stringstream ss;
	ss << std::hex << hex;
	ss >> x;
	return x;

}



void CPID::update5(std::string longcpid, uint256 blockhash)
{
	std::string shash = blockhash.GetHex();

    
	std::string entropy = "";
	for (int z = 0; z < length; z++)
	{
		char c  = input[z];
		//int rorcount = BitwiseCount(shash,hexpos);
		int ic = (int)c;
		//ic = (char)rotate_right(b,rorcount+1);
		//char cc1 = (char)rotate_left(ic,1);
		char cc1 = (char)ic;
		input[z] = cc1;
		entropy += cc1;

	}
	printf("Entropy class %s",entropy.c_str());
	

  //  std::string cpid3 = "";
   int hexpos = 0;
   unsigned char* input = new unsigned char[(longcpid.length()/2)+1];
	
   for (int z = 0; z < longcpid.length(); z=z+2)
   {
	    //char c1 = input[i];
		std::string hex = longcpid.substr(z,2);
    	//std::string hex = cpid2.substr(i,2);
		int b  = HexToByte(hex);
		int rorcount = BitwiseCount(shash,hexpos);
		entropy += (char)rotate_right(b,rorcount);
		//cpid3 += (char)rotate_right(b,rorcount+1);
		input[hexpos]=(unsigned char)rotate_right(b,rorcount);
		hexpos++;
    }
    printf("Entropy length %u class %s",longcpid.length(),entropy.c_str());
	//	har *path = new char[pathSize];
	////////////////////////////////////////////////const unsigned char* input = (const unsigned char*)entropy.c_str(); 


	//	unsigned char* input = (unsigned char*)entropy.c_str();
	unsigned char* input2 = new unsigned char[entropy.size()+1];
	memcpy(input2, entropy.c_str(), entropy.size());
	input2[entropy.size()]=0;
	//unsigned char *input[] = (unsigned char*)entropy.c_str();
	input[entropy.size()]=0;


	 size_type length = entropy.length();


  // compute number of bytes mod 64
  size_type index = count[0] / 8 % blocksize;
 
  // Update number of bits
  if ((count[0] += (length << 3)) < (length << 3))
    count[1]++;
  count[1] += (length >> 29);
 
  // number of bytes we need to fill in buffer
  size_type firstpart = 64 - index;
 
  size_type i;
 
  // transform as many times as possible.
  if (length >= firstpart)
  {
    // fill buffer first, transform
    memcpy(&buffer[index], input, firstpart);
    transform(buffer);
 
    // transform chunks of blocksize (64 bytes)
    for (i = firstpart; i + blocksize <= length; i += blocksize)
      transform(&input[i]);
 
    index = 0;
  }
  else
    i = 0;
 
  // buffer remaining input
  memcpy(&buffer[index], &input[i], length-i);
}












 
//////////////////////////////
 
// CPID finalization. Ends an CPID message-digest operation, writing the
// the message digest and zeroizing the context.
CPID& CPID::finalize()
{
  static unsigned char padding[64] = {
    0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
  };
 
  if (!finalized) {
    // Save number of bits
    unsigned char bits[8];
    encode(bits, count, 8);
 
    // pad out to 56 mod 64.
    size_type index = count[0] / 8 % 64;
    size_type padLen = (index < 56) ? (56 - index) : (120 - index);
    update(padding, padLen);
 
    // Append length (before padding)
    update(bits, 8);
 
    // Store state in digest
    encode(digest, state, 16);
 
    // Zeroize sensitive information.
    memset(buffer, 0, sizeof buffer);
    memset(count, 0, sizeof count);
 
    finalized=true;
  }
 
  return *this;
}
 
//////////////////////////////
 
// return hex representation of digest as string
std::string CPID::hexdigest() const
{
  if (!finalized)
    return "";
 
  char buf[33];
  for (int i=0; i<16; i++)
    sprintf(buf+i*2, "%02x", digest[i]);
  buf[32]=0;
 
  return std::string(buf);
}
 
//////////////////////////////


template< typename T >
std::string LongToHex( T i )
{
  std::stringstream stream;
  stream << "0x" 
         << std::setfill ('0') << std::setw(sizeof(T)*2) 
         << std::hex << i;
  return stream.str();
}

template< typename T >
std::string ByteToHex( T i )
{
  std::stringstream stream;
  stream << std::setfill ('0') << std::setw(2) 
         << std::hex << i;
  return stream.str();
}


std::string CPID::boincdigest() const
{

  if (!finalized)
    return "";
    
  char buf[16];
  for (int i=0; i<16; i++)
  {
		sprintf(buf+i*2, "%02x", digest[i]);
  }
  char ch;
	
  std::string non_finalized(buf);
  std::string shash = blockhash.GetHex();

  for (int i = 0; i < merged_hash.length(); i++)
  {
	    int asc1 = (int)merged_hash.at(i);
		int rorcount = BitwiseCount(shash, i);
		int asc2 = rotate_left(asc1,rorcount);
		non_finalized = non_finalized + ByteToHex(asc2);
  }

 
  return non_finalized;
}






bool CPID::Compare(std::string usercpid, std::string longcpid, uint256 blockhash)
{

   if (longcpid.length() < 18) return false;
   std::string cpid1 = longcpid.substr(0,32);
   std::string cpid2 = longcpid.substr(32,cpid2.length()-31);
   std::string shash = blockhash.GetHex();
   std::string cpid3 = "";
   int hexpos = 0;
   for (int i = 0; i < cpid2.length(); i=i+2)
   {
    	std::string hex = cpid2.substr(i,2);
		int b  = HexToByte(hex);
		int rorcount = BitwiseCount(shash,hexpos);
		cpid3 += (char)rotate_right(b,rorcount);
		//cpid3 += (char)rotate_right(b,rorcount+1);
		hexpos++;
    }
   printf("Comparing %s",cpid2.c_str());

    CPID c = CPID(cpid2,0,blockhash);
	std::string shortcpid = c.hexdigest();
	if (shortcpid == cpid1 && cpid1==usercpid && shortcpid == usercpid) return true;
	return false;
}





std::ostream& operator<<(std::ostream& out, CPID CPID)
{
  return out << CPID.hexdigest();
}
 
//////////////////////////////






std::string boinc_hash(const std::string str)
{
    CPID c = CPID(str);
    return c.hexdigest();
}


std::string cpid_hash(std::string email, std::string bpk, uint256 blockhash)
{
	   //Given a block hash, a boinc e-mail, and a boinc public key, generate a cpid hash
      CPID c = CPID(email,bpk,blockhash);
      return c.boincdigest();
}


bool IsCPIDValid(std::string cpid, std::string longcpid, uint256 blockhash)
{
	CPID c = CPID(cpid);
	bool compared = c.Compare(cpid,longcpid,blockhash);
	return compared;
}



*/

