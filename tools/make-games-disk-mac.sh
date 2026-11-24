#!/bin/bash
# Make a FAT32 "games disk" for FlashOS in VirtualBox, on a Mac.
# Copyright (c) 2026 FlashOS. All rights reserved.
#
#   ./make-games-disk-mac.sh ~/Desktop/MyGames
#
# Copies every .swf in the folder onto a new 128 MB FAT32 disk image and
# converts it to FlashGames.vdi (next to this script's working folder).
# In VirtualBox: Settings > Storage > IDE controller > Add hard disk >
# choose FlashGames.vdi. FlashOS lists the games with a USB tag.
set -e
SRC="${1:?usage: $0 FOLDER_WITH_SWF_FILES}"
OUT="${2:-FlashGames}"
TMP=$(mktemp -d /tmp/flashgames.XXXX)

hdiutil create -size 128m -fs "MS-DOS FAT32" -volname FLASHGAMES -layout MBRSPUD -type UDIF "$TMP/games.dmg" >/dev/null
MNT=$(hdiutil attach "$TMP/games.dmg" | awk -F'\t' '/FLASHGAMES/ {print $NF}')
find "$SRC" -maxdepth 1 -iname '*.swf' -exec cp {} "$MNT/" \;
ls "$MNT"
dot_clean "$MNT" 2>/dev/null || true
hdiutil detach "$MNT" >/dev/null
hdiutil convert "$TMP/games.dmg" -format UDTO -o "$TMP/games" >/dev/null
rm -f "$OUT.vdi"
VBoxManage convertfromraw "$TMP/games.cdr" "$OUT.vdi" --format VDI
rm -rf "$TMP"
echo "Made $OUT.vdi - attach it in VirtualBox (Storage > IDE > Add hard disk)."
echo "If you remade it, remove the old one first (File > Virtual Media Manager)."
