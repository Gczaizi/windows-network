#include "initsock.h"
#include <stdio.h>
#include "../../WindowsSock/WindowsSock/LocalHostInfo.h"

CInitSock initSock;	//��ʼ�� WinSock ��

void main()
{
	char szHost[26];
	::gethostname(szHost, 256);	//ȡ�ñ���������
	//printf(szHost);
	hostent *pHost = ::gethostbyname(szHost);	//ͨ���������õ���ַ��Ϣ
	in_addr addr;
	for (int i = 0; ; i++)
	{//��ӡ�����е�ַ
		char *p = pHost->h_addr_list[i];	//p ָ��һ�� 32 λ�� IP ��ַ
		if (p == NULL)
			break;
		memcpy(&addr.S_un.S_addr, p, pHost->h_length);
		char *szIp = ::inet_ntoa(addr);
		printf("ip %s \n", szIp);
	}

	GetGlobalData();
}