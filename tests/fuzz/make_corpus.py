#!/usr/bin/env python3
"""Builds the seed corpora of the fuzz targets (tests/fuzz/corpus/<target>/). Deterministic and small: hand-made, well-formed examples of
every message and file the targets parse, so the mutations start from inputs that get deep into the parsers. No data of a real profile.
Run again after adding a seed here. Regression inputs found by the fuzzers are added to the corpus by hand (file names regress-*)."""
import os, re, struct, zlib, sys

ROOT = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(ROOT, "corpus")
OP = {m.group(1): int(m.group(2), 0) for m in re.finditer(r"#define\s+(\w+)\s+(0x[0-9A-Fa-f]+|\d+)\b", open(os.path.join(ROOT, "../../core/Opcodes.h"), encoding="utf8", errors="replace").read())}

u8 = lambda v: struct.pack("<B", v)
u16 = lambda v: struct.pack("<H", v)
u32 = lambda v: struct.pack("<I", v)
u64 = lambda v: struct.pack("<Q", v)
HASH = bytes(range(0xA0, 0xB0))
def s16(s): b = s.encode(); return u16(len(b)) + b
def tag_str(i, v): return u8(0x82) + u8(i) + s16(v)
def tag_int(i, v): return u8(0x83) + u8(i) + u32(v)
def tag_i64(i, v): return u8(0x8B) + u8(i) + u64(v)
def tag_hash(i, v): return u8(0x81) + u8(i) + v
def frame(prot, op, payload): return u8(prot) + u32(len(payload) + 1) + u8(op) + payload
ED, EM, PK = 0xE3, 0xC5, 0xD4
def put(target, name, data):
    d = os.path.join(OUT, target); os.makedirs(d, exist_ok=True)
    open(os.path.join(d, name), "wb").write(data)

# ---- packet / tags
tags = tag_str(1, "movie.avi") + tag_int(2, 734003200) + tag_int(0x15, 42) + tag_hash(0x10, HASH) + tag_i64(0x3A, 1 << 40) + u8(0x02) + u16(5) + b"mytag" + u16(2) + b"hi"
put("packet", "tags", u8(0) + tags)
put("packet", "frame_hello", frame(ED, 1, u8(16) + HASH + u32(0x0A000001) + u16(4662) + u32(2) + tag_str(1, "nick") + tag_int(0x11, 60) + u32(0) + u16(0)))
put("packet", "frame_packed", frame(PK, 0x42, zlib.compress(b"\x01" * 40 + HASH * 3)))
put("packet", "frame_emule", frame(EM, 1, u8(0x43) + u8(1) + u32(1) + tag_int(0xF9, 3)))

# ---- ed2k links
for i, l in enumerate([
    "ed2k://|file|movie.avi|734003200|0123456789ABCDEF0123456789ABCDEF|/",
    "ed2k://|file|movie.avi|734003200|0123456789ABCDEF0123456789ABCDEF|h=ABCDEFGHIJKLMNOPQRSTUVWXYZ234567|/|sources,1.2.3.4:4662|/",
    "ed2k://|file|a%20b.mkv|1234|0123456789ABCDEF0123456789ABCDEF|p=0123456789ABCDEF0123456789ABCDEF:0123456789ABCDEF0123456789ABCDEF|/|s=http://example.com/f|/",
    "ed2k://|server|1.2.3.4|4661|/", "ed2k://|serverlist|http://example.com/server.met|/", "ed2k://|nodeslist|http://example.com/nodes.dat|/",
    "ed2k://|search|fight club|/", "ed2k://|file|x|1|0123456789ABCDEF0123456789ABCDEF|/|sources,[::1]:4662|/"]):
    put("ed2k_link", "link%d" % i, l.encode())

