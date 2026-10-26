// LINUX (codice nuovo): guarda un elenco di cartelle con inotify e, quando qualcosa cambia (file nuovo, spostato dentro, riscritto, tolto), chiama onChange dopo un
// po' di quiete (debounce): copiare un file grosso fa molti eventi, il ricontrollo deve partire una volta sola a copia finita (Bench 653).
#pragma once
#include "net/EventLoop.h"
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace emule::net {

class DirWatcher : public IoSource
{
public:
	DirWatcher(EventLoop &loop, std::function<void()> onChange, unsigned quiet_ms = 10000);
	~DirWatcher() override;
	DirWatcher(const DirWatcher &) = delete;
	DirWatcher &operator=(const DirWatcher &) = delete;

	// Guarda queste cartelle (non le sottocartelle) al posto di quelle di prima. Ritorna quante ne sta guardando (una cartella assente si salta).
	size_t Set(const std::vector<std::string> &dirs);
	size_t watching() const { return wd_.size(); }
	void OnIo(uint32_t epoll_events) override;

private:
	EventLoop &loop_;
	std::function<void()> onChange_;
	unsigned quiet_ms_;
	int fd_ = -1;
	uint64_t id_ = 0, timer_ = 0;
	std::map<int, std::string> wd_;		// descrittore di inotify -> cartella
};

} // namespace emule::net
