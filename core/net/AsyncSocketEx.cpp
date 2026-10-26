// LINUX (codice nuovo): vedi AsyncSocketEx.h
#include "StdAfx.h"
#include "net/AsyncSocketEx.h"
#include <cstring>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>

static thread_local int g_wsa_error = 0;
int WSAGetLastError() { return g_wsa_error; }
void WSASetLastError(int e) { g_wsa_error = e; }

using emule::net::EventLoop;

IMPLEMENT_DYNAMIC(CAsyncSocketEx, CObject)

static int Err() { return errno == EINPROGRESS ? WSAEWOULDBLOCK : errno; }	// connect() non bloccante: Linux dice EINPROGRESS, Winsock WSAEWOULDBLOCK

static bool SetNonBlocking(int fd) { int fl = ::fcntl(fd, F_GETFL, 0); return fl >= 0 && ::fcntl(fd, F_SETFL, fl | O_NONBLOCK) == 0; }

CAsyncSocketEx::CAsyncSocketEx() = default;

CAsyncSocketEx::~CAsyncSocketEx() { CAsyncSocketEx::Close(); }

template<class F> void CAsyncSocketEx::PostIfAlive(F f)
{
	if (!loop_ || !id_) return;
	EventLoop *loop = loop_; uint64_t id = id_;
	loop->Post([this, loop, id, f]() {
		if (loop->Find(id) == static_cast<emule::net::IoSource*>(this)) f(this);	// solo un confronto di puntatori: non si tocca this se e' stato distrutto
	});
}

bool CAsyncSocketEx::Register()
{
	loop_ = EventLoop::Current();
	if (!loop_) { WSASetLastError(EINVAL); return false; }		// serve un EventLoop sul thread (ASSERT in debug)
	id_ = loop_->Add(this);
	read_armed_ = true; write_armed_ = false; close_notified_ = false;
	return true;
}

void CAsyncSocketEx::Unregister()
{
	if (loop_ && id_) loop_->Del(id_, fd_);
	id_ = 0;
}

// Eventi richiesti a epoll in base allo stato. Zero = socket "disarmato": non e' piu' in epoll e non puo' far girare il ciclo a vuoto.
void CAsyncSocketEx::UpdateInterest()
{
	if (!loop_ || !id_ || fd_ == INVALID_SOCKET) return;
	uint32_t ev = 0;
	if (close_notified_ || hold_) ev = 0;
	else if (type_ == SOCK_STREAM && (state_ == unconnected || state_ == notsock || state_ == closed || state_ == aborted)) ev = 0;	// un TCP non connesso segnala subito EPOLLHUP: va tenuto fuori
	else if (state_ == connecting) ev = EPOLLOUT;
	else if (state_ == listening) ev = read_armed_ && (lEvent_ & FD_ACCEPT) ? EPOLLIN : 0;
	else {
		if (read_armed_ && (lEvent_ & (FD_READ | FD_CLOSE))) ev |= EPOLLIN | EPOLLRDHUP;
		if (write_armed_ && (lEvent_ & FD_WRITE)) ev |= EPOLLOUT;
	}
	loop_->Update(id_, fd_, ev);
}

bool CAsyncSocketEx::Create(UINT nSocketPort, int nSocketType, long lEvent, const CString &sSocketAddress, ADDRESS_FAMILY nFamily, bool reusable)
{
	if (fd_ != INVALID_SOCKET) { WSASetLastError(WSAEALREADY); return false; }
	if (nFamily != AF_INET && nFamily != AF_INET6) { WSASetLastError(WSAEINVAL); return false; }	// AF_UNSPEC (famiglia decisa alla connessione): non portato
	int fd = ::socket(nFamily, nSocketType | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
	if (fd < 0) { WSASetLastError(Err()); return false; }
	fd_ = fd; family_ = nFamily; type_ = nSocketType; lEvent_ = lEvent;
	if (!Register()) { ::close(fd_); fd_ = INVALID_SOCKET; return false; }
	if (reusable && nSocketPort != 0) { int one = 1; ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one); }
	if (!Bind(nSocketPort, sSocketAddress)) { int e = WSAGetLastError(); Close(); WSASetLastError(e); return false; }
	state_ = unconnected;
	UpdateInterest();
	// Winsock manda FD_WRITE ai datagrammi appena pronti: ClientUDPSocket se ne serve per svuotare la coda di invio
	if (type_ == SOCK_DGRAM && (lEvent_ & FD_WRITE)) PostIfAlive([](CAsyncSocketEx *s) { if (s->lEvent_ & FD_WRITE) s->OnSend(0); });
	return true;
}

