﻿#define NOMINMAX

#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <detours.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <stdint.h>
#include <algorithm>

#include "mybuffer.h"



#pragma comment( lib, "detours.lib" )
FILE* logfile;

/** 12个月份的缩写 */
const char* monthStr[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };


/**
* @param type 日志的类型，可以是任何字符串，不含双引号
* @param data 日志的内容
*/
static void logToFile(const std::string& type, const std::string& data)
{
	SYSTEMTIME st, lt;
	GetSystemTime(&st);
	GetLocalTime(&lt);
	std::ostringstream oss;
	oss << monthStr[lt.wMonth - 1] << " " << lt.wDay << ", " << lt.wYear << " " << lt.wHour << ":" << lt.wMinute << ":" << lt.wSecond << "." << lt.wMilliseconds;
	oss << " " << type << " " << data << "\n";
	std::string msg = oss.str();
	fwrite(msg.c_str(), 1, msg.length(), logfile);
	fflush(logfile);
}

/**
* 打开日志文件
* @filename
*/
void initLogFile(const char* filename){
	logfile = fopen(filename, "a+");
}

/**
* 关闭日志文件
*/
void closeLogFile(){
	if (logfile != NULL)
		fclose(logfile);
}

class Option{
	uint64_t key_;
	MyData data_;
public:
	Option(uint64_t key, const uint8_t* data, size_t len) :key_(key), data_(data, len){}

	std::string toString(){
		std::ostringstream oss;
		oss << "{key=0x" <<std::hex<< key_ <<std::dec<< ",value=" << data_.toHexString() << "}";
		return oss.str();
	}

	static Option* readOption(const uint8_t*& buff, const uint8_t* end){
		uint32_t optionLen;
		int l = ::readVarLength(buff, &optionLen, end);
		if (l <= 0 || !optionLen) return NULL;
		buff += l;

		const uint8_t* optionEnd = buff + optionLen;
		uint64_t key = 0;
		int size = readVarInt64(buff, &key, optionEnd);
		const uint8_t* valueBegin = buff + size;
		buff = optionEnd;
		return new Option(key, valueBegin, optionEnd - valueBegin);
	}
};

struct ChunkName{
	uint8_t chunkId;
	const char* name;
	void(*toStream)(std::ostream& os, const uint8_t* buff, int length);
};

void printToStream(std::ostream& os, const uint8_t* buff, int length){
	os << hexBuffer(buff, length);
}

void printIHelloChunk(std::ostream& oss, const uint8_t* buff, int length){
	oss << "{";
	const uint8_t* end = buff + length;
	MyData epdArray = MyData::readVarData(buff, end);
	if (epdArray.data()){
		//logToFile("debug","hasEPDArray");
		const uint8_t* rdptr = epdArray.data();
		const uint8_t* end2 = epdArray.end();
		while (true){
			MyData epd = MyData::readVarData(rdptr, end2);
			if (epd.size() == 0) break;
			int epdType = *epd.data();
			oss << " epdType 0x" << hexUINT8(epdType);
			if (epdType = 0x0A)
				oss << " epd:" << std::string(epd.data() + 1, epd.end());
			else
				oss << " epd:" << hexBuffer(epd.data() + 1, epd.size() - 1);
		}
	}
	oss << " tag:" << hexBuffer(buff, end - buff);
	oss << "}";
}

void printRHelloChunk(std::ostream& oss, const uint8_t* buff, int length){
	oss << "{";
	const uint8_t* end = buff + length;
	MyData tag = MyData::readVarData(buff, end);
	MyData cookie = MyData::readVarData(buff, end);
	oss << "tag: " << tag.toHexString() << " cookie:" << cookie.toHexString() << " cert:" << hexBuffer(buff, end - buff);
	oss << "}";
}

std::string printCert(const MyData& data){
	std::ostringstream oss;	
	Option* p = NULL;
	const uint8_t* buff = data.data();
	const uint8_t* end = data.end();	
	while (p = Option::readOption(buff, end)){
		oss << " option:" << p->toString();
		delete p;
	}
	return oss.str();
}

