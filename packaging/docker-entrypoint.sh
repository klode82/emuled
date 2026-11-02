#!/bin/sh
# emuled in Docker: prepares the single data volume (/data) and starts the daemon.
#   /data/config    profile (preferences, server.met, users, token, ...)   EMULED_CONFIG
#   /data/Temp      downloads in progress                  } created by the daemon next to the profile
#   /data/Incoming  completed downloads                    }
#   /data/Shared    folders to share (choose them in Preferences > Directories)
# Started as root (the default) it runs the daemon as the user `emule` with the uid/gid PUID/PGID (default 1000), so a folder of the
# host owned by that user can be mounted on /data. Started with `docker run --user UID:GID` it just runs as that user.
set -e
DATA=${EMULED_DATA:-/data}

if [ "$(id -u)" = 0 ]; then
	PUID=${PUID:-1000}
	PGID=${PGID:-1000}
	[ "$(id -g emule)" = "$PGID" ] || groupmod -o -g "$PGID" emule
	[ "$(id -u emule)" = "$PUID" ] || usermod -o -u "$PUID" emule
	mkdir -p "$DATA/config" "$DATA/Shared"
	# Every folder is checked on its own: Temp, Incoming ... may be separate mounts (Docker creates a new volume as root). Only the
	# ones that belong to somebody else are touched. The profile and Temp (few files: the downloads in progress) are done in depth;
	# the others only at their top level, they can hold millions of files on a NAS.
	own() { [ "$(stat -c %u:%g "$1")" = "$PUID:$PGID" ]; }
	if ! own "$DATA"; then
		echo "emuled: giving $DATA to $PUID:$PGID" >&2
		chown "$PUID:$PGID" "$DATA"
	fi
	for d in config Temp; do
		if [ -d "$DATA/$d" ] && ! own "$DATA/$d"; then
			echo "emuled: giving $DATA/$d to $PUID:$PGID (in depth)" >&2
			chown -R "$PUID:$PGID" "$DATA/$d"
		fi
	done
	for d in Shared Incoming logs guardian; do
		if [ -d "$DATA/$d" ] && ! own "$DATA/$d"; then
			echo "emuled: giving $DATA/$d to $PUID:$PGID (top level only)" >&2
			chown "$PUID:$PGID" "$DATA/$d"
		fi
	done
	exec gosu emule "$0" "$@"
fi

if ! mkdir -p "$DATA/config" "$DATA/Shared" 2>/dev/null || [ ! -w "$DATA" ] || [ ! -w "$DATA/config" ]; then
	echo "emuled: cannot write to $DATA as uid $(id -u): fix the owner of the folder mounted on $DATA, or set PUID and PGID" >&2
	exit 1
fi
exec emuled "$@"
