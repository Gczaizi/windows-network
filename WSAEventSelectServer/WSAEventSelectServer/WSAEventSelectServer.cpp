#include "../../WindowsSocket/WindowsSocket/initsock.h"
#include <iostream>

using namespace std;

CInitSock wsaSock;

typedef struct _SOCKET_OBJ
{//��¼ÿ���ͻ����׽��ֵ���Ϣ
	SOCKET s;	//�׽��־��
	HANDLE event;	//����׽�����������¼�������
	sockaddr_in addrRemote;	//�ͻ��˵�ַ��Ϣ
	_SOCKET_OBJ *pNext;	//ָ����һ�� SOCKET_OBJ ����������һ����
}SOCKET_OBJ, *PSOCKET_OBJ;
typedef struct _THREAD_OBJ
{//��¼ÿ���̵߳���Ϣ
	HANDLE events[WSA_MAXIMUM_WAIT_EVENTS];	//��¼��ǰ�߳�Ҫ�ȴ���ʱ�����ľ��
	int nSocketCount;	//��¼��ǰ�̴߳�����׽��ֵ�����
	PSOCKET_OBJ pSockHeader;	//��ǰ�̴߳�����׽��ֶ����б�ָ���ͷ
	PSOCKET_OBJ pSockTail;	//ָ���β
	CRITICAL_SECTION cs;	//�ؼ�����α�����ͬ���Ա��ṹ�ķ���
	_THREAD_OBJ *pNext;	//ָ����һ�� THREAD_OBJ ����������һ����
}THREAD_OBJ, *PTHREAD_OBJ;
//ȫ�ֱ���
PTHREAD_OBJ g_pThreadList;
CRITICAL_SECTION g_cs;
LONG g_nTotalConnections;
LONG g_nCurrentConnections;
//��������
PSOCKET_OBJ GetSocketObj(SOCKET s);

PSOCKET_OBJ GetSocketObj(SOCKET s)
{
	PSOCKET_OBJ pSocket = (PSOCKET_OBJ)::GlobalAlloc(GPTR, sizeof(SOCKET_OBJ));
	if (pSocket != NULL)
	{
		pSocket->s = s;
		pSocket->event = ::WSACreateEvent();
	}
	return pSocket;
}

void FreeSocketObj(PSOCKET_OBJ pSocket)
{
	::CloseHandle(pSocket->event);
	if (pSocket->s != INVALID_SOCKET)
	{
		::closesocket(pSocket->s);
	}
	::GlobalFree(pSocket);
}

PTHREAD_OBJ GetThreadObj()	//����һ���̶߳��󣬳�ʼ�����ĳ�Ա����������ӵ��̶߳����б���
{
	PTHREAD_OBJ pThread = (PTHREAD_OBJ)::GlobalAlloc(GPTR, sizeof(THREAD_OBJ));
	if (pThread != NULL)
	{
		::InitializeCriticalSection(&pThread->cs);
		//����һ���¼���������ָʾ���̵߳ľ��������Ҫ�ؽ�
		pThread->events[0] = ::WSACreateEvent();
		//����������̶߳�����ӵ��б���
		::EnterCriticalSection(&g_cs);
		pThread->pNext = g_pThreadList;
		g_pThreadList = pThread;
		::LeaveCriticalSection(&g_cs);
	}
	return pThread;
}

void FreeThreadObj(PTHREAD_OBJ pThread)
{
	::EnterCriticalSection(&g_cs);
	PTHREAD_OBJ p = g_pThreadList;
	if (p == pThread)
	{//pThread �ǵ�һ��
		g_pThreadList = p->pNext;
	}
	else
	{
		while (p != NULL&&p->pNext != pThread)
		{
			p = p->pNext;
		}
		if (p != NULL)
		{//��ʱ��p �� pThread ��ǰһ����p->next == pThread
			p->pNext = pThread->pNext;	//ȥ�� pThread
		}
	}
	::LeaveCriticalSection(&g_cs);
	::CloseHandle(pThread->events[0]);
	::DeleteCriticalSection(&pThread->cs);
	::GlobalFree(pThread);
}

