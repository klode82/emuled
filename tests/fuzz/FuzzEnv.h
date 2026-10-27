// Fuzzing (Bench 612): the parser entry points of the remote-input code, driven with arbitrary bytes and no real network.
// Each target is a function Fuzz*(data, size); the same functions run under libFuzzer (EMULED_FUZZ, clang) and in the deterministic smoke test.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace emule::fuzz {

// Moves the process into an empty network namespace (user + net) and brings only the loopback up: whatever a fuzzed packet makes the code
// do (connect to a client, send an UDP reply, ask a server), nothing can leave the machine. false if the kernel refuses (then nothing may run).
// Must be called first, while the process is single-threaded.
bool IsolateNetwork();

// One-time environment: temporary profile, event loop, upload throttler, the objects of theApp. Safe to call again.
void InitEnv();
void ShutdownEnv();	// stops the threads of the environment; call before the process exits (std::atexit after the first run)

std::string TempDir();	// scratch folder of this process (files the targets write); removed by ShutdownEnv

using TargetFn = void (*)(const uint8_t *data, size_t size);
struct Target { const char *name; TargetFn fn; };
extern const Target kTargets[];
extern const size_t kTargetCount;
const Target *FindTarget(const char *name);

} // namespace emule::fuzz
