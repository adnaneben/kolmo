#!/bin/sh
# Fetch the two data sets used in src/experimental/KAUD.md into DIR/esc and DIR/music.
# Needs curl and flac (the music is published as FLAC; `flac -d` gives the 16-bit WAV files measured).
#
# ESC-50 (K. J. Piczak, https://github.com/karolpiczak/ESC-50): 40 clips, 5 s, 44.1 kHz mono 16-bit.
#   Licence: Creative Commons Attribution-NonCommercial 3.0 (clips taken from freesound.org).
# Music (archive.org), 16-bit stereo:
#   arrows_voyage_07          ncr3327_voyage/07.flac, "Voyage" by The Sound of Arrows (NCR3327),
#                             CC0 1.0, 44.1 kHz, 110.2 s
#   dso_shostakovich5_mvt4    dso20060204old/dso20060204.4.flac, Shostakovich Symphony No. 5, 4th mvt.,
#                             public domain dedication (creativecommons.org/licenses/publicdomain), 48 kHz, 288.2 s
#   nin_ghosts_07             nineinchnails_ghosts_I_IV/07_Ghosts_I.flac, Nine Inch Nails "Ghosts I" 7,
#                             CC BY-NC-SA 3.0 US, 44.1 kHz, 120.6 s
#   revivalists_amber_live    revivalists2016-10-01.mk21.flac16/revivalists2016-10-01d1t02_Amber.flac,
#                             The Revivalists live at Hoopla 2016-10-01, CC0 1.0, 44.1 kHz, 283.7 s
set -e
D=${1:?usage: fetch_data.sh DIR}
mkdir -p "$D/esc" "$D/music" "$D/flac"
for n in 1-100032-A-0 1-16746-A-15 1-208757-B-2 1-28808-A-43 1-40621-A-28 1-51433-A-17 1-60997-A-20 1-77241-A-3 \
         2-100648-A-43 2-108761-A-14 2-118624-A-30 2-133889-A-30 2-18766-A-12 2-51173-A-35 2-70938-A-42 2-82455-A-23 \
         3-100018-A-18 3-116135-A-1 3-128512-A-47 3-143560-A-47 3-151212-A-24 3-157149-A-10 3-170383-A-38 3-246513-B-16 \
         4-102844-A-49 4-140034-A-46 4-161099-B-47 4-169127-B-41 4-180337-A-28 4-187384-A-34 4-196671-A-8 4-212604-B-15 \
         5-103415-A-2 5-177614-A-5 5-192191-A-19 5-201170-A-46 5-212454-A-0 5-220027-A-21 5-237315-A-31 5-251957-A-47; do
    curl -sSfL -o "$D/esc/$n.wav" "https://raw.githubusercontent.com/karolpiczak/ESC-50/master/audio/$n.wav"
done
get() { curl -sSfL -o "$D/flac/$2.flac" "https://archive.org/download/$1"; flac -s -d -f -o "$D/music/$2.wav" "$D/flac/$2.flac"; }
get ncr3327_voyage/07.flac arrows_voyage_07
get dso20060204old/dso20060204.4.flac dso_shostakovich5_mvt4
get nineinchnails_ghosts_I_IV/07_Ghosts_I.flac nin_ghosts_07
get revivalists2016-10-01.mk21.flac16/revivalists2016-10-01d1t02_Amber.flac revivalists_amber_live
cd "$D" && sha256sum esc/*.wav music/*.wav > SHA256SUMS
