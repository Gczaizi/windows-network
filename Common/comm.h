#ifndef __COMM_H__
#define __COMM_H__

USHORT checksum(USHORT *buffer, int size);

BOOL SetTTL(SOCKET s, int nValue);
BOOL SetTimeout(SOCKET s, int nTime, BOOL bRecv = TRUE);

#endif	// __COMM_H__