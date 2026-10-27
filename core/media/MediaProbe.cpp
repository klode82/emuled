// LINUX (codice nuovo): vedi MediaProbe.h
#include "media/MediaProbe.h"
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <sstream>

extern char **environ;
using nlohmann::json;
namespace emule::media {

std::string FindBinary(const std::string &configured)
{
	auto exec = [](const std::string &p) { return !p.empty() && ::access(p.c_str(), X_OK) == 0; };
	const std::string name = configured.empty() ? "mediainfo" : configured;
	if (name.find('/') != std::string::npos) return exec(name) ? name : std::string();
	const char *path = std::getenv("PATH"); std::stringstream ss(path ? path : "/usr/bin:/bin"); std::string dir;
	while (std::getline(ss, dir, ':')) if (!dir.empty() && exec(dir + "/" + name)) return dir + "/" + name;
	return std::string();
}

// the value as text (mediainfo writes everything as a string) -> number, or null if it is not a number
static json Num(const json &t, const char *k)
{
	auto it = t.find(k); if (it == t.end() || !it->is_string()) return nullptr;
	const std::string s = it->get<std::string>(); char *end = nullptr; double v = std::strtod(s.c_str(), &end);
	if (end == s.c_str() || *end) return nullptr;
	return v == (double)(long long)v ? json((long long)v) : json(v);
}
static json Str(const json &t, const char *k) { auto it = t.find(k); return it != t.end() && it->is_string() && !it->get<std::string>().empty() ? *it : json(nullptr); }

json Compact(const json &mi)
{
	json out = {{"state", "done"}, {"recognized", false}};
	if (!mi.is_object() || !mi.contains("media") || !mi["media"].is_object() || !mi["media"].contains("track") || !mi["media"]["track"].is_array()) return out;
	json video = json::array(), audio = json::array(), text = json::array(), general = nullptr;
	for (auto &t : mi["media"]["track"]) {
		if (!t.is_object()) continue;
		const std::string ty = t.value("@type", std::string());
		if (ty == "General") general = {{"format", Str(t, "Format")}, {"size", Num(t, "FileSize")}, {"duration_s", Num(t, "Duration")}, {"overall_bitrate", Num(t, "OverallBitRate")}, {"title", Str(t, "Title")}, {"movie", Str(t, "Movie")}, {"date", Str(t, "Encoded_Date")}};
		else if (ty == "Video") video.push_back({{"codec", Str(t, "Format")}, {"profile", Str(t, "Format_Profile")}, {"codec_id", Str(t, "CodecID")}, {"bitrate", Num(t, "BitRate")}, {"width", Num(t, "Width")}, {"height", Num(t, "Height")}, {"aspect", Num(t, "DisplayAspectRatio")}, {"fps", Num(t, "FrameRate")}, {"lang", Str(t, "Language")}, {"title", Str(t, "Title")}});
		else if (ty == "Audio") audio.push_back({{"codec", Str(t, "Format")}, {"commercial", Str(t, "Format_Commercial_IfAny")}, {"bitrate", Num(t, "BitRate")}, {"channels", Num(t, "Channels")}, {"sampling", Num(t, "SamplingRate")}, {"lang", Str(t, "Language")}, {"title", Str(t, "Title")}, {"default", t.value("Default", std::string()) == "Yes"}});
		else if (ty == "Text") text.push_back({{"codec", Str(t, "Format")}, {"lang", Str(t, "Language")}, {"title", Str(t, "Title")}, {"default", t.value("Default", std::string()) == "Yes"}, {"forced", t.value("Forced", std::string()) == "Yes"}});
	}
	// mediainfo always describes a readable file with a General track: if it did not understand the format and found no streams, for us the file is not recognized
	if (general.is_null() || (general["format"].is_null() && video.empty() && audio.empty() && text.empty())) return out;
	out["recognized"] = true; out["general"] = general; out["video"] = video; out["audio"] = audio; out["text"] = text;
	return out;
}

json Probe(const std::string &binary, const std::string &path, int timeout_s)
{
	if (binary.empty()) return {{"state", "unavailable"}, {"reason", "not_installed"}};
	int fd[2]; if (::pipe(fd) != 0) return {{"state", "failed"}, {"reason", "spawn"}};
	posix_spawn_file_actions_t fa; posix_spawn_file_actions_init(&fa);
	posix_spawn_file_actions_adddup2(&fa, fd[1], 1); posix_spawn_file_actions_addopen(&fa, 2, "/dev/null", 1, 0); posix_spawn_file_actions_addclose(&fa, fd[0]); posix_spawn_file_actions_addclose(&fa, fd[1]);
	std::string a0 = binary, a1 = "--Output=JSON", a2 = path;
	char *argv[] = {a0.data(), a1.data(), a2.data(), nullptr};
	pid_t pid = 0; int rc = posix_spawn(&pid, binary.c_str(), &fa, nullptr, argv, environ);
	posix_spawn_file_actions_destroy(&fa); ::close(fd[1]);
	if (rc != 0) { ::close(fd[0]); return {{"state", "failed"}, {"reason", "spawn"}}; }
	std::string out; bool timedOut = false; const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_s);
	for (;;) {
		const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(end - std::chrono::steady_clock::now()).count();
		if (left <= 0) { timedOut = true; break; }
		pollfd p{fd[0], POLLIN, 0}; int n = ::poll(&p, 1, (int)std::min<long long>(left, 200));
		if (n > 0) { char buf[8192]; ssize_t r = ::read(fd[0], buf, sizeof buf); if (r <= 0) break; out.append(buf, (size_t)r); if (out.size() > (8u << 20)) { timedOut = true; break; } }		// beyond 8 MB it is not a report
	}
	::close(fd[0]);
	if (timedOut) ::kill(pid, SIGKILL);
	int st = 0; ::waitpid(pid, &st, 0);
	if (timedOut) return {{"state", "failed"}, {"reason", "timeout"}};
	json j = json::parse(out, nullptr, false);
	if (j.is_discarded()) return {{"state", "failed"}, {"reason", "bad_output"}};
	return Compact(j);
}

} // namespace emule::media
