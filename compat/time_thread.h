// Tempo e thread MFC. Solo cio che usa il sorgente (censimento Bench #565-567).
#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <ctime>
#include <functional>
#include <mutex>
#include <thread>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>

inline DWORD GetTickCount()
{
	using namespace std::chrono;
	return (DWORD)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
inline ULONGLONG GetTickCount64() { return (ULONGLONG)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
inline void Sleep(DWORD ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

class CTimeSpan
{
	long long s_;
public:
	CTimeSpan(long long secs = 0) : s_(secs) {}
	CTimeSpan(long days, int hours, int mins, int secs) : s_(((long long)days * 24 + hours) * 3600 + mins * 60 + secs) {}
	long long GetTotalSeconds() const { return s_; }
	long long GetTotalMinutes() const { return s_ / 60; }
	long long GetTotalHours() const { return s_ / 3600; }
	long long GetDays() const { return s_ / 86400; }
	int GetHours() const { return (int)(s_ / 3600 % 24); }
	int GetMinutes() const { return (int)(s_ / 60 % 60); }
	int GetSeconds() const { return (int)(s_ % 60); }
	CTimeSpan operator+(CTimeSpan o) const { return CTimeSpan(s_ + o.s_); }
	CTimeSpan operator-(CTimeSpan o) const { return CTimeSpan(s_ - o.s_); }
	bool operator==(CTimeSpan o) const { return s_ == o.s_; }
	bool operator<(CTimeSpan o) const { return s_ < o.s_; }
};

class CTime
{
	time_t t_;
public:
	CTime() : t_(0) {}
	CTime(time_t t) : t_(t) {}
	static CTime GetCurrentTime() { return CTime(time(nullptr)); }
	time_t GetTime() const { return t_; }
	struct tm *GetLocalTm(struct tm *out) const { return localtime_r(&t_, out); }
	struct tm *GetGmtTm(struct tm *out) const { return gmtime_r(&t_, out); }
	int GetYear() const { struct tm t; localtime_r(&t_, &t); return t.tm_year + 1900; }
	int GetMonth() const { struct tm t; localtime_r(&t_, &t); return t.tm_mon + 1; }
	int GetDay() const { struct tm t; localtime_r(&t_, &t); return t.tm_mday; }
	int GetHour() const { struct tm t; localtime_r(&t_, &t); return t.tm_hour; }
	int GetMinute() const { struct tm t; localtime_r(&t_, &t); return t.tm_min; }
	int GetSecond() const { struct tm t; localtime_r(&t_, &t); return t.tm_sec; }
	int GetDayOfWeek() const { struct tm t; localtime_r(&t_, &t); return t.tm_wday + 1; }	// MFC: 1 = domenica
	CString Format(const char *fmt) const { struct tm t; localtime_r(&t_, &t); char b[256]; size_t n = strftime(b, sizeof b, fmt, &t); return CString(b, (int)n); }
	CString FormatGmt(const char *fmt) const { struct tm t; gmtime_r(&t_, &t); char b[256]; size_t n = strftime(b, sizeof b, fmt, &t); return CString(b, (int)n); }
	CTimeSpan operator-(const CTime &o) const { return CTimeSpan((long long)t_ - o.t_); }
	CTime operator-(CTimeSpan s) const { return CTime((time_t)(t_ - s.GetTotalSeconds())); }
	CTime operator+(CTimeSpan s) const { return CTime((time_t)(t_ + s.GetTotalSeconds())); }
	bool operator==(const CTime &o) const { return t_ == o.t_; }
	bool operator!=(const CTime &o) const { return t_ != o.t_; }
	bool operator<(const CTime &o) const { return t_ < o.t_; }
	bool operator>(const CTime &o) const { return t_ > o.t_; }
	bool operator<=(const CTime &o) const { return t_ <= o.t_; }
	bool operator>=(const CTime &o) const { return t_ >= o.t_; }
};

// Evento di sincronizzazione (CEvent): manuale (bManualReset) o a reset automatico.
class CEvent
{
	std::mutex m_; std::condition_variable cv_; bool set_;
	bool manual_;
public:
	explicit CEvent(bool initiallyOwn = false, bool manualReset = false, const char * = nullptr) : set_(initiallyOwn), manual_(manualReset) {}
	// notify con il mutex preso: chi attende non puo' tornare e distruggere l'evento mentre la notifica e' ancora in corso
	bool SetEvent() { std::lock_guard<std::mutex> l(m_); set_ = true; cv_.notify_all(); return true; }
	bool ResetEvent() { std::lock_guard<std::mutex> l(m_); set_ = false; return true; }
	bool PulseEvent() { SetEvent(); ResetEvent(); return true; }
	bool Lock(DWORD timeoutMs = 0xFFFFFFFF)
	{
		std::unique_lock<std::mutex> l(m_);
		if (timeoutMs == 0xFFFFFFFF) cv_.wait(l, [this] { return set_; });
		else if (!cv_.wait_for(l, std::chrono::milliseconds(timeoutMs), [this] { return set_; })) return false;
		if (!manual_) set_ = false;
		return true;
	}
	bool Unlock() { return true; }
	operator HANDLE() const { return const_cast<CEvent*>(this); }			// il codice originale passa un CEvent dove serve un HANDLE
};
// WaitForSingleObject vale solo per i CEvent (gli unici handle che il core attende)
inline DWORD WaitForSingleObject(HANDLE h, DWORD ms) { return static_cast<CEvent*>(h)->Lock(ms) ? WAIT_OBJECT_0 : WAIT_TIMEOUT; }
inline DWORD timeGetTime() { return GetTickCount(); }

// Priorita' dei thread: i valori MFC si traducono in nice (solo per il thread corrente).
#define THREAD_PRIORITY_IDLE			(-15)
#define THREAD_PRIORITY_LOWEST			(-2)
#define THREAD_PRIORITY_BELOW_NORMAL	(-1)
#define THREAD_PRIORITY_NORMAL			0
#define THREAD_PRIORITY_ABOVE_NORMAL	1
#define THREAD_PRIORITY_HIGHEST			2
#define CREATE_SUSPENDED				0x00000004
#define STILL_ACTIVE					259

struct CRuntimeClass;
class CObject
{
public:
	virtual ~CObject() = default;
	virtual const CRuntimeClass *GetRuntimeClass() const;
	bool IsKindOf(const CRuntimeClass *pClass) const;
	static const CRuntimeClass classCObject;
	static const CRuntimeClass *RtcGet() { return &classCObject; }
};
struct CRuntimeClass { const char *name; CObject *(*create)(); const CRuntimeClass *base; CObject *CreateObject() const { return create ? create() : nullptr; } };
inline const CRuntimeClass CObject::classCObject = {"CObject", nullptr, nullptr};
inline const CRuntimeClass *CObject::GetRuntimeClass() const { return &classCObject; }
inline bool CObject::IsKindOf(const CRuntimeClass *pClass) const
{
	for (const CRuntimeClass *c = GetRuntimeClass(); c; c = c->base)
		if (c == pClass)
			return true;
	return false;
}
// La base puo' non essere un CObject (CAsyncSocketEx, CException...): allora la catena finisce li'.
template<class B> inline const CRuntimeClass *RtcBase() { if constexpr (requires { B::RtcGet(); }) return B::RtcGet(); else return nullptr; }
#define RUNTIME_CLASS(c)				(&c::class##c)
#define DECLARE_DYNAMIC(c)				public: static const CRuntimeClass class##c; static const CRuntimeClass *RtcGet() { return &class##c; } virtual const CRuntimeClass *GetRuntimeClass() const;
#define DECLARE_DYNCREATE(c)			DECLARE_DYNAMIC(c) static CObject *CreateObject() { return new c; }
#define IMPLEMENT_DYNAMIC(c, base_)		const CRuntimeClass c::class##c = {#c, nullptr, RtcBase<base_>()}; const CRuntimeClass *c::GetRuntimeClass() const { return &c::class##c; }
#define IMPLEMENT_DYNCREATE(c, base_)	const CRuntimeClass c::class##c = {#c, &c::CreateObject, RtcBase<base_>()}; const CRuntimeClass *c::GetRuntimeClass() const { return &c::class##c; }

typedef UINT (*AFX_THREADPROC)(LPVOID);

// CWinThread: thread con InitInstance/Run/ExitInstance come in MFC. La procedura puo' essere una funzione o una sottoclasse.
class CWinThread : public CObject
{
public:
	bool m_bAutoDelete = true;					// MFC: vero per predefinizione. Con CREATE_SUSPENDED si puo' mettere a false prima di ResumeThread()
	DWORD m_nThreadID = 0;
	int m_nPriority = 0;
	CWinThread() = default;
	CWinThread(AFX_THREADPROC proc, LPVOID param) : proc_(proc), param_(param) {}
	~CWinThread() override { Join(); }
	virtual bool InitInstance() { return true; }
	virtual int ExitInstance() { return exit_code_; }
	virtual int Run() { return proc_ ? (int)proc_(param_) : 0; }
	bool CreateThread(DWORD flags = 0)
	{
		// Il corpo del thread parte solo quando th_ e' stato assegnato (suspended_ interno): altrimenti un thread che finisce
		// subito e si autodistrugge toccherebbe th_ mentre il creatore lo sta ancora assegnando.
		{ std::lock_guard<std::mutex> l(m_); suspended_ = true; }
		th_ = std::thread([this] { Body(); });
		if (!(flags & CREATE_SUSPENDED)) ResumeThread();
		return true;
	}
	DWORD ResumeThread() { std::lock_guard<std::mutex> l(m_); suspended_ = false; cv_.notify_all(); return 1; }	// notify col mutex preso: con m_bAutoDelete il thread puo' distruggere this subito dopo
	bool SetThreadPriority(int p) { m_nPriority = p; if (running_) apply_(); return true; }	// effetto reale solo dall'interno del thread
	void Join() { if (th_.joinable() && th_.get_id() != std::this_thread::get_id()) th_.join(); else if (th_.joinable()) th_.detach(); }
	bool IsRunning() const { return running_; }
private:
	void apply_() { setpriority(PRIO_PROCESS, (id_t)syscall(SYS_gettid), m_nPriority < 0 ? (m_nPriority <= -15 ? 19 : -m_nPriority * 5) : 0); }
	void Body()
	{
		{ std::unique_lock<std::mutex> l(m_); cv_.wait(l, [this] { return !suspended_; }); }
		m_nThreadID = (DWORD)syscall(SYS_gettid); running_ = true; apply_();
		if (InitInstance()) exit_code_ = Run();
		exit_code_ = ExitInstance();
		running_ = false;
		if (m_bAutoDelete) { th_.detach(); delete this; }		// come MFC: il thread con m_bAutoDelete si distrugge alla fine. Dopo questo punto nessun accesso a this
	}
	AFX_THREADPROC proc_ = nullptr; LPVOID param_ = nullptr;
	std::thread th_; std::mutex m_; std::condition_variable cv_;
	bool suspended_ = false; std::atomic<bool> running_{false};
	int exit_code_ = 0;
};

inline CWinThread *AfxBeginThread(AFX_THREADPROC proc, LPVOID param, int priority = THREAD_PRIORITY_NORMAL, UINT = 0, DWORD flags = 0, void * = nullptr)
{
	// MFC: il thread si autodistrugge a fine corsa (m_bAutoDelete = true). Per attenderlo: CREATE_SUSPENDED, m_bAutoDelete = false, ResumeThread().
	CWinThread *t = new CWinThread(proc, param); t->m_nPriority = priority; t->CreateThread(flags); return t;
}
inline CWinThread *AfxBeginThread(const CRuntimeClass *rc, int priority = THREAD_PRIORITY_NORMAL, UINT = 0, DWORD flags = 0, void * = nullptr)
{
	CWinThread *t = rc && rc->create ? dynamic_cast<CWinThread*>(rc->create()) : nullptr;
	if (t) { t->m_nPriority = priority; t->CreateThread(flags); }
	return t;
}
