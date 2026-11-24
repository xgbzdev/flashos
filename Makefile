# FlashOS build.  Copyright (c) 2026 FlashOS. All rights reserved.
#
#   make            -> build/FlashOS.iso   (boot CD / VM / USB stick)
#   make run        -> boot it in QEMU
#   make sim        -> render screenshots with the host simulator (no emulator needed)

CC      ?= gcc
PY      ?= python3
CFLAGS  := -m32 -march=i686 -mno-sse -mno-sse2 -mno-mmx -mfpmath=387 -O2 \
           -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fno-builtin-printf \
           -fno-asynchronous-unwind-tables -fno-exceptions -nostdlib -Wall -Wextra
LDFLAGS := -m elf_i386 -T kernel/linker.ld -nostdlib

KSRC    := kernel/main.c kernel/lib.c kernel/video.c kernel/fs.c kernel/inflate.c \
           kernel/render.c kernel/player.c kernel/avm1.c kernel/ata.c kernel/net.c
KOBJ    := $(patsubst kernel/%.c,build/%.o,$(KSRC) kernel/plat_pc.c)
FILES   := $(wildcard swf/*.swf) $(wildcard files/*)

all: build/FlashOS.iso

build/%.o: kernel/%.c kernel/kernel.h kernel/swf.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/entry.o: kernel/entry.asm | build
	nasm -f elf32 $< -o $@

build/flashkrn.elf: build/entry.o $(KOBJ) kernel/linker.ld
	ld $(LDFLAGS) -o $@ build/entry.o $(KOBJ)
	@if nm $@ | grep -q ' U '; then nm $@ | grep ' U '; echo "undefined symbols"; exit 1; fi

build/flashkrn.bin: build/flashkrn.elf
	objcopy -O binary $< $@

build/mbr.bin: boot/mbr.asm | build
	nasm -f bin $< -o $@

S2FLAGS := -m16 -march=i386 -Os -ffreestanding -fno-pic -fno-pie -fno-stack-protector \
           -fno-asynchronous-unwind-tables -fno-tree-loop-distribute-patterns -mno-sse -Wall -Wextra

build/stage2.o: boot/stage2.c | build
	$(CC) $(S2FLAGS) -c $< -o $@

build/s2entry.o: boot/s2entry.asm | build
	nasm -f elf32 $< -o $@

build/stage2.bin: build/s2entry.o build/stage2.o boot/stage2.ld
	ld -m elf_i386 -T boot/stage2.ld -nostdlib -o build/stage2.elf build/s2entry.o build/stage2.o
	@if nm build/stage2.elf | grep -q ' U '; then nm build/stage2.elf | grep ' U '; exit 1; fi
	objcopy -O binary build/stage2.elf $@
	@test $$(stat -c %s $@) -le 28672 || (echo "stage2 too big"; exit 1)
	@if objdump -d -Mi8086 build/stage2.elf | grep -E '^\s+[0-9a-f]+:\s+(f[23] )?(a[4-7]|a[a-f])\s' ; then \
		echo "stage2: 16-bit string instruction (would wrap at 64 KB)"; exit 1; fi

build/loader.bin: boot/loader.asm | build
	nasm -f bin $< -o $@

swf/bounce.swf swf/flashlogo.swf &: tools/make_demo_swf.py
	$(PY) tools/make_demo_swf.py swf

build/flashfs.bin: tools/mkfs.py swf/bounce.swf swf/flashlogo.swf $(FILES) | build
	$(PY) tools/mkfs.py $@ $(sort $(wildcard swf/*.swf) $(wildcard files/*))

build/FlashOS.iso: build/mbr.bin build/loader.bin build/stage2.bin build/flashkrn.bin build/flashfs.bin tools/mkiso.py
	$(PY) tools/mkiso.py build/mbr.bin build/loader.bin build/flashkrn.bin build/flashfs.bin build/stage2.bin $@

run: build/FlashOS.iso
	qemu-system-i386 -m 256 -vga std -cdrom $< -boot d

run-usb: build/FlashOS.iso
	qemu-system-i386 -m 256 -vga std -drive file=$<,format=raw

# ---- host simulator: the same kernel code, rendered to PNG files ----
build/sim: host/sim.c $(KSRC) kernel/kernel.h kernel/swf.h | build
	cc -DHOSTED -O2 -g -Ikernel -o $@ host/sim.c $(KSRC) -lm

build/s2test: boot/stage2.c host/s2test.c | build
	cc -DHOSTED_S2 -O1 -g -o $@ boot/stage2.c host/s2test.c

sim: build/sim build/flashfs.bin
	$(PY) host/make_font.py build/font.bin
	cd build && ./sim ../host/script.txt

build:
	mkdir -p build

clean:
	rm -rf build

.PHONY: all run run-usb sim clean