# ---- server TCP (frames from a server)
hit = lambda hb, name, size, src: bytes([hb]) * 16 + u32(0x11223344) + u16(4662) + u32(3) + tag_str(1, name) + tag_int(2, size) + tag_int(0x15, src)
put("server_tcp", "message", frame(ED, OP["OP_SERVERMESSAGE"], s16("server version 17.15\r\nWelcome")))
put("server_tcp", "idchange", frame(ED, OP["OP_IDCHANGE"], u32(0x0A0B0C0D) + u32(0x0000B7FF) + u32(4661) + u32(0x01020304)))
put("server_tcp", "ident", frame(ED, OP["OP_SERVERIDENT"], HASH + u32(0x01020304) + u16(4661) + u32(2) + tag_str(1, "srv") + tag_str(0xB, "a server")))
put("server_tcp", "status", frame(ED, OP["OP_SERVERSTATUS"], u32(321) + u32(654)))
put("server_tcp", "searchresult", frame(ED, OP["OP_SEARCHRESULT"], u32(2) + hit(10, "Fight Club ITA.avi", 734003200, 42) + hit(11, "fight.club.mkv", 1500000, 7) + u8(1)))
put("server_tcp", "foundsources", frame(ED, OP["OP_FOUNDSOURCES"], HASH + u8(2) + u32(0x01020304) + u16(4662) + u32(0x05060708) + u16(4662)))
put("server_tcp", "serverlist", frame(ED, OP["OP_SERVERLIST"], u8(2) + u32(0x01020304) + u16(4661) + u32(0x05060708) + u16(4661)))
put("server_tcp", "callback", frame(ED, OP["OP_CALLBACKREQUESTED"], u32(0x01020304) + u16(4662)))
put("server_tcp", "reject", frame(ED, OP["OP_REJECT"], b""))
put("server_tcp", "packed", frame(PK, OP["OP_SEARCHRESULT"], zlib.compress(u32(1) + hit(12, "packed.avi", 99999, 3))))
put("server_tcp", "obfu_sources", frame(ED, OP["OP_FOUNDSOURCES_OBFU"], HASH + u8(1) + u32(0x01020304) + u16(4662) + u8(0x80) + HASH))

# ---- server UDP (first byte: trust mode, then the datagram)
def dg(mode, op, payload): return u8(mode) + u8(ED) + u8(op) + payload
put("server_udp", "globsearchres", dg(1, OP["OP_GLOBSEARCHRES"], hit(10, "Fight Club", 734003200, 42) + u8(ED) + u8(OP["OP_GLOBSEARCHRES"]) + hit(11, "second", 5, 1)))
put("server_udp", "globfoundsources", dg(1, OP["OP_GLOBFOUNDSOURCES"], HASH + u8(1) + u32(0x01020304) + u16(4662)))
put("server_udp", "statres", dg(1, OP["OP_GLOBSERVSTATRES"], u32(0x55AA55AA) + u32(100) + u32(2000) + u32(300) + u32(4000) + u32(500) + u32(0) + u32(0x1FF) + u16(4672) + u32(0x01020304) + u32(0x01020304)))
put("server_udp", "descres", dg(1, OP["OP_SERVER_DESC_RES"], s16("Name") + s16("Description")))
put("server_udp", "descres_ext", dg(1, OP["OP_SERVER_DESC_RES"], u32(0xF0FF0000 | 0x0201) + u32(2) + tag_str(1, "nm") + tag_str(0xB, "desc")))
put("server_udp", "obfuscated", u8(6) + bytes(range(1, 40)))
put("server_udp", "regress_1byte", u8(1) + u8(ED))		# found by the fuzzer: a 1-byte datagram made the payload size negative
put("server_udp", "unknown_peer", dg(0, OP["OP_GLOBSEARCHRES"], hit(10, "x", 1, 1)))

# ---- client UDP (eMule protocol + Kad, as seen on the socket)
EMULEUDP = [("reaskfileping", "OP_REASKFILEPING", HASH), ("reaskack", "OP_REASKACK", u16(1) + u16(2) + b"\x00" * 8),
    ("filenotfound", "OP_FILENOTFOUND", b""), ("queuefull", "OP_QUEUEFULL", b""), ("reaskcallbackudp", "OP_REASKCALLBACKUDP", HASH + b"\x00" * 6 + u32(0x0A000001) + u16(4672)),
    ("portest", "OP_PORTTEST", b"\x12")]
for n, op, pl in EMULEUDP:
    if op in OP: put("client_udp", n, u8(EM) + u8(OP[op]) + pl)
