#include <iostream>
#include <MSWSock.h>
#include <WinSock2.h>
//#include "../../WindowsSocket/WindowsSocket/initsock.h"
#define BUFFER_SIZE 1024
#define MAX_THREAD 2

class CIOCPServer
{
public:
	CIOCPServer();
	~CIOCPServer();
	//开始服务
	BOOL Start(int nPort = 4567, int nMaxConnections = 2000,
		int nMaxFreeBuffers = 200, int nMaxFreeContexts = 100, int nInitialReads = 4);
	//停止服务
	void Shutdown();
	//关闭一个连接和关闭所有连接
	void CloseAConnection(CIOCPContext *pContext);
	void CloseAllConnections();
	//取得当前的连接数量
	ULONG GetCurrentConnection() { return m_nCurrentConnection; }
	//向指定客户发送文本
	BOOL SendText(CIOCPContext *pContext, char *pszText, int nLen);
protected:
	//申请和释放缓冲区对象
	CIOCPBuffer *AllocateBuffer(int nLen);
	void ReleaseBuffer(CIOCPBuffer *pBuffer);
	//申请和释放套接字上下文
	CIOCPContext *AllocateContext(SOCKET s);
	void ReleaseContext(CIOCPContext *pContext);
	//释放空闲缓冲区对象列表和空闲上下文对象列表
	void FreeBuffers();
	void FreeContexts();
	//向连接列表中添加一个连接
	BOOL AddAConnection(CIOCPContext *pContext);
	//插入和移除未决的接受的请求
	BOOL InsertPendingAccept(CIOCPBUffer *pBuffer);
	BOOL RemovePendingAccept(CIOCPBuffer *pBuffer);
	//取得下一个要读取的
	CIOCPBuffer *GetNextReadBuffer(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
	//投递接受 I/O、发送 I/O、接收 I/O
	BOOL PostAccept(CIOCPBuffer *pBuffer);
	BOOL PostSend(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
	BOOL PostRecv(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
	void HandleIO(DWORD dwKey, CIOCPBuffer *pBuffer, DWORD dwTrans, int nError);
	//建立了一个新的连接
	virtual void OnConnectionEstablished(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
	//一个连接关闭
	virtual void OnConnectionClosing(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
	//在一个连接上发生了错误
	virtual void OnConnectionError(CIOCPContext *pContext, CIOCPBuffer *pBuffer, int nError);
	//一个连接上的读操作完成
	virtual void OnReadCompleted(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
	//一个连接上的写操作完成
	virtual void OnWriteCompleted(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
protected:
	//记录空闲结构信息
	CIOCPBuffer *m_pFreeBufferList;
	CIOCPContext *m_pFreeContextList;
	int m_nFreeBufferCount;
	int m_nFreeContextCount;
	CRITICAL_SECTION m_FreeBufferListLock;
	CRITICAL_SECTION m_FreeContextListLock;
	//记录抛出的 Accept 请求
	CIOCPBuffer *m_pPendingAccepts;
	long m_nPendingAcceptCount;
	CRITICAL_SECTION m_PendingAcceptsLock;
	//记录连接列表
	CIOCPContext *m_pConnectionList;
	int m_nCurrentConnection;
	CRITICAL_SECTION m_ConnectionListLock;
	//用于投递 Accept 请求
	HANDLE m_hAcceptEvent;
	HANDLE m_hRepostEvent;
	LONG m_nRepsotCount;
	int m_nPort;
	int m_nInitialAccepts;
	int m_nMaxAccepts;
	int m_nMaxSends;
	int m_nMaxFreeBuffers;
	int m_nMaxFreeContexts;
	int m_nMaxConnections;
	HANDLE m_hListenThread;
	HANDLE m_hCompletion;
	SOCKET m_sListen;
	LPFN_ACCEPTEX m_lpfnAcceptEx;
	LPFN_GETACCEPTEXSOCKADDRS m_lpfnGetAcceptExSockaddrs;
	BOOL m_bShutDown;
	BOOL m_bbServerStarted;
private:
	static DWORD WINAPI _ListenThreadProc(LPVOID lpParam);
	static DWORD WINAPI _WorkerThreadProc(LPVOID lpParam);
};

CIOCPServer::CIOCPServer()
{
	//列表
	m_pFreeBufferList = NULL;
	m_pFreeContextList = NULL;
	m_pPendingAccepts = NULL;
	m_pConnectionList = NULL;
	m_nFreeBufferCount = 0;
	m_nFreeContextCount = 0;
	m_nPendingAcceptCount = 0;
	m_nCurrentConnection = 0;
	::InitializeCriticalSection(&m_FreeBufferListLock);
	::InitializeCriticalSection(&m_FreeContextListLock);
	::InitializeCriticalSection(&m_PendingAcceptsLock);
	::InitializeCriticalSection(&m_ConnectionListLock);

}

CIOCPServer::~CIOCPServer()
{
}