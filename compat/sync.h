// Primitive di sincronizzazione MFC su std::recursive_mutex (in MFC sono rientranti).
#pragma once
#include <mutex>

class CSyncObject
{
protected:
	std::recursive_mutex m_;
public:
	virtual ~CSyncObject() = default;
	virtual bool Lock(DWORD = 0xFFFFFFFF) { m_.lock(); return true; }
	virtual bool Unlock() { m_.unlock(); return true; }
};
class CCriticalSection : public CSyncObject {};
class CMutex : public CSyncObject
{
public:
	CMutex(bool = false, const char * = nullptr) {}
};

class CSingleLock
{
	CSyncObject *o_; bool locked_ = false;
public:
	explicit CSingleLock(CSyncObject *o, bool initial = false) : o_(o) { if (initial) Lock(); }
	~CSingleLock() { Unlock(); }
	bool Lock(DWORD t = 0xFFFFFFFF) { if (!locked_ && o_->Lock(t)) locked_ = true; return locked_; }
	bool Unlock() { if (locked_) { o_->Unlock(); locked_ = false; } return true; }
	bool IsLocked() const { return locked_; }
};
