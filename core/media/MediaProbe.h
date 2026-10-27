// LINUX (new code): content of a video/audio file ("Content" tab of eMule) via the external program `mediainfo`.
// Launched without a shell (posix_spawn, the path is an argument), with a time limit, and its JSON is read. Also works on an incomplete
// .part: as long as the container header is there (for mkv the first bytes) mediainfo describes the whole file.
#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace emule::media {

// The program: `configured` if given (path or name), otherwise "mediainfo" searched in PATH. Empty if it does not exist/is not executable.
std::string FindBinary(const std::string &configured);
// Runs `binary --Output=JSON path` and returns the result in compact form:
//   {"state":"done","recognized":true,"general":{...},"video":[...],"audio":[...],"text":[...]}
//   {"state":"done","recognized":false}                       mediainfo does not recognize the file (still too incomplete?)
//   {"state":"failed","reason":"timeout|bad_output|spawn"}    e {"state":"unavailable","reason":"not_installed"}
// Blocking (up to timeout_s): call it from a thread.
nlohmann::json Probe(const std::string &binary, const std::string &path, int timeout_s = 10);
// From the mediainfo JSON (--Output=JSON) to the compact form; separate so it can be tested without launching anything.
nlohmann::json Compact(const nlohmann::json &mediainfo);

} // namespace emule::media