BOOL CAsyncSocketEx::Attach(SOCKET hSocket, long lEvent)
{
	if (hSocket == INVALID_SOCKET) return FALSE;
	if (fd_ != INVALID_SOCKET) { WSASetLastError(WSAEALREADY); return FALSE; }
	fd_ = hSocket; lEvent_ = lEvent;
	SetNonBlocking(fd_);
	int t = 0; socklen_t tl = sizeof t; if (::getsockopt(fd_, SOL_SOCKET, SO_TYPE, &t, &tl) == 0) type_ = t;
	sockaddr_storage ss = {}; socklen_t sl = sizeof ss; if (::getsockname(fd_, (sockaddr*)&ss, &sl) == 0) family_ = ss.ss_family;
	if (!Register()) { fd_ = INVALID_SOCKET; return FALSE; }
	state_ = attached;
	UpdateInterest();
	return TRUE;
}

SOCKET CAsyncSocketEx::Detach()
{
	SOCKET s = fd_;
	Unregister(); fd_ = INVALID_SOCKET; state_ = notsock; resolve_token_++;
	return s;
}

BOOL CAsyncSocketEx::AsyncSelect(long lEvent)
{
	lEvent_ = lEvent;
	UpdateInterest();
	return TRUE;
}

BOOL CAsyncSocketEx::Accept(CAsyncSocketEx &rConnected, LPSOCKADDR lpSockAddr, int *lpSockAddrLen)
{
	socklen_t len = lpSockAddrLen ? (socklen_t)*lpSockAddrLen : 0;
	int h = ::accept4(fd_, lpSockAddr, lpSockAddrLen ? &len : nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK);
	// Winsock riarma FD_ACCEPT con accept(), anche quando non c'e' nulla da accettare
	read_armed_ = true; UpdateInterest();
	if (h < 0) { WSASetLastError(Err()); return FALSE; }
	if (lpSockAddrLen) *lpSockAddrLen = (int)len;
	rConnected.fd_ = h; rConnected.type_ = type_; rConnected.family_ = family_; rConnected.lEvent_ = FD_DEFAULT;
	if (!rConnected.Register()) { ::close(h); rConnected.fd_ = INVALID_SOCKET; return FALSE; }
	rConnected.state_ = connected;
	rConnected.UpdateInterest();
	return TRUE;
}

bool CAsyncSocketEx::Bind(UINT nSocketPort, const CString &sSocketAddress)
{
	if (family_ == AF_INET6) {
		sockaddr_in6 a = {}; a.sin6_family = AF_INET6; a.sin6_port = htons((uint16_t)nSocketPort); a.sin6_addr = in6addr_any;
		if (!sSocketAddress.IsEmpty() && ::inet_pton(AF_INET6, sSocketAddress, &a.sin6_addr) != 1) { WSASetLastError(WSAEINVAL); return false; }
		return Bind((LPSOCKADDR)&a, sizeof a) != FALSE;
	}
	sockaddr_in a = {}; a.sin_family = AF_INET; a.sin_port = htons((uint16_t)nSocketPort); a.sin_addr.s_addr = htonl(INADDR_ANY);
	if (!sSocketAddress.IsEmpty() && ::inet_pton(AF_INET, sSocketAddress, &a.sin_addr) != 1) { WSASetLastError(WSAEINVAL); return false; }
	return Bind((LPSOCKADDR)&a, sizeof a) != FALSE;
}

BOOL CAsyncSocketEx::Bind(const LPSOCKADDR lpSockAddr, int nSockAddrLen)
{
	if (::bind(fd_, lpSockAddr, (socklen_t)nSockAddrLen) < 0) { WSASetLastError(Err()); return FALSE; }
	return TRUE;
}

void CAsyncSocketEx::Close()
{
	resolve_token_++;										// una risoluzione di nome ancora in volo non trova piu' nessuno
	pending_addrs_.clear();
	Unregister();											// dopo questo nessun evento raggiunge piu' il socket
	if (fd_ != INVALID_SOCKET) { ::close(fd_); fd_ = INVALID_SOCKET; }
	state_ = closed;
	read_armed_ = true; write_armed_ = false; close_notified_ = false;
}

