# FlashOS

**FlashOS** is an operating system that boots straight into a red retro
terminal and can only run `.swf` (Flash) files.

It runs on **FlashKernel**, its own proprietary kernel written from scratch.
There's no Linux, BusyBox or GRUB, and no third-party code. Everything from
the boot sector up is in this repository.

```
 ___ _         _    ___  ___
| __| |__ _ __| |_ / _ \/ __|
| _|| / _` (_-< ' \ (_) \__ \
|_| |_\__,_/__/_||_\___/|___/
Welcome to FlashOS,Insert An Command,



>_
```

## Boot animation

The screen goes black, then the logo and welcome line are drawn **one pixel row
at a time, from the top down**, like an old PC slowly painting an image while
loading it. There are short pauses as each "disk block" arrives. Then the prompt
appears.

## Commands

| Command            | What it does                                  |
|--------------------|-----------------------------------------------|
| `LS`               | list files                                    |
| `RUN <name or #>`  | play a SWF (or just type `game.swf` / `game`) |
| `CLS`, `ABOUT`, `HELP`, `REBOOT`, `SHUTDOWN` | the usual           |

Anything that isn't a real SWF file is refused with **ACCESS DENIED**. FlashOS
checks the file extension and also the file's header (`FWS`/`CWS`/`ZWS`). While a
movie plays, **Esc**, **F12** or **Ctrl+Q** returns to the prompt.

## The built-in Flash player

FlashKernel has its own SWF player:

* vector shapes with straight and curved edges, anti-aliased
* solid, linear-gradient, radial-gradient and bitmap fills, transparency, outlines
* the timeline, sprites (movie clips) with their own timelines, colour effects
* lossless bitmaps, and compressed `CWS` files (FlashKernel's own zlib decoder)
* the movie is scaled to fit the screen

Not supported yet: ActionScript (no interactivity), text and fonts, JPEG images,
sound, masks, morph shapes, and LZMA-compressed `ZWS` files.

## Build

You need `nasm`, `gcc` with 32-bit support, `make` and `python3`:

```sh
sudo apt install nasm gcc-multilib make python3 qemu-system-x86
make            # -> build/FlashOS.iso  (about 100 KB)
make run        # boot it as a CD in QEMU
make run-usb    # boot it as a USB disk in QEMU
```

On GitHub, every push to `main` builds the ISO, boots it in QEMU, takes
screenshots, and publishes `FlashOS.iso` on the **Releases** page.

To boot a real PC, write the ISO to a USB stick
(`sudo dd if=build/FlashOS.iso of=/dev/sdX bs=4M`) or use Rufus in **DD mode**.
The PC must support **legacy BIOS / CSM** boot; FlashOS doesn't boot through UEFI yet.
The keyboard layout is US.

**Adding your own games:** drop `.swf` files into `swf/` and run `make`. They're
packed into the FlashFS archive on the disc. The total must stay under about
200 MB, and the PC needs about 64 MB of RAM plus the size of your files.

## How it fits together

```
boot/mbr.asm        USB/HDD boot sector: loads the stage loader
boot/loader.asm     stage loader: A20, BIOS font, loads kernel + FlashFS
                    through unreal mode, picks a VESA 1024x768 mode,
                    enters 32-bit protected mode
kernel/entry.asm    kernel entry
kernel/main.c       boot animation + shell
kernel/video.c      framebuffer, back buffer, 16x32 text console
kernel/plat_pc.c    PS/2 keyboard, TSC/PIT timer, reboot / power-off
kernel/fs.c         FlashFS read-only file archive
kernel/inflate.c    DEFLATE decoder
kernel/swf.c        SWF parser, timeline and anti-aliased vector rasteriser
tools/mkiso.py      writes the ISO 9660 + El Torito + MBR hybrid image
tools/mkfs.py       packs files into FlashFS
tools/make_demo_swf.py  builds the demo movies
host/sim.c          runs the real kernel code on your PC and saves screenshots
                    (`make sim`): handy for testing without an emulator
```

Copyright (c) 2026 FlashOS. All rights reserved.
