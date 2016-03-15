#include "../../windowsSocket/WindowsSocket/initsock.h"
#include <iostream>
#define BUFFER_SIZE 1024

using namespace std;

CInitSock iocpSock;

typedef struct _PER_HANDLE_DATA
{//per-handle 数据
	SOCKET s;	//对应的套接字句柄
	sockaddr_in addr;	//客户端地址
}PER_HANDLE_DATA, *PPER_HANDLE_DATA;

typedef struct _PER_IO_DATA
{//per-I/O 数据
	OVERLAPPED ol;	//重叠结构
	char buf[BUFFER_SIZE];	//数据缓冲区
	int nOperationType;	//操作类型
#define OP_READ 1
#define OP_WRITE 2
#define OP_ACCEPT 3
}PER_IO_DATA, *PPER_IO_DATA;

DWORD WINAPI ServerThread(LPVOID lpParam)
{
	//得到完成端口对象句柄
	HANDLE hCompletion = (HANDLE)lpParam;
	DWORD dwTrans;
	PPER_HANDLE_DATA pPerHandle;
	PPER_IO_DATA pPerIO;
	while (TRUE)
	{
		//在关联到此完成端口的所有套接字上等待 I/O 完成
		BOOL bOK = ::GetQueuedCompletionStatus(hCompletion,
			&dwTrans, (LPDWORD)&pPerHandle, (LPOVERLAPPED*)&pPerIO, WSA_INFINITE);
		if (!bOK)
		{//此套接字上有错误发生
			::closesocket(pPerHandle->s);
			::GlobalFree(pPerHandle);
			::GlobalFree(pPerIO);
			continue;
		}
		if (dwTrans == 0 &&
			(pPerIO->nOperationType == OP_READ || pPerIO->nOperationType == OP_WRITE))
		{//套接字被对方关闭
			::closesocket(pPerHandle->s);
			::GlobalFree(pPerHandle);
			::GlobalFree(pPerIO);
			continue;
		}
		switch (pPerIO->nOperationType)
		{
		case OP_READ:
		{
			pPerIO->buf[dwTrans] = '\0';
			cout << pPerIO->buf << endl;
			//继续投递接收 I/O 请求
			WSABUF buf;
			buf.buf = pPerIO->buf;
			buf.len = BUFFER_SIZE;
			pPerIO->nOperationType = OP_READ;
			DWORD nFlags = 0;
			::WSARecv(pPerHandle->s, &buf, 1, &dwTrans, &nFlags, &pPerIO->ol, NULL);
		}
		break;
		case OP_WRITE:
		case OP_ACCEPT:
			break;
		}
	}
	return 0;
}

void main()
{
	int nPort = 4567;
	//创建完成端口对象，创建工作线程处理完成端口对象中事件
	HANDLE hCompletion = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	::CreateThread(NULL, 0, ServerThread, (LPVOID)hCompletion, 0, 0);
	//创建监听套接字，绑定到本地地址，开始监听
	SOCKET sListen = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = ::htons(nPort);
	sin.sin_addr.S_un.S_addr = INADDR_ANY;
	::bind(sListen, (sockaddr*)&sin, sizeof(sin));
	::listen(sListen, 5);
	//循环处理到来的连接
	while (TRUE)
	{
		//等待接受未决的连接请求
		sockaddr_in saRemote;
		int nRemoteLen = sizeof(saRemote);
		SOCKET sNew = ::accept(sListen, (sockaddr*)&saRemote, &nRemoteLen);
		//接受到新连接之后，为它创建一个 per-handle 数据，并将它们关联到完成端口对象
		PPER_HANDLE_DATA pPerHandle = (PPER_HANDLE_DATA)::GlobalAlloc(GPTR, sizeof(PER_HANDLE_DATA));
		pPerHandle->s = sNew;
		memcpy(&pPerHandle->addr, &saRemote, nRemoteLen);
		::CreateIoCompletionPort((HANDLE)pPerHandle->s, hCompletion, (DWORD)pPerHandle, 0);
		//投递一个接收请求
		PPER_IO_DATA pPerIO = (PPER_IO_DATA)::GlobalAlloc(GPTR, sizeof(PER_IO_DATA));
		pPerIO->nOperationType = OP_READ;
		WSABUF buf;
		buf.buf = pPerIO->buf;
		buf.len = BUFFER_SIZE;
		DWORD dwRecv;
		DWORD dwFlags = 0;
		::WSARecv(pPerHandle->s, &buf, 1, &dwRecv, &dwFlags, &pPerIO->ol, NULL);
	}
}