bool CAsyncSocketEx::BeginConnect(const sockaddr *sa, socklen_t len)
{
	if (::connect(fd_, sa, len) == 0) {
		state_ = connecting; hold_ = true;					// anche a connessione immediata l'esito arriva come evento FD_CONNECT
		PostIfAlive([](CAsyncSocketEx *s) { s->FinishConnect(0); });
		return true;
	}
	if (errno == EINPROGRESS) {
		state_ = connecting; hold_ = false; UpdateInterest();
		WSASetLastError(WSAEWOULDBLOCK);
		return false;
	}
	WSASetLastError(Err());
	return false;
}

BOOL CAsyncSocketEx::Connect(const LPSOCKADDR lpSockAddr, int nSockAddrLen)
{
	if (fd_ == INVALID_SOCKET) { WSASetLastError(WSAEINVAL); return FALSE; }
	return BeginConnect(lpSockAddr, (socklen_t)nSockAddrLen) ? TRUE : FALSE;
}

bool CAsyncSocketEx::Connect(const CString &sHostAddress, UINT nHostPort)
{
	if (fd_ == INVALID_SOCKET || sHostAddress.IsEmpty()) { WSASetLastError(WSAEINVAL); return false; }
	if (family_ == AF_INET) {
		sockaddr_in a = {}; a.sin_family = AF_INET; a.sin_port = htons((uint16_t)nHostPort);
		if (::inet_pton(AF_INET, sHostAddress, &a.sin_addr) == 1) return Connect((LPSOCKADDR)&a, sizeof a) != FALSE;
	} else {
		sockaddr_in6 a = {}; a.sin6_family = AF_INET6; a.sin6_port = htons((uint16_t)nHostPort);
		if (::inet_pton(AF_INET6, sHostAddress, &a.sin6_addr) == 1) return Connect((LPSOCKADDR)&a, sizeof a) != FALSE;
	}
	// un nome: lo risolve un thread, il resto avviene nel ciclo. Come WSAAsyncGetHostByName: ritorna false con WSAEWOULDBLOCK.
	EventLoop *loop = loop_; uint64_t id = id_, token = ++resolve_token_;
	state_ = connecting; hold_ = true;						// finche' il nome non e' risolto non c'e' nulla da attendere in epoll
	loop->ResolveAsync((LPCTSTR)sHostAddress, std::to_string(nHostPort), family_, [this, loop, id, token](int gai, std::vector<sockaddr_storage> addrs) {
		if (loop->Find(id) != static_cast<emule::net::IoSource*>(this) || token != resolve_token_) return;	// chiuso nel frattempo
		if (gai != 0 || addrs.empty()) { hold_ = false; FinishConnect(ENOENT); return; }	// nome non risolto
		if (family_ == AF_INET && !OnHostNameResolved((const SOCKADDR_IN*)&addrs[0])) { hold_ = false; return; }
		if (loop->Find(id) != static_cast<emule::net::IoSource*>(this)) return;
		pending_addrs_.assign(addrs.begin() + 1, addrs.end());
		const sockaddr *sa = (const sockaddr*)&addrs[0];
		socklen_t sl = sa->sa_family == AF_INET6 ? sizeof(sockaddr_in6) : sizeof(sockaddr_in);
		if (!BeginConnect(sa, sl) && WSAGetLastError() != WSAEWOULDBLOCK) { hold_ = false; FinishConnect(WSAGetLastError()); }
	});
	WSASetLastError(WSAEWOULDBLOCK);
	return false;
}

BOOL CAsyncSocketEx::Listen(int nConnectionBacklog)
{
	if (::listen(fd_, nConnectionBacklog) < 0) { WSASetLastError(Err()); return FALSE; }
	state_ = listening; read_armed_ = true; UpdateInterest();
	return TRUE;
}

int CAsyncSocketEx::Receive(void *lpBuf, int nBufLen, int nFlags)
{
	ssize_t n = ::recv(fd_, lpBuf, (size_t)nBufLen, nFlags);
	if (n < 0) WSASetLastError(Err());
	// Winsock riarma FD_READ con recv(): se resta altro da leggere (o c'e' una chiusura da notificare) la notifica ritorna
	read_armed_ = true; UpdateInterest();
	return (int)n;
}

