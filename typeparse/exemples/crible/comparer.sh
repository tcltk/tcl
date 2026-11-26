#!/bin/sh
# comparer.sh -- le crible dans Tcl et dans les langages présents.
#   sh comparer.sh /chemin/vers/tclsh
# Colonnes : créer, cribler, compter, total (ms, meilleur de 5), N = 10^6.
TCLSH=${1:-tclsh9.0}
ICI=$(cd "$(dirname "$0")" && pwd)
A=$ICI/autres
"$TCLSH" "$ICI/crible.tcl"
echo "-- autres langages"
if command -v gcc >/dev/null; then
    T=$(mktemp -d); gcc -O2 -o "$T/crible" "$A/crible.c" && "$T/crible"; rm -rf "$T"
fi
command -v node    >/dev/null && node "$A/crible.js"
command -v python3 >/dev/null && python3 "$A/crible.py"
command -v php     >/dev/null && php "$A/crible.php"
command -v ruby    >/dev/null && ruby "$A/crible.rb"
command -v perl    >/dev/null && perl "$A/crible.pl"
exit 0
