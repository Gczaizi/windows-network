#include "../../WindowsSocket/WindowsSocket/initsock.h"
#include <iostream>
#include <Mswsock.h>
#include <Windows.h>
#define BUFFER_SIZE 1024

using namespace std;

CInitSock ioSock;

typedef struct _SOCKET_OBJ
{
	SOCKET s;	//套接字句柄
	int nOutstandingOps;	//记录此套接字上的重叠 I/O 数量
	LPFN_ACCEPTEX lpfnAcceptEx;	//扩展函数 AcceptEx 的指针
}SOCKET_OBJ, *PSOCKET_OBJ;

typedef struct _BUFFER_OBJ
{
	OVERLAPPED ol;	//重叠结构
	char *buff;	//send/recv/accept 所使用的缓冲区
	int nLen;	//buff 的长度
	PSOCKET_OBJ pSocket;	//此 I/O 所属的套接字
	int nOperation;	//提交的操作类型
#define OP_ACCEPT 1
#define OP_READ 2
#define OP_WRITE 3
	SOCKET sAccept;	//用来保存 AcceptEx 接收的客户端套接字
	_BUFFER_OBJ *pNext;
}BUFFER_OBJ, *PBUFFER_OBJ;
//全局变量
HANDLE g_events[WSA_MAXIMUM_WAIT_EVENTS];	//I/O 事件句柄数组
int g_nBufferCount;	//上面数组中有效句柄数量
PBUFFER_OBJ g_pBufferHead, g_pBufferTail;	//记录缓冲区对象组成的表的地址

PSOCKET_OBJ GetSocketObj(SOCKET s)
{
	PSOCKET_OBJ pSocket = (PSOCKET_OBJ)::GlobalAlloc(GPTR, sizeof(SOCKET_OBJ));
	if (pSocket != NULL)
		pSocket->s = s;
	return pSocket;
}

void FreeSocketObj(PSOCKET_OBJ pSocket)
{
	if (pSocket->s != INVALID_SOCKET)
		::closesocket(pSocket->s);
	::GlobalFree(pSocket);
}

PBUFFER_OBJ GetBufferObj(PSOCKET_OBJ pSocket, ULONG nLen)
{
	if (g_nBufferCount > WSA_MAXIMUM_WAIT_EVENTS - 1)
		return NULL;
	PBUFFER_OBJ pBuffer = (PBUFFER_OBJ)::GlobalAlloc(GPTR, sizeof(BUFFER_OBJ));
	if (pBuffer != NULL)
	{
		pBuffer->buff = (char*)::GlobalAlloc(GPTR, nLen);
		pBuffer->ol.hEvent = ::WSACreateEvent();
		pBuffer->pSocket = pSocket;
		pBuffer->sAccept = INVALID_SOCKET;
		//将新的 BUFFER_OBJ 添加到列表中
		if (g_pBufferHead == NULL)
			g_pBufferHead = g_pBufferTail = pBuffer;
		else
		{
			g_pBufferTail->pNext = pBuffer;
			g_pBufferTail = pBuffer;
		}
		g_events[g_nBufferCount] = pBuffer->ol.hEvent;
		g_nBufferCount++;
	}
	return pBuffer;
}

void FreeBufferObj(PBUFFER_OBJ pBuffer)
{
	PBUFFER_OBJ pTest = g_pBufferHead;
	BOOL bFind = FALSE;
	if (pTest == pBuffer)
	{//头结点后移一位，若只有一个结点，则列表变为空
		g_pBufferHead = g_pBufferHead->pNext;
		bFind = TRUE;
	}
	else
	{
		while (pTest != NULL&&pTest->pNext != pBuffer)
			pTest = pTest->pNext;
		if (pTest != NULL)
		{//找到 pBuffer，从列表中删除
			pTest->pNext = pBuffer->pNext;
			if (pTest->pNext == NULL)
				g_pBufferTail = pTest;
			bFind = TRUE;
		}
	}
	//释放它占用的内存空间
	if (bFind)
	{
		g_nBufferCount--;
		::CloseHandle(pBuffer->ol.hEvent);
		::GlobalFree(pBuffer->buff);
		::GlobalFree(pBuffer);
	}
}

