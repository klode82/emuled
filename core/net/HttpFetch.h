// LINUX (new code): downloads an http/https address into memory (libcurl). Used to update the server list from an address.
#pragma once
#include <string>

namespace emule::net {

// Follows up to 5 redirects, verifies the certificate, stops beyond maxBytes or after timeoutS seconds. http and https only. Blocking: call it in a thread.
bool HttpFetch(const std::string &url, size_t maxBytes, int timeoutS, std::string &out, std::string &err);

} // namespace emule::net
