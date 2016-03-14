#include "../../WindowsSocket/WindowsSocket/initsock.h"
#include <iostream>
#include <Mswsock.h>

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
	{//判断条件有错，需要修改
		g_pBufferHead = g_pBufferHead->pNext;
		bFind = TRUE;
	}
}