PBUFFER_OBJ FindBufferObj(HANDLE hEvent)
{//根据受信对象句柄找到对应的 BUFFER_OBJ
	PBUFFER_OBJ pBuffer = g_pBufferHead;
	while (pBuffer != NULL)
	{
		if (pBuffer->ol.hEvent == hEvent)
			break;
		pBuffer = pBuffer->pNext;
	}
	return pBuffer;
}

void RebuildArray()
{//更新事件句柄数组 g_events
	PBUFFER_OBJ pBuffer = g_pBufferHead;
	int i = 1;
	while (pBuffer != NULL)
	{
		g_events[i] = pBuffer->ol.hEvent;
		pBuffer = pBuffer->pNext;
		i++;
	}
}

BOOL PostAccept(PBUFFER_OBJ pBuffer)
{
	PSOCKET_OBJ pSocket = pBuffer->pSocket;
	if (pSocket->lpfnAcceptEx != NULL)
	{
		//设置 I/O 类型，增加套接字上的重叠 I/O 计数
		pBuffer->nOperation = OP_ACCEPT;
		pSocket->nOutstandingOps++;
		//投递此重叠 I/O
		DWORD dwBytes;
		pBuffer->sAccept = ::WSASocket(AF_INET, SOCK_STREAM, 0, 
			NULL, 0, WSA_FLAG_OVERLAPPED);
		BOOL b = pSocket->lpfnAcceptEx(pSocket->s,
			pBuffer->sAccept,
			pBuffer->buff,
			BUFFER_SIZE - ((sizeof(sockaddr_in) + 16) * 2),
			sizeof(sockaddr_in) + 16,
			sizeof(sockaddr_in) + 16,
			&dwBytes,
			&pBuffer->ol);
		if (!b)
		{
			if (::WSAGetLastError() != WSA_IO_PENDING)
				return FALSE;
		}
		return TRUE;
	}
	return FALSE;
}

BOOL PostRecv(PBUFFER_OBJ pBuffer)
{
	pBuffer->nOperation = OP_READ;
	pBuffer->pSocket->nOutstandingOps++;

	DWORD dwBytes;
	DWORD dwFlags = 0;
	WSABUF buf;
	buf.buf = pBuffer->buff;
	buf.len = pBuffer->nLen;
	if (::WSARecv(pBuffer->pSocket->s, &buf, 1,
		&dwBytes, &dwFlags, &pBuffer->ol, NULL) != NO_ERROR)
	{
		if (::WSAGetLastError() != WSA_IO_PENDING)
			return FALSE;
	}
	return TRUE;
}

BOOL PostSend(PBUFFER_OBJ pBuffer)
{
	pBuffer->nOperation = OP_WRITE;
	pBuffer->pSocket->nOutstandingOps++;

	DWORD dwBytes;
	DWORD dwFlags = 0;
	WSABUF buf;
	buf.buf = pBuffer->buff;
	buf.len = pBuffer->nLen;
	if (::WSASend(pBuffer->pSocket->s, &buf, 1,
		&dwBytes, dwFlags, &pBuffer->ol, NULL) != NO_ERROR)
	{
		if (::WSAGetLastError() != WSA_IO_PENDING)
			return FALSE;
	}
	return TRUE;
}

