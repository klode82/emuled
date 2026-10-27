// LINUX (codice nuovo): vedi IniFile.h
#include "StdAfx.h"
#include "profile/IniFile.h"
#include "StringConversion.h"
#include <cstring>

namespace emule::profile {
using met::Report; using met::Verdict;

static std::string Lower(std::string s) { for (auto &c : s) c = (char)tolower((unsigned char)c); return s; }
static std::string Trim(const std::string &s)
{
	size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
	return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

DecodedText DecodeText(const std::string &b)
{
	DecodedText d; std::string text;
	if (b.size() >= 2 && (uint8_t)b[0] == 0xFF && (uint8_t)b[1] == 0xFE) {
		d.enc = TextEnc::Utf16LE;
		if ((b.size() - 2) % 2) d.ok = false;
		std::vector<WCHAR> w((b.size() - 2) / 2);
		for (size_t i = 0; i < w.size(); ++i) w[i] = (WCHAR)((uint8_t)b[2 + 2 * i] | (uint8_t)b[3 + 2 * i] << 8);
		text = CString(w.data(), (int)w.size()).str();
	} else if (b.size() >= 3 && !memcmp(b.data(), "\xEF\xBB\xBF", 3)) { d.enc = TextEnc::Utf8Bom; text = b.substr(3); }
	else text = b;
	d.eol = text.find("\r\n") != std::string::npos ? "\r\n" : (text.find('\n') != std::string::npos ? "\n" : "\r\n");
	size_t pos = 0;
	while (pos < text.size()) {
		size_t e = text.find('\n', pos), end = e == std::string::npos ? text.size() : e;
		size_t le = end; if (le > pos && text[le - 1] == '\r') --le;
		d.lines.push_back(text.substr(pos, le - pos));
		pos = e == std::string::npos ? text.size() : e + 1;
	}
	return d;
}

IniFile IniFile::FromBytes(const std::string &bytes)
{
	IniFile f;
	DecodedText d = DecodeText(bytes);
	f.enc_ = d.enc; f.eol_ = d.eol;
	f.final_eol_ = bytes.empty() || bytes.back() == '\n' || (d.enc == TextEnc::Utf16LE && bytes.size() >= 2 && bytes[bytes.size() - 2] == '\n');
	// raw lines: for ANSI and UTF-8 we work on the original bytes, for UTF-16 we re-encode only if touched
	std::string body = bytes;
	if (d.enc == TextEnc::Utf8Bom) body = bytes.substr(3);
	if (d.enc == TextEnc::Ansi || d.enc == TextEnc::Utf8Bom) {
		size_t pos = 0; size_t i = 0;
		while (pos < body.size() && i < d.lines.size()) {
			size_t e = body.find('\n', pos), end = e == std::string::npos ? body.size() : e;
			size_t le = end; if (le > pos && body[le - 1] == '\r') --le;
			std::string raw = body.substr(pos, le - pos);
			IniFile::Line l; l.raw = raw;
			if (d.enc == TextEnc::Ansi && !IsValidUtf8(raw.c_str(), (UINT)raw.size())) { l.text = LocalCpToStr(raw.c_str(), (UINT)raw.size()).str(); f.ansi_non_utf8_ = true; }
			else l.text = raw;
			f.lines_.push_back(l); ++i; pos = e == std::string::npos ? body.size() : e + 1;
		}
	} else for (auto &t : d.lines) { Line l; l.text = t; f.lines_.push_back(l); }
	return f;
}

bool IniFile::Load(const std::string &path, IniFile &out, std::string &err)
{
	FILE *fp = fopen(path.c_str(), "rb");
	if (!fp) { err = errno == ENOENT ? "absent" : strerror(errno); return false; }
	std::string b; char buf[1 << 16]; size_t k;
	while ((k = fread(buf, 1, sizeof buf, fp)) > 0) b.append(buf, k);
	bool bad = ferror(fp); fclose(fp);
	if (bad) { err = "read error"; return false; }
	out = FromBytes(b); return true;
}

// Finds the value line. If sec_last_line is given, stores in it the last "useful" line of the section (or -1 if the section does not exist).
int IniFile::Find(const std::string &sec, const std::string &key, int *sec_last_line) const
{
	std::string ls = Lower(sec), lk = Lower(key); bool in = false; int last = -1, found = -1;
	for (int i = 0; i < (int)lines_.size(); ++i) {
		std::string t = Trim(lines_[i].text);
		if (t.empty() || t[0] == ';') continue;
		if (t[0] == '[') { size_t e = t.find(']'); in = e != std::string::npos && Lower(Trim(t.substr(1, e - 1))) == ls; if (in) last = i; continue; }
		if (!in) continue;
		last = i;
		size_t eq = t.find('=');
		if (found < 0 && eq != std::string::npos && Lower(Trim(t.substr(0, eq))) == lk) found = i;
	}
	if (sec_last_line) *sec_last_line = last;
	return found;
}

std::vector<std::string> IniFile::Sections() const
{
	std::vector<std::string> v;
	for (auto &l : lines_) { std::string t = Trim(l.text); if (!t.empty() && t[0] == '[') { size_t e = t.find(']'); if (e != std::string::npos) v.push_back(Trim(t.substr(1, e - 1))); } }
	return v;
}

bool IniFile::Has(const std::string &s, const std::string &k) const { return Find(s, k) >= 0; }

std::string IniFile::Get(const std::string &s, const std::string &k, const std::string &def) const
{
	int i = Find(s, k);
	if (i < 0) return def;
	const std::string &t = lines_[i].text; size_t eq = t.find('=');
	return Trim(t.substr(eq + 1));
}

int64_t IniFile::GetInt(const std::string &s, const std::string &k, int64_t def) const
{
	int i = Find(s, k);
	if (i < 0) return def;
	std::string v = Get(s, k); char *end; errno = 0;
	long long n = strtoll(v.c_str(), &end, 10);
	return (end == v.c_str() || errno) ? def : n;
}

void IniFile::Set(const std::string &sec, const std::string &key, const std::string &value)
{
	int last; int i = Find(sec, key, &last);
	Line l; l.text = key + "=" + value; l.dirty = true;
	if (i >= 0) {
		const std::string &old = lines_[i].text; size_t eq = old.find('=');
		l.text = Trim(old.substr(0, eq)) + "=" + value;		// keeps the original spelling of the key
		lines_[i] = l; return;
	}
	if (last >= 0) { lines_.insert(lines_.begin() + last + 1, l); return; }
	if (!lines_.empty() && !Trim(lines_.back().text).empty()) { Line b; b.dirty = true; lines_.push_back(b); }
	Line h; h.text = "[" + sec + "]"; h.dirty = true; lines_.push_back(h); lines_.push_back(l);
}

bool IniFile::dirty() const { for (auto &l : lines_) if (l.dirty) return true; return false; }

std::string IniFile::Encode(const std::string &u8) const
{
	if (enc_ == TextEnc::Ansi && ansi_non_utf8_) return StrToLocalCp(CString(u8));
	return u8;
}

std::string IniFile::Bytes() const
{
	std::string out;
	if (enc_ == TextEnc::Utf8Bom) out += "\xEF\xBB\xBF";
	for (size_t i = 0; i < lines_.size(); ++i) {
		const Line &l = lines_[i];
		out += (enc_ == TextEnc::Utf16LE) ? l.text : (l.dirty ? Encode(l.text) : l.raw);
		if (i + 1 < lines_.size() || final_eol_) out += eol_;
	}
	if (enc_ != TextEnc::Utf16LE) return out;
	// UTF-8 -> UTF-16LE with BOM
	std::string w = "\xFF\xFE";
	for (size_t i = 0; i < out.size();) {
		unsigned char c = (unsigned char)out[i]; uint32_t cp; int n;
		if (c < 0x80) { cp = c; n = 1; } else if (c >= 0xF0) { cp = c & 7; n = 4; } else if (c >= 0xE0) { cp = c & 15; n = 3; } else { cp = c & 31; n = 2; }
		for (int k = 1; k < n && i + k < out.size(); ++k) cp = cp << 6 | ((unsigned char)out[i + k] & 0x3F);
		i += n;
		auto put = [&](uint16_t u) { w += (char)(u & 0xFF); w += (char)(u >> 8); };
		if (cp >= 0x10000) { cp -= 0x10000; put((uint16_t)(0xD800 + (cp >> 10))); put((uint16_t)(0xDC00 + (cp & 0x3FF))); } else put((uint16_t)cp);
	}
	return w;
}

bool IniFile::Save(const std::string &path, std::string &err) const
{
	met::SafeWriteResult r = met::SafeWriteMet(path, Bytes(), ValidateIni);
	if (!r.ok) err = r.error;
	return r.ok;
}

Report ValidateIni(const uint8_t *data, size_t size)
{
	Report rep; rep.version = size ? data[0] : 0;
	std::string b((const char*)data, size);
	DecodedText d = DecodeText(b);
	if (!d.ok) { rep.reason = "UTF-16 of odd length"; return rep; }
	if (d.enc != TextEnc::Utf16LE && memchr(data, 0, size)) { rep.reason = "null bytes in a text file"; return rep; }
	uint64_t secs = 0;
	for (auto &l : d.lines) { std::string t = Trim(l); if (!t.empty() && t[0] == '[' && t.find(']') != std::string::npos) ++secs; }
	rep.records = secs;
	if (!secs) { rep.reason = "no section"; return rep; }
	rep.verdict = Verdict::Valid; return rep;
}

} // namespace emule::profile
