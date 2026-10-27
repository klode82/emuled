// LINUX: the "network addresses" topic of OtherFunctions.h (ipstr, ipstrA). An IPv4 address is a uint32 with bytes in network order,
// as in eMule (the first byte in memory is the first digit). Origin: OtherFunctions.cpp lines 2726-2756.
#pragma once
#include "StdAfx.h"

CString ipstr(uint32 nIP);
CString ipstr(uint32 nIP, uint16 nPort);
CString ipstr(LPCTSTR pszAddress, uint16 nPort);
CStringA ipstrA(uint32 nIP);
void ipstrA(CHAR *pszAddress, int iMaxAddress, uint32 nIP);
bool IsLANIP(uint32 nIP);		// 0.*, 10.*, 172.16-31.*, 192.168.* (origine: OtherFunctions.cpp IsLANIP)
bool IsGoodIP(uint32 nIP, bool forceCheck = false);		// rejects 0.*, loopback, multicast, reserved; with "filter LAN" (default) also local network addresses
bool IsGoodIPPort(uint32 nIP, uint16 nPort);
inline CString ipstr(in_addr nIP) { return ipstr(*(uint32*)&nIP); }
inline CStringA ipstrA(in_addr nIP) { return ipstrA(*(uint32*)&nIP); }
inline bool IsLowID(uint32 id) { return id < 16777216u; }			// ed2k ID < 2^24: unreachable client (low ID)
