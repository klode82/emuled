// STUB (sostituisce srchybrid/LastCommonRouteFinder.h): l'"UploadSpeedSense" di eMule. Qui ritorna il limite impostato (default: nessuno).
#pragma once
#include "StdAfx.h"
#include "Opcodes.h"
#include <atomic>

struct CurrentParamStruct
{
	double	dPingTolerance;
	uint32	uCurUpload; //bytes
	uint32	uMinUpload; //kilobytes
	uint32	uMaxUpload; //kilobytes
	uint32	uPingToleranceMilliseconds;
	uint32	uGoingUpDivider;
	uint32	uGoingDownDivider;
	uint32	uNumberOfPingsForAverage;
	uint32	uLowestInitialPingAllowed;
	bool	bUseMillisecondPingTolerance;
	bool	bEnabled;
};

class CLastCommonRouteFinder
{
public:
	bool AcceptNewClient() const { return true; }					// UploadSpeedSense non porta il veto a nessun slot
	bool SetPrefs(const CurrentParamStruct &) { return false; }		// false = l'interfaccia non deve ridisegnare la barra di stato
	uint32 GetUpload() const { return upload_; }
	void SetUpload(uint32 v) { upload_ = v; }
	template<class T> bool AddHostsToCheck(T &) { return false; }			// traceroute verso i server per l'UploadSpeedSense: non portato
private:
	std::atomic<uint32> upload_{UNLIMITED};
};