put("client_udp", "kad_hello_req", u8(0xE4) + u8(OP["KADEMLIA2_HELLO_REQ"]) + bytes(range(16)) + u16(4662) + u8(8) + u8(0))
put("client_udp", "kad_packed", u8(0xE5) + u8(OP["KADEMLIA2_BOOTSTRAP_RES"]) + zlib.compress(bytes(range(16)) + u16(4662) + u8(8) + u16(1) + bytes(range(16)) + u32(0x0A000002) + u16(4672) + u16(4662) + u8(8)))
put("client_udp", "random_obfuscated", bytes([0x31, 0x07, 0xB1]) + bytes(range(7, 60)))

# ---- Kad listener: [port 2][validkey 1][key 4][E4 opcode payload]
NODE = bytes(range(1, 17)); TARGET = bytes(range(32, 48))
def kad(name, op, payload, valid=1): put("kad_udp", name, u16(4672) + u8(valid) + u32(0x12345678) + u8(0xE4) + u8(OP[op]) + payload)
kad("bootstrap_req", "KADEMLIA2_BOOTSTRAP_REQ", b"")
kad("bootstrap_res", "KADEMLIA2_BOOTSTRAP_RES", NODE + u16(4662) + u8(8) + u16(2) + NODE + u32(0x0A000002) + u16(4672) + u16(4662) + u8(8) + TARGET + u32(0x0A000003) + u16(4672) + u16(4662) + u8(8))
kad("hello_req", "KADEMLIA2_HELLO_REQ", NODE + u16(4662) + u8(8) + u8(1) + u8(0x80 | 2) + u8(0xFE) + u8(0) + u8(0x10) )
kad("hello_res", "KADEMLIA2_HELLO_RES", NODE + u16(4662) + u8(8) + u8(0))
kad("hello_res_ack", "KADEMLIA2_HELLO_RES_ACK", NODE + u8(0))
kad("req", "KADEMLIA2_REQ", u8(11) + TARGET + NODE)
kad("res", "KADEMLIA2_RES", TARGET + u8(1) + NODE + u32(0x0A000002) + u16(4672) + u16(4662) + u8(8))
kad("search_key_req", "KADEMLIA2_SEARCH_KEY_REQ", TARGET + u16(0x8000) + u8(1) + u8(0) + u16(2) + b"hi")
kad("search_source_req", "KADEMLIA2_SEARCH_SOURCE_REQ", TARGET + u16(0) + u64(734003200))
kad("search_notes_req", "KADEMLIA2_SEARCH_NOTES_REQ", TARGET + u64(734003200))
kad("search_res", "KADEMLIA2_SEARCH_RES", NODE + TARGET + u16(1) + HASH + u8(2) + u8(0x02) + u16(1) + u8(1) + s16("Fight Club ITA.avi") + u8(0x03) + u16(1) + u8(2) + u32(734003200))
kad("publish_key_req", "KADEMLIA2_PUBLISH_KEY_REQ", TARGET + u16(1) + HASH + u8(2) + u8(0x02) + u16(1) + u8(1) + s16("file.avi") + u8(0x03) + u16(1) + u8(2) + u32(1234))
kad("publish_source_req", "KADEMLIA2_PUBLISH_SOURCE_REQ", TARGET + HASH + u8(3) + u8(0x03) + u16(1) + u8(0xFF) + u32(4) + u8(0x03) + u16(1) + u8(0xFC) + u32(4662) + u8(0x03) + u16(1) + u8(0xD3) + u32(1234))
kad("publish_res", "KADEMLIA2_PUBLISH_RES", TARGET + u8(10) + u8(1))
kad("publish_notes_req", "KADEMLIA2_PUBLISH_NOTES_REQ", TARGET + HASH + u8(1) + u8(0x02) + u16(1) + u8(0x22) + s16("note"))
kad("ping", "KADEMLIA2_PING", b"")
kad("pong", "KADEMLIA2_PONG", u16(4672))
kad("firewalled2", "KADEMLIA_FIREWALLED2_REQ", u16(4662) + NODE + u8(0))
kad("firewallud", "KADEMLIA2_FIREWALLUDP", u8(1) + u16(4672))
kad("findbuddy", "KADEMLIA_FINDBUDDY_REQ", TARGET + NODE + u16(4662))
kad("callback", "KADEMLIA_CALLBACK_REQ", TARGET + HASH + u16(4662))
kad("search_res_v1", "KADEMLIA_SEARCH_RES", TARGET + u16(1) + HASH + u16(1) + u8(0x02) + u16(1) + u8(1) + s16("old.avi"))

