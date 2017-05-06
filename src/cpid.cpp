
#include "cpid.h"
#include <cstdio>
#include <boost/algorithm/string/case_conv.hpp>
#include <iomanip>

#define S11 (0x9e5+1366-0xf34)
#define S12 (0x161b+3182-0x227d)
#define S13 (0x39b+3002-0xf44)
#define S14 (0xe94+6207-0x26bd)
#define S21 (0x4ef+2412-0xe56)
#define S22 (0x311+2131-0xb5b)
#define S23 (0x6cd+6218-0x1f09)
#define S24 (0x16+1307-0x51d)
#define S31 (0x13b1+3020-0x1f79)
#define S32 (0x10d4+714-0x1393)
#define S33 (0x1f32+1918-0x26a0)
#define S34 (0x514+6441-0x1e26)
#define S41 (0x2df+307-0x40c)
#define S42 (0x131b+4053-0x22e6)
#define S43 (0x817+1038-0xc16)
#define S44 (0x1021+2805-0x1b01)


inline CPID::uint4 CPID::F(uint4 x,uint4 y,uint4 z){return((x&y)|(~x&z));}inline
 CPID::uint4 CPID::G(uint4 x,uint4 y,uint4 z){return((x&z)|(y&~z));}inline CPID
::uint4 CPID::H(uint4 x,uint4 y,uint4 z){return(x^y^z);}inline CPID::uint4 CPID
::I(uint4 x,uint4 y,uint4 z){return y^(x|~z);}
inline CPID::uint4 CPID::rotate_left(uint4 x,int n){return(x<<n)|(x>>(
(0x724+7488-0x2444)-n));}inline CPID::uint4 CPID::rotate_right(uint4 x,int n){
return(x>>n)|(x<<((0xf51+544-0x1151)-n));}
inline CPID::uint4 CPID::rotate_left8(int x,int n){return(x<<n)|(x>>(
(0x1673+891-0x19e6)-n));}inline CPID::uint4 CPID::rotate_right8(int x,int n){
return(x>>n)|(x<<((0x12e9+2293-0x1bd6)-n));}

inline void CPID::FF(uint4&a,uint4 b,uint4 c,uint4 d,uint4 x,uint4 s,uint4 ac){a
=rotate_left(a+F(b,c,d)+x+ac,s)+b;}inline void CPID::GG(uint4&a,uint4 b,uint4 c,
uint4 d,uint4 x,uint4 s,uint4 ac){a=rotate_left(a+G(b,c,d)+x+ac,s)+b;}inline 
void CPID::HH(uint4&a,uint4 b,uint4 c,uint4 d,uint4 x,uint4 s,uint4 ac){a=
rotate_left(a+H(b,c,d)+x+ac,s)+b;}inline void CPID::II(uint4&a,uint4 b,uint4 c,
uint4 d,uint4 x,uint4 s,uint4 ac){a=rotate_left(a+I(b,c,d)+x+ac,s)+b;}

CPID::CPID(){init();}

CPID::CPID(std::string text){init();update(text.c_str(),text.length());finalize(
);}

