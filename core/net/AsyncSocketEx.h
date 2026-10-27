// LINUX (new code, same public interface as srchybrid/AsyncSocketEx.h): non-blocking event-driven socket.
//
// Windows delivers events with WSAAsyncSelect and a hidden window per thread; here an EventLoop on epoll delivers them.
// The semantics eMule expects are those of WSAAsyncSelect, reproduced like this:
//  - FD_READ: one notification at a time. After OnReceive no more arrive until the application calls Receive()
//      (which re-arms reading: if more is left to read, the notification arrives again). So eMule can DELAY reading to
//      limit download bandwidth without the loop spinning idle.
//  - FD_WRITE: when the connection completes, and after a Send() that answered WSAEWOULDBLOCK, when writing is possible again.
//  - FD_ACCEPT: one notification at a time, re-armed by Accept().
//  - FD_CONNECT: when the connection completes or fails (error code). FD_CLOSE: when the peer closes, AFTER the data
//      still to be read has been read.
//  - TriggerEvent(): queues a logical event (also FD_FORCEREAD) without the kernel being involved.
//  - Close() inside a callback, or delete this, is safe: no event arrives any more to a closed socket.
// Not ported for now: the layered system (proxy), which arrives with the SOCKS/HTTP proxy (Bench, Phase 3).
#pragma once
#include "StdAfx.h"
#include "win_net.h"
#include "net/EventLoop.h"
#include <string>
#include <vector>

#define FD_FORCEREAD (1 << 15)
#define FD_DEFAULT (FD_READ | FD_WRITE | FD_OOB | FD_ACCEPT | FD_CONNECT | FD_CLOSE)

enum AsyncSocketExState : uint8
{
	notsock,
	unconnected,
	connecting,
	listening,
	connected,
	closed,
	aborted,
	attached
};

// --- Layered system (proxy): ONLY the part of the interface that derived classes use. The real system arrives with the SOCKS and HTTP Proxy task. ---
class CAsyncSocketExLayer;
struct t_callbackMsg
{
	CAsyncSocketExLayer *pLayer;
	char *str;
	WPARAM wParam;
	LPARAM lParam;
	int nType;
};
#define LAYERCALLBACK_STATECHANGE 0
#define LAYERCALLBACK_LAYERSPECIFIC 1

class CAsyncSocketEx : public CObject, private emule::net::IoSource
{
	DECLARE_DYNAMIC(CAsyncSocketEx)
public:
	enum { receives = 0, sends = 1, both = 2 };		// CAsyncSocket::ShutDown

	CAsyncSocketEx();
	~CAsyncSocketEx() override;
	CAsyncSocketEx(const CAsyncSocketEx &) = delete;
	CAsyncSocketEx &operator=(const CAsyncSocketEx &) = delete;

	// Creates the socket. The event loop is that of the current thread (EventLoop::Current()).
	bool Create(UINT nSocketPort = 0, int nSocketType = SOCK_STREAM, long lEvent = FD_DEFAULT, const CString &sSocketAddress = CString(), ADDRESS_FAMILY nFamily = AF_INET, bool reusable = false);

	BOOL Attach(SOCKET hSocket, long lEvent = FD_DEFAULT);
	SOCKET Detach();
	static int GetLastError() { return WSAGetLastError(); }

	BOOL GetPeerName(CString &rPeerAddress, UINT &rPeerPort);
	BOOL GetPeerName(LPSOCKADDR lpSockAddr, int *lpSockAddrLen);
	bool GetSockName(CString &rSocketAddress, UINT &rSocketPort) const;
	BOOL GetSockName(LPSOCKADDR lpSockAddr, int *lpSockAddrLen) const;
	BOOL GetSockOpt(int nOptionName, void *lpOptionValue, int *lpOptionLen, int nLevel = SOL_SOCKET) const;
	BOOL SetSockOpt(int nOptionName, const void *lpOptionValue, int nOptionLen, int nLevel = SOL_SOCKET);
	ADDRESS_FAMILY GetFamily() const { return family_; }
	bool SetFamily(ADDRESS_FAMILY nFamily) { if (fd_ != INVALID_SOCKET) return false; family_ = nFamily; return true; }