# ---- client TCP (stream of a peer that has just connected: HELLO first)
HELLO = frame(ED, OP["OP_HELLO"], u8(16) + HASH + u32(0x0A000001) + u16(4662) + u32(3) + tag_str(1, "peer") + tag_int(0x11, 60) + tag_int(0xFA, 0x00F3_3D81 & 0xFFFFFFFF) + u32(0x01020304) + u16(4661))
put("client_tcp", "hello", HELLO)
CT = [("OP_REQUESTFILENAME", HASH), ("OP_SETREQFILEID", HASH), ("OP_STARTUPLOADREQ", HASH), ("OP_FILEREQANSWER", HASH + s16("x.avi")), ("OP_FILESTATUS", HASH + u16(8) + b"\xff"),
      ("OP_HASHSETREQUEST", HASH), ("OP_HASHSETANSWER", HASH + u16(2) + HASH + HASH), ("OP_REQUESTPARTS", HASH + u32(0) + u32(9728000) + u32(0) + u32(100) + u32(200) + u32(0)),
      ("OP_SENDINGPART", HASH + u32(0) + u32(8) + b"12345678"), ("OP_QUEUERANKING", u16(5)), ("OP_MESSAGE", s16("hello")), ("OP_ASKSHAREDFILES", b""),
      ("OP_ASKSHAREDFILESANSWER", u32(1) + HASH + u32(0x01020304) + u16(4662) + u32(1) + tag_str(1, "f")), ("OP_OUTOFPARTREQS", b""), ("OP_CANCELTRANSFER", b""), ("OP_ACCEPTUPLOADREQ", b""),
      ("OP_FILEREQANSNOFIL", HASH), ("OP_CHANGE_CLIENT_ID", u32(1) + u32(2))]
for op, pl in CT:
    if op in OP: put("client_tcp", op.lower(), HELLO + frame(ED, OP[op], pl))
EMT = [("OP_EMULEINFO", u8(0x43) + u8(1) + u32(3) + tag_int(0xF9, 3) + tag_int(0xFA, 0x20) + tag_int(0xFB, 1)), ("OP_EMULEINFOANSWER", u8(0x43) + u8(1) + u32(1) + tag_int(0xF9, 3)),
       ("OP_COMPRESSEDPART", HASH + u32(0) + u32(9) + zlib.compress(b"abcdefgh")[:9]), ("OP_REQUESTSOURCES2", u8(1) + u16(1) + HASH), ("OP_ANSWERSOURCES2", u8(4) + HASH + u16(1) + u32(0x01020304) + u16(4662) + u32(0x05060708) + u16(4661) + HASH + u8(0)),
       ("OP_SECIDENTSTATE", u8(1) + u32(0x01020304)), ("OP_PUBLICKEY", u8(8) + b"\x30\x06" + b"abcdef"), ("OP_SIGNATURE", u8(4) + b"abcd" + u8(1)), ("OP_QUEUERANK", u32(5)),
       ("OP_REQUESTPREVIEW", HASH), ("OP_PUBLICIP_REQ", b""), ("OP_PUBLICIP_ANSWER", u32(0x01020304)), ("OP_AICHANSWER", HASH + u16(5) + HASH[:0]), ("OP_FILEDESC", u8(5) + s16("nice")), ("OP_MULTIPACKET_EXT2", HASH + u8(0) + u64(1000) + u8(0x4F)),
       ("OP_HASHSETREQUEST2", HASH + u8(3) + u8(1))]
for op, pl in EMT:
    if op in OP: put("client_tcp", op.lower(), HELLO + frame(EM, OP[op], pl))

