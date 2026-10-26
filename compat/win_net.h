// Tipi e costanti di Winsock sopra i socket POSIX. Vedi core/net/AsyncSocketEx.h.
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

// Codici d'errore di Winsock = errno di Linux
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

// Eventi di WSAAsyncSelect
#ifndef FD_READ
#define FD_READ		0x01
#define FD_WRITE	0x02
#define FD_OOB		0x04
#define FD_ACCEPT	0x08
#define FD_CONNECT	0x10
#define FD_CLOSE	0x20
#endif

// Ultimo errore di rete del thread: lo imposta ogni chiamata di CAsyncSocketEx che fallisce.
int WSAGetLastError();
void WSASetLastError(int e);

// Overlapped I/O di Windows: non esiste su Linux. I tipi servono solo perche' le classi originali li dichiarano;
// CEMSocket lo usa solo se m_bUseOverlappedSend e' vero, e qui e' sempre falso (si usa l'invio normale).
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
