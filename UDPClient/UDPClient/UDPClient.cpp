#include "../../WindowsSocket/WindowsSocket/initsock.h"
#include <iostream>

using namespace std;

CInitSock initSock;

int main()
{
	SOCKET s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(4567);
	addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");

	char szText[] = "UDP Server Demo!\r\n";
	::sendto(s, szText, strlen(szText), 0, (sockaddr*)&addr, sizeof(addr));
	return 0;
}