CPID::CPID(std::string text,int entropybit,uint256 hash_block){init();entropybit
++;update5(text,hash_block);finalize();}template<typename T>std::string 
ByteToHex(T i){std::stringstream stream;stream<<std::setfill(
((char)(0xbac+70-0xbc2)))<<std::setw((0x1344+4775-0x25e9))<<std::hex<<i;return 
stream.str();}std::string CPID::HashKey(std::string email1,std::string bpk1){
boost::algorithm::to_lower(bpk1);boost::algorithm::to_lower(email1);
boinc_hash_new=bpk1+email1;CPID c=CPID(boinc_hash_new);std::string non_finalized
="";non_finalized=c.hexdigest();return non_finalized;}int BitwiseCount(std::
string str,int pos){char ch;if(pos<(int)str.length()){ch=str.at(pos);int asc=(
int)ch;if(asc>(0x87c+6520-0x21c5)&&asc<(0x1597+4174-0x259e))asc=asc-
(0x4c5+8720-0x26a6);return asc;}return(0x8b0+1872-0xfff);}std::string HashHex(
uint256 blockhash){CPID c2=CPID(blockhash.GetHex());std::string shash=c2.
hexdigest();return shash;}std::string ROR(std::string blockhash,int iPos,std::
string hash){if(iPos<=(int)hash.length()-(0x1f5b+1342-0x2498)){int asc1=(int)
hash.at(iPos);int rorcount=BitwiseCount(blockhash,iPos);std::string hex=
ByteToHex(asc1+rorcount);return hex;}return"\x30\x30";}std::string CPID::CPID_V2
(std::string email1,std::string bpk1,uint256 block_hash){std::string 
non_finalized=HashKey(email1,bpk1);std::string digest=Update6(non_finalized,
block_hash);
return digest;}
void CPID::init(){finalized=false;count[(0x88d+1394-0xdff)]=(0x1fe5+1717-0x269a)
;count[(0x373+6812-0x1e0e)]=(0x65b+2790-0x1141);
state[(0xc88+3077-0x188d)]=1732584193;state[(0x1230+1876-0x1983)]=4023233417;
state[(0xada+3060-0x16cc)]=2562383102;state[(0x8d+3707-0xf05)]=271733878;}

void CPID::decode(uint4 output[],const uint1 input[],size_type len){for(unsigned
 int i=(0xbb+8818-0x232d),j=(0xcad+2297-0x15a6);j<len;i++,j+=
(0x150f+1818-0x1c25))output[i]=((uint4)input[j])|(((uint4)input[j+
(0x320+7218-0x1f51)])<<(0x1c36+2528-0x260e))|(((uint4)input[j+
(0x20c3+1239-0x2598)])<<(0x9d+5272-0x1525))|(((uint4)input[j+(0xbb7+2557-0x15b1)
])<<(0x131b+3426-0x2065));}


void CPID::encode(uint1 output[],const uint4 input[],size_type len){for(
size_type i=(0x3e2+7679-0x21e1),j=(0x23a4+674-0x2646);j<len;i++,j+=
(0x23d3+36-0x23f3)){output[j]=input[i]&(0xa45+3187-0x15b9);output[j+
(0x90a+3950-0x1877)]=(input[i]>>(0x201+7372-0x1ec5))&(0x9bf+6024-0x2048);output[
j+(0x94+872-0x3fa)]=(input[i]>>(0x396+7043-0x1f09))&(0x561+8582-0x25e8);output[j
+(0x1a86+2527-0x2462)]=(input[i]>>(0xd14+4931-0x203f))&(0x37b+3944-0x11e4);}}

