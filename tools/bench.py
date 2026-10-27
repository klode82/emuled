"""Helper Bench. Token da env BT. Uso:
  bench.py list                      -> id, stato, titolo di ogni task
  bench.py note ID "testo"           -> aggiunge nota
  bench.py st ID STATUS [PERCENT]    -> cambia stato (7 aperto, 8 in corso, 10 completato)
  bench.py dep TASK TARGET [task|phase] [start|complete] -> TASK aspetta TARGET
  bench.py new PHASE TYPE "titolo" "descrizione" -> crea task
  bench.py sub PARENT "titolo" "descrizione" -> crea un sotto-task (stessa fase e tipo del padre)
"""
import json, os, sys, urllib.request

B, P = "http://localhost:8201/api", 10


def call(m, path, body=None):
    r = urllib.request.Request(B + path, method=m, data=json.dumps(body).encode() if body is not None else None,
                               headers={"Authorization": "Bearer " + os.environ["BT"], "Content-Type": "application/json"})
    with urllib.request.urlopen(r) as f:
        return json.load(f)


a = sys.argv[1:]
if a[0] == "list":
    for p in call("GET", f"/projects/{P}/roadmap")["phases"]:
        print(f"## {p['id']} {p['title']}")
        for t in p["tasks"]:
            print(f"{t['id']:4} s{t['status_id']:<3} {t['completion_percent']:3}%  {t['title']}")
elif a[0] == "note":
    call("POST", f"/tasks/{a[1]}/notes", {"body": a[2]})
elif a[0] == "st":
    b = {"status_id": int(a[2])}
    if len(a) > 3:
        b["completion_percent"] = int(a[3])
    print(call("PATCH", f"/tasks/{a[1]}", b)["completion_percent"])
elif a[0] == "new":
    print(call("POST", "/tasks", {"phase_id": int(a[1]), "task_type_id": int(a[2]), "title": a[3], "description": a[4] if len(a) > 4 else "", "status_id": 7})["id"])
elif a[0] == "dep":      # dep TASK_ID TARGET [task|phase] [start|complete] -> TASK_ID aspetta TARGET
    kind = a[3] if len(a) > 3 else "task"
    print(call("POST", f"/tasks/{a[1]}/dependencies", {kind + "_id": int(a[2]), "blocks": a[4] if len(a) > 4 else "complete"}))
elif a[0] == "sub":      # sub PARENT "titolo" "descrizione": sotto-task nella fase del padre, tipo 54
    phase = next(p["id"] for p in call("GET", f"/projects/{P}/roadmap")["phases"] for t in p["tasks"] if t["id"] == int(a[1]))
    print(call("POST", "/tasks", {"phase_id": phase, "parent_task_id": int(a[1]), "task_type_id": 54, "title": a[2], "description": a[3] if len(a) > 3 else "", "status_id": 7})["id"])
