/* 
CPID
*/


#ifndef BZF_CPID_H
#define BZF_CPID_H
 


#include <cstring>
#include <iostream>
#include "util.h"


#include <boost/algorithm/string/case_conv.hpp> // for to_lower()


class CPID
{
public:
  typedef unsigned int size_type; // must be 32bit
 
  CPID();
  CPID(std::string text);
  CPID(std::string text,int entropybit,uint256 block_hash);

  void update(const unsigned char *buf, size_type length);
  void update(const char *buf, size_type length);
  
  void update5(std::string inp,uint256 hash_block);
  
  std::string CPID_V2(std::string email, std::string bpk,uint256 block_hash);

  CPID& finalize();
  std::string hexdigest() const;
  std::string boincdigest(std::string email, std::string bpk,uint256 hash_block);


//  bool Compare(std::string usercpid, std::string longcpid, uint256 blockhash);
 
  friend std::ostream& operator<<(std::ostream&, CPID cpid);
 
private:
  void init();
  typedef unsigned char uint1; //  8bit
  typedef unsigned int  uint4;  // 32bit
  enum {blocksize = 64};       // VC6 requires const static int here
 
  void transform(const uint1 block[blocksize]);
  static void decode(uint4 output[], const uint1 input[], size_type len);
  static void encode(uint1 output[], const uint4 input[], size_type len);
  std::string HashKey(std::string email1, std::string bpk1);
  
  std::string Update6(std::string non_finalized, uint256 block_hash);

  std::string boinc_public_key;
  std::string email_hash;
  std::string merged_hash_old;
  std::string boinc_hash_new;

  
  uint256 blockhash_old;
  bool finalized;
  uint1 buffer[blocksize]; // bytes that didn't fit in last 64 byte chunk
  uint4 count[2];   // 64bit counter for number of bits (lo, hi)
  uint4 state[4];   // digest so far
  uint1 digest[16]; // the result
 
  // low level logic operations
  static inline uint4 F(uint4 x, uint4 y, uint4 z);
  static inline uint4 G(uint4 x, uint4 y, uint4 z);
  static inline uint4 H(uint4 x, uint4 y, uint4 z);
  static inline uint4 I(uint4 x, uint4 y, uint4 z);
  static inline uint4 rotate_left(uint4 x, int n);
  static inline uint4 rotate_right(uint4 x, int n);
  static inline uint4 rotate_left8(int x, int n);
  static inline uint4 rotate_right8(int x, int n);

  static inline void FF(uint4 &a, uint4 b, uint4 c, uint4 d, uint4 x, uint4 s, uint4 ac);
  static inline void GG(uint4 &a, uint4 b, uint4 c, uint4 d, uint4 x, uint4 s, uint4 ac);
  static inline void HH(uint4 &a, uint4 b, uint4 c, uint4 d, uint4 x, uint4 s, uint4 ac);
  static inline void II(uint4 &a, uint4 b, uint4 c, uint4 d, uint4 x, uint4 s, uint4 ac);
};
 
std::string cpid(const std::string str);

bool CPID_IsCPIDValid(std::string cpid, std::string longcpid, uint256 blockhash);


#endif




