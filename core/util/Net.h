// LINUX: argomento "indirizzi di rete" di OtherFunctions.h (ipstr, ipstrA). Un indirizzo IPv4 e' un uint32 con i byte in ordine di rete,
// come in eMule (il primo byte in memoria e' la prima cifra). Origine: OtherFunctions.cpp righe 2726-2756.
#pragma once
#include "StdAfx.h"

CString ipstr(uint32 nIP);
CString ipstr(uint32 nIP, uint16 nPort);
CString ipstr(LPCTSTR pszAddress, uint16 nPort);
CStringA ipstrA(uint32 nIP);
void ipstrA(CHAR *pszAddress, int iMaxAddress, uint32 nIP);
bool IsLANIP(uint32 nIP);		// 0.*, 10.*, 172.16-31.*, 192.168.* (origine: OtherFunctions.cpp IsLANIP)
bool IsGoodIP(uint32 nIP, bool forceCheck = false);		// scarta 0.*, loopback, multicast, riservati; con "filtra LAN" (predefinito) anche gli indirizzi di rete locale
bool IsGoodIPPort(uint32 nIP, uint16 nPort);
inline CString ipstr(in_addr nIP) { return ipstr(*(uint32*)&nIP); }
inline CStringA ipstrA(in_addr nIP) { return ipstrA(*(uint32*)&nIP); }
inline bool IsLowID(uint32 id) { return id < 16777216u; }			// ID ed2k < 2^24: client non raggiungibile (ID basso)
