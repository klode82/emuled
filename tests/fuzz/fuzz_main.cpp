// libFuzzer entry point (EMULED_FUZZ, clang). One executable per target: -DFUZZ_TARGET_NAME="kad_udp".
// Run it in an empty network namespace is not needed: IsolateNetwork() does it by itself and refuses to go on if it cannot.
#include "FuzzEnv.h"
#include <cstdio>
#include <cstdlib>

using namespace emule::fuzz;

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
	if (!IsolateNetwork()) { std::fprintf(stderr, "fuzz: cannot create an isolated network namespace: refusing to run with the real network\n"); std::exit(1); }
	InitEnv();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	static const Target *t = FindTarget(FUZZ_TARGET_NAME);
	t->fn(data, size);
	static const int once = std::atexit(ShutdownEnv);		// registered after the first run: the static objects it needs are already built, so they go away after it
	(void)once;
	return 0;
}
