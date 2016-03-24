#pragma once
#include <WinSock2.h>
#pragma comment(lib, "WS2_32")

class CInitSock
{//管理 Winsock 库
public:
	CInitSock(BYTE minorVer = 2, BYTE majorVer = 2)
	{//初始化 W32_32.dll
		WSADATA wsaData;
		WORD sockVersion = MAKEWORD(minorVer, majorVer);
		if (::WSAStartup(sockVersion, &wsaData) != 0)
		{
			exit(0);
		}
	}
	~CInitSock()
	{
		::WSACleanup();
	}
};