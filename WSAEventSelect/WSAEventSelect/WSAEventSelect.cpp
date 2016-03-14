#include <iostream>
#include "../../WindowsSocket/WindowsSocket/initsock.h"

using namespace std;

CInitSock wsaSock;

int main()
{
	//�¼�������׽��־������
	WSAEVENT eventArray[WSA_MAXIMUM_WAIT_EVENTS];
	SOCKET sockArray[WSA_MAXIMUM_WAIT_EVENTS];
	int nEventTotal = 0;
	USHORT nPort = 4567;
	//���������׽���
	SOCKET sListen = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(nPort);
	sin.sin_addr.S_un.S_addr = INADDR_ANY;
	if (::bind(sListen, (sockaddr*)&sin, sizeof(sin)) == SOCKET_ERROR)
	{
		cout << "Failed bind()." << endl;
		return 0;
	}
	::listen(sListen, 5);
	//����ʱ����󣬲��������µ��׽���
	WSAEVENT event = ::WSACreateEvent();
	::WSAEventSelect(sListen, event, FD_ACCEPT | FD_CLOSE);
	//��ӵ�ʱ�¼�������׽��־������
	eventArray[nEventTotal] = event;
	sockArray[nEventTotal] = sListen;
	nEventTotal++;
	//���������¼�
	while (TRUE)
	{
		//�������¼��ϵȴ�
		int nIndex = ::WSAWaitForMultipleEvents(nEventTotal, eventArray, FALSE, WSA_INFINITE, FALSE);
		//��ÿ���¼����� WSAWaitForMultipleEvents �������Ա�ȷ������״̬
		nIndex = nIndex - WSA_WAIT_EVENT_0;
		for (int i = nIndex; i < nEventTotal; i++)
		{
			nIndex = ::WSAWaitForMultipleEvents(1, &eventArray[i], TRUE, 1000, FALSE);
			if (nIndex == WSA_WAIT_FAILED || nIndex == WSA_WAIT_TIMEOUT)
			{
				continue;
			}
			else
			{	//��ȡ������֪ͨ��Ϣ��WSAEnumNetworkEvents �������Զ����������¼�
				WSANETWORKEVENTS event;
				::WSAEnumNetworkEvents(sockArray[i], eventArray[i], &event);
				if (event.lNetworkEvents & FD_ACCEPT)
				{//���� FD_ACCEPT ֪ͨ��Ϣ
					if (event.iErrorCode[FD_ACCEPT_BIT] == 0)
					{
						if (nEventTotal > WSA_MAXIMUM_WAIT_EVENTS)
						{
							cout << "Too many connections!" << endl;
							continue;
						}
						SOCKET sNew = ::accept(sockArray[i], NULL, NULL);
						WSAEVENT event = ::WSACreateEvent();
						::WSAEventSelect(sNew, event, FD_READ | FD_CLOSE | FD_WRITE);
						//��ӵ�����������
						eventArray[nEventTotal] = event;
						sockArray[nEventTotal] = sNew;
						nEventTotal++;
					}
				}
				else if (event.lNetworkEvents & FD_READ)
				{
					if (event.iErrorCode[FD_READ_BIT] == 0)
					{
						char szText[26];
						int nRecv = ::recv(sockArray[i], szText, strlen(szText), 0);
						if (nRecv > 0)
						{
							szText[nRecv] = '\0';
							cout << "Recieve Data: " << szText << endl;
						}
					}
				}
				else if (event.lNetworkEvents & FD_CLOSE)
				{
					if (event.iErrorCode[FD_CLOSE_BIT] == 0)
					{
						::closesocket(sockArray[i]);
						for (int j = i; j < nEventTotal; j++)
						{
							sockArray[j] = sockArray[j + 1];
							eventArray[j] = eventArray[j + 1];
						}
						nEventTotal--;
					}
				}
				else if (event.lNetworkEvents & FD_WRITE)
				{ }
			}
		}
	}
	return 0;
}