	virtual BOOL Accept(CAsyncSocketEx &rConnectedSocket, LPSOCKADDR lpSockAddr = nullptr, int *lpSockAddrLen = nullptr);
	BOOL AsyncSelect(long lEvent = FD_DEFAULT);
	bool Bind(UINT nSocketPort, const CString &sSocketAddress = CString());
	BOOL Bind(const LPSOCKADDR lpSockAddr, int nSockAddrLen);
	virtual void Close();
	// As the original: returns false with WSAEWOULDBLOCK when the connection is in progress (numeric address) or waiting
	// for name resolution; the result arrives with OnConnect(). true only if the connection is already complete.
	virtual bool Connect(const CString &sHostAddress, UINT nHostPort);
	virtual BOOL Connect(const LPSOCKADDR lpSockAddr, int nSockAddrLen);
	BOOL IOCtl(long lCommand, DWORD *lpArgument);
	BOOL Listen(int nConnectionBacklog = 5);
	virtual int Receive(void *lpBuf, int nBufLen, int nFlags = 0);
	virtual int Send(const void *lpBuf, int nBufLen, int nFlags = 0);
	BOOL ShutDown(int nHow = sends);

	virtual void OnAccept(int nErrorCode);
	virtual void OnClose(int nErrorCode);
	virtual void OnConnect(int nErrorCode);
	virtual void OnReceive(int nErrorCode);
	virtual void OnSend(int nErrorCode);
	// The name was resolved. Returning false cancels the connection.
	virtual bool OnHostNameResolved(const SOCKADDR_IN *pSockAddr);

	SOCKET GetSocketHandle() const { return fd_; }
	BOOL TriggerEvent(long lEvent);
	AsyncSocketExState GetState() const { return state_; }
	bool IsLayerAttached() const { return false; }
	virtual void RemoveAllLayers() {}
	BOOL AddLayer(CAsyncSocketExLayer *) { WSASetLastError(EOPNOTSUPP); return FALSE; }		// layers (proxy) not ported yet
	virtual int OnLayerCallback(std::vector<t_callbackMsg> &) { return 0; }

protected:
	void SetState(AsyncSocketExState s) { state_ = s; }
	void ArmRead() { read_armed_ = true; UpdateInterest(); }		// like the FD_READ re-arm done by recv()
	void ArmWrite() { write_armed_ = true; UpdateInterest(); }	// FD_WRITE at the next writable moment (only from the loop thread)
	bool InLoopThread() const { return loop_ && loop_->InLoopThread(); }
	void PostArmWrite();										// like ArmWrite, but from another thread

private:
	void OnIo(uint32_t epoll_events) override;
	bool Register();
	void Unregister();
	void UpdateInterest();
	void FinishConnect(int err);
	void HandlePeerClose(int err);
	bool BeginConnect(const sockaddr *sa, socklen_t len);
	// Runs f(this) in the loop, only if this socket (same registration) still exists.
	template<class F> void PostIfAlive(F f);

	SOCKET fd_ = INVALID_SOCKET;
	uint64_t id_ = 0;
	emule::net::EventLoop *loop_ = nullptr;
	int type_ = SOCK_STREAM;
	ADDRESS_FAMILY family_ = AF_INET;
	long lEvent_ = 0;
	AsyncSocketExState state_ = notsock;
	bool read_armed_ = true, write_armed_ = false, close_notified_ = false;
	bool hold_ = false;									// connection being prepared (name resolution or immediate outcome): no epoll event until it is ready
	std::vector<sockaddr_storage> pending_addrs_;		// addresses still to try after a name resolution
	uint64_t resolve_token_ = 0;						// invalidates in-flight resolutions after Close()
};

// MFC CAsyncSocket: the same sockets plus datagrams (used by ClientUDPSocket and UDPSocket).
class CAsyncSocket : public CAsyncSocketEx
{
public:
	int SendTo(const void *lpBuf, int nBufLen, UINT nHostPort, LPCTSTR lpszHostAddress = nullptr, int nFlags = 0);
	int SendTo(const void *lpBuf, int nBufLen, const LPSOCKADDR lpSockAddr, int nSockAddrLen, int nFlags = 0);
	int ReceiveFrom(void *lpBuf, int nBufLen, CString &rSocketAddress, UINT &rSocketPort, int nFlags = 0);
	int ReceiveFrom(void *lpBuf, int nBufLen, LPSOCKADDR lpSockAddr, int *lpSockAddrLen, int nFlags = 0);
};