BOOL HandleIO(PBUFFER_OBJ pBuffer)
{
	PSOCKET_OBJ pSocket = pBuffer->pSocket;
	pSocket->nOutstandingOps--;

	DWORD dwTrans;
	DWORD dwFlags;
	BOOL bRet = ::WSAGetOverlappedResult(pSocket->s, &pBuffer->ol, &dwTrans, FALSE, &dwFlags);
	if (!bRet)
	{//在此套接字上有错误发生，关闭套接字，移除缓冲区，
	 //如果没有其他抛出的 I/O 请求， 释放缓冲区对象
		if (pSocket->s != INVALID_SOCKET)
		{
			::closesocket(pSocket->s);
			pSocket->s = INVALID_SOCKET;
		}
		if (pSocket->nOutstandingOps == 0)
			FreeSocketObj(pSocket);

		FreeBufferObj(pBuffer);
		return FALSE;
	}
	//没有错误发生，处理已完成的 I/O
	switch (pBuffer->nOperation)
	{
	case OP_ACCEPT://接收到一个新的链接，并接收到了对方发来的第一个封包
	{
		//为新客户创建一个 SOCKET_OBJ 对象
		PSOCKET_OBJ pClient = GetSocketObj(pBuffer->sAccept);
		//为发送数据创建一个 BUFFER_OBJ 对象，这个对象会在套接字出错或者关闭时释放
		PBUFFER_OBJ pSend = GetBufferObj(pClient, BUFFER_SIZE);
		if (pSend == NULL)
		{
			cout << "Too many connections." << endl;
			FreeSocketObj(pClient);
			return FALSE;
		}
		RebuildArray();
		//将数据复制到发送缓冲区
		pSend->nLen = dwTrans;
		memcpy(pSend->buff, pBuffer->buff, dwTrans);
		//投递此 I/O
		if (!PostSend(pSend))
		{//出错，释放申请的对象
			FreeSocketObj(pSocket);
			FreeBufferObj(pBuffer);
			return FALSE;
		}
		//继续接受 I/O
		PostAccept(pBuffer);
	}
	break;
	case OP_READ:
	{
		if (dwTrans > 0)
		{
			PBUFFER_OBJ pSend = pBuffer;
			pSend->nLen = dwTrans;
			PostSend(pSend);
		}
		else
		{
			if (pSocket->s != INVALID_SOCKET)
			{
				::closesocket(pSocket->s);
				pSocket->s = INVALID_SOCKET;
			}
			if (pSocket->nOutstandingOps == 0)
				FreeSocketObj(pSocket);
			FreeBufferObj(pBuffer);
			return FALSE;
		}
	}
	break;
	case OP_WRITE:
	{
		if (dwTrans > 0)
		{
			pBuffer->nLen = BUFFER_SIZE;
			PostRecv(pBuffer);
		}
		else
		{
			if (pSocket->s != INVALID_SOCKET)
			{
				::closesocket(pSocket->s);
				pSocket->s = INVALID_SOCKET;
			}
			if (pSocket->nOutstandingOps == 0)
				FreeSocketObj(pSocket);
			FreeBufferObj(pBuffer);
			return FALSE;
		}
	}
	break;
	}
	return TRUE;
}

int main()
{
	int nPort = 4567;
	SOCKET sListen = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,
		NULL, 0, WSA_FLAG_OVERLAPPED);
	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = nPort;
	sin.sin_addr.S_un.S_addr = INADDR_ANY;
	::bind(sListen, (sockaddr*)&sin, sizeof(sin));
	::listen(sListen, 200);
	//为监听套接字创建一个 SOCKET_OBJ 对象
	PSOCKET_OBJ pListen = GetSocketObj(sListen);
	//加载扩展函数 AcceptEx
	GUID GuidAcceptEx = WSAID_ACCEPTEX;
	DWORD dwBytes;
	WSAIoctl(pListen->s,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidAcceptEx,
		sizeof(GuidAcceptEx),
		&pListen->lpfnAcceptEx,
		sizeof(pListen->lpfnAcceptEx),
		&dwBytes,
		NULL,
		NULL);
	//创建用来重新建立 g_events 数组的事件对象
	g_events[0] = ::WSACreateEvent();
	for (int i = 0; i < 5; i++)
		PostAccept(GetBufferObj(pListen, BUFFER_SIZE));
	while (TRUE)
	{
		int nIndex = ::WSAWaitForMultipleEvents(g_nBufferCount + 1,
			g_events, FALSE, WSA_INFINITE, FALSE);
		if (nIndex == WSA_WAIT_FAILED)
		{
			cout << "Failed WSAWaitForMultipleEvents()." << endl;
			break;
		}
		nIndex = nIndex - WSA_WAIT_EVENT_0;
		for (int i = 0; i < nIndex; i++)
		{
			int nRet = ::WSAWaitForMultipleEvents(1, &g_events[i], TRUE, 0, FALSE);
			if (nRet == WSA_WAIT_TIMEOUT)
				continue;
			else
			{
				::WSAResetEvent(g_events[i]);
				//重新建立 g_events 数组
				if (i == 0)
				{
					RebuildArray();
					continue;
				}
				//处理这个 I/O
				PBUFFER_OBJ pBuffer = FindBufferObj(g_events[i]);
				if (pBuffer != NULL)
				{
					if (!HandleIO(pBuffer))
						RebuildArray();
				}
			}
		}
	}
}