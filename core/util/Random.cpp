// LINUX: vedi Random.h
#include "StdAfx.h"
#include "util/Random.h"
#include <random>

static std::mt19937 &Gen() { thread_local std::mt19937 g{std::random_device{}()}; return g; }
uint32 GetRandomUInt32() { return (uint32)Gen()(); }
uint16 GetRandomUInt16() { return (uint16)(Gen()() >> 8); }
