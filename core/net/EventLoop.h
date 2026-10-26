// LINUX (codice nuovo): ciclo di eventi su epoll. Sostituisce la finestra nascosta e WSAAsyncSelect di CAsyncSocketEx
// e i timer di finestra (SetTimer) del thread principale di eMule.
//
// Modello a thread (Bench #560): UN thread logico possiede il ciclo e tutti i socket. Gli altri thread parlano con esso
// solo tramite Post(), che e' l'unica funzione sicura da chiamare da fuori.
#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <atomic>
#include <memory>
#include <queue>
#include <string>
#include <thread>
#include <vector>
#include <sys/socket.h>

namespace emule::net {

// Chi riceve gli eventi del kernel per un descrittore.
class IoSource
{
public:
	virtual ~IoSource() = default;
	virtual void OnIo(uint32_t epoll_events) = 0;
};

class EventLoop
{
public:
	EventLoop();
	~EventLoop();
	EventLoop(const EventLoop &) = delete;
	EventLoop &operator=(const EventLoop &) = delete;

	// Il ciclo del thread corrente: i socket vi si registrano da soli. SetCurrent(nullptr) lo toglie.
	static EventLoop *Current();
	static void SetCurrent(EventLoop *l);

	// Registra una sorgente: ritorna un id (mai 0) valido finche' non si chiama Del. Non iscrive ancora nulla a epoll.
	// Dopo Del nessun evento gia' letto dal kernel ma non ancora consegnato raggiunge piu' la sorgente.
	uint64_t Add(IoSource *src);
	// Imposta gli eventi richiesti per il descrittore fd della sorgente. events == 0 toglie fd da epoll (la sorgente resta registrata).
	// Serve a "disarmare" un socket: epoll segnala sempre HUP/ERR e una lettura rimandata dall'applicazione farebbe girare il ciclo a vuoto.
	bool Update(uint64_t id, int fd, uint32_t events);
	void Del(uint64_t id, int fd);
	IoSource *Find(uint64_t id) const;					// nullptr se non piu' registrato

	// Risoluzione di nomi (getaddrinfo) in un thread; cb gira nel thread del ciclo. family: AF_INET, AF_INET6 o AF_UNSPEC.
	using ResolveCb = std::function<void(int gai_error, std::vector<sockaddr_storage> addrs)>;
	void ResolveAsync(std::string host, std::string service, int family, ResolveCb cb);

	// Una iterazione: attende fino a timeout_ms (-1 = senza limite, 0 = non attende), consegna eventi, compiti e timer.
	// Ritorna il numero di eventi di I/O consegnati.
	int RunOnce(int timeout_ms);
	void Run();											// finche' Stop()
	void Stop();										// sicura da qualunque thread

	// Esegue fn nel thread del ciclo. Sicura da qualunque thread.
	void Post(std::function<void()> fn);

	// Timer nel thread del ciclo. ms > 0. repeat = si ripete ogni ms. Ritorna un id per CancelTimer.
	uint64_t AddTimer(unsigned ms, std::function<void()> fn, bool repeat = false);
	void CancelTimer(uint64_t id);

	size_t sources() const { return sources_.size(); }
	// Vero se il thread chiamante e' quello che fa girare il ciclo. Serve a sapere se si puo' toccare lo stato dei socket direttamente
	// o se bisogna passare da Post() (es. Send() chiamata dal thread del throttler).
	bool InLoopThread() const { return owner_.load() == std::this_thread::get_id(); }

private:
	struct Timer { uint64_t id; int64_t due_ms; unsigned period_ms; std::function<void()> fn; };
	int64_t NowMs() const;
	int NextTimeoutMs(int requested) const;
	void RunTimers();
	void RunPosted();

	int ep_ = -1, wake_ = -1;
	uint64_t next_id_ = 1;
	struct Src { IoSource *s; bool in_ep; };
	std::map<uint64_t, Src> sources_;
	struct Resolver { std::thread th; std::shared_ptr<std::atomic<bool>> done; };
	std::vector<Resolver> resolvers_;
	std::vector<Timer> timers_;
	uint64_t next_timer_ = 1;
	std::mutex pm_;
	std::vector<std::function<void()>> posted_;
	bool stop_ = false;
	std::atomic<std::thread::id> owner_{std::this_thread::get_id()};
};

// Impone il ciclo corrente per la durata di uno scope.
class ScopedLoop
{
public:
	explicit ScopedLoop(EventLoop &l) : prev_(EventLoop::Current()) { EventLoop::SetCurrent(&l); }
	~ScopedLoop() { EventLoop::SetCurrent(prev_); }
private:
	EventLoop *prev_;
};

} // namespace emule::net
