// LINUX: eMule random numbers (OtherFunctions.cpp: GetRandomUInt16/32). The original used rand() reseeded with GetTickCount on every call,
// with the warning "every thread must call srand"; here a per-thread generator seeded from /dev/urandom (random_device): no shared state.
#pragma once
#include "StdAfx.h"
uint16 GetRandomUInt16();
uint32 GetRandomUInt32();
