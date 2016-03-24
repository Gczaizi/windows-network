#include <iostream>
#include "../../Common/initsock.h"
#include "../../Common/protoinfo.h"
#include "../../Common/comm.h"

using namespace std;

CInitSock icmpSock;

typedef struct _ICMP_HDR
{
	unsigned char icmp_type;		//消息类型
	unsigned char icmp_code;		//代码
	unsigned short icmp_checksum;	//校验和
	//下面是回显头
	unsigned short icmp_id;			//用来惟一标识此请求的 ID 号，通常设置为进程 ID
	unsigned short icmp_sequence;	//序列号
	unsigned long icmp_timestamp;	//时间戳
}ICMP_HDR, *PICMP_HDR;

USHORT checksum(USHORT *buffer, int size)
{
	unsigned long cksum = 0;
	//将数据以字为单位累加到 cksum 中
	while (size > 1)
	{
		cksum += *buffer++;
		size -= sizeof(USHORT);
	}
	//如果为奇数，将最后一个字节扩展到双字，再累加到 cksum 中
	if (size)
		cksum += *(UCHAR*)buffer;
	//将 cksum 的高 16 位和低 16 位相加，取反后得到校验和
	cksum = (cksum >> 16) + (cksum & 0xffff);
	cksum += (cksum >> 16);
	return (USHORT)(~cksum);
}

int main()
{
	char szDestIp[] = "127.0.0.1";
	SOCKET sRaw = ::socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	SetTimeout(sRaw, 1000, TRUE);
	//设置目的地址
	sockaddr_in dest;
	dest.sin_family = AF_INET;
	dest.sin_port = htons(0);
	dest.sin_addr.S_un.S_addr = inet_addr(szDestIp);
	//创建 ICMP 封包
	char buff[sizeof(ICMP_HDR)+32];
	ICMP_HDR* pIcmp = (ICMP_HDR*)buff;
	//填写 ICMP 封包数据
	pIcmp->icmp_type = 8;
	pIcmp->icmp_code = 0;
	pIcmp->icmp_sequence = 0;
	//填充数据部分，可以为任意
	memset(&buff[sizeof(ICMP_HDR)], 'E', 32);
	//开始发送和接收 ICMP 封包
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
		//下面开始解析接收到的 ICMP 封包
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