int CAsyncSocketEx::Send(const void *lpBuf, int nBufLen, int nFlags)
{
	ssize_t n = ::send(fd_, lpBuf, (size_t)nBufLen, nFlags | MSG_NOSIGNAL);
	if (n < 0) {
		WSASetLastError(Err());
		if (errno == EAGAIN || errno == EWOULDBLOCK) {			// FD_WRITE quando si potra' scrivere di nuovo
			// Send() puo' essere chiamata dal thread del throttler, non solo da quello del ciclo: lo stato e la tabella degli eventi
			// si toccano solo nel thread del ciclo, quindi da fuori il riarmo e' un compito Post().
			if (loop_ && loop_->InLoopThread()) { write_armed_ = true; UpdateInterest(); }
			else PostIfAlive([](CAsyncSocketEx *s) { s->write_armed_ = true; s->UpdateInterest(); });
		}
	}
	return (int)n;
}

BOOL CAsyncSocketEx::ShutDown(int nHow)
{
	int how = nHow == receives ? SHUT_RD : nHow == sends ? SHUT_WR : SHUT_RDWR;
	if (::shutdown(fd_, how) < 0) { WSASetLastError(Err()); return FALSE; }
	return TRUE;
}

BOOL CAsyncSocketEx::IOCtl(long lCommand, DWORD *lpArgument)
{
	if (lCommand == (long)FIONBIO) return SetNonBlocking(fd_) ? TRUE : FALSE;		// i nostri socket sono sempre non bloccanti
	int arg = (int)(lpArgument ? *lpArgument : 0);
	if (::ioctl(fd_, (unsigned long)lCommand, &arg) < 0) { WSASetLastError(Err()); return FALSE; }
	if (lpArgument) *lpArgument = (DWORD)arg;
	return TRUE;
}

BOOL CAsyncSocketEx::GetPeerName(LPSOCKADDR sa, int *len)
{
	socklen_t l = (socklen_t)*len;
	if (::getpeername(fd_, sa, &l) < 0) { WSASetLastError(Err()); return FALSE; }
	*len = (int)l; return TRUE;
}
BOOL CAsyncSocketEx::GetPeerName(CString &addr, UINT &port)
{
	sockaddr_storage ss = {}; int l = sizeof ss;
	if (!GetPeerName((LPSOCKADDR)&ss, &l)) return FALSE;
	char b[INET6_ADDRSTRLEN] = {};
	if (ss.ss_family == AF_INET6) { ::inet_ntop(AF_INET6, &((sockaddr_in6*)&ss)->sin6_addr, b, sizeof b); port = ntohs(((sockaddr_in6*)&ss)->sin6_port); }
	else { ::inet_ntop(AF_INET, &((sockaddr_in*)&ss)->sin_addr, b, sizeof b); port = ntohs(((sockaddr_in*)&ss)->sin_port); }
	addr = b; return TRUE;
}
BOOL CAsyncSocketEx::GetSockName(LPSOCKADDR sa, int *len) const
{
	socklen_t l = (socklen_t)*len;
	if (::getsockname(fd_, sa, &l) < 0) { WSASetLastError(Err()); return FALSE; }
	*len = (int)l; return TRUE;
}
bool CAsyncSocketEx::GetSockName(CString &addr, UINT &port) const
{
	sockaddr_storage ss = {}; int l = sizeof ss;
	if (!GetSockName((LPSOCKADDR)&ss, &l)) return false;
	char b[INET6_ADDRSTRLEN] = {};
	if (ss.ss_family == AF_INET6) { ::inet_ntop(AF_INET6, &((sockaddr_in6*)&ss)->sin6_addr, b, sizeof b); port = ntohs(((sockaddr_in6*)&ss)->sin6_port); }
	else { ::inet_ntop(AF_INET, &((sockaddr_in*)&ss)->sin_addr, b, sizeof b); port = ntohs(((sockaddr_in*)&ss)->sin_port); }
	addr = b; return true;
}
BOOL CAsyncSocketEx::GetSockOpt(int name, void *val, int *len, int level) const
{
	socklen_t l = (socklen_t)*len;
	if (::getsockopt(fd_, level, name, val, &l) < 0) { WSASetLastError(Err()); return FALSE; }
	*len = (int)l; return TRUE;
}
BOOL CAsyncSocketEx::SetSockOpt(int name, const void *val, int len, int level)
{
	if (::setsockopt(fd_, level, name, val, (socklen_t)len) < 0) { WSASetLastError(Err()); return FALSE; }
	return TRUE;
}

