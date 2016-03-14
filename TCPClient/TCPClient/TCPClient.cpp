#include "../../WindowsSocket/WindowsSocket/initsock.h"
#include <stdio.h>

CInitSock initSock;

int main()
{
	SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == INVALID_SOCKET)
	{
		printf("Failed socket().\n");
		return 0;
	}
	sockaddr_in servAddr;
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(4567);
	servAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	if (::connect(s, (sockaddr*)&servAddr, sizeof(servAddr)) == -1)
	{
		printf("Failed connect().\n");
		return 0;
	}
	char buff[256];
	char szText[] = "Test message.";
	::send(s, szText, sizeof(szText), 0);
	int nRecv = ::recv(s, buff, 256, 0);
	if (nRecv > 0)
	{
		buff[nRecv] = '\0';
		printf("Recieve data: %s\r\n", buff);
	}
	::closesocket(s);
	return 0;
}