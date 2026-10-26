// LINUX: numeri casuali di eMule (OtherFunctions.cpp: GetRandomUInt16/32). L'originale usava rand() rigenerato con GetTickCount a ogni chiamata,
// con l'avviso "ogni thread deve chiamare srand"; qui un generatore per thread inizializzato da /dev/urandom (random_device): niente stato condiviso.
#pragma once
#include "StdAfx.h"
uint16 GetRandomUInt16();
uint32 GetRandomUInt32();
