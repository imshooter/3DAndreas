#!/bin/bash
# Builds a test script against the 3DAndreas include and runs it on an isolated
# copy of the open.mp server in SERVER, with the freshly built 3DAndreas.dll, the
# server's world.db and its models folder: SERVER=<folder> run.sh [script]
# [timeout]. DLL=<path> runs another build, such as the installed one. The copy
# binds 127.0.0.1 on a free port and ends when the script stops it or at the
# timeout. Git Bash.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
server="${SERVER:?set SERVER to the folder of an open.mp server}"
name="${1:-issues}"
limit="${2:-120}"
dll="${DLL:-$root/build/Release/3DAndreas.dll}"
run="$TEMP/a3d-test"

case "$run" in
    */a3d-test) ;;
    *) echo "BAD RUN DIR"; exit 1 ;;
esac

rm -rf "$run"
mkdir -p "$run/components" "$run/gamemodes" "$run/scriptfiles/3dandreas" "$run/models"
cp "$server/omp-server.exe" "$run/"
for c in Actors Classes Console CustomModels GangZones LegacyNetwork NPCs Objects Pawn Timers Variables Vehicles; do
    cp "$server/components/$c.dll" "$run/components/" || exit 1
done
cp "$dll" "$run/components/3DAndreas.dll" || { echo "NO DLL"; exit 1; }
cp "$server/scriptfiles/3dandreas/world.db" "$run/scriptfiles/3dandreas/" || exit 1
[ -d "$server/models" ] && cp -r "$server/models/." "$run/models/"
[ -f "$root/scriptfiles/3dandreas/models.json" ] && cp "$root/scriptfiles/3dandreas/models.json" "$run/scriptfiles/3dandreas/"

"$server/qawno/pawncc.exe" "$(cygpath -w "$here/$name.pwn")" "-i$(cygpath -w "$root/include")" "-i$(cygpath -w "$server/qawno/include")" "-;+" "-(+" "-d2" "-o$(cygpath -w "$run/gamemodes/$name.amx")" > "$run/build.txt" 2>&1
[ -f "$run/gamemodes/$name.amx" ] || { echo "COMPILE FAILED"; grep -i "error\|warning" "$run/build.txt" | head; exit 1; }
grep -i "warning" "$run/build.txt" | head

port=$(powershell -NoProfile -Command '$u=[Net.Sockets.UdpClient]::new(0);$u.Client.LocalEndPoint.Port;$u.Close()' | tr -d '\r')
cat > "$run/config.json" <<EOF
{
    "announce": false,
    "enable_query": false,
    "max_bots": 4,
    "max_players": 8,
    "name": "3dandreas test",
    "network": { "bind": "127.0.0.1", "port": $port },
    "pawn": { "legacy_plugins": [], "main_scripts": ["$name"], "side_scripts": [] },
    "rcon": { "enable": false, "password": "a3d-test-local-only" },
    "logging": { "use_prefix": false, "use_timestamp": false },
    "artwork": { "enable": true, "models_path": "models", "port": $port, "web_server_bind": "127.0.0.1", "cdn": "" },
    "3dandreas": { "threads": 2 }
}
EOF

cd "$run" && timeout "$limit" ./omp-server.exe > stdout.txt 2>&1
grep -a "\[$name\]\|3DAndreas\|rror\|crash\|Run time" "$run/stdout.txt"
