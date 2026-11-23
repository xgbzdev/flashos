#!/bin/sh
# Boot FlashOS in QEMU. Optional: ./run-qemu.sh /path/to/folder-with-swfs
HERE="$(cd "$(dirname "$0")" && pwd)"
IMG="$HERE/output/images"

EXTRA=""
if [ -n "$1" ] && [ -d "$1" ]; then
    # expose a host folder as a FAT "USB stick"
    EXTRA="-drive file=fat:ro:$1,format=raw,if=none,id=usb0 -device usb-storage,bus=xhci.0,drive=usb0"
fi

KVM=""
[ -w /dev/kvm ] && KVM="-enable-kvm -cpu host"

# shellcheck disable=SC2086
exec qemu-system-x86_64 $KVM -m 1024 -smp 2 \
    -vga std \
    -cdrom "$IMG/rootfs.iso9660" -boot d \
    -audiodev pa,id=snd0 -device intel-hda -device hda-duplex,audiodev=snd0 \
    -device qemu-xhci,id=xhci -device usb-tablet,bus=xhci.0 \
    $EXTRA