void printIIKeyingChunk(std::ostream& oss, const uint8_t* buff, int length){
	oss << "{";
	const uint8_t* end = buff + length;
	uint32_t sid;
	memcpy(&sid, buff, sizeof(sid));
	buff += sizeof(sid);
	MyData cookie = MyData::readVarData(buff, end);
	MyData cert = MyData::readVarData(buff, end);
	MyData skic = MyData::readVarData(buff, end);
	int sig = *buff;
	oss << "sid:" << sid << " cookie: " << cookie.toHexString() << " cert:[" << printCert(cert) << "] skic:[" << printCert(skic) << "] sig:" << sig;
	oss << "}";
}

void printRIKeyingChunk(std::ostream& oss, const uint8_t* buff, int length){
	oss << "{";
	const uint8_t* end = buff + length;
	uint32_t sid;
	memcpy(&sid, buff, sizeof(sid));
	buff += sizeof(sid);
	MyData skrc = MyData::readVarData(buff, end);
	int sig = *buff;
	oss << "sid:" << sid << " skrc:" << skrc.toHexString() << " sig:" << sig;
	oss << "}";
}



void printDataChunk(std::ostream& oss, const uint8_t* buff, int length){
	oss << "{";
	const uint8_t* end = buff + length;
	uint8_t flag = *buff++;
	uint64_t flowID, sequenceNumber, fsnOffset;
	int size;
	size = readVarInt64(buff, &flowID, end);
	buff += size;
	size = readVarInt64(buff, &sequenceNumber, end);
	buff += size;
	size = readVarInt64(buff, &fsnOffset, end);
	buff += size;
	oss << "flag:0x" << hexUINT8(flag) << ", flowID:" << flowID << ",sequenceNumber:" << sequenceNumber << ",fsnOffset:" << fsnOffset;
	if (flag & 0x80){ //has options		
		Option* p = NULL;
		while (p = Option::readOption(buff,end)){
			oss << " option:" << p->toString();
			delete p;
		}
	}
	oss << " Data:" << hexBuffer(buff, end - buff);
	oss << "}";
}

ChunkName chunkNames[] = { { 0x7f, "Fragment" },
{ 0x30, "IHello", printIHelloChunk },
{ 0xF, "FHello", printToStream },
{ 0x70, "RHello", printRHelloChunk },
{ 0x71, "Redirect", printToStream },
{ 0x79, "CookieChange", printToStream },
{ 0x38, "IIKeying", printIIKeyingChunk },
{ 0x78, "RIKeying", printRIKeyingChunk },
{ 0x1, "Ping", printToStream },
{ 0x41, "Pong", printToStream },
{ 0x10, "UserData", printDataChunk },
{ 0x11, "NextUserData", printToStream },
{ 0x50, "AckBitmap", printToStream },
{ 0x51, "AckRanges", printToStream },
{ 0x18, "BufferProbe", printToStream },
{ 0x5E, "FlowException", printToStream },
{ 0xC, "CloseRequest", printToStream },
{ 0x4C, "CloseAck", printToStream },
};
const size_t chunkTypesCount = sizeof(chunkNames) / sizeof(chunkNames[0]);
static_assert(18 == chunkTypesCount, "chunk types length error");


static std::string jsonArray(const uint8_t* buff, int length){
	std::ostringstream oss;
	oss << "[";
	for (int i = 0; i != length; ++i){
		oss << (uint32_t)buff[i];
		if (i != length - 1)
			oss << ",";
	}
	oss << "]";
	return oss.str();
}


//only used for detours inject
__declspec(dllexport) void __cdecl dummyfunc(void){

}




/**
* 地址信息
*/
class SockAddr{
public:
	int vtable;
	void* unknown1;
	union {
		ADDRESS_FAMILY  sin_family;
		sockaddr_in v4;
		sockaddr_in6 v6;
	};
	int addrlen;
};

