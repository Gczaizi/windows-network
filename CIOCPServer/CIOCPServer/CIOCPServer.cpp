#include "IOCP.h"
#pragma comment(lib, "WS2_32.lib")

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
	//Accpet ����
	m_hAcceptEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	m_hRepostEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	m_nRepsotCount = 0;
	m_nPort = 4567;
	m_nInitialAccepts = 10;
	m_nInitialReads = 4;
	m_nMaxAccepts = 100;
	m_nMaxSends = 20;
	m_nMaxFreeBuffers = 200;
	m_nMaxFreeContexts = 100;
	m_nMaxConnections = 2000;
	m_hListenThread = NULL;
	m_hCompletion = NULL;
	m_sListen = INVALID_SOCKET;
	m_lpfnAcceptEx = NULL;
	m_lpfnGetAcceptExSockaddrs = NULL;
	m_bShutDown = FALSE;
	m_bServerStarted = FALSE;
	//��ʼ�� WS_32.dll
	WSADATA wsaData;
	WORD sockVersion = MAKEWORD(2, 2);
	::WSAStartup(sockVersion, &wsaData);
}

CIOCPServer::~CIOCPServer()
{
	Shutdown();
	if (m_sListen != INVALID_SOCKET)
		::closesocket(m_sListen);
	if (m_hListenThread != NULL)
		::CloseHandle(m_hListenThread);
	::CloseHandle(m_hRepostEvent);
	::CloseHandle(m_hAcceptEvent);
	::DeleteCriticalSection(&m_FreeBufferListLock);
	::DeleteCriticalSection(&m_FreeContextListLock);
	::DeleteCriticalSection(&m_PendingAcceptsLock);
	::DeleteCriticalSection(&m_ConnectionListLock);
	::WSACleanup();
}

CIOCPBuffer *CIOCPServer::AllocateBuffer(int nLen)
{//���뻺��������
	CIOCPBuffer *pBuffer = NULL;
	if (nLen > BUFFER_SIZE)
		return NULL;
	//Ϊ���������������ڴ�
	::EnterCriticalSection(&m_FreeBufferListLock);
	if (m_pFreeBufferList == NULL)	//�ڴ��Ϊ�գ������µ��ڴ�
		pBuffer = (CIOCPBuffer*)::HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(CIOCPBuffer) + BUFFER_SIZE);
	else
	{//���ڴ����ȡһ����ʹ��
		pBuffer = m_pFreeBufferList;
		m_pFreeBufferList = m_pFreeBufferList->pNext;
		pBuffer->pNext = NULL;
		m_nFreeBufferCount--;
	}
	::LeaveCriticalSection(&m_FreeBufferListLock);
	//��ʼ���µĻ���������
	if (pBuffer != NULL)
	{
		pBuffer->buff = (char*)(pBuffer + 1);
		pBuffer->nLen = nLen;
	}
	return pBuffer;
}

void CIOCPServer::ReleaseBuffer(CIOCPBuffer *pBuffer)
{//�ͷŻ���������
	::EnterCriticalSection(&m_FreeBufferListLock);
	if (m_nFreeBufferCount <= m_nMaxFreeBuffers)
	{//��Ҫ�ͷŵ��ڴ���ӵ������б���
		memset(pBuffer, 0, sizeof(CIOCPBuffer) + BUFFER_SIZE);
		pBuffer->pNext = m_pFreeBufferList;
		m_pFreeBufferList = pBuffer;
		m_nFreeBufferCount++;
	}
	else	//�����ڴ��Ѵﵽ���ֵ���������ͷ��ڴ�
		::HeapFree(::GetProcessHeap(), 0, pBuffer);
	::LeaveCriticalSection(&m_FreeBufferListLock);
}

CIOCPContext *CIOCPServer::AllocateContext(SOCKET s)
{
	CIOCPContext *pContext;
	::EnterCriticalSection(&m_FreeContextListLock);
	if (m_nFreeContextCount == NULL)
		pContext = (CIOCPContext*)::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(CIOCPContext));
	else
	{
		pContext = m_pFreeContextList;
		m_pFreeContextList = m_pFreeContextList->pNext;
		pContext->pNext = NULL;
		m_nFreeContextCount--;
	}
	::LeaveCriticalSection(&m_FreeContextListLock);
	if (pContext != NULL)
	{
		pContext->s = s;
		::InitializeCriticalSection(&pContext->Lock);//���λ�ô���
	}
	return pContext;
}

void CIOCPServer::ReleaseContext(CIOCPContext *pContext)
{
	if (pContext->s != INVALID_SOCKET)
		::closesocket(pContext->s);
	//�����ͷŴ��׽����ϵ�û�а�˳����ɵĶ� I/O �Ļ�����
	CIOCPBuffer *pNext;
	while (pContext->pOutOfOrderReads->pNext != NULL)
	{
		pNext = pContext->pOutOfOrderReads->pNext;
		ReleaseBuffer(pContext->pOutOfOrderReads);
		pContext->pOutOfOrderReads = pNext;
	}
	::EnterCriticalSection(&m_FreeContextListLock);
	if (m_nFreeContextCount <= m_nMaxFreeContexts)
	{
		CRITICAL_SECTION cstmp = pContext->Lock;
		memset(pContext, 0, sizeof(CIOCPContext));
		pContext->Lock = cstmp;
		pContext->pNext = m_pFreeContextList;
		m_pFreeContextList = pContext;
		m_nFreeContextCount++;
	}
	else
	{
		::DeleteCriticalSection(&pContext->Lock);
		::HeapFree(::GetProcessHeap(), 0, pContext);
	}
	::LeaveCriticalSection(&m_FreeContextListLock);
}

void CIOCPServer::FreeBuffers()
{
	::EnterCriticalSection(&m_FreeBufferListLock);
	CIOCPBuffer *pFreeBuffer = m_pFreeBufferList;
	CIOCPBuffer *pNextBuffer;
	while (pFreeBuffer != NULL)
	{
		pNextBuffer = pFreeBuffer->pNext;
		if (!::HeapFree(::GetProcessHeap(), 0, pFreeBuffer))
		{
#ifdef _DEBUG
			::OutputDebugString(TEXT("Failed FreeBuffers()!"));
#endif // _DEBUG
			break;
		}
		pFreeBuffer = pNextBuffer;
	}
	m_pFreeBufferList = NULL;
	m_nFreeBufferCount = 0;
	::LeaveCriticalSection(&m_FreeBufferListLock);
}

void CIOCPServer::FreeContexts()
{
	::EnterCriticalSection(&m_FreeContextListLock);
	CIOCPContext *pFreeContext = m_pFreeContextList;
	CIOCPContext *pNextContext;
	while (pFreeContext != NULL)
	{
		pNextContext = pFreeContext->pNext;
		::DeleteCriticalSection(&pFreeContext->Lock);
		if (!::HeapFree(::GetProcessHeap(), 0, pFreeContext))
		{
#ifdef _DEBUG
			::OutputDebugString(TEXT("Failed FreeContexts()!"));
#endif // _DEBUG
			break;
		}
		pFreeContext = pNextContext;
	}
	m_pFreeContextList = NULL;
	m_nFreeContextCount = 0;
	::LeaveCriticalSection(&m_FreeContextListLock);
}

