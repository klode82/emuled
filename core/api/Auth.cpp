// LINUX (codice nuovo): vedi Auth.h
#include "api/Auth.h"
#include <cryptopp/hex.h>
#include <cryptopp/hmac.h>
#include <cryptopp/misc.h>
#include <cryptopp/osrng.h>
#include <cryptopp/pwdbased.h>
#include <cryptopp/sha.h>
#include <fcntl.h>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

namespace emule::api {

namespace {
const unsigned kIterations = 200000;				// PBKDF2: ~0,1-0,2 s su un PC, accettabile anche su un Raspberry
std::string Hex(const std::string &raw) { std::string o; CryptoPP::StringSource(raw, true, new CryptoPP::HexEncoder(new CryptoPP::StringSink(o), false)); return o; }
bool Unhex(const std::string &h, std::string &out)
{
	if (h.size() % 2) return false;
	for (char c : h) if (!isxdigit((unsigned char)c)) return false;
	out.clear(); CryptoPP::StringSource(h, true, new CryptoPP::HexDecoder(new CryptoPP::StringSink(out))); return true;
}
std::string Random(size_t n) { std::string s(n, '\0'); CryptoPP::AutoSeededRandomPool().GenerateBlock((CryptoPP::byte *)&s[0], n); return s; }
bool Equal(const std::string &a, const std::string &b) { return a.size() == b.size() && CryptoPP::VerifyBufsEqual((const CryptoPP::byte *)a.data(), (const CryptoPP::byte *)b.data(), a.size()); }
std::string Pbkdf2(const std::string &pw, const std::string &salt, unsigned iter)
{
	std::string out(32, '\0');
	CryptoPP::PKCS5_PBKDF2_HMAC<CryptoPP::SHA256> p;
	p.DeriveKey((CryptoPP::byte *)&out[0], out.size(), 0, (const CryptoPP::byte *)pw.data(), pw.size(), (const CryptoPP::byte *)salt.data(), salt.size(), iter);
	return out;
}
std::string Hmac(const std::string &key, const std::string &msg)
{
	std::string out(32, '\0');
	CryptoPP::HMAC<CryptoPP::SHA256> h((const CryptoPP::byte *)key.data(), key.size());
	h.CalculateDigest((CryptoPP::byte *)&out[0], (const CryptoPP::byte *)msg.data(), msg.size());
	return out;
}
}

Auth::Auth(std::string dir)
{
	if (!dir.empty() && dir.back() != '/') dir += '/';
	path_ = dir + "emuled.users";
}

bool Auth::ValidName(const std::string &n)
{
	if (n.empty() || n.size() > 32) return false;
	for (char c : n) if (!(isalnum((unsigned char)c) || c == '.' || c == '_' || c == '-')) return false;
	return true;
}

bool Auth::ValidPassword(const std::string &pw, std::string &err)
{
	if (pw.size() < 8) { err = "la password deve avere almeno 8 caratteri"; return false; }
	if (pw.size() > 256) { err = "la password e' troppo lunga (massimo 256 caratteri)"; return false; }
	return true;
}

bool Auth::Load(User &u)
{
	struct stat st {};
	if (::stat(path_.c_str(), &st) != 0) { loaded_ = false; stamp_ = -1; user_ = User(); u = User(); return false; }
	const long long stamp = (long long)st.st_mtim.tv_sec * 1000000000LL + st.st_mtim.tv_nsec + st.st_size;
	if (!loaded_ || stamp != stamp_) {
		User n; std::ifstream f(path_); std::string line;
		while (std::getline(f, line)) {
			size_t eq = line.find('='); if (eq == std::string::npos) continue;
			std::string k = line.substr(0, eq), v = line.substr(eq + 1);
			if (k == "user") n.name = v; else if (k == "salt") Unhex(v, n.salt); else if (k == "hash") Unhex(v, n.hash);
			else if (k == "secret") Unhex(v, n.secret); else if (k == "iter") n.iter = (unsigned)std::strtoul(v.c_str(), nullptr, 10);
		}
		stamp_ = stamp;
		loaded_ = ValidName(n.name) && n.salt.size() == 16 && n.hash.size() == 32 && n.secret.size() == 32 && n.iter >= 1000;
		user_ = loaded_ ? n : User();
	}
	u = user_;
	return loaded_;
}

bool Auth::Configured() { std::lock_guard<std::mutex> g(m_); User u; return Load(u); }

bool Auth::SetUser(const std::string &name, const std::string &password, std::string &err)
{
	if (!ValidName(name)) { err = "nome utente non valido (1-32 caratteri: lettere, cifre, punto, trattino, trattino basso)"; return false; }
	if (!ValidPassword(password, err)) return false;
	User n; n.name = name; n.iter = iter_override_ ? iter_override_ : kIterations; n.salt = Random(16); n.secret = Random(32);
	n.hash = Pbkdf2(password, n.salt, n.iter);
	const std::string tmp = path_ + ".tmp";
	{
		int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
		if (fd < 0) { err = "non riesco a scrivere " + path_; return false; }
		const std::string txt = "user=" + n.name + "\niter=" + std::to_string(n.iter) + "\nsalt=" + Hex(n.salt) + "\nhash=" + Hex(n.hash) + "\nsecret=" + Hex(n.secret) + "\n";
		ssize_t w = ::write(fd, txt.data(), txt.size()); ::fsync(fd); ::close(fd);
		if (w != (ssize_t)txt.size()) { ::unlink(tmp.c_str()); err = "scrittura incompleta di " + path_; return false; }
	}
	if (::rename(tmp.c_str(), path_.c_str()) != 0) { ::unlink(tmp.c_str()); err = "non riesco a salvare " + path_; return false; }
	std::lock_guard<std::mutex> g(m_); loaded_ = false;		// si rilegge alla prossima richiesta
	return true;
}

bool Auth::CheckLogin(const std::string &name, const std::string &password)
{
	User u; { std::lock_guard<std::mutex> g(m_); if (!Load(u)) return false; }
	const std::string h = Pbkdf2(password, u.salt, u.iter);		// sempre, anche se il nome e' sbagliato
	return Equal(h, u.hash) & Equal(name, u.name);
}

std::string Auth::IssueToken(time_t now, long ttl_s)
{
	User u; { std::lock_guard<std::mutex> g(m_); if (!Load(u)) return std::string(); }
	const std::string payload = u.name + "|" + std::to_string((long long)now + ttl_s);
	return "s1." + Hex(payload) + "." + Hex(Hmac(u.secret, payload));
}

bool Auth::VerifyToken(const std::string &token, time_t now)
{
	if (token.rfind("s1.", 0) != 0) return false;
	size_t dot = token.find('.', 3); if (dot == std::string::npos) return false;
	std::string payload, mac;
	if (!Unhex(token.substr(3, dot - 3), payload) || !Unhex(token.substr(dot + 1), mac)) return false;
	User u; { std::lock_guard<std::mutex> g(m_); if (!Load(u)) return false; }
	if (!Equal(mac, Hmac(u.secret, payload))) return false;
	size_t bar = payload.find('|'); if (bar == std::string::npos || payload.substr(0, bar) != u.name) return false;
	return std::atoll(payload.c_str() + bar + 1) > (long long)now;
}

bool Auth::AllowAttempt(const std::string &peer, time_t now)
{
	std::lock_guard<std::mutex> g(m_);
	auto it = fails_.find(peer);
	return it == fails_.end() || it->second.lock_until <= now;
}

void Auth::RecordFailure(const std::string &peer, time_t now)
{
	std::lock_guard<std::mutex> g(m_);
	if (fails_.size() > 1024) fails_.clear();
	Fails &f = fails_[peer];
	if (f.n == 0 || now - f.first > 60) { f.n = 0; f.first = now; }
	if (++f.n >= 5) { f.lock_until = now + 60; f.n = 0; }
}

void Auth::RecordSuccess(const std::string &peer) { std::lock_guard<std::mutex> g(m_); fails_.erase(peer); }

} // namespace emule::api
