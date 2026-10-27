#!/usr/bin/env python3
"""Rigenera core/kademlia/io/KadLowerMap.inc da srchybrid/res/WideCharLowerMap.bin (sola lettura del sorgente Windows)."""
import struct, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from upstream import SRCHYBRID
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
d = open(os.path.join(SRCHYBRID, "res", "WideCharLowerMap.bin"), "rb").read()
m = struct.unpack("<65536H", d)
diffs = [(i, v) for i, v in enumerate(m) if i != v]
out = ["// GENERATED from srchybrid/res/WideCharLowerMap.bin (the Unicode -> lowercase map that ALL Kad nodes must use identically).",
       "// Differences only (character, lowercase): every other character stays the same. %d entries, sorted. Do not edit by hand." % len(diffs),
       "// Regenerate: python3 tools/gen_kad_lowermap.py", "static const uint16_t kKadLowerDiffs[][2] = {"]
for k in range(0, len(diffs), 8):
    out.append("\t" + ",".join("{0x%04X,0x%04X}" % p for p in diffs[k:k + 8]) + ",")
out.append("};")
open(os.path.join(root, "core", "kademlia", "io", "KadLowerMap.inc"), "w").write("\n".join(out) + "\n")
