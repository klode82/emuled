// CArray, CList, CMap con l'API MFC usata dal core, su STL. POSITION come in MFC (iterazione con GetNext(pos)).
#pragma once
#include "win_types.h"
#include <vector>
#include <atomic>
#include <type_traits>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <string>

#define AFXAPI
typedef struct __POSITION *POSITION;

// std::vector<bool> e' una specializzazione che non da' riferimenti a bool: CArray<bool> usa un contenitore di singoli byte
struct BoolSlot { bool v; BoolSlot(bool x = false) : v(x) {} };

template<class T, class ARG = const T&>
class CArray
{
	using Elem = std::conditional_t<std::is_same_v<T, bool>, BoolSlot, T>;
	static T &R(Elem &e) { if constexpr (std::is_same_v<T, bool>) return e.v; else return e; }
	static const T &R(const Elem &e) { if constexpr (std::is_same_v<T, bool>) return e.v; else return e; }
	std::vector<Elem> v_;
public:
	INT_PTR GetSize() const { return (INT_PTR)v_.size(); }
	INT_PTR GetCount() const { return GetSize(); }
	INT_PTR GetUpperBound() const { return GetSize() - 1; }
	bool IsEmpty() const { return v_.empty(); }
	void SetSize(INT_PTR n, INT_PTR = -1) { v_.resize((size_t)n); }
	void RemoveAll() { v_.clear(); }
	void FreeExtra() { v_.shrink_to_fit(); }
	T &GetAt(INT_PTR i) { return R(v_.at((size_t)i)); }
	const T &GetAt(INT_PTR i) const { return R(v_.at((size_t)i)); }
	void SetAt(INT_PTR i, ARG x) { R(v_.at((size_t)i)) = x; }
	T &operator[](INT_PTR i) { return R(v_.at((size_t)i)); }
	const T &operator[](INT_PTR i) const { return R(v_.at((size_t)i)); }
	INT_PTR Add(ARG x) { v_.push_back(x); return GetUpperBound(); }
	INT_PTR Append(const CArray &o) { INT_PTR n = GetSize(); v_.insert(v_.end(), o.v_.begin(), o.v_.end()); return n; }
	void Copy(const CArray &o) { v_ = o.v_; }
	void InsertAt(INT_PTR i, ARG x, INT_PTR n = 1) { v_.insert(v_.begin() + i, (size_t)n, x); }
	void InsertAt(INT_PTR i, const CArray *a) { v_.insert(v_.begin() + i, a->v_.begin(), a->v_.end()); }
	void RemoveAt(INT_PTR i, INT_PTR n = 1) { v_.erase(v_.begin() + i, v_.begin() + i + n); }
	void SetAtGrow(INT_PTR i, ARG x) { if ((size_t)i >= v_.size()) v_.resize((size_t)i + 1); R(v_[(size_t)i]) = x; }
	T *GetData() { return reinterpret_cast<T*>(v_.data()); }
	const T *GetData() const { return reinterpret_cast<const T*>(v_.data()); }
};

