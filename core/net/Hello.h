// LINUX (new code): minimal OP_HELLO / OP_HELLOANSWER, like eMule's SendHelloTypePacket (BaseClient.cpp) but with only the tags that are honest for a node that
// does not exchange files: name, ed2k version, UDP ports, eMule version. Used by the server probe's minimal listener (answer to the server's OP_HELLO) and by the
// Kad UDP firewall test (OP_HELLO towards the contact: without it, the contact refuses every other packet).
#pragma once
#include <cstdint>
#include <string>

namespace emule::net {

// Complete TCP packet (header included). answer=false: OP_HELLO (with the 0x10 hash length byte); true: OP_HELLOANSWER.
std::string BuildHelloPacket(bool answer, const uint8_t userhash[16], uint32_t id, uint16_t tcpPort, uint16_t udpPort, uint16_t kadUdpPort, const std::string &nick);

} // namespace emule::net
