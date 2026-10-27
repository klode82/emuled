// Origin: OtherFunctions.cpp:4037 GetMyConnectOptions - the connection options we send in HELLO and in callback requests.
#include "StdAfx.h"
#include "emule.h"
#include "Preferences.h"
#include "kademlia/kademlia/Kademlia.h"
#include "kademlia/kademlia/UDPFirewallTester.h"
#include "ConnectOptions.h"

uint8 GetMyConnectOptions(bool bEncryption, bool bCallback)
{
	// Connect options Tag
	// 4 Reserved (!)
	// 1 Direct Callback
	// 1 CryptLayer Required
	// 1 CryptLayer Requested
	// 1 CryptLayer Supported
	const uint8 uSupportsCryptLayer = static_cast<uint8>(thePrefs.IsCryptLayerEnabled() && bEncryption);
	const uint8 uRequestsCryptLayer = static_cast<uint8>(thePrefs.IsCryptLayerPreferred() && bEncryption);
	const uint8 uRequiresCryptLayer = static_cast<uint8>(thePrefs.IsCryptLayerRequired() && bEncryption);
	// direct callback is only possible if connected to kad, TCP firewalled and verified UDP open (for example on a full cone NAT)
	const uint8 uDirectUDPCallback = static_cast<uint8>(bCallback && theApp.IsFirewalled() && Kademlia::CKademlia::IsRunning() && !Kademlia::CUDPFirewallTester::IsFirewalledUDP(true) && Kademlia::CUDPFirewallTester::IsVerified());

	const uint8 byCryptOptions = (uDirectUDPCallback << 3) | (uRequiresCryptLayer << 2) | (uRequestsCryptLayer << 1) | (uSupportsCryptLayer << 0);
	return byCryptOptions;
}