static_assert(sizeof(SockAddr) == 0x28, "size error");
std::string sockAddrToString(SockAddr* a4){
	char ipstringbuffer[128];
	DWORD ipstringbufferLength = 128;

	size_t addrlen;
	if (a4->sin_family == AF_INET) addrlen = sizeof(sockaddr_in);
	else if (a4->sin_family == AF_INET6) addrlen = sizeof(sockaddr_in6);
	else throw std::runtime_error("unknown addrtype");
	WSAAddressToStringA((LPSOCKADDR)&a4->v4, addrlen, NULL, ipstringbuffer, &ipstringbufferLength);
	return std::string(ipstringbuffer);
}

struct ListItem
{
	ListItem *next;
	ListItem *prev;
	void *itemptr;
	char flag;
};

struct Data
{
	int *vtable;
	int unknown;
	uint8_t *data;
	int length;
	int pos;
	char flags;
};

struct RtmfpList
{
	int vtable;
	int ref;
	int cap;
	int unknown;
	int size;
	int(__cdecl *onAdd)(int);
	int(__cdecl *onDel)(int);
	ListItem *begin;
	char buf[64];
};

struct RandomNumberGenerator
{
	int vtable;
	int ref;
	void *randomProvider;
};


struct BasicCryptoIdentity
{
	int vtable;
	int ref;
	Data *peerid;
	Data *hexPeerID;
	Data *data3;
	Data *url;
};

struct BasicCryptoCert
{
	int vtable;
	int ref;
	Data cert;
	int len;
	Data *p1;
	int v1;
	int v2;
	int v3;
	int v4;
	int v5;
	int v6;
	char flag;
	char _padding[3];
};

struct SHA256Context
{
	char data[120];
};


struct HMACSHA256Context
{
	int vtable;
	int ref;
	SHA256Context c1;
	SHA256Context c2;
	SHA256Context c3;
};


struct IndexSet
{
	int vtable;
	int ref;
	RtmfpList list;
};



class BasicCryptoKey;

struct BasicCryptoAdapter
{
	int vtable;
	Data *d1;
	Data d2;
	RandomNumberGenerator *rand;
	BasicCryptoKey *key;
	BasicCryptoIdentity id;
	BasicCryptoCert cert;
	int v1;
	bool b1;
	int v2;
	int v3;
	int v4;
	int v5;
	int v6;
};

struct Dictionary
{
	char data[48];
};

struct Set
{
	char data[48];
};

struct InstanceTimerList
{
	char data[64];
};

struct Instance;


struct NoSession
{
	int vtable;
	int ref;
	Instance *instance;
	RtmfpList nosessionItems;
	void processInput(SockAddr *addressInfo, int sessionid, int interfaceid);

};

#include "func_pointers.inc"



std::string payloadToString(const uint8_t* data, const size_t len){
	std::ostringstream oss;
	const uint8_t* end = data + len;
	while (data != end){
		uint8_t chunkId = *data++;
		if (chunkId == 0x00 || chunkId == 0xFF) break;
		uint32_t chunkLen = *data++;
		chunkLen = chunkLen << 8 | *data++;
		auto end = chunkNames + chunkTypesCount;
		auto ret = std::find_if(chunkNames, end, [chunkId](const ChunkName& n){return n.chunkId == chunkId; });
		if (ret != end){
			oss << " " << ret->name << ":";
			ret->toStream(oss, data, chunkLen);
		}
		else
			oss << "unknownChunkType " << chunkId;

		data += chunkLen;
	}
	return oss.str();
}

std::string flagsToString(uint8_t flags){
	std::ostringstream oss;
	int mode = flags & 0x3;
	oss << "mode:" << mode;
	if (flags & (1 << 2)){
		oss << ",TimestampEchoPresent";
	}
	if (flags & (1 << 3)){
		oss << ",TimestampPresent";
	}
	if (flags & (1 << 6)){
		oss << ",TimeCriticalReverse";
	}
	if (flags & (1 << 7)){
		oss << ",TimeCritical";
	}
	return oss.str();
}

