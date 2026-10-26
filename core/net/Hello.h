// LINUX (codice nuovo): OP_HELLO / OP_HELLOANSWER minimi, come SendHelloTypePacket di eMule (BaseClient.cpp) ma con i soli tag onesti per un nodo che
// non scambia file: nome, versione ed2k, porte UDP, versione eMule. Usato dall'ascolto minimo della sonda del server (risposta all'OP_HELLO del server) e dal
// test del firewall UDP di Kad (OP_HELLO verso il contatto: senza, il contatto rifiuta ogni altro pacchetto).
#pragma once
#include <cstdint>
#include <string>

namespace emule::net {

// Pacchetto TCP completo (intestazione compresa). answer=false: OP_HELLO (con il byte 0x10 della lunghezza dell'hash); true: OP_HELLOANSWER.
std::string BuildHelloPacket(bool answer, const uint8_t userhash[16], uint32_t id, uint16_t tcpPort, uint16_t udpPort, uint16_t kadUdpPort, const std::string &nick);

} // namespace emule::net
