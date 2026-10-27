// STUB (replaces srchybrid/LastCommonRouteFinder.h): eMule's "UploadSpeedSense". Here it returns the configured limit (default: none).
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
	bool AcceptNewClient() const { return true; }					// UploadSpeedSense vetoes no slot
	bool SetPrefs(const CurrentParamStruct &) { return false; }		// false = the interface must not redraw the status bar
	uint32 GetUpload() const { return upload_; }
	void SetUpload(uint32 v) { upload_ = v; }
	template<class T> bool AddHostsToCheck(T &) { return false; }			// traceroute to the servers for UploadSpeedSense: not ported
private:
	std::atomic<uint32> upload_{UNLIMITED};
};