struct Instance
{
	int vtable;
	int ref;
	void *rtmfpPlatformAdapter;
	void *rtmpMetadataAdapter;
	BasicCryptoAdapter *basicCryptoAdapter;
	void *p1;
	int v1;
	RtmfpList interfaces;
	RtmfpList sessions;
	Dictionary dic1;
	Dictionary dic2;
	Set s1;
	Dictionary dic3;
	Dictionary dic4;
	InstanceTimerList timers;
	RtmfpList l1;
	NoSession nosession;
	char rand1[64];
	char rand2[32];
	int v2;
	int v3;
	int v4;
	unsigned char flags;
	char gap_345[3];
	int timestamp;
	int timestampEcho;
	char recvbuf[8192];
	char *ptr;
	size_t len;
	int v5;
	int pos;
	char sendbuf[8196];
	size_t v7;
	Data d1;
	int v8;
	void *p2;
	int v9;
	int v10;
	int v11;
	int v12;
	int v13;
	int v14;
	bool b1;
	bool b2;
	char gap_43A2[2];
	int v15;
	bool v16;
	bool v17;
	char gap_43AA[2];
	int v18;
	int fillPacketHeader(int a1, int sessionid){
		std::ostringstream oss;
		oss << "sessionid:" << sessionid << ",flags: " << flagsToString(this->flags)
			<< ",data: " << payloadToString((unsigned char*)this->ptr, this->len);
		std::string msg = oss.str();
		logToFile("createPacket", msg);
		int ret = oldfillPacketHeader(this, 0, a1, sessionid);
		return ret;
	}
};


void NoSession::processInput(SockAddr *addressInfo, int sessionid, int interfaceid){
	std::ostringstream oss;
	oss << "sessionid:" << sessionid << ",addr:" << sockAddrToString(addressInfo) << ",chunks: " << payloadToString((uint8_t*)this->instance->ptr, this->instance->len);
	std::string msg = oss.str();
	logToFile("NoSesionProcessInput", msg);
	oldNoSessionProcessInput(this, 0, addressInfo, sessionid, interfaceid);
}


/**
* CCMEAESContext
*/
class C00B4F258{
public:
	//construct a key. 
	char func007AE1E1(const unsigned char *key, int keyType, int direction){
		size_t keylength;
		if (keyType)
		{
			if (keyType == 1)
			{
				keylength = 192;
			}
			else
			{
				if (keyType != 2){
					//unexpected key type!!!
					return 0;
				}
				keylength = 256;
			}
		}
		else
		{
			keylength = 128;
		}
		keylength = keylength / 8;
		std::ostringstream oss;
		oss << "key: " << hexBuffer(key, keylength) << ",direction:" << direction;
		logToFile("keyinfo", oss.str());
		char ret = oldfunc007AE1E1(this, 0, key, keyType, direction);
		return ret;
	}
};



/*char (__fastcall  *oldfunc7A6807)(void* pthis,int dummy,char *dhpublicnumber, unsigned int length)=
	(char (__fastcall*)(void* pthis,int dummy,char *dhpublicnumber, unsigned int length))0x007A6807;*/

/**
* DiffieHellmanContext::DiffieHellmanContext vtable=00B4C8E8
*/
class DiffieHellmanContext{
public:
	int vtable;
	int ref;
	int unknown1;
	MyBuffer b1;
	MyBuffer b2;
	MyBuffer b3;
	MyBuffer b4;

	/*
	char func7A6807(char *dhpublicnumber, unsigned int length){
	int ret=oldfunc7A6807(this,0,dhpublicnumber,length);
	std::ostringstream oss;
	oss<<"{type: \"dhinfo\",data: {b4:"<<hexBuffer(this->b4.data,this->b4.length)<<"}}";
	std::string msg=oss.str();
	logToFile(msg.c_str());
	return ret;
	}*/
};