template<class T, class ARG = const T&>
class CList
{
	struct Node { Node *prev, *next; T data; };
	Node *head_ = nullptr, *tail_ = nullptr;
	std::atomic<INT_PTR> n_{0};		// atomico: il numero di elementi e' letto (approssimato, come in eMule) anche da altri thread
	static Node *N(POSITION p) { return reinterpret_cast<Node*>(p); }
	static POSITION P(Node *n) { return reinterpret_cast<POSITION>(n); }
public:
	CList() = default;
	explicit CList(INT_PTR /*nBlockSize*/) {}
	CList(const CList &) = delete;
	CList &operator=(const CList &) = delete;
	~CList() { RemoveAll(); }
	INT_PTR GetCount() const { return n_; }
	INT_PTR GetSize() const { return n_; }
	bool IsEmpty() const { return n_ == 0; }
	POSITION GetHeadPosition() const { return P(head_); }
	POSITION GetTailPosition() const { return P(tail_); }
	T &GetHead() { return head_->data; }
	T &GetTail() { return tail_->data; }
	T &GetNext(POSITION &p) { Node *n = N(p); p = P(n->next); return n->data; }
	T &GetPrev(POSITION &p) { Node *n = N(p); p = P(n->prev); return n->data; }
	T &GetAt(POSITION p) { return N(p)->data; }
	const T &GetAt(POSITION p) const { return N(p)->data; }
	const T &GetNext(POSITION &p) const { Node *n = N(p); p = P(n->next); return n->data; }
	const T &GetHead() const { return head_->data; }
	const T &GetTail() const { return tail_->data; }
	POSITION FindIndex(INT_PTR i) const { Node *n = head_; while (n && i-- > 0) n = n->next; return P(n); }
	// ordinamento stabile con confronto "a < b"
	template<class Less> void Sort(Less less)
	{
		std::vector<T> v; for (Node *n = head_; n; n = n->next) v.push_back(n->data);
		std::stable_sort(v.begin(), v.end(), less);
		Node *n = head_; for (auto &x : v) { n->data = x; n = n->next; }
	}
	void SetAt(POSITION p, ARG x) { N(p)->data = x; }
	POSITION AddTail(ARG x)
	{
		Node *n = new Node{tail_, nullptr, x};
		(tail_ ? tail_->next : head_) = n; tail_ = n; ++n_; return P(n);
	}
	POSITION AddHead(ARG x)
	{
		Node *n = new Node{nullptr, head_, x};
		(head_ ? head_->prev : tail_) = n; head_ = n; ++n_; return P(n);
	}
	POSITION InsertAfter(POSITION p, ARG x)
	{
		if (!p) return AddTail(x);
		Node *a = N(p), *n = new Node{a, a->next, x};
		(a->next ? a->next->prev : tail_) = n; a->next = n; ++n_; return P(n);
	}
	POSITION InsertBefore(POSITION p, ARG x)
	{
		if (!p) return AddHead(x);
		Node *b = N(p), *n = new Node{b->prev, b, x};
		(b->prev ? b->prev->next : head_) = n; b->prev = n; ++n_; return P(n);
	}
	void RemoveAt(POSITION p)
	{
		Node *n = N(p);
		(n->prev ? n->prev->next : head_) = n->next;
		(n->next ? n->next->prev : tail_) = n->prev;
		delete n; --n_;
	}
	T RemoveHead() { T t = head_->data; RemoveAt(P(head_)); return t; }
	T RemoveTail() { T t = tail_->data; RemoveAt(P(tail_)); return t; }
	void RemoveAll() { while (head_) RemoveAt(P(head_)); }
	POSITION Find(ARG x, POSITION start = nullptr) const
	{
		for (Node *n = start ? N(start) : head_; n; n = n->next) if (n->data == x) return P(n);
		return nullptr;
	}
};

// HashKey come in MFC: i tipi con chiave non banale la specializzano (es. CAICHHash).
template<class ARG_KEY> inline UINT AFXAPI HashKey(ARG_KEY key) { return (UINT)(((UINT_PTR)key) >> 4); }

