#!/bin/sh
# Buildroot post-build: install Ruffle + demo SWFs into the target rootfs.
set -e
TARGET_DIR="$1"
BOARD_DIR="$(dirname "$0")"
DL="$BOARD_DIR/../../dl-flashos"

chmod 0755 "$TARGET_DIR/usr/bin/flash-run" "$TARGET_DIR/usr/bin/flash-mount" \
           "$TARGET_DIR/etc/init.d/S50flashos"

# Ruffle (downloaded by build.sh)
if [ -f "$DL/ruffle.tar.gz" ]; then
    rm -rf "$TARGET_DIR/opt/ruffle"
    mkdir -p "$TARGET_DIR/opt/ruffle"
    tar -xzf "$DL/ruffle.tar.gz" -C "$TARGET_DIR/opt/ruffle"
    RUFFLE_BIN=$(find "$TARGET_DIR/opt/ruffle" -type f -name ruffle -perm -u+x | head -n1)
    [ "$RUFFLE_BIN" != "$TARGET_DIR/opt/ruffle/ruffle" ] && mv "$RUFFLE_BIN" "$TARGET_DIR/opt/ruffle/ruffle"
    # strip docs/icons we don't need
    find "$TARGET_DIR/opt/ruffle" -mindepth 1 ! -name ruffle -exec rm -rf {} + 2>/dev/null || true

    # Warn about any shared library Ruffle needs that is missing in the image
    for lib in $(readelf -d "$TARGET_DIR/opt/ruffle/ruffle" | sed -n 's/.*Shared library: \[\(.*\)\]/\1/p'); do
        if ! find "$TARGET_DIR/lib" "$TARGET_DIR/usr/lib" -name "$lib" 2>/dev/null | grep -q .; then
            echo "WARNING: ruffle needs $lib which is not in the target" >&2
        fi
    done
else
    echo "WARNING: $DL/ruffle.tar.gz not found - run ./build.sh (Ruffle not installed)" >&2
fi

# SWF library baked into the image
mkdir -p "$TARGET_DIR/swf"
cp -f "$BOARD_DIR"/swf/*.swf "$TARGET_DIR/swf/" 2>/dev/null || true