/**
* RTMFP::BasicCryptoKey vtable=00B4C820
*/
class BasicCryptoKey{
public:
	int vtable;
	int ref;
	int v1;
	int v2;
	DiffieHellmanContext *info;
	int v4;
	HMACSHA256Context *hmacContext;
	int v6;
	int v7;
	HMACSHA256Context *hmacContext2;
	int v9;
	int v10;
	int writeSSEQ;
	int v12;
	__int64 seq;
	int v15;
	IndexSet *v16;
	Data *initiatorNonce;
	Data *responderNonce;
	uint8_t nearNonce[32];
	uint8_t farNonce[32];


	char func007A17EA(uint8_t *dhpublicnumber, int length, int keyType){
		std::ostringstream oss;
		oss << "dhpublicnumber:" << hexBuffer(dhpublicnumber, length)
			<< ",skic:" << hexBuffer(this->initiatorNonce->data, this->initiatorNonce->length)
			<< ",skrc:" << hexBuffer(this->responderNonce->data, this->responderNonce->length)
			<< ",dhprime:" << hexBuffer(this->info->b1.data, this->info->b1.length)
			<< ",dhprivatekey:" << hexBuffer(this->info->b2.data, this->info->b2.length);
		char ret = oldfunc7A17EA(this, 0, dhpublicnumber, length, keyType);
		oss << ",farNonce:" << hexBuffer(this->farNonce, sizeof(this->farNonce))
			<< ",nearNonce:" << hexBuffer(this->nearNonce, sizeof(this->nearNonce));
		std::string msg = oss.str();
		logToFile("secinfo", msg.c_str());

		return ret;
	}
};

struct SparseArray
{
	char data[48];
};

struct SumList
{
	int vtable;
	int ref;
	int cap;
	int unknown;
	int unknown2;
	int(__cdecl *onAdd)(int);
	int(__cdecl *onDel)(int);
	ListItem *begin;
	char buf[64];
	int unknown3;
	int unknown4;
};


struct Session
{
	int vtable;
	int ref;
	Instance *instance;
	int v1;
	int v2;
	int responderSessionID;
	SockAddr addr;
	int interfaceId;
	int v4;
	int v5;
	int v6;
	int v7;
	int v8;
	int v9;
	int v10;
	int v11;
	int v12;
	int v13;
	int v14;
	int v15;
	int v16;
	int v17;
	int v18;
	int v19;
	int v20;
	int v21;
	int v22;
	int timestamp;
	int timestampEcho;
	int v23;
	int v24;
	Data *epd;
	Data *tag;
	Data *initiatorNonce;
	int v25;
	int v26;
	int v27;
	int v28;
	int v29;
	int v30;
	int v31;
	int v32;
	int v33;
	int v34;
	int v35;
	int v36;
	int v37;
	int v38;
	int v39;
	void *v40;
	int v41;
	int v42;
	int v43;
	RtmfpList list1;
	SparseArray flows;
	Set set1;
	SumList sl;
	RtmfpList lists[8];
	char f1;
	char f2;
	char f3;
	char gap_523[1];
	int vend;

	void processInput(SockAddr *addressInfo, int sessionid, int interfaceid){
		std::ostringstream oss;
		oss << "sessionid:" << sessionid << ",addr:" << sockAddrToString(addressInfo) << ",data: " << payloadToString((unsigned char*)this->instance->ptr, this->instance->len);
		std::string msg = oss.str();
		logToFile("SesionProcessInput", msg);
		oldSessionProcessInput(this, 0, addressInfo, sessionid, interfaceid);
	}

};


void logerror(const char* file, long line, const std::string& msg){
	std::ostringstream oss;
	oss << "error:\"" << msg << "\",file: \"" << file << "\",line: " << line;
	std::string err = oss.str();
	logToFile("error", err.c_str());
}

#define LOG_ERROR(msg) {logerror(__FILE__,__LINE__,msg);}