# ---- files
srv = u8(0xE0) + u32(2) + b"".join(u32(0x01020304 + i) + u16(4661) + u32(2) + tag_str(1, "srv%d" % i) + tag_int(0x87, 50) for i in range(2))
put("server_met", "two_servers", srv); put("met", "server", u8(2) + srv)
known = u8(0x0E) + u32(1) + u32(1700000000) + HASH + u16(0) + u32(3) + tag_str(1, "f0") + tag_int(2, 1234) + tag_i64(0x3A, 77)
put("met", "known", u8(1) + known)
part = u8(0xE0) + u32(1700000000) + HASH + u16(0) + u32(5) + tag_str(1, "film.mkv") + tag_int(2, 100000) + tag_int(0x18, 1) + u8(0x03) + u16(2) + b"\x09" + b"0" + u32(10000) + u8(0x03) + u16(2) + b"\x0a" + b"0" + u32(20000)
put("part_met", "gaps", part); put("met", "part", u8(0) + part)
nodes2 = u32(0) + u32(2) + u32(2) + b"".join(bytes(range(i, i + 16)) + u32(0x0A000001 + i) + u16(4672) + u16(4662) + u8(8) + u32(0x11223344) + u32(0x0A000001) + u8(1) for i in range(2))
nodes0 = u32(2) + b"".join(bytes(range(i, i + 16)) + u32(0x0A000001 + i) + u16(4672) + u16(4662) + u8(1) for i in range(2))
nodesb = u32(0) + u32(3) + u32(1) + u32(1) + bytes(range(16)) + u32(0x0A000009) + u16(4672) + u16(4662) + u8(8)
put("nodes_dat", "v2", nodes2); put("nodes_dat", "v0", nodes0); put("nodes_dat", "bootstrap", nodesb); put("met", "nodes", u8(4) + nodes2)
put("met", "clients", u8(2) + u8(0x12) + u32(1) + b"x" * 119); put("met", "prefs_dat", u8(5) + u8(0x14) + HASH + b"\0" * 44); put("met", "prefs_kad", u8(6) + u32(0x0A000001) + u16(0) + HASH + u8(0))
put("met", "cancelled", u8(7) + u8(0x0E) + u8(1) + u32(0x58302D94) + u32(1) + HASH); put("met", "known2", u8(8) + u8(0x05) + HASH + u32(2) + HASH + HASH)

# ---- HTTP / WebSocket
A = "Authorization: Bearer tok\r\n"
def req(method, path, extra="", body=""):
    return (method + " " + path + " HTTP/1.1\r\nHost: x\r\n" + extra + ("Content-Length: %d\r\n" % len(body) if body else "") + "\r\n" + body).encode()
put("http", "ping", req("GET", "/api/v1/ping", A))
put("http", "noauth", req("GET", "/api/v1/downloads"))
put("http", "query_token", req("GET", "/api/v1/status?token=tok"))
put("http", "add_download", req("POST", "/api/v1/downloads", A + "Content-Type: application/json\r\n", '{"link":"ed2k://|file|a.avi|1000|0123456789ABCDEF0123456789ABCDEF|/","category":0,"paused":true}'))
put("http", "patch_prefs", req("PATCH", "/api/v1/preferences", A, '{"nick":"x","max_download_kbps":100,"upnp_enabled":false}'))
put("http", "search", req("POST", "/api/v1/searches", A, '{"query":"fight club","method":"kad","type":"video"}'))
put("http", "servers", req("POST", "/api/v1/servers", A, '{"address":"1.2.3.4","port":4661,"name":"s"}'))
put("http", "download_item", req("GET", "/api/v1/downloads/0123456789ABCDEF0123456789ABCDEF/details", A))
put("http", "static", req("GET", "/assets/a.js"))
put("http", "traversal", req("GET", "/assets/..%2f..%2f..%2fetc/passwd"))
put("http", "template", req("GET", "/t/t1/"))
put("http", "chunked", req("POST", "/api/v1/downloads", A + "Transfer-Encoding: chunked\r\n", "5\r\nhello\r\n0\r\n\r\n"))
ws = req("GET", "/api/v1/events?token=tok", "Upgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n")
def mask(payload, op=1):
    m = b"\x01\x02\x03\x04"; return bytes([0x80 | op, 0x80 | len(payload)]) + m + bytes(b ^ m[i & 3] for i, b in enumerate(payload))
put("http", "websocket", ws + mask(b'{"hello":1}') + mask(b"", 9) + mask(b"", 8))
print("corpus written in", OUT, sum(len(f) for _, _, f in os.walk(OUT)), "files")
