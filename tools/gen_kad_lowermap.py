#!/usr/bin/env python3
"""Rigenera core/kademlia/io/KadLowerMap.inc da srchybrid/res/WideCharLowerMap.bin (sola lettura del sorgente Windows)."""
import struct, sys, os
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
d = open(os.path.join(root, "srchybrid", "res", "WideCharLowerMap.bin"), "rb").read()
m = struct.unpack("<65536H", d)
diffs = [(i, v) for i, v in enumerate(m) if i != v]
out = ["// GENERATO da srchybrid/res/WideCharLowerMap.bin (la mappa Unicode -> minuscolo che TUTTI i nodi Kad devono usare identica).",
       "// Solo le differenze (carattere, minuscolo): tutti gli altri caratteri restano uguali. %d voci, ordinate. Non modificare a mano." % len(diffs),
       "// Rigenerare: python3 tools/gen_kad_lowermap.py", "static const uint16_t kKadLowerDiffs[][2] = {"]
for k in range(0, len(diffs), 8):
    out.append("\t" + ",".join("{0x%04X,0x%04X}" % p for p in diffs[k:k + 8]) + ",")
out.append("};")
open(os.path.join(root, "linux", "core", "kademlia", "io", "KadLowerMap.inc"), "w").write("\n".join(out) + "\n")
