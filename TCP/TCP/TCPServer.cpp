#include "../../WindowsSocket/WindowsSocket/initsock.h"
#include <stdio.h>

CInitSock initSock;

int main()
{
	SOCKET sListen = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sListen == INVALID_SOCKET)
	{
		printf("Failed socket().\n");
		return 0;
	}
	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(4567);
	sin.sin_addr.S_un.S_addr = INADDR_ANY;
	if (::bind(sListen,(LPSOCKADDR)&sin,sizeof(sin)) == SOCKET_ERROR)
	{
		printf("Failed bind().\n");
		return 0;
	}
	if (::listen(sListen, 2) == SOCKET_ERROR)
	{
		printf("Failed listen*(.\n");
		return 0;
	}
	sockaddr_in remoteAddr;
	int nAddrLen = sizeof(remoteAddr);
	SOCKET sClient;
	char szText[] = "TCP Server Demo!\r\n";
	while (TRUE)
	{
		sClient = ::accept(sListen, (SOCKADDR*)&remoteAddr, &nAddrLen);
		if (sClient == INVALID_SOCKET)
		{
			printf("Failed accept().\n");
			continue;
		}
		printf("Recieve Client: %s\r\n", inet_ntoa(remoteAddr.sin_addr));
		::send(sClient, szText, strlen(szText), 0);
		::closesocket(sClient);
	}
	::closesocket(sListen);
	return 0;
}