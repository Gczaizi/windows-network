#include <Windows.h>
#include <stdio.h>
#include <IPHlpApi.h>
#pragma comment(lib, "IPHlpApi.lib")
#pragma comment(lib, "WS2_32.lib")

//全局数据
u_char g_ucLocalMac[6];	//本地 MAC 地址
DWORD g_dwGatewayIP;	//网关 IP 地址
DWORD g_dwLocalIP;		//本地 IP 地址
DWORD g_dwMask;			//子网掩码

BOOL GetGlobalData()
{
	PIP_ADAPTER_INFO pAdapterInfo = NULL;
	ULONG ulLen = 0;

	::GetAdaptersInfo(pAdapterInfo, &ulLen);
	pAdapterInfo = (PIP_ADAPTER_INFO)::GlobalAlloc(GPTR, ulLen);//为适配器申请内存

	if (::GetAdaptersInfo(pAdapterInfo, &ulLen) == ERROR_SUCCESS)
	{//取得本地适配器结果信息，函数调用成功返回 ERROE_SUCCESS
		if (pAdapterInfo != NULL)
		{
			memcpy(g_ucLocalMac, pAdapterInfo->Address, 6);
			g_dwGatewayIP = ::inet_addr(pAdapterInfo->GatewayList.IpAddress.String);
			g_dwLocalIP = ::inet_addr(pAdapterInfo->IpAddressList.IpAddress.String);
			g_dwMask = ::inet_addr(pAdapterInfo->IpAddressList.IpMask.String);
		}
	}
	printf("\n----------本地主机信息----------\n");
	in_addr in;
	in.S_un.S_addr = g_dwLocalIP;
	printf("IP: %s\n", ::inet_ntoa(in));
	in.S_un.S_addr = g_dwMask;
	printf("Mask: %s\n", ::inet_ntoa(in));
	in.S_un.S_addr = g_dwGatewayIP;
	printf("Gateway: %s\n", ::inet_ntoa(in));
	u_char *p = g_ucLocalMac;
	printf("MAC: %02X%02X%02X%02X%02X%02X\n", p[0], p[1], p[2], p[3], p[4], p[5]);

	return TRUE;
}

void main()
{
	GetGlobalData();
}