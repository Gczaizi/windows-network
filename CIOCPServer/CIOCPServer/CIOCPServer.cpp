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
	//��ʼ����
	BOOL Start(int nPort = 4567, int nMaxConnections = 2000,
		int nMaxFreeBuffers = 200, int nMaxFreeContexts = 100, int nInitialReads = 4);
	//ֹͣ����
	void Shutdown();
	//�ر�һ�����Ӻ͹ر���������
	void CloseAConnection(CIOCPContext *pContext);
	void CloseAllConnections();
	//ȡ�õ�ǰ����������
	ULONG GetCurrentConnection() { return m_nCurrentConnection; }
	//��ָ���ͻ������ı�
	BOOL SendText(CIOCPContext *pContext, char *pszText, int nLen);
protected:
	//������ͷŻ���������
	CIOCPBuffer *AllocateBuffer(int nLen);
	void ReleaseBuffer(CIOCPBuffer *pBuffer);
	//������ͷ��׽���������
	CIOCPContext *AllocateContext(SOCKET s);
	void ReleaseContext(CIOCPContext *pContext);
	//�ͷſ��л����������б�Ϳ��������Ķ����б�
	void FreeBuffers();
	void FreeContexts();
	//�������б������һ������
	BOOL AddAConnection(CIOCPContext *pContext);
	//������Ƴ�δ���Ľ��ܵ�����
	BOOL InsertPendingAccept(CIOCPBUffer *pBuffer);
	BOOL RemovePendingAccept(CIOCPBuffer *pBuffer);
	//ȡ����һ��Ҫ��ȡ��
	CIOCPBuffer *GetNextReadBuffer(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
	//Ͷ�ݽ��� I/O������ I/O������ I/O
	BOOL PostAccept(CIOCPBuffer *pBuffer);
	BOOL PostSend(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
	BOOL PostRecv(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
	void HandleIO(DWORD dwKey, CIOCPBuffer *pBuffer, DWORD dwTrans, int nError);
	//������һ���µ�����
	virtual void OnConnectionEstablished(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
	//һ�����ӹر�
	virtual void OnConnectionClosing(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
	//��һ�������Ϸ����˴���
	virtual void OnConnectionError(CIOCPContext *pContext, CIOCPBuffer *pBuffer, int nError);
	//һ�������ϵĶ��������
	virtual void OnReadCompleted(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
	//һ�������ϵ�д�������
	virtual void OnWriteCompleted(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
protected:
	//��¼���нṹ��Ϣ
	CIOCPBuffer *m_pFreeBufferList;
	CIOCPContext *m_pFreeContextList;
	int m_nFreeBufferCount;
	int m_nFreeContextCount;
	CRITICAL_SECTION m_FreeBufferListLock;
	CRITICAL_SECTION m_FreeContextListLock;
	//��¼�׳��� Accept ����
	CIOCPBuffer *m_pPendingAccepts;
	long m_nPendingAcceptCount;
	CRITICAL_SECTION m_PendingAcceptsLock;
	//��¼�����б�
	CIOCPContext *m_pConnectionList;
	int m_nCurrentConnection;
	CRITICAL_SECTION m_ConnectionListLock;
	//����Ͷ�� Accept ����
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
	//�б�
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