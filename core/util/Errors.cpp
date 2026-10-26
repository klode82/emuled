// LINUX: vedi util/Errors.h
#include "StdAfx.h"
#include "util/Errors.h"
#include <cstdarg>
#include <pthread.h>
#include <cstring>

void DbgSetThreadName(LPCSTR szThreadName, ...)
{
	char name[64]; va_list ap; va_start(ap, szThreadName); vsnprintf(name, sizeof name, szThreadName, ap); va_end(ap);
	name[15] = '\0';											// Linux accetta al massimo 15 caratteri
	pthread_setname_np(pthread_self(), name);
}

int GetErrorMessage(DWORD dwError, CString &rstrErrorMsg, DWORD /*dwFlags*/)
{
	char buf[256]; const char *m = strerror_r((int)dwError, buf, sizeof buf);
	rstrErrorMsg = m ? m : "";
	return rstrErrorMsg.IsEmpty() ? 0 : 1;
}

CString GetErrorMessage(DWORD dwError, DWORD dwFlags)
{
	CString s; GetErrorMessage(dwError, s, dwFlags); return s;
}
