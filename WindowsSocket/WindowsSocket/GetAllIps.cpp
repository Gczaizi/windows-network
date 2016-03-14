#include "initsock.h"
#include <stdio.h>
#include "../../WindowsSock/WindowsSock/LocalHostInfo.h"

CInitSock initSock;	//初始化 WinSock 库

void main()
{
	char szHost[26];
	::gethostname(szHost, 256);	//取得本地主机名
	//printf(szHost);
	hostent *pHost = ::gethostbyname(szHost);	//通过主机名得到地址信息
	in_addr addr;
	for (int i = 0; ; i++)
	{//打印出所有地址
		char *p = pHost->h_addr_list[i];	//p 指向一个 32 位的 IP 地址
		if (p == NULL)
			break;
		memcpy(&addr.S_un.S_addr, p, pHost->h_length);
		char *szIp = ::inet_ntoa(addr);
		printf("ip %s \n", szIp);
	}

	GetGlobalData();
}