void CPID::transform(const uint1 block[blocksize]){uint4 a=state[
(0x8a5+4443-0x1a00)],b=state[(0x643+3050-0x122c)],c=state[(0x280+7521-0x1fdf)],d
=state[(0x1de9+985-0x21bf)],x[(0x1784+2549-0x2169)];decode(x,block,blocksize);
FF(a,b,c,d,x[(0xd59+2856-0x1881)],S11,3614090360);
FF(d,a,b,c,x[(0x4b9+4745-0x1741)],S12,3905402710);
FF(c,d,a,b,x[(0x82a+7856-0x26d8)],S13,606105819);
FF(b,c,d,a,x[(0x9a1+574-0xbdc)],S14,3250441966);
FF(a,b,c,d,x[(0xfeb+4629-0x21fc)],S11,4118548399);
FF(d,a,b,c,x[(0x1016+3805-0x1eee)],S12,1200080426);
FF(c,d,a,b,x[(0xb8f+2088-0x13b1)],S13,2821735955);
FF(b,c,d,a,x[(0xbcc+5992-0x232d)],S14,4249261313);
FF(a,b,c,d,x[(0xd35+3872-0x1c4d)],S11,1770035416);
FF(d,a,b,c,x[(0x1104+5535-0x269a)],S12,2336552879);
FF(c,d,a,b,x[(0xd1b+1447-0x12b8)],S13,4294925233);
FF(b,c,d,a,x[(0xdfc+289-0xf12)],S14,2304563134);
FF(a,b,c,d,x[(0x1ed7+2070-0x26e1)],S11,1804603682);
FF(d,a,b,c,x[(0x175d+2428-0x20cc)],S12,4254626195);
FF(c,d,a,b,x[(0xd9c+2608-0x17be)],S13,2792965006);
FF(b,c,d,a,x[(0xddd+6185-0x25f7)],S14,1236535329);

GG(a,b,c,d,x[(0x1e0d+484-0x1ff0)],S21,4129170786);
GG(d,a,b,c,x[(0xa2+8809-0x2305)],S22,3225465664);
GG(c,d,a,b,x[(0x1983+383-0x1af7)],S23,643717713);
GG(b,c,d,a,x[(0xbe7+2585-0x1600)],S24,3921069994);
GG(a,b,c,d,x[(0x1e91+161-0x1f2d)],S21,3593408605);
GG(d,a,b,c,x[(0x45d+7119-0x2022)],S22,38016083);
GG(c,d,a,b,x[(0x60b+5977-0x1d55)],S23,3634488961);
GG(b,c,d,a,x[(0xeef+2607-0x191a)],S24,3889429448);
GG(a,b,c,d,x[(0xcb+9332-0x2536)],S21,568446438);
GG(d,a,b,c,x[(0x5cd+1125-0xa24)],S22,3275163606);
GG(c,d,a,b,x[(0x101+6897-0x1bef)],S23,4107603335);
GG(b,c,d,a,x[(0x5e8+2294-0xed6)],S24,1163531501);
GG(a,b,c,d,x[(0x10b8+3222-0x1d41)],S21,2850285829);
GG(d,a,b,c,x[(0x903+1561-0xf1a)],S22,4243563512);
GG(c,d,a,b,x[(0x880+2802-0x136b)],S23,1735328473);
GG(b,c,d,a,x[(0xcb6+3479-0x1a41)],S24,2368359562);

HH(a,b,c,d,x[(0x1a5f+1242-0x1f34)],S31,4294588738);
HH(d,a,b,c,x[(0x3b9+11-0x3bc)],S32,2272392833);
HH(c,d,a,b,x[(0x161a+3175-0x2276)],S33,1839030562);
HH(b,c,d,a,x[(0x134b+2374-0x1c83)],S34,4259657740);
HH(a,b,c,d,x[(0xe5b+1402-0x13d4)],S31,2763975236);
HH(d,a,b,c,x[(0x7aa+6444-0x20d2)],S32,1272893353);
HH(c,d,a,b,x[(0x9b0+2801-0x149a)],S33,4139469664);
HH(b,c,d,a,x[(0x180+2489-0xb2f)],S34,3200236656);
HH(a,b,c,d,x[(0x14d+7675-0x1f3b)],S31,681279174);
HH(d,a,b,c,x[(0xeaf+5786-0x2549)],S32,3936430074);
HH(c,d,a,b,x[(0x94d+6302-0x21e8)],S33,3572445317);
HH(b,c,d,a,x[(0x4e3+4075-0x14c8)],S34,76029189);
HH(a,b,c,d,x[(0x183f+1914-0x1fb0)],S31,3654602809);
HH(d,a,b,c,x[(0x785+531-0x98c)],S32,3873151461);
HH(c,d,a,b,x[(0x76+4452-0x11cb)],S33,530742520);
HH(b,c,d,a,x[(0x60a+7659-0x23f3)],S34,3299628645);

II(a,b,c,d,x[(0xdab+1728-0x146b)],S41,4096336452);
II(d,a,b,c,x[(0x8a7+5703-0x1ee7)],S42,1126891415);
II(c,d,a,b,x[(0x2526+30-0x2536)],S43,2878612391);
II(b,c,d,a,x[(0xd53+6032-0x24de)],S44,4237533241);
II(a,b,c,d,x[(0x23+3964-0xf93)],S41,1700485571);
II(d,a,b,c,x[(0x22f1+300-0x241a)],S42,2399980690);
II(c,d,a,b,x[(0x7ad+7010-0x2305)],S43,4293915773);
II(b,c,d,a,x[(0x1afa+728-0x1dd1)],S44,2240044497);
II(a,b,c,d,x[(0x535+4538-0x16e7)],S41,1873313359);
II(d,a,b,c,x[(0x1902+3148-0x253f)],S42,4264355552);
II(c,d,a,b,x[(0x9ed+599-0xc3e)],S43,2734768916);
II(b,c,d,a,x[(0x1955+3023-0x2517)],S44,1309151649);
II(a,b,c,d,x[(0xa53+2102-0x1285)],S41,4149444226);
II(d,a,b,c,x[(0x993+4512-0x1b28)],S42,3174756917);
II(c,d,a,b,x[(0x1621+413-0x17bc)],S43,718787259);
II(b,c,d,a,x[(0xf32+3228-0x1bc5)],S44,3951481745);
state[(0x13e0+4114-0x23f2)]+=a;state[(0x864+5609-0x1e4c)]+=b;state[
(0x1bb+1236-0x68d)]+=c;state[(0x92f+6920-0x2434)]+=d;
memset(x,(0x123d+2605-0x1c6a),sizeof x);}