BOOL InsertSocketObj(PTHREAD_OBJ pThread, PSOCKET_OBJ pSocket)
{//��һ���̵߳��׽����б��в���һ���׽���
	BOOL bRet = FALSE;
	::EnterCriticalSection(&pThread->cs);
	if (pThread->nSocketCount < WSA_MAXIMUM_WAIT_EVENTS - 1)
	{
		if (pThread->pSockHeader == NULL)
		{
			pThread->pSockHeader = pThread->pSockTail;
		}
		else
		{
			pThread->pSockTail->pNext = pSocket;
			pThread->pSockTail = pSocket;
		}
		pThread->nSocketCount++;
		bRet = TRUE;
	}
	::LeaveCriticalSection(&g_cs);
	//����ɹ���˵���ɹ������˿ͻ��˵���������
	if (bRet)
	{
		::InterlockedIncrement(&g_nTotalConnections);
		::InterlockedIncrement(&g_nCurrentConnections);
	}
	return bRet;
}

void RebuildArray(PTHREAD_OBJ pThread)
{
	::EnterCriticalSection(&pThread->cs);
	PSOCKET_OBJ pSocket = pThread->pSockHeader;
	int n = 1;
	while (pSocket != NULL)
	{
		pThread->events[n++] = pSocket->event;
		pSocket = pSocket->pNext;
	}
	::LeaveCriticalSection(&pThread->cs);
}

PSOCKET_OBJ FindSocketObj(PTHREAD_OBJ pThread, int nIndex)
{
	PSOCKET_OBJ pSocket = pThread->pSockHeader;
	while (--nIndex)
	{
		if (pSocket == NULL)
			return NULL;
		pSocket = pSocket->pNext;
	}
	return pSocket;
}

void RemoveSocketObj(PTHREAD_OBJ pThread, PSOCKET_OBJ pSocket)
{
	::EnterCriticalSection(&pThread->cs);
	//���׽��ֶ����б��в���ָ�����׽��ֶ����ҵ����Ƴ�
	PSOCKET_OBJ pTest = pThread->pSockHeader;	//�ӵ�һ����ʼѰ��
	if (pTest == pSocket)
	{
		if (pThread->pSockHeader == pThread->pSockTail)
			pThread->pSockTail = pThread->pSockHeader = pTest->pNext;
		else
			pThread->pSockHeader = pTest->pNext;
	}
	else
	{
		while (pTest != NULL&&pTest->pNext != pSocket)
			pTest = pTest->pNext;
		if (pTest != NULL)
		{
			if (pThread->pSockTail == pSocket)
				pThread->pSockTail = pTest;
			pTest->pNext = pSocket->pNext;
		}
	}
	pThread->nSocketCount--;
	::LeaveCriticalSection(&pThread->cs);

	::WSASetEvent(pThread->events[0]);
	::InterlockedDecrement(&g_nCurrentConnections);
}

BOOL HandleIO(PTHREAD_OBJ pThread, PSOCKET_OBJ pSocket)
{
	WSANETWORKEVENTS event;
	::WSAEnumNetworkEvents(pSocket->s, pSocket->event, &event);
	do
	{
		if (event.lNetworkEvents & FD_READ)
		{
			if (event.iErrorCode[FD_READ_BIT] == 0)
			{
				char szText[256];
				int nRecv = ::recv(pSocket->s, szText, sizeof(szText), 0);
				if (nRecv > 0)
				{
					szText[nRecv] = '\0';
					cout << "Recieve Data: " << szText << endl;
				}
			}
			else
				break;
		}
		else if (event.lNetworkEvents & FD_CLOSE)
			break;
		else if (event.lNetworkEvents & FD_WRITE)
		{
			if (event.iErrorCode[FD_WRITE_BIT] == 0)
			{	}
			else
				break;
		}
		return TRUE;
	} while (FALSE);
	//�׽��ֹرգ����߳����д�����������ת��������ִ��
	RemoveSocketObj(pThread, pSocket);
	FreeSocketObj(pSocket);
	return FALSE;
}

