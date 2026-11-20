#!/bin/bash
# Runs each named test with the 3DAndreas.dll installed in SERVER and with the
# fresh build, then shows the lines that differ, without timings:
# SERVER=<folder> compare.sh test...
set -u
here=$(cd "$(dirname "$0")" && pwd)
installed="${SERVER:?set SERVER to the folder of an open.mp server}/components/3DAndreas.dll"
out="$TEMP/a3d-compare"
mkdir -p "$out"
for name in "$@"; do
    for side in installed fresh; do
        if [ "$side" = installed ]; then
            DLL="$installed" bash "$here/run.sh" "$name" 120 > /dev/null
        else
            bash "$here/run.sh" "$name" 120 > /dev/null
        fi
        grep -a -v "^Loading\|^	\|Successfully\|read in\|built in\| ms\b\|[0-9]ms\|milliseconds\|Run time" "$TEMP/a3d-test/stdout.txt" | sed 's/[0-9]*\.[0-9]* ms//g' > "$out/$name.$side.txt"
    done
    echo "== $name: $(wc -l < "$out/$name.fresh.txt") lines"
    diff "$out/$name.installed.txt" "$out/$name.fresh.txt" | head -20
    grep -a "FAIL\|DONE\|rror" "$out/$name.fresh.txt" | head -8
done