void CPID::update(const unsigned char input[],size_type length){
size_type index=count[(0xbaf+535-0xdc6)]/(0x1609+3405-0x234e)%blocksize;
if((count[(0x12f6+3141-0x1f3b)]+=(length<<(0x1894+3130-0x24cb)))<(length<<
(0x902+5767-0x1f86)))count[(0xed4+4990-0x2251)]++;count[(0x1410+3546-0x21e9)]+=(
length>>(0x1325+1183-0x17a7));
size_type firstpart=(0xcf2+5399-0x21c9)-index;size_type i;
if(length>=firstpart){
memcpy(&buffer[index],input,firstpart);transform(buffer);
for(i=firstpart;i+blocksize<=length;i+=blocksize)transform(&input[i]);index=
(0x786+6933-0x229b);}else i=(0x3c9+6892-0x1eb5);
memcpy(&buffer[index],&input[i],length-i);}

void CPID::update(const char input[],size_type length){update((const unsigned 
char*)input,length);}int HexToByte(std::string hex){int x=(0x4a+4863-0x1349);std
::stringstream ss;ss<<std::hex<<hex;ss>>x;return x;}int ROL(std::string 
blockhash,int iPos,std::string hash,int hexpos){std::string cpid3="";if(iPos<=(
int)hash.length()-(0x1c97+497-0x1e87)){std::string hex=hash.substr(iPos,
(0xa5d+6424-0x2373));int rorcount=BitwiseCount(blockhash,hexpos);int b=HexToByte
(hex)-rorcount;if(b>=(0x5b2+1768-0xc9a)){return b;}}return HexToByte("\x30\x30")
;}std::string CPID::Update6(std::string non_finalized,uint256 block_hash){std::
string shash=HashHex(block_hash);for(int i=(0x1258+3278-0x1f26);i<(int)
boinc_hash_new.length();i++){non_finalized+=ROR(shash,i,boinc_hash_new);}return 
non_finalized;}std::string Update7(std::string longcpid,uint256 hash_block){std
::string shash=HashHex(hash_block);int hexpos=(0x632+1664-0xcb2);std::string 
non_finalized="";for(int i1=(0xbd+8943-0x23ac);i1<(int)longcpid.length();i1=i1+
(0x1ac0+2812-0x25ba)){non_finalized+=ROL(shash,i1,longcpid,hexpos);hexpos++;}
CPID c7=CPID(non_finalized);std::string hexstring=c7.hexdigest();return 
hexstring;}void CPID::update5(std::string longcpid,uint256 hash_block){std::
string shash=HashHex(hash_block);int hexpos=(0x43b+6491-0x1d96);unsigned char*
input=new unsigned char[(longcpid.length()/(0x534+8432-0x2622))+
(0xa19+5289-0x1ec1)];for(int i1=(0x1348+1245-0x1825);i1<(int)longcpid.length();
i1=i1+(0x478+5707-0x1ac1)){input[hexpos]=ROL(shash,i1,longcpid,hexpos);hexpos++;
}input[longcpid.length()/(0x7ac+7506-0x24fc)+(0xba+3605-0xece)]=
(0x3c3+5976-0x1b1b);size_type length=longcpid.length()/(0x1186+4729-0x23fd);
size_type index=count[(0xf02+2743-0x19b9)]/(0x174+6416-0x1a7c)%blocksize;
if((count[(0x1eb3+1439-0x2452)]+=(length<<(0x89c+7639-0x2670)))<(length<<
(0x1f07+935-0x22ab)))count[(0x488+7340-0x2133)]++;count[(0xb5+5597-0x1691)]+=(
length>>(0x374+3554-0x1139));
size_type firstpart=(0x195+7412-0x1e49)-index;size_type i;
if(length>=firstpart){
memcpy(&buffer[index],input,firstpart);transform(buffer);
for(i=firstpart;i+blocksize<=length;i+=blocksize)transform(&input[i]);index=
(0x144f+4713-0x26b8);}else i=(0x1053+3145-0x1c9c);
memcpy(&buffer[index],&input[i],length-i);}


