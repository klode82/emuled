// LINUX (codice nuovo): vedi HttpFetch.h
#include "net/HttpFetch.h"
#include <curl/curl.h>

namespace emule::net {

namespace {
struct Sink { std::string *out; size_t max; bool over = false; };
size_t Write(char *p, size_t sz, size_t n, void *ud)
{
	Sink *s = static_cast<Sink *>(ud);
	if (s->out->size() + sz * n > s->max) { s->over = true; return 0; }		// 0 = errore: curl interrompe il trasferimento
	s->out->append(p, sz * n); return sz * n;
}
} // namespace

bool HttpFetch(const std::string &url, size_t maxBytes, int timeoutS, std::string &out, std::string &err)
{
	out.clear(); err.clear();
	static const bool init = (curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK); (void)init;
	CURL *c = curl_easy_init();
	if (!c) { err = "libcurl non si avvia"; return false; }
	Sink sink{&out, maxBytes};
	char errbuf[CURL_ERROR_SIZE] = {};
	curl_easy_setopt(c, CURLOPT_URL, url.c_str());
	curl_easy_setopt(c, CURLOPT_PROTOCOLS_STR, "http,https");
	curl_easy_setopt(c, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
	curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L); curl_easy_setopt(c, CURLOPT_MAXREDIRS, 5L);
	curl_easy_setopt(c, CURLOPT_TIMEOUT, (long)timeoutS); curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 15L);
	curl_easy_setopt(c, CURLOPT_FAILONERROR, 1L);		// una pagina 404 non e' un server.met
	curl_easy_setopt(c, CURLOPT_USERAGENT, "emuled");
	curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, Write); curl_easy_setopt(c, CURLOPT_WRITEDATA, &sink);
	curl_easy_setopt(c, CURLOPT_ERRORBUFFER, errbuf);
	curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);			// usato da un thread
	CURLcode rc = curl_easy_perform(c);
	curl_easy_cleanup(c);
	if (rc == CURLE_OK) return true;
	err = sink.over ? "file troppo grande" : (errbuf[0] ? errbuf : curl_easy_strerror(rc));
	out.clear(); return false;
}

} // namespace emule::net
