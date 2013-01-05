char  (__fastcall *oldfunc)(void* pthis,int dummy,const unsigned char *key, int keyType, int direction)=(char  (__fastcall *)(void* pthis,int dummy,const unsigned char *key, int keyType, int direction))0x82E556;

int (__fastcall  *oldfunc7A17EA)(void* pthis,int dummy,uint8_t *dhpublicnumber, int length, int keyType)=
	(int (__fastcall  *)(void* pthis,int dummy,uint8_t *dhpublicnumber, int length, int keyType))0x821B59;

int (__fastcall  *oldfunc5DCFFE)(void* pthis,int dummy,uint8_t *buf, int len, SockAddr* a4)=
	(int (__fastcall *)(void* pthis,int dummy,uint8_t *buf, int len, SockAddr* a4))0x0062BFEF;

int (__fastcall  *oldfunc5DD07D)(void* pthis,int dummy,uint8_t *buf, int len, SockAddr* a4)=
	(int (__fastcall *)(void* pthis,int dummy,uint8_t *buf, int len, SockAddr* a4))0x0062C06E;

int (__fastcall  *oldfunc5DD293)(void* pthis,int dummy,uint8_t *buf, int len, int port, int addressFamily)=
	(int (__fastcall *)(void* pthis,int dummy,uint8_t *buf, int len, int port, int addressFamily))0x0062C284;

char  (__fastcall *oldfillPacketHeader)(void* pthis,int dummy,int a1,int sessionid)=(char  (__fastcall *)(void* pthis,int dummy,int a1,int sessionid))0x0081F8AA;

void  (__fastcall *oldNoSessionProcessInput)(void* pthis,int dummy,SockAddr *addressInfo, int sessionid, int interfaceid)
	=(void  (__fastcall *)(void* pthis,int dummy,SockAddr *addressInfo, int sessionid, int interfaceid))0x00833161;

void  (__fastcall *oldSessionProcessInput)(void* pthis,int dummy,SockAddr *addressInfo, int sessionid, int interfaceid)
	=(void  (__fastcall *)(void* pthis,int dummy,SockAddr *addressInfo, int sessionid, int interfaceid))0x008296CF;
