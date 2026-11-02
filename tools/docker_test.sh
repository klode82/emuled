#!/bin/bash
# Real test of the Docker image (single data volume, PUID/PGID, separate Temp/Incoming volumes). Needs Docker; always --offline.
#   tools/docker_test.sh [IMAGE]       default image: emuled-vol:test  (build it first: docker build -t emuled-vol:test .)
# Bench 922-927. Not part of ctest: it needs Docker and takes about a minute.
IMG=${1:-emuled-vol:test}
S=$(mktemp -d)
trap 'docker rm -f d1 d2 d3 d4 d5 d6 >/dev/null 2>&1; docker volume rm dv1 dv2 dv3 dv4a dv4b dv4c >/dev/null 2>&1; chmod -R u+rwx "$S" 2>/dev/null; rm -rf "$S"' EXIT
ok(){ echo "PASS  $1"; }; ko(){ echo "FAIL  $1  [$2]"; FAILS=$((FAILS+1)); }
FAILS=0; cleanup(){ docker rm -f d1 d2 d3 d4 d5 d6 >/dev/null 2>&1; docker volume rm dv1 dv2 dv3 dv4a dv4b dv4c >/dev/null 2>&1; }
cleanup; rm -rf $S/h1 $S/h5 $S/h6; mkdir -p $S/h1 $S/h5 $S/h6; chmod 555 $S/h6
up(){ docker ps --format '{{.Names}}' | grep -qx "$1"; }
own(){ docker exec "$1" stat -c %u:%g "${@:2}" | tr '\n' ' '; }
# 1 host dir mine, default PUID
docker run -d --name d1 -v $S/h1:/data $IMG --offline >/dev/null; sleep 6
up d1 && [ "$(ls -ln $S/h1 | awk 'NR>1{print $3":"$4}' | sort -u)" = "1000:1000" ] && ok "1 host folder uid 1000 default" || ko "1 host folder default" "$(ls -ln $S/h1 | head -3)"
# 2 PUID/PGID on a new volume
docker run -d --name d2 -e PUID=1234 -e PGID=2345 -v dv2:/data $IMG --offline >/dev/null; sleep 6
up d2 && [ "$(own d2 /data /data/config /data/Temp /data/Shared)" = "1234:2345 1234:2345 1234:2345 1234:2345 " ] && ok "2 PUID=1234 PGID=2345 new volume" || ko "2 PUID/PGID" "$(own d2 /data /data/config /data/Temp /data/Shared)"
# 3 volume of root files
docker run --rm -v dv3:/data alpine sh -c 'mkdir -p /data/config /data/Temp /data/Incoming && echo x > /data/config/old.txt && echo y > /data/Temp/old.part && echo z > /data/Incoming/done.mkv && chown -R 0:0 /data'
docker run -d --name d3 -v dv3:/data $IMG --offline >/dev/null; sleep 6
R=$(own d3 /data/config/old.txt /data/Temp /data/Temp/old.part /data/Incoming /data/Incoming/done.mkv)
up d3 && [ "$R" = "1000:1000 1000:1000 1000:1000 1000:1000 0:0 " ] && ok "3 root-owned volume: config/Temp deep, Incoming top only" || ko "3 root volume" "$R"
# 4 separate volumes for Temp and Incoming (new, root-owned mount points)
docker run -d --name d4 -v dv4a:/data -v dv4b:/data/Temp -v dv4c:/data/Incoming -p 127.0.0.1:18998:8765 $IMG --offline >/dev/null; sleep 7
W=$(docker exec --user 1000:1000 d4 sh -c 'touch /data/Temp/x.part && touch /data/Incoming/x.mkv && echo writable' 2>&1)
up d4 && [ "$W" = "writable" ] && ok "4 Temp and Incoming as separate volumes are writable" || ko "4 separate volumes" "$W"
X=$(docker exec --user 1000:1000 d4 perl -e 'rename("/data/Temp/x.part","/data/Incoming/x.part") ? print "rename-ok" : print "$!"')
[ "$X" = "Invalid cross-device link" ] && ok "4b rename between the two mounts gives EXDEV (so the copy fallback is the real path here)" || ko "4b EXDEV" "$X"
T=$(docker exec d4 cat /data/config/emuled.token); D=$(curl -s -H "Authorization: Bearer $T" http://127.0.0.1:18998/api/v1/directories)
echo "$D" | grep -q '"incoming":"/data/Incoming/"' && echo "$D" | grep -q '"temp":\["/data/Temp/"\]' && ok "4c API directories: /data/Temp and /data/Incoming" || ko "4c API" "$D"
# 5 --user without root
docker run -d --name d5 --user 1000:1000 -v $S/h5:/data $IMG --offline >/dev/null; sleep 5; up d5 && ok "5 --user 1000:1000 runs" || ko "5 --user" ""
# 6 --user on a non-writable dir
M=$(docker run --rm --user 4242:4242 -v $S/h6:/data $IMG --offline 2>&1 | head -1)
echo "$M" | grep -q "cannot write to /data as uid 4242" && ok "6 non-writable folder: clear message" || ko "6 message" "$M"
# 7 persistence on rm + run (volume 2)
T1=$(docker exec d2 cat /data/config/emuled.token); docker rm -f d2 >/dev/null; docker run -d --name d2 -e PUID=1234 -e PGID=2345 -v dv2:/data $IMG --offline >/dev/null; sleep 6
T2=$(docker exec d2 cat /data/config/emuled.token); [ -n "$T1" ] && [ "$T1" = "$T2" ] && ok "7 token identical after rm + run" || ko "7 persistence" "$T1 / $T2"
echo "FAILS=$FAILS"; exit $((FAILS != 0))
