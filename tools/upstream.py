"""Cartella dei sorgenti ORIGINALI di eMule (sola lettura). Il valore sta in un posto solo: EMULED_UPSTREAM in CMakeLists.txt."""
import os, re
ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
_m = re.search(r'^set\(EMULED_UPSTREAM\s+"?([^")\s]+)', open(os.path.join(ROOT, "CMakeLists.txt"), encoding="utf-8").read(), re.M)
UPSTREAM = os.path.join(ROOT, _m.group(1))
SRCHYBRID = os.path.join(UPSTREAM, "srchybrid")