DWORD WINAPI ServerThread(LPVOID lpParam)
{	
	//ȡ�ñ��̶߳����ָ��
	PTHREAD_OBJ pThread = (PTHREAD_OBJ)lpParam;
	while (TRUE)
	{	
		//�ȴ������¼�
		int nIndex = ::WSAWaitForMultipleEvents(pThread->nSocketCount + 1,
			pThread->events, FALSE, WSA_INFINITE, FALSE);
		nIndex = nIndex - WSA_WAIT_EVENT_0;
		//�鿴���ŵ��¼�����
		for (int i = nIndex; i < pThread->nSocketCount + 1; i++)
		{
			nIndex = ::WSAWaitForMultipleEvents(1, &pThread->events[i], TRUE, 1000, FALSE);
			if (nIndex == WSA_WAIT_FAILED || nIndex == WSA_WAIT_TIMEOUT)
			{	continue;	}
			else
			{
				if (i == 0)
				{//events[0] ���ţ��ؽ�����
					RebuildArray(pThread);
					if (pThread->nSocketCount == 0)
					{//���û�пͻ� I/O ��Ҫ�������˳��߳�
						FreeThreadObj(pThread);
						return 0;
					}
					::WSAResetEvent(pThread->events[0]);
				}
				else
				{//���������¼�
					PSOCKET_OBJ pSocket = (PSOCKET_OBJ)FindSocketObj(pThread, i);
					if (pSocket != NULL)
					{
						if (!HandleIO(pThread, pSocket))
							RebuildArray(pThread);
					}
					else
						cout << "Unable to find socket object." << endl;
				}
			}
		}
	}
	return 0;
}

void AssignToFreeThread(PSOCKET_OBJ pSocket)
{
	pSocket->pNext = NULL;
	::EnterCriticalSection(&g_cs);
	PTHREAD_OBJ pThread = g_pThreadList;
	//��ͼ���뵽�ִ��߳�
	while (TRUE)
	{
		if (InsertSocketObj(pThread, pSocket))
			break;
		pThread = pThread->pNext;
	}
	//û�п����̣߳�Ϊ����׽��ִ����µ��߳�
	if (pThread == NULL)
	{
		pThread = GetThreadObj();
		InsertSocketObj(pThread, pSocket);
		::CreateThread(NULL, 0, ServerThread, pThread, 0, NULL);
	}
	::LeaveCriticalSection(&g_cs);
	//ָʾ�߳��ؽ��������
	::WSASetEvent(pThread->events[0]);
}

int main()
{
	USHORT nPort = 4567;
	//���������׽���
	SOCKET sListen = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = nPort;
	sin.sin_addr.S_un.S_addr = INADDR_ANY;

	if (::bind(sListen, (sockaddr*)&sin, sizeof(sin)) == SOCKET_ERROR)
	{
		cout << "Failed bind()." << endl;
		return 0;
	}
	::listen(sListen, 200);
	//�����¼����󣬲������������׽���
	WSAEVENT event = ::WSACreateEvent();
	::WSAEventSelect(sListen, event, FD_ACCEPT | FD_CLOSE);
	::InitializeCriticalSection(&g_cs);
	//����ͻ����������󣬴�ӡ״̬��Ϣ
	while (TRUE)
	{
		int nRet = ::WaitForSingleObject(event, 5 * 1000);
		if (nRet == WAIT_FAILED)
		{
			cout << "Failed WaitForSingleObject()." << endl;
			break;
		}
		else if (nRet == WSA_WAIT_TIMEOUT)
		{
			cout << endl;
			cout << "TotalConnections: " << g_nTotalConnections << endl;
			cout << "CurrentConnections: " << g_nCurrentConnections << endl;
			continue;
		}
		else
		{
			::ResetEvent(event);
			//ѭ����������δ������������
			while (TRUE)
			{
				sockaddr_in si;
				int nLen = sizeof(si);
				SOCKET sNew = ::accept(sListen, (sockaddr*)&si, &nLen);
				if (sNew == SOCKET_ERROR)
					break;
				PSOCKET_OBJ pSocket = GetSocketObj(sNew);
				pSocket->addrRemote = si;
				::WSAEventSelect(pSocket->s, pSocket->event, FD_READ | FD_CLOSE | FD_WRITE);
				AssignToFreeThread(pSocket);
			}
		}
	}
	::DeleteCriticalSection(&g_cs);
	return 0;
}