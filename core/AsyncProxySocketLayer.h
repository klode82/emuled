// STUB (replaces srchybrid/AsyncProxySocketLayer.h): only the names that CEMSocket uses to compile. With the proxy disabled it never runs.
// Decision of 25/10/2026: SOCKS/HTTP proxies are NOT ported (a VPN with an open port is used). The block stays only because CEMSocket names it.
#pragma once
#include "StdAfx.h"
#include "net/AsyncSocketEx.h"

#define PROXYTYPE_NOPROXY	0
#define PROXYTYPE_SOCKS4	1
#define PROXYTYPE_SOCKS4A	2
#define PROXYTYPE_SOCKS5	3
#define PROXYTYPE_HTTP10	4
#define PROXYTYPE_HTTP11	5

#define PROXYERROR_NOERROR			0
#define PROXYERROR_NOCONN			1
#define PROXYERROR_REQUESTFAILED	2
#define PROXYERROR_AUTHREQUIRED		3
#define PROXYERROR_AUTHTYPEUNKNOWN	4
#define PROXYERROR_AUTHFAILED		5
#define PROXYERROR_AUTHNOLOGON		6
#define PROXYERROR_CANTRESOLVEHOST	7

class CAsyncSocketExLayer { public: virtual ~CAsyncSocketExLayer() = default; };

class CAsyncProxySocketLayer : public CAsyncSocketExLayer
{
public:
	void SetProxy(int, const CString &, UINT) {}
	void SetProxy(int, const CString &, UINT, const CString &, const CString &) {}
};

inline CString GetProxyError(int) { return CString("proxy not supported"); }
