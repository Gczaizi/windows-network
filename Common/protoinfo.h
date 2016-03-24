#ifndef __PROTOINFO_H__
#define __PROTOINFO_H__

#define ETHERTYPE_IP	0x0800
#define ETHERTYPE_ARP	0x0806

typedef struct _ETHeader
{
	UCHAR	dhost[6];
	UCHAR	shost[6];
	USHORT	type;
}ETHeader, *PETHeader;

#define ARPHARD_ETHER	1

//
#define ARPOP_REQUEST	1
#define ARPOP_REPLY		2

typedef struct _ARPHeader
{
	USHORT	hrd;
	USHORT	eth_type;
	UCHAR	maclen;
	UCHAR	iplen;
	USHORT	opcode;
	UCHAR	smac[6];
	UCHAR	saddr[4];
	UCHAR	dmac[6];
	UCHAR	daddr[4];
}ARPHeader, *PARPHeader;

#define PROTO_ICMP	1
#define PROTO_IGMP	2
#define PROTO_TCP	6
#define PROTO_UDP	17

typedef struct _IPHeader
{
	UCHAR	iphVerLen;
	UCHAR	ipTOS;
	USHORT	ipLength;
	USHORT	ipID;
	USHORT	ipFlags;
	UCHAR	ipTTL;
	UCHAR	ipProtocol;
	USHORT	ipChecksum;
	ULONG	ipSource;
	ULONG	ipDestination;
}IPHeader, *PIPHeader;

//
#define TCP_FIN	0x01
#define TCP_SYN	0x02
#define TCP_RST	0x04
#define TCP_PSH	0x08
#define TCP_ACK	0x10
#define TCP_URG	0x20
#define TCP_ACE	0x40
#define TCP_CWR	0x80

typedef struct _TCPHeader
{
	USHORT	sourcePort;
	USHORT	destinationPort;
	ULONG	sequenceNumber;
	ULONG	acknowledgeNumber;
	UCHAR	dataoffset;
	UCHAR	flags;

	USHORT	windows;
	USHORT	checksum;
	USHORT	urgentPointer;
}TCPHeader, *PTCPHeader;

typedef struct _UDPHeader
{
	USHORT	sourcePort;
	USHORT	destinationPort;
	USHORT	len;
	USHORT	checksum;
}UDPHeader, *PUDPHeader;

#endif	//	__PROTOINFO_H__