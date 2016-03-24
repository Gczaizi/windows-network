#include <iostream>
#include "../../Common/initsock.h"
#include "../../Common/protoinfo.h"
#include "../../Common/comm.h"

using namespace std;

CInitSock icmpSock;

typedef struct _ICMP_HDR
{
	unsigned char icmp_type;		//��Ϣ����
	unsigned char icmp_code;		//����
	unsigned short icmp_checksum;	//У���
	//�����ǻ���ͷ
	unsigned short icmp_id;			//����Ωһ��ʶ������� ID �ţ�ͨ������Ϊ���� ID
	unsigned short icmp_sequence;	//���к�
	unsigned long icmp_timestamp;	//ʱ���
}ICMP_HDR, *PICMP_HDR;

USHORT checksum(USHORT *buffer, int size)
{
	unsigned long cksum = 0;
	//����������Ϊ��λ�ۼӵ� cksum ��
	while (size > 1)
	{
		cksum += *buffer++;
		size -= sizeof(USHORT);
	}
	//���Ϊ�����������һ���ֽ���չ��˫�֣����ۼӵ� cksum ��
	if (size)
		cksum += *(UCHAR*)buffer;
	//�� cksum �ĸ� 16 λ�͵� 16 λ��ӣ�ȡ����õ�У���
	cksum = (cksum >> 16) + (cksum & 0xffff);
	cksum += (cksum >> 16);
	return (USHORT)(~cksum);
}

int main()
{
	char szDestIp[] = "127.0.0.1";
	SOCKET sRaw = ::socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	SetTimeout(sRaw, 1000, TRUE);
	//����Ŀ�ĵ�ַ
	sockaddr_in dest;
	dest.sin_family = AF_INET;
	dest.sin_port = htons(0);
	dest.sin_addr.S_un.S_addr = inet_addr(szDestIp);
	//���� ICMP ���
	char buff[sizeof(ICMP_HDR)+32];
	ICMP_HDR* pIcmp = (ICMP_HDR*)buff;
	//��д ICMP �������
	pIcmp->icmp_type = 8;
	pIcmp->icmp_code = 0;
	pIcmp->icmp_sequence = 0;
	//������ݲ��֣�����Ϊ����
	memset(&buff[sizeof(ICMP_HDR)], 'E', 32);
	//��ʼ���ͺͽ��� ICMP ���
	USHORT nSeq = 0;
	char recvBuf[1024];
	sockaddr_in from;
	int nLen = sizeof(from);
	while (TRUE)
	{
		static int nCount = 0;
		int nRet;
		if (nCount++)
			break;
		pIcmp->icmp_checksum = 0;
		pIcmp->icmp_timestamp = ::GetTickCount();
		pIcmp->icmp_sequence = nSeq++;
		pIcmp->icmp_checksum = checksum((USHORT*)buff, sizeof(ICMP_HDR)+32);
		nRet = ::sendto(sRaw, buff, sizeof(ICMP_HDR)+32, 0, (sockaddr*)&dest, sizeof(dest));
		if (nRet == SOCKET_ERROR)
		{
			cout << "Failed sendto(): " << WSAGetLastError() << endl;
			return -1;
		}
		nRet = ::recvfrom(sRaw, recvBuf, 1024, 0, (sockaddr*)&from, &nLen);
		if (nRet == SOCKET_ERROR)
		{
			if (::WSAGetLastError() == WSAETIMEDOUT)
			{
				cout << "time out" << endl;
				continue;
			}
			cout << "Failed recvfrom(): " << WSAGetLastError() << endl;
			return -1;
		}
		//���濪ʼ�������յ��� ICMP ���
		int nTick = ::GetTickCount();
		if (nRet < sizeof(IPHeader)+sizeof(ICMP_HDR))
			cout << "Too few bytes from: " << ::inet_ntoa(from.sin_addr);
		ICMP_HDR *pRecvIcmp = (ICMP_HDR*)(recvBuf + sizeof(IPHeader));
		if (pRecvIcmp->icmp_type != 0)
		{
			cout << "nonecho type " << pRecvIcmp->icmp_type << "recvd" << endl;
			return -1;
		}
		if (pRecvIcmp->icmp_id != ::GetCurrentProcessId())
		{
			cout << "someone else's packet" << endl;
			return -1;
		}
		cout << nRet << " bytes from " << inet_ntoa(from.sin_addr) << endl;
		cout << "icmp_seq = " << pRecvIcmp->icmp_sequence << endl;
		cout << "time: " << nTick - pRecvIcmp->icmp_timestamp << " ms" << endl;
		cout << endl;
		::Sleep(1000);
	}
	return 0;
}