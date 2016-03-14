#include "../../WindowsSocket/WindowsSocket/initsock.h"
#include <iostream>
#include <Mswsock.h>

using namespace std;

CInitSock ioSock;

typedef struct _SOCKET_OBJ
{
	SOCKET s;	//�׽��־��
	int nOutstandingOps;	//��¼���׽����ϵ��ص� I/O ����
	LPFN_ACCEPTEX lpfnAcceptEx;	//��չ���� AcceptEx ��ָ��
}SOCKET_OBJ, *PSOCKET_OBJ;

typedef struct _BUFFER_OBJ
{
	OVERLAPPED ol;	//�ص��ṹ
	char *buff;	//send/recv/accept ��ʹ�õĻ�����
	int nLen;	//buff �ĳ���
	PSOCKET_OBJ pSocket;	//�� I/O �������׽���
	int nOperation;	//�ύ�Ĳ�������
#define OP_ACCEPT 1
#define OP_READ 2
#define OP_WRITE 3
	SOCKET sAccept;	//�������� AcceptEx ���յĿͻ����׽���
	_BUFFER_OBJ *pNext;
}BUFFER_OBJ, *PBUFFER_OBJ;
//ȫ�ֱ���
HANDLE g_events[WSA_MAXIMUM_WAIT_EVENTS];	//I/O �¼��������
int g_nBufferCount;	//������������Ч�������
PBUFFER_OBJ g_pBufferHead, g_pBufferTail;	//��¼������������ɵı�ĵ�ַ

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
		//���µ� BUFFER_OBJ ��ӵ��б���
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
	{//�ж������д���Ҫ�޸�
		g_pBufferHead = g_pBufferHead->pNext;
		bFind = TRUE;
	}
}