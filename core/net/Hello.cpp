// LINUX (new code): see Hello.h
#include "StdAfx.h"
#include "net/Hello.h"
#include "Packets.h"
#include "SafeFile.h"
#include "Opcodes.h"

namespace emule::net {

static constexpr uint32_t kEdonkeyVersion = 0x3C;				// EDONKEYVERSION
static constexpr uint32_t kVerMajor = 0, kVerMinor = 72, kVerUpdate = 0;	// eMule 0.72a

std::string BuildHelloPacket(bool answer, const uint8_t userhash[16], uint32_t id, uint16_t tcpPort, uint16_t udpPort, uint16_t kadUdpPort, const std::string &nick)
{
	CSafeMemFile d(256);
	if (!answer) d.WriteUInt8(16);									// size of the user hash (OP_HELLO only)
	d.WriteHash16(userhash); d.WriteUInt32(id); d.WriteUInt16(tcpPort); d.WriteUInt32(5);
	CTag(CT_NAME, CString(nick)).WriteTagToFile(d, UTF8strRaw);
	CTag(CT_VERSION, (uint64)kEdonkeyVersion).WriteTagToFile(d);
	CTag(CT_EMULE_UDPPORTS, (uint64)(((uint32_t)kadUdpPort << 16) | udpPort)).WriteTagToFile(d);		// Kad port in the high 16 bits, client UDP port in the low ones
	CTag(CT_EMULE_VERSION, (uint64)((kVerMajor << 17) | (kVerMinor << 10) | (kVerUpdate << 7))).WriteTagToFile(d);
	CTag(CT_EMULE_MISCOPTIONS2, (uint64)KADEMLIA_VERSION).WriteTagToFile(d);		// Kad version in the low 4 bits (needed by the UDP firewall test: below 6 the client gives up)
	d.WriteUInt32(0); d.WriteUInt16(0);								// server we are connected to: none
	std::string out; out += (char)OP_EDONKEYPROT; uint32_t len = (uint32_t)d.GetLength() + 1; out.append((const char*)&len, 4); out += (char)(answer ? OP_HELLOANSWER : OP_HELLO);
	out.append((const char*)d.GetBuffer(), (size_t)d.GetLength());
	return out;
}

} // namespace emule::net
