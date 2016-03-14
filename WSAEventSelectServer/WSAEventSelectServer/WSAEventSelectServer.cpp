#include "../../WindowsSocket/WindowsSocket/initsock.h"
#include <iostream>

using namespace std;

CInitSock wsaSock;

typedef struct _SOCKET_OBJ
{//记录每个客户端套接字的信息
	SOCKET s;	//套接字句柄
	HANDLE event;	//与此套接字相关联的事件对象句柄
	sockaddr_in addrRemote;	//客户端地址信息
	_SOCKET_OBJ *pNext;	//指向下一个 SOCKET_OBJ 对象，以连成一个表
}SOCKET_OBJ, *PSOCKET_OBJ;
typedef struct _THREAD_OBJ
{//记录每个线程的信息
	HANDLE events[WSA_MAXIMUM_WAIT_EVENTS];	//记录当前线程要等待的时间对象的句柄
	int nSocketCount;	//记录当前线程处理的套接字的数量
	PSOCKET_OBJ pSockHeader;	//当前线程处理的套接字对象列表，指向表头
	PSOCKET_OBJ pSockTail;	//指向表尾
	CRITICAL_SECTION cs;	//关键代码段变量，同步对本结构的访问
	_THREAD_OBJ *pNext;	//指向下一个 THREAD_OBJ 对象，以连成一个表
}THREAD_OBJ, *PTHREAD_OBJ;
//全局变量
PTHREAD_OBJ g_pThreadList;
CRITICAL_SECTION g_cs;
LONG g_nTotalConnections;
LONG g_nCurrentConnections;
//函数声明
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

PTHREAD_OBJ GetThreadObj()	//申请一个线程对象，初始化它的成员，并将它添加到线程对象列表中
{
	PTHREAD_OBJ pThread = (PTHREAD_OBJ)::GlobalAlloc(GPTR, sizeof(THREAD_OBJ));
	if (pThread != NULL)
	{
		::InitializeCriticalSection(&pThread->cs);
		//创建一个事件对象，用于指示该线程的句柄数组需要重建
		pThread->events[0] = ::WSACreateEvent();
		//将新申请的线程对象添加到列表中
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
	{//pThread 是第一个
		g_pThreadList = p->pNext;
	}
	else
	{
		while (p != NULL&&p->pNext != pThread)
		{
			p = p->pNext;
		}
		if (p != NULL)
		{//此时，p 是 pThread 的前一个，p->next == pThread
			p->pNext = pThread->pNext;	//去掉 pThread
		}
	}
	::LeaveCriticalSection(&g_cs);
	::CloseHandle(pThread->events[0]);
	::DeleteCriticalSection(&pThread->cs);
	::GlobalFree(pThread);
}

BOOL InsertSocketObj(PTHREAD_OBJ pThread, PSOCKET_OBJ pSocket)
{//向一个线程的套接字列表中插入一个套接字
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
	//插入成功，说明成功处理了客户端的连接请求
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
	//在套接字对象列表中查找指定的套接字对象，找到后移除
	PSOCKET_OBJ pTest = pThread->pSockHeader;	//从第一个开始寻找
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
	//套接字关闭，或者程序有错误发生，都会转到这里来执行
	RemoveSocketObj(pThread, pSocket);
	FreeSocketObj(pSocket);
	return FALSE;
}

DWORD WINAPI ServerThread(LPVOID lpParam)
{	
	//取得本线程对象的指针
	PTHREAD_OBJ pThread = (PTHREAD_OBJ)lpParam;
	while (TRUE)
	{	
		//等待网络事件
		int nIndex = ::WSAWaitForMultipleEvents(pThread->nSocketCount + 1,
			pThread->events, FALSE, WSA_INFINITE, FALSE);
		nIndex = nIndex - WSA_WAIT_EVENT_0;
		//查看受信的事件对象
		for (int i = nIndex; i < pThread->nSocketCount + 1; i++)
		{
			nIndex = ::WSAWaitForMultipleEvents(1, &pThread->events[i], TRUE, 1000, FALSE);
			if (nIndex == WSA_WAIT_FAILED || nIndex == WSA_WAIT_TIMEOUT)
			{	continue;	}
			else
			{
				if (i == 0)
				{//events[0] 受信，重建数组
					RebuildArray(pThread);
					if (pThread->nSocketCount == 0)
					{//如果没有客户 I/O 需要处理，则退出线程
						FreeThreadObj(pThread);
						return 0;
					}
					::WSAResetEvent(pThread->events[0]);
				}
				else
				{//处理网络事件
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
	//试图插入到现存线程
	while (TRUE)
	{
		if (InsertSocketObj(pThread, pSocket))
			break;
		pThread = pThread->pNext;
	}
	//没有空闲线程，为这个套接字创建新的线程
	if (pThread == NULL)
	{
		pThread = GetThreadObj();
		InsertSocketObj(pThread, pSocket);
		::CreateThread(NULL, 0, ServerThread, pThread, 0, NULL);
	}
	::LeaveCriticalSection(&g_cs);
	//指示线程重建句柄数组
	::WSASetEvent(pThread->events[0]);
}

int main()
{
	USHORT nPort = 4567;
	//创建监听套接字
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
	//创建事件对象，并关联到监听套接字
	WSAEVENT event = ::WSACreateEvent();
	::WSAEventSelect(sListen, event, FD_ACCEPT | FD_CLOSE);
	::InitializeCriticalSection(&g_cs);
	//处理客户端连接请求，打印状态信息
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
			//循环处理所有未决的连接请求
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