CPID&CPID::finalize(){static unsigned char padding[(0xbf3+5821-0x2270)]={
(0x14b4+2295-0x1d2b),(0xd2+9568-0x2632),(0xa60+6685-0x247d),(0x1749+2154-0x1fb3)
,(0xb80+3428-0x18e4),(0x9d+9832-0x2705),(0x6d5+1494-0xcab),(0x714+3-0x717),
(0x13bf+4470-0x2535),(0x1a98+1182-0x1f36),(0x1398+3220-0x202c),
(0x50f+1352-0xa57),(0x97c+2239-0x123b),(0xa12+2843-0x152d),(0x934+3404-0x1680),
(0xb50+4854-0x1e46),(0x1166+928-0x1506),(0x1def+2302-0x26ed),(0xe84+1817-0x159d)
,(0x205+8670-0x23e3),(0x1311+2117-0x1b56),(0x23fa+91-0x2455),(0xec0+2817-0x19c1)
,(0x12b6+5050-0x2670),(0x141c+2938-0x1f96),(0x1c2b+389-0x1db0),
(0x8b6+3730-0x1748),(0x1274+509-0x1471),(0xc82+2063-0x1491),(0x15e6+916-0x197a),
(0x9b7+6733-0x2404),(0x1463+4638-0x2681),(0x48c+4237-0x1519),(0x6d3+8197-0x26d8)
,(0x1531+3021-0x20fe),(0x1723+1018-0x1b1d),(0xccb+5869-0x23b8),
(0x65c+1335-0xb93),(0x22bd+52-0x22f1),(0x1592+1786-0x1c8c),(0x7ff+7337-0x24a8),
(0x825+1675-0xeb0),(0x1ed+210-0x2bf),(0x7ea+398-0x978),(0x337+1617-0x988),
(0x84c+7067-0x23e7),(0x5a1+7001-0x20fa),(0x22f3+735-0x25d2),(0x1819+914-0x1bab),
(0x16b+2277-0xa50),(0x9e3+731-0xcbe),(0x298+6656-0x1c98),(0xd89+2525-0x1766),
(0xe8d+3380-0x1bc1),(0x2346+220-0x2422),(0x111+6611-0x1ae4),(0x37+8793-0x2290),
(0xa62+5983-0x21c1),(0x1508+1558-0x1b1e),(0x6ba+6988-0x2206),(0x76a+2299-0x1065)
,(0x977+4133-0x199c),(0xca+8498-0x21fc),(0x1c0a+802-0x1f2c)};if(!finalized){
unsigned char bits[(0x1bfb+232-0x1cdb)];encode(bits,count,(0xba5+5616-0x218d));
size_type index=count[(0x176d+89-0x17c6)]/(0x8c1+7152-0x24a9)%
(0xa16+4066-0x19b8);size_type padLen=(index<(0x1da+3012-0xd66))?(
(0x1b22+1127-0x1f51)-index):((0x1b2a+2082-0x22d4)-index);update(padding,padLen);
update(bits,(0xdb2+2231-0x1661));
encode(digest,state,(0x2c1+8014-0x21ff));
memset(buffer,(0xa7d+2489-0x1436),sizeof buffer);memset(count,
(0x101+8152-0x20d9),sizeof count);finalized=true;}return*this;}