template<class KEY, class ARG_KEY = const KEY&, class VALUE = int, class ARG_VALUE = const VALUE&>
class CMap
{
public:
	struct CPair { const KEY key; VALUE value; };
private:
	struct Hash { size_t operator()(const KEY &k) const { return HashKey<ARG_KEY>(k); } };
	struct Entry { CPair pair; Entry *prev, *next; };
	std::unordered_map<KEY, Entry*, Hash> idx_;
	Entry *head_ = nullptr, *tail_ = nullptr;
	static Entry *E(POSITION p) { return reinterpret_cast<Entry*>(p); }
public:
	CMap() = default;
	CMap(const CMap &) = delete;
	CMap &operator=(const CMap &) = delete;
	~CMap() { RemoveAll(); }
	INT_PTR GetCount() const { return (INT_PTR)idx_.size(); }
	bool IsEmpty() const { return idx_.empty(); }
	bool Lookup(ARG_KEY k, VALUE &v) const { auto i = idx_.find(k); if (i == idx_.end()) return false; v = i->second->pair.value; return true; }
	const CPair *PLookup(ARG_KEY k) const { auto i = idx_.find(k); return i == idx_.end() ? nullptr : &i->second->pair; }
	CPair *PLookup(ARG_KEY k) { auto i = idx_.find(k); return i == idx_.end() ? nullptr : &i->second->pair; }
	VALUE &operator[](ARG_KEY k)
	{
		auto i = idx_.find(k);
		if (i == idx_.end()) {
			Entry *e = new Entry{{KEY(k), VALUE()}, tail_, nullptr};		// KEY(k): costruzione diretta, senza copia (CCKey ha il costruttore di copia "explicit")
			(tail_ ? tail_->next : head_) = e; tail_ = e; i = idx_.emplace(k, e).first;
		}
		return i->second->pair.value;
	}
	void SetAt(ARG_KEY k, ARG_VALUE v) { (*this)[k] = v; }
	bool RemoveKey(ARG_KEY k)
	{
		auto i = idx_.find(k);
		if (i == idx_.end()) return false;
		Entry *e = i->second;
		(e->prev ? e->prev->next : head_) = e->next;
		(e->next ? e->next->prev : tail_) = e->prev;
		idx_.erase(i); delete e; return true;
	}
	void RemoveAll() { for (Entry *e = head_, *n; e; e = n) { n = e->next; delete e; } head_ = tail_ = nullptr; idx_.clear(); }
	void InitHashTable(UINT n, bool = true) { idx_.reserve(n); }
	// Iterazione per puntatore alla coppia (la coppia e' il primo membro di Entry).
	const CPair *PGetFirstAssoc() const { return head_ ? &head_->pair : nullptr; }
	CPair *PGetFirstAssoc() { return head_ ? &head_->pair : nullptr; }
	const CPair *PGetNextAssoc(const CPair *p) const { const Entry *e = reinterpret_cast<const Entry*>(p); return e->next ? &e->next->pair : nullptr; }
	CPair *PGetNextAssoc(const CPair *p) { Entry *e = reinterpret_cast<Entry*>(const_cast<CPair*>(p)); return e->next ? &e->next->pair : nullptr; }
	POSITION GetStartPosition() const { return reinterpret_cast<POSITION>(head_); }
	void GetNextAssoc(POSITION &p, KEY &k, VALUE &v) const { Entry *e = E(p); k = e->pair.key; v = e->pair.value; p = reinterpret_cast<POSITION>(e->next); }
};

// ---- varianti MFC "tipizzate" e per puntatori (stessa semantica, sopra CList/CArray/CMap) ----
class CPtrList : public CList<void*, void*> {};
class CObList : public CList<void*, void*> {};
template<class BASE, class TYPE> class CTypedPtrList : public CList<TYPE, TYPE> {};
template<class BASE, class TYPE> class CTypedPtrArray : public CArray<TYPE, TYPE> {};
class CPtrArray : public CArray<void*, void*> {};
typedef CArray<BYTE, BYTE> CByteArray;
typedef CArray<UINT, UINT> CUIntArray;
typedef CArray<DWORD, DWORD> CDWordArray;
typedef CArray<WORD, WORD> CWordArray;

// CSimpleArray (ATL): vettore semplice con l'interfaccia di ATL (GetSize, Add, Remove, Find, RemoveAt, operator[])
template<class T>
class CSimpleArray
{
public:
	int GetSize() const { return (int)v_.size(); }
	BOOL Add(const T &t) { v_.push_back(t); return TRUE; }
	BOOL Remove(const T &t) { int i = Find(t); if (i < 0) return FALSE; v_.erase(v_.begin() + i); return TRUE; }
	BOOL RemoveAt(int i) { if (i < 0 || i >= (int)v_.size()) return FALSE; v_.erase(v_.begin() + i); return TRUE; }
	void RemoveAll() { v_.clear(); }
	int Find(const T &t) const { for (size_t i = 0; i < v_.size(); ++i) if (v_[i] == t) return (int)i; return -1; }
	T &operator[](int i) { return v_[(size_t)i]; }
	const T &operator[](int i) const { return v_[(size_t)i]; }
	T *GetData() { return v_.data(); }
	const T *GetData() const { return v_.data(); }
	bool IsEmpty() const { return v_.empty(); }
private:
	std::vector<T> v_;
};
