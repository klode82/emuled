// LINUX: vedi util/Net.h
#include "StdAfx.h"
#include "util/Net.h"
#include "Preferences.h"
#include <arpa/inet.h>

CString ipstr(LPCTSTR pszAddress, uint16 nPort)
{
	CString strIPPort;
	strIPPort.Format(_T("%s:%u"), pszAddress, nPort);
	return strIPPort;
}

CString ipstr(uint32 nIP, uint16 nPort)
{
	return ipstr((LPCTSTR)ipstr(nIP), nPort);
}

CString ipstr(uint32 nIP)
{
	char b[INET_ADDRSTRLEN]; in_addr a; a.s_addr = nIP;
	return CString(inet_ntop(AF_INET, &a, b, sizeof b) ? b : "");
}

CStringA ipstrA(uint32 nIP)
{
	return CStringA((LPCTSTR)ipstr(nIP));
}

void ipstrA(CHAR *pszAddress, int iMaxAddress, uint32 nIP)
{
	const BYTE *pucIP = (BYTE*)&nIP;
	_snprintf(pszAddress, iMaxAddress, "%u.%u.%u.%u", pucIP[0], pucIP[1], pucIP[2], pucIP[3]);
}

bool IsLANIP(uint32 nIP)
{
	// LAN IP's: 0.* ("this" network), 10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16
	uint8 nFirst = (uint8)nIP;
	uint8 nSecond = (uint8)(nIP >> 8);
	if (nFirst == 192 && nSecond == 168) // check this 1st, because those LANs IPs are most common
		return true;
	if (nFirst == 172 && nSecond >= 16 && nSecond <= 31)
		return true;
	return nFirst == 0 || nFirst == 10;
}

bool IsGoodIP(uint32 nIP, bool forceCheck)
{
	// sempre scartati: 0.0.0.0, 127.* (loopback), 224-239.* (multicast), 240-255.* (riservati)
	if (nIP == 0 || (uint8)nIP == 127 || (uint8)nIP >= 224)
		return false;
	return (!thePrefs.FilterLANIPs() && !forceCheck) || !IsLANIP(nIP);
}

bool IsGoodIPPort(uint32 nIP, uint16 nPort)
{
	return IsGoodIP(nIP) && nPort != 0;
}