std::string CPID::hexdigest()const{if(!finalized){return
"\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30"
;}char buf[(0x1605+2905-0x213d)];for(int i=(0xba6+2047-0x13a5);i<
(0x96b+2692-0x13df);i++)sprintf(buf+i*(0x6b6+3760-0x1564),"\x25\x30\x32\x78",
digest[i]);buf[(0x1064+790-0x135a)]=(0xaa7+5325-0x1f74);return std::string(buf);
}
template<typename T>std::string LongToHex(T i){std::stringstream stream;stream<<
"\x30\x78"<<std::setfill(((char)(0x225+6873-0x1cce)))<<std::setw(sizeof(T)*
(0x27d+3962-0x11f5))<<std::hex<<i;return stream.str();}std::string CPID::
boincdigest(std::string email,std::string bpk,uint256 hash_block){
if(!finalized)return"";char buf[(0x151+8463-0x2250)];for(int i=
(0x95f+3677-0x17bc);i<(0x13f8+3123-0x201b);i++){sprintf(buf+i*
(0x639+7515-0x2392),"\x25\x30\x32\x78",digest[i]);}char ch;std::string 
non_finalized(buf);std::string shash=HashHex(hash_block);std::string debug="";
boost::algorithm::to_lower(bpk);boost::algorithm::to_lower(email);std::string 
cpid_non=bpk+email;for(int i=(0x450+3069-0x104d);i<(int)cpid_non.length();i++){
non_finalized+=ROR(shash,i,cpid_non);}
return non_finalized;}bool CompareCPID(std::string usercpid,std::string longcpid
,uint256 blockhash){if(longcpid.length()<(0x1287+3377-0x1f96))return false;std::
string cpid1=longcpid.substr((0x14eb+3914-0x2435),(0x652+2202-0xecc));std::
string cpid2=longcpid.substr((0x315+271-0x404),longcpid.length()-
(0x477+4848-0x1748));std::string shash=HashHex(blockhash);
std::string shortcpid=Update7(cpid2,blockhash);if(shortcpid=="")return false;if(
fDebug10)printf(
"\x73\x68\x6f\x72\x74\x63\x70\x69\x64\x20\x25\x73\x2c\x20\x63\x70\x69\x64\x31\x20\x25\x73\x2c\x20\x75\x73\x65\x72\x63\x70\x69\x64\x20\x25\x73\x20" "\r\n"
,shortcpid.c_str(),cpid1.c_str(),usercpid.c_str());if(shortcpid==cpid1&&cpid1==
usercpid&&shortcpid==usercpid)return true;if(fDebug10)printf(
"\x73\x68\x6f\x72\x74\x63\x70\x69\x64\x20\x25\x73\x2c\x20\x63\x70\x69\x64\x31\x20\x25\x73\x2c\x20\x75\x73\x65\x72\x63\x70\x69\x64\x20\x25\x73\x20" "\r\n"
,shortcpid.c_str(),cpid1.c_str(),usercpid.c_str());return false;}std::ostream&
operator<<(std::ostream&out,CPID CPID){return out<<CPID.hexdigest();}
bool CPID_IsCPIDValid(std::string cpid1,std::string longcpid,uint256 blockhash){
if(cpid1.empty())return false;if(longcpid.empty())return false;if(longcpid.
length()<(0x21f0+603-0x242b))return false;if(cpid1.length()<(0x14+9069-0x237c))
return false;if(cpid1=="\x49\x4e\x56\x45\x53\x54\x4f\x52"||longcpid==
"\x49\x4e\x56\x45\x53\x54\x4f\x52")return true;if(cpid1.length()==
(0x80d+6641-0x21fe)||longcpid.length()==(0x1054+4502-0x21ea)){printf(
"\x4e\x55\x4c\x4c\x20\x43\x70\x69\x64\x20\x72\x65\x63\x65\x69\x76\x65\x64" "\r\n"
);return false;}if(longcpid.length()<(0x39f+6435-0x1cbd))return false;
bool compared=CompareCPID(cpid1,longcpid,blockhash);return compared;}
