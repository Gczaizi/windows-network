#include "../../WindowsSocket/WindowsSocket/initsock.h"
#include <iostream>

using namespace std;

CInitSock initSock;

int main()
{
	SOCKET s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == INVALID_SOCKET)
	{
		cout << "Failed socket()." << endl;
		return 0;
	}
	
	sockaddr_in	sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(4567);
	sin.sin_addr.S_un.S_addr = INADDR_ANY;

	if (::bind(s, (LPSOCKADDR)&sin, sizeof(sin)) == SOCKET_ERROR)
	{
		cout << "Failed bind()." << endl;
		return 0;
	}

	char buff[1024];
	sockaddr_in addr;
	int nLen = sizeof(addr);
	while (TRUE)
	{
		int nRecv = ::recvfrom(s, buff, 1024, 0, (sockaddr*)&addr, &nLen);
		if (nRecv > 0)
		{
			buff[nRecv] = '\0';
			cout << "Recieve Data: " << ::inet_ntoa(addr.sin_addr) << buff << endl;
		}
	}
	::closesocket(s);
}