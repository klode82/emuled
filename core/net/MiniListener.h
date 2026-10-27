// LINUX (new code): minimal TCP listener, until the real ListenSocket (Phase 5).
#pragma once
#include "StdAfx.h"
#include "Opcodes.h"
#include <algorithm>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <poll.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace emule::net {

// Minimal listener: accepts, answers OP_HELLO with OP_HELLOANSWER and keeps the connection open. At login the server tries to connect to the declared port: if it succeeds it assigns the high ID.
// The real client listening socket (ListenSocket, ed2k peer protocol) arrives with Phase 5.
struct MiniListener
{
	std::string hello_answer;				// complete OP_HELLOANSWER packet, sent at once to whoever sends OP_HELLO (the server sends a FIN in the same segment: the answer cannot wait)
	int fd = -1; std::atomic<bool> stop{false}; std::atomic<int> accepted{0}, answered{0}; std::thread th; std::mutex m; std::string first;		// first: first bytes received on incoming connections
	bool Start(uint16_t port)
	{
		fd = ::socket(AF_INET, SOCK_STREAM, 0); if (fd < 0) return false;
		int one = 1; setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
		sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_ANY); a.sin_port = htons(port);
		if (::bind(fd, (sockaddr*)&a, sizeof a) || ::listen(fd, 8)) { ::close(fd); fd = -1; return false; }
		th = std::thread([this] {
			std::vector<int> conns;
			while (!stop) {
				std::vector<pollfd> ps{{fd, POLLIN, 0}}; for (int s : conns) ps.push_back({s, POLLIN, 0});
				if (::poll(ps.data(), ps.size(), 200) <= 0) continue;
				if (ps[0].revents & POLLIN) { int c = ::accept(fd, nullptr, nullptr); if (c >= 0) { ++accepted; conns.push_back(c); } }
				for (size_t i = 1; i < ps.size(); ++i) if (ps[i].revents & (POLLIN | POLLHUP)) {		// keeps the connection open and logs what the other side sends
					unsigned char b[64]; ssize_t n = ::read(ps[i].fd, b, sizeof b);
					if (n >= 6 && b[0] == OP_EDONKEYPROT && b[5] == OP_HELLO && !hello_answer.empty()) { ::send(ps[i].fd, hello_answer.data(), hello_answer.size(), MSG_NOSIGNAL); ++answered; }
					if (n > 0) { std::lock_guard<std::mutex> g(m); for (ssize_t k = 0; k < n && first.size() < 120; ++k) { char h[4]; snprintf(h, sizeof h, "%02X ", b[k]); first += h; } }
					else if (n == 0) { ::close(ps[i].fd); conns.erase(std::find(conns.begin(), conns.end(), ps[i].fd)); break; }
				}
			}
			for (int s : conns) ::close(s);
		});
		return true;
	}
	~MiniListener() { stop = true; if (th.joinable()) th.join(); if (fd >= 0) ::close(fd); }
};


} // namespace emule::net
