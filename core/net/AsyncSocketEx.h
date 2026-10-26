// LINUX (codice nuovo, stessa interfaccia pubblica di srchybrid/AsyncSocketEx.h): socket non bloccante a eventi.
//
// Windows consegna gli eventi con WSAAsyncSelect e una finestra nascosta per thread; qui li consegna un EventLoop su epoll.
// La semantica che eMule si aspetta e' quella di WSAAsyncSelect, riprodotta cosi':
//  - FD_READ: una notifica per volta. Dopo OnReceive non ne arrivano altre finche' l'applicazione non chiama Receive()
//    (che riarma la lettura: se resta altro da leggere, la notifica arriva di nuovo). Cosi' eMule puo' RIMANDARE la lettura per
//    limitare la banda in download senza che il ciclo giri a vuoto.
//  - FD_WRITE: alla connessione completata, e dopo un Send() che ha risposto WSAEWOULDBLOCK, quando si puo' scrivere di nuovo.
//  - FD_ACCEPT: una notifica per volta, riarmata da Accept().
//  - FD_CONNECT: a connessione completata o fallita (codice d'errore). FD_CLOSE: alla chiusura del peer, DOPO che i dati
//    ancora da leggere sono stati letti.
//  - TriggerEvent(): accoda un evento logico (anche FD_FORCEREAD) senza che il kernel c'entri.
//  - Close() dentro una callback, o delete this, e' sicuro: nessun evento arriva piu' a un socket chiuso.
// Non portati per ora: il sistema a strati (proxy), che arriva con il proxy SOCKS/HTTP (Bench, Fase 3).
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

// --- Sistema a strati (proxy): SOLO la parte dell'interfaccia che le classi derivate usano. Il vero sistema arriva con il task Proxy SOCKS e HTTP. ---
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

	// Crea il socket. Il ciclo di eventi e' quello del thread corrente (EventLoop::Current()).
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
	// Come l'originale: ritorna false con WSAEWOULDBLOCK quando la connessione e' in corso (indirizzo numerico) o in attesa
	// della risoluzione del nome; il risultato arriva con OnConnect(). true solo se la connessione e' gia' completa.
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
	// Il nome e' stato risolto. Ritornando false si annulla la connessione.
	virtual bool OnHostNameResolved(const SOCKADDR_IN *pSockAddr);

	SOCKET GetSocketHandle() const { return fd_; }
	BOOL TriggerEvent(long lEvent);
	AsyncSocketExState GetState() const { return state_; }
	bool IsLayerAttached() const { return false; }
	virtual void RemoveAllLayers() {}
	BOOL AddLayer(CAsyncSocketExLayer *) { WSASetLastError(EOPNOTSUPP); return FALSE; }		// strati (proxy) non ancora portati
	virtual int OnLayerCallback(std::vector<t_callbackMsg> &) { return 0; }

protected:
	void SetState(AsyncSocketExState s) { state_ = s; }
	void ArmRead() { read_armed_ = true; UpdateInterest(); }		// come il riarmo di FD_READ fatto da recv()
	void ArmWrite() { write_armed_ = true; UpdateInterest(); }	// FD_WRITE al prossimo momento scrivibile (solo dal thread del ciclo)
	bool InLoopThread() const { return loop_ && loop_->InLoopThread(); }
	void PostArmWrite();										// come ArmWrite, ma da un altro thread

private:
	void OnIo(uint32_t epoll_events) override;
	bool Register();
	void Unregister();
	void UpdateInterest();
	void FinishConnect(int err);
	void HandlePeerClose(int err);
	bool BeginConnect(const sockaddr *sa, socklen_t len);
	// Esegue f(this) nel ciclo, solo se questo socket (stessa registrazione) esiste ancora.
	template<class F> void PostIfAlive(F f);

	SOCKET fd_ = INVALID_SOCKET;
	uint64_t id_ = 0;
	emule::net::EventLoop *loop_ = nullptr;
	int type_ = SOCK_STREAM;
	ADDRESS_FAMILY family_ = AF_INET;
	long lEvent_ = 0;
	AsyncSocketExState state_ = notsock;
	bool read_armed_ = true, write_armed_ = false, close_notified_ = false;
	bool hold_ = false;									// connessione in preparazione (risoluzione del nome o esito immediato): nessun evento epoll finche' non e' pronta
	std::vector<sockaddr_storage> pending_addrs_;		// indirizzi ancora da provare dopo una risoluzione di nome
	uint64_t resolve_token_ = 0;						// invalida le risoluzioni in volo dopo Close()
};

// CAsyncSocket di MFC: gli stessi socket piu' i datagrammi (usato da ClientUDPSocket e UDPSocket).
class CAsyncSocket : public CAsyncSocketEx
{
public:
	int SendTo(const void *lpBuf, int nBufLen, UINT nHostPort, LPCTSTR lpszHostAddress = nullptr, int nFlags = 0);
	int SendTo(const void *lpBuf, int nBufLen, const LPSOCKADDR lpSockAddr, int nSockAddrLen, int nFlags = 0);
	int ReceiveFrom(void *lpBuf, int nBufLen, CString &rSocketAddress, UINT &rSocketPort, int nFlags = 0);
	int ReceiveFrom(void *lpBuf, int nBufLen, LPSOCKADDR lpSockAddr, int *lpSockAddrLen, int nFlags = 0);
};