BOOL CAsyncSocketEx::TriggerEvent(long lEvent)
{
	if (fd_ == INVALID_SOCKET || !id_) return FALSE;
	PostIfAlive([lEvent](CAsyncSocketEx *s) {
		if ((lEvent & (FD_READ | FD_FORCEREAD)) && (s->lEvent_ & FD_READ)) { EventLoop *l = s->loop_; uint64_t id = s->id_; s->OnReceive(0); if (l->Find(id) != static_cast<emule::net::IoSource*>(s)) return; }
		if ((lEvent & FD_WRITE) && (s->lEvent_ & FD_WRITE)) { EventLoop *l = s->loop_; uint64_t id = s->id_; s->OnSend(0); if (l->Find(id) != static_cast<emule::net::IoSource*>(s)) return; }
		if ((lEvent & FD_ACCEPT) && (s->lEvent_ & FD_ACCEPT)) { EventLoop *l = s->loop_; uint64_t id = s->id_; s->OnAccept(0); if (l->Find(id) != static_cast<emule::net::IoSource*>(s)) return; }
		if ((lEvent & FD_CONNECT) && (s->lEvent_ & FD_CONNECT)) { EventLoop *l = s->loop_; uint64_t id = s->id_; s->OnConnect(0); if (l->Find(id) != static_cast<emule::net::IoSource*>(s)) return; }
		if ((lEvent & FD_CLOSE) && (s->lEvent_ & FD_CLOSE)) s->OnClose(0);
	});
	return TRUE;
}

// ---- consegna degli eventi ----------------------------------------------------------------------------------------

void CAsyncSocketEx::FinishConnect(int err)
{
	EventLoop *loop = loop_; uint64_t id = id_;
	auto alive = [&]() { return loop->Find(id) == static_cast<emule::net::IoSource*>(this); };
	if (err != 0 && !pending_addrs_.empty()) {				// c'e' un altro indirizzo del nome: lo si prova prima di arrendersi
		sockaddr_storage next = pending_addrs_.front(); pending_addrs_.erase(pending_addrs_.begin());
		socklen_t sl = next.ss_family == AF_INET6 ? sizeof(sockaddr_in6) : sizeof(sockaddr_in);
		if (::connect(fd_, (sockaddr*)&next, sl) == 0 || errno == EINPROGRESS || errno == EALREADY) { state_ = connecting; hold_ = false; UpdateInterest(); return; }
		err = Err();											// su Linux un connect() fallito si puo' ritentare sullo stesso socket (stessa famiglia)
	}
	if (err != 0) {
		state_ = aborted; hold_ = false; close_notified_ = true; UpdateInterest();
		if (lEvent_ & FD_CONNECT) OnConnect(err);
		return;
	}
	state_ = connected; hold_ = false; write_armed_ = false; read_armed_ = true;
	UpdateInterest();
	if (lEvent_ & FD_CONNECT) { OnConnect(0); if (!alive()) return; }
	if (lEvent_ & FD_WRITE) { OnSend(0); if (!alive()) return; }	// FD_WRITE segue sempre la connessione
	UpdateInterest();
}

// Il peer ha chiuso (o c'e' un errore). I dati ancora in coda si consegnano prima: FD_CLOSE solo quando non resta nulla da leggere.
void CAsyncSocketEx::HandlePeerClose(int err)
{
	if (close_notified_) return;
	char c;
	ssize_t n = ::recv(fd_, &c, 1, MSG_PEEK);
	if (n > 0) return;										// restano dati: l'applicazione li legge (la notifica di lettura e' gia' in corso o armata), poi si torna qui
	if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) err = errno;
	close_notified_ = true;
	state_ = err ? aborted : closed;
	UpdateInterest();										// fuori da epoll
	if (lEvent_ & FD_CLOSE) OnClose(err);
}