/**
* 网络管理器。它的构造函数会调用WSAStartup
*/
class C00B0C408{
	int vtable;
	int ref;
	int socket;
public:
	int func5DD293(uint8_t *buf, int len, int port, int addressFamily){
		std::ostringstream oss;
		oss << "socket:" << this->socket << ",port:" << port << ",addressFamily:" << addressFamily << ",data: " << hexBuffer(buf, len);
		std::string msg = oss.str();
		logToFile("send2", msg);
		return oldfunc5DD293(this, 0, buf, len, port, addressFamily);
	}

	int func5DD07D(uint8_t *buf, int len, SockAddr* a4){
		std::ostringstream oss;
		oss << "socket:" << this->socket << ",addr:\"" << sockAddrToString(a4) << "\",data: " << hexBuffer(buf, len);
		logToFile("send", oss.str());
		return oldfunc5DD07D(this, 0, buf, len, a4);
	}
	int func5DCFFE(uint8_t *buf, int len, SockAddr* a4){
		int ret = oldfunc5DCFFE(this, 0, buf, len, a4);
		if (ret > 0){
			std::ostringstream oss;
			oss << "socket:" << this->socket << ",addr:\"" << sockAddrToString(a4) << "\",data: " << hexBuffer(buf, ret);
			logToFile("recv", oss.str());
		}
		return ret;
	}
};


template <typename T>
union CONV{
	void* p;
	T orig;
};

template <typename T>
static void* toPVOID(T t){
	CONV<decltype(t)> c;
	c.orig = t;
	return c.p;
}

static void doRegister(){
	LONG error;
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());




	//记录key
	DetourAttach(&(PVOID &)oldfunc007AE1E1, toPVOID(&C00B4F258::func007AE1E1));

	//计算AES key
	DetourAttach(&(PVOID &)oldfunc7A17EA, toPVOID(&BasicCryptoKey::func007A17EA));	

	DetourAttach(&(PVOID &)oldfillPacketHeader, toPVOID(&Instance::fillPacketHeader));
	DetourAttach(&(PVOID &)oldNoSessionProcessInput, toPVOID(&NoSession::processInput));
	DetourAttach(&(PVOID &)oldSessionProcessInput, toPVOID(&Session::processInput));

	/*//发送局域网UDP广播
	DetourAttach(&(PVOID &)oldfunc5DD293, toPVOID(&C00B0C408::func5DD293));
	//收到UDP包
	DetourAttach(&(PVOID &)oldfunc5DCFFE, toPVOID(&C00B0C408::func5DCFFE));
	//发送UDP包
	DetourAttach(&(PVOID &)oldfunc5DD07D, toPVOID(&C00B0C408::func5DD07D));*/

	error = DetourTransactionCommit();
	if (error == NO_ERROR){
		logToFile("begin", "");
	}
}

static void doUnRegister(){
	LONG error;
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	//DetourDetach( &(PVOID &)oldfunc7A6807,(PVOID)(&(PVOID&) DiffieHellmanContext::func7A6807));
	DetourDetach(&(PVOID &)oldfunc7A17EA, toPVOID(&BasicCryptoKey::func007A17EA));
	DetourDetach(&(PVOID &)oldfunc007AE1E1, toPVOID(&C00B4F258::func007AE1E1));
	DetourDetach(&(PVOID &)oldfillPacketHeader, toPVOID(&Instance::fillPacketHeader));
	DetourDetach(&(PVOID &)oldNoSessionProcessInput, toPVOID(&NoSession::processInput));
	DetourDetach(&(PVOID &)oldSessionProcessInput, toPVOID(&Session::processInput));

	/*	DetourDetach(&(PVOID &)oldfunc5DD293, toPVOID(&C00B0C408::func5DD293));
	DetourDetach(&(PVOID &)oldfunc5DCFFE, toPVOID(&C00B0C408::func5DCFFE));
	DetourDetach(&(PVOID &)oldfunc5DD07D, toPVOID(&C00B0C408::func5DD07D));*/
	error = DetourTransactionCommit();
	logToFile("end", "");
}

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
	)
{

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		initLogFile("flash.log");
		doRegister();
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		doUnRegister();
		closeLogFile();
		break;
	}
	return TRUE;
}

