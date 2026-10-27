// Winsock types and constants on top of POSIX sockets. See core/net/AsyncSocketEx.h.
#pragma once
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cerrno>
#include "win_types.h"

typedef int SOCKET;
typedef struct sockaddr SOCKADDR, *LPSOCKADDR;
typedef struct sockaddr_in SOCKADDR_IN, *LPSOCKADDR_IN;
typedef struct sockaddr_in6 SOCKADDR_IN6;
typedef sa_family_t ADDRESS_FAMILY;
#define INVALID_SOCKET	(-1)
#define SOCKET_ERROR	(-1)

// Winsock error codes = Linux errno
#define WSAEWOULDBLOCK		EWOULDBLOCK
#define WSAEINPROGRESS		EINPROGRESS
#define WSAEALREADY			EALREADY
#define WSAECONNRESET		ECONNRESET
#define WSAECONNABORTED		ECONNABORTED
#define WSAEADDRNOTAVAIL EADDRNOTAVAIL
#define WSAECONNREFUSED		ECONNREFUSED
#define WSAETIMEDOUT		ETIMEDOUT
#define WSAENOTCONN			ENOTCONN
#define WSAEISCONN			EISCONN
#define WSAESHUTDOWN		ESHUTDOWN
#define WSAEINVAL			EINVAL
#define WSAEADDRINUSE		EADDRINUSE
#define WSAENETUNREACH		ENETUNREACH
#define WSAEHOSTUNREACH		EHOSTUNREACH
#define WSAEMSGSIZE			EMSGSIZE
#define WSAENOBUFS			ENOBUFS

// WSAAsyncSelect events
#ifndef FD_READ
#define FD_READ		0x01
#define FD_WRITE	0x02
#define FD_OOB		0x04
#define FD_ACCEPT	0x08
#define FD_CONNECT	0x10
#define FD_CLOSE	0x20
#endif

// Last network error of the thread: set by every CAsyncSocketEx call that fails.
int WSAGetLastError();
void WSASetLastError(int e);

// Windows overlapped I/O: does not exist on Linux. The types are only here because the original classes declare them;
// CEMSocket uses it only if m_bUseOverlappedSend is true, and here it is always false (normal send is used).
struct WSABUF { ULONG len; char *buf; };
typedef WSABUF *LPWSABUF;
struct WSAOVERLAPPED { int unused; HANDLE hEvent; };
typedef WSAOVERLAPPED *LPWSAOVERLAPPED;
inline int WSASend(SOCKET, LPWSABUF, DWORD, DWORD *, DWORD, LPWSAOVERLAPPED, void *) { WSASetLastError(ENOTSUP); return SOCKET_ERROR; }
inline BOOL WSAGetOverlappedResult(SOCKET, LPWSAOVERLAPPED, DWORD *, BOOL, DWORD *) { WSASetLastError(ENOTSUP); return FALSE; }

#define WSA_IO_PENDING		997
#define WSA_IO_INCOMPLETE	996
#define WSANOTINITIALISED	10093
#define WSAENETDOWN			ENETDOWN
#define WSAENOTSOCK			ENOTSOCK
#define WSAEOPNOTSUPP		EOPNOTSUPP
inline BOOL CancelIo(HANDLE) { return FALSE; }
