#!/bin/sh
# Regenerate the SDK's watch fonts (sdk/fonts/*.c) from Barlow Condensed (OFL).
# Numbers: digits + punctuation only (space..colon) to keep flash small.
set -e
cd "$(dirname "$0")/.."
F=tools/fonts/barlow-condensed
gen() { python3 tools/mkfont.py "$F/$1" "$2" "$3" --tabular ${4:+--range $4} > "sdk/fonts/$3.c"; echo "  $3"; }
gen BarlowCondensed-SemiBold.ttf 96 vw_font_num_96 32-58
gen BarlowCondensed-SemiBold.ttf 56 vw_font_num_56 32-58
gen BarlowCondensed-SemiBold.ttf 36 vw_font_num_36 32-58
gen BarlowCondensed-SemiBold.ttf 24 vw_font_label_24
gen BarlowCondensed-Medium.ttf   18 vw_font_label_18
