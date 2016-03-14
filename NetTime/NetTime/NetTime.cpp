#include <iostream>
//#include <Windows.h>
#include <stdlib.h>
#include "../../WindowsSocket/WindowsSocket/initsock.h"

using namespace std;

CInitSock initSock;

void SetTimeFromTCP(ULONG ulTime)
{
	FILETIME ft;
	SYSTEMTIME st;

	st.wYear = 1900;
	st.wMonth = 1;
	st.wDay = 1;
	st.wHour = 0;
	st.wMinute = 0;
	st.wSecond = 0;
	st.wMilliseconds = 0;

	SystemTimeToFileTime(&st, &ft);
	LONGLONG *pLLong = (LONGLONG*)&ft;
	*pLLong += (LONGLONG)10000000 * ulTime;
	FileTimeToSystemTime(&ft, &st);
	SetSystemTime(&st);
}

int main()
{
	SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == INVALID_SOCKET)
	{
		cout << "Failed socket()." << endl;
		return 0;
	}
	sockaddr_in servAddr;
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(37);
	servAddr.sin_addr.S_un.S_addr = inet_addr("210.72.145.44");
	if (::connect(s, (sockaddr*)&servAddr, sizeof(servAddr)) == -1)
	{
		cout << "Failed connect()." << endl;
		return 0;
	}
	ULONG ulTime = 0;
	int nRecv = ::recv(s, (char*)&ulTime, sizeof(ulTime), 0);
	if (nRecv > 0)
	{
		ulTime = ntohl(ulTime);
		SetTimeFromTCP(ulTime);
		cout << "Syn Success!" << endl;
	}
	else
	{
		cout << "Syn Failed!" << endl;
	}
	::closesocket(s);
	//system("pause");
	getchar();
	//Sleep(10000);
	return 0;
}