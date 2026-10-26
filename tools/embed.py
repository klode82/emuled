#!/usr/bin/env python3
"""embed.py FILE OUT.h NOME: scrive in OUT.h un array di char con il contenuto di FILE (per includerlo nel binario)."""
import sys
data = open(sys.argv[1], "rb").read()
name = sys.argv[3]
with open(sys.argv[2], "w") as o:
    o.write("// generato da tools/embed.py: non modificare\n#pragma once\nstatic const char %s[] = {\n" % name)
    for i in range(0, len(data), 24):
        o.write(",".join(str(b - 256 if b > 127 else b) for b in data[i:i + 24]) + ",\n")		# char con segno: un byte UTF-8 oltre 127 non puo' essere una costante senza segno (narrowing)
    o.write("0};\nstatic const unsigned long %s_size = %d;\n" % (name, len(data)))
