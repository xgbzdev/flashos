#!/bin/sh
# Build FlashOS: downloads Buildroot + Ruffle, then builds a bootable ISO.
# Needs a normal Linux build host (Debian/Ubuntu: see README for packages).
set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
BR_VERSION="${BR_VERSION:-2025.02.x}"        # Buildroot LTS branch
BR_DIR="$HERE/buildroot"
OUT_DIR="$HERE/output"
DL="$HERE/dl-flashos"

mkdir -p "$DL"

# 1. Buildroot
if [ ! -d "$BR_DIR" ]; then
    echo ">>> Fetching Buildroot $BR_VERSION"
    git clone --depth 1 --branch "$BR_VERSION" https://gitlab.com/buildroot.org/buildroot.git "$BR_DIR"
fi

# 2. Ruffle (latest Linux x86_64 build from GitHub)
if [ ! -f "$DL/ruffle.tar.gz" ]; then
    echo ">>> Fetching Ruffle"
    AUTH=""; [ -n "$GITHUB_TOKEN" ] && AUTH="Authorization: Bearer $GITHUB_TOKEN"
    URL="${RUFFLE_URL:-$(curl -fsSL ${AUTH:+-H "$AUTH"} https://api.github.com/repos/ruffle-rs/ruffle/releases?per_page=5 \
        | grep -o '"browser_download_url": *"[^"]*linux-x86_64\.tar\.gz"' \
        | head -n1 | sed 's/.*"\(https[^"]*\)"/\1/')}"
    [ -n "$URL" ] || { echo "Could not find a Ruffle Linux build; set RUFFLE_URL"; exit 1; }
    echo "    $URL"
    curl -fL -o "$DL/ruffle.tar.gz" "$URL"
fi

# 3. Demo SWF
mkdir -p "$HERE/board/flashos/swf"
python3 "$HERE/tools/make_demo_swf.py" "$HERE/board/flashos/swf/bounce.swf"

# 4. Build
make -C "$BR_DIR" O="$OUT_DIR" BR2_EXTERNAL="$HERE" flashos_defconfig
make -C "$BR_DIR" O="$OUT_DIR"

echo
echo ">>> Done:"
ls -lh "$OUT_DIR/images/rootfs.iso9660" "$OUT_DIR/images/bzImage" "$OUT_DIR/images/rootfs.cpio.gz"
echo "Run it with ./run-qemu.sh, or write the ISO to a USB stick:"
echo "    sudo dd if=$OUT_DIR/images/rootfs.iso9660 of=/dev/sdX bs=4M status=progress"