void CAsyncSocketEx::OnIo(uint32_t ev)
{
	if (fd_ == INVALID_SOCKET) return;
	EventLoop *loop = loop_; uint64_t id = id_;
	auto alive = [&]() { return loop->Find(id) == static_cast<emule::net::IoSource*>(this); };

	if (state_ == listening) {
		if (ev & EPOLLIN) { read_armed_ = false; UpdateInterest(); OnAccept(0); }
		return;
	}
	if (state_ == connecting) {
		if (ev & (EPOLLOUT | EPOLLERR | EPOLLHUP)) {
			int err = 0; socklen_t l = sizeof err;
			::getsockopt(fd_, SOL_SOCKET, SO_ERROR, &err, &l);
			if (err == 0 && (ev & (EPOLLERR | EPOLLHUP)) && !(ev & EPOLLOUT)) err = ECONNREFUSED;
			FinishConnect(err);
		}
		return;
	}
	const bool hup = (ev & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) != 0;
	if ((ev & EPOLLIN) && read_armed_ && (lEvent_ & (FD_READ | FD_FORCEREAD))) {
		bool eof_only = false;								// chiusura senza dati da leggere: Winsock manda solo FD_CLOSE, non FD_READ
		if (hup) { char c; ssize_t n = ::recv(fd_, &c, 1, MSG_PEEK); eof_only = n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK); }
		if (!eof_only) {
			read_armed_ = false; UpdateInterest();			// una notifica per volta: la riarma Receive()
			OnReceive(0);
			if (!alive()) return;
		}
	}
	if ((ev & EPOLLOUT) && write_armed_) {
		write_armed_ = false; UpdateInterest();
		OnSend(0);
		if (!alive()) return;
	}
	if (hup) {
		int err = 0;
		if (ev & EPOLLERR) { socklen_t l = sizeof err; ::getsockopt(fd_, SOL_SOCKET, SO_ERROR, &err, &l); }
		HandlePeerClose(err);
	}
}

void CAsyncSocketEx::OnAccept(int) {}
void CAsyncSocketEx::OnClose(int) {}
void CAsyncSocketEx::OnConnect(int) {}
void CAsyncSocketEx::OnReceive(int) {}
void CAsyncSocketEx::OnSend(int) {}
bool CAsyncSocketEx::OnHostNameResolved(const SOCKADDR_IN *) { return true; }

void CAsyncSocketEx::PostArmWrite() { PostIfAlive([](CAsyncSocketEx *s) { s->ArmWrite(); }); }

// ---- CAsyncSocket: datagrammi -------------------------------------------------------------------------------------

int CAsyncSocket::SendTo(const void *lpBuf, int nBufLen, const LPSOCKADDR lpSockAddr, int nSockAddrLen, int nFlags)
{
	ssize_t n = ::sendto(GetSocketHandle(), lpBuf, (size_t)nBufLen, nFlags | MSG_NOSIGNAL, lpSockAddr, (socklen_t)nSockAddrLen);
	if (n < 0) { WSASetLastError(Err()); if (errno == EAGAIN || errno == EWOULDBLOCK) { if (InLoopThread()) ArmWrite(); else PostArmWrite(); } }
	return (int)n;
}

int CAsyncSocket::SendTo(const void *lpBuf, int nBufLen, UINT nHostPort, LPCTSTR lpszHostAddress, int nFlags)
{
	sockaddr_in a = {}; a.sin_family = AF_INET; a.sin_port = htons((uint16_t)nHostPort);
	if (!lpszHostAddress || ::inet_pton(AF_INET, lpszHostAddress, &a.sin_addr) != 1) { WSASetLastError(WSAEINVAL); return -1; }
	return SendTo(lpBuf, nBufLen, (LPSOCKADDR)&a, sizeof a, nFlags);
}

int CAsyncSocket::ReceiveFrom(void *lpBuf, int nBufLen, LPSOCKADDR lpSockAddr, int *lpSockAddrLen, int nFlags)
{
	socklen_t l = lpSockAddrLen ? (socklen_t)*lpSockAddrLen : 0;
	ssize_t n = ::recvfrom(GetSocketHandle(), lpBuf, (size_t)nBufLen, nFlags, lpSockAddr, lpSockAddrLen ? &l : nullptr);
	if (n < 0) WSASetLastError(Err());
	if (lpSockAddrLen) *lpSockAddrLen = (int)l;
	ArmRead();										// riarma la lettura, come Receive()
	return (int)n;
}

int CAsyncSocket::ReceiveFrom(void *lpBuf, int nBufLen, CString &rAddr, UINT &rPort, int nFlags)
{
	sockaddr_storage ss = {}; int l = sizeof ss;
	int n = ReceiveFrom(lpBuf, nBufLen, (LPSOCKADDR)&ss, &l, nFlags);
	if (n >= 0) {
		char b[INET6_ADDRSTRLEN] = {};
		if (ss.ss_family == AF_INET6) { ::inet_ntop(AF_INET6, &((sockaddr_in6*)&ss)->sin6_addr, b, sizeof b); rPort = ntohs(((sockaddr_in6*)&ss)->sin6_port); }
		else { ::inet_ntop(AF_INET, &((sockaddr_in*)&ss)->sin_addr, b, sizeof b); rPort = ntohs(((sockaddr_in*)&ss)->sin_port); }
		rAddr = b;
	}
	return n;
}
