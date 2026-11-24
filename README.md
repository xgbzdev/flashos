# FlashOS 1.0

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
| `MEM`              | show how much RAM is installed                 |
| `CLS`, `ABOUT`, `HELP`, `REBOOT`, `SHUTDOWN` | the usual           |

**Up / Down arrows** bring back earlier commands, as in a normal terminal.

Anything that isn't a real SWF file is refused with **ACCESS DENIED**. FlashOS
checks the file extension and also the file's header (`FWS`/`CWS`/`ZWS`).

While a movie plays, **F12** or **Ctrl+Q** returns to the prompt. Esc also works
for plain animations; in ActionScript games Esc goes to the game.

## The built-in Flash player

FlashKernel has its own SWF player:

* vector shapes with straight and curved edges, anti-aliased
* solid, linear-gradient, radial-gradient and bitmap fills, transparency, outlines
* the timeline, sprites (movie clips) with their own timelines, colour effects
* lossless bitmaps, and compressed `CWS` files (FlashKernel's own zlib decoder)
* the movie is scaled to fit the screen
* text: static text drawn from the movie's own fonts, and dynamic text fields
  drawn in the PC's pixel font

### ActionScript 1.0 / 2.0

FlashKernel runs ActionScript 1.0 and 2.0 (the AVM1 engine used by Flash 5–8
games):

* the full AVM1 instruction set: variables, functions, closures, registers,
  objects, prototypes, AS2 classes (`extends`, `super`), `with`, `try`/`catch`
* timeline control: `play`, `stop`, `gotoAndPlay`, `gotoAndStop`, frame labels
* buttons (up / over / down / hit states) and `on (press)`, `on (release)`,
  `on (rollOver)`, `on (keyPress "<Left>")`…
* `onClipEvent(load / enterFrame / mouseDown / keyDown …)`, `onEnterFrame`,
  `onPress`, `onRelease`, `onRollOver`…
* **keyboard:** `Key.isDown()`, `Key.getCode()`, `Key.getAscii()`,
  `Key.addListener()` with `onKeyDown` / `onKeyUp`, and all `Key.LEFT`… constants
* **mouse:** PS/2 mouse, `_xmouse` / `_ymouse`, `startDrag` / `stopDrag`,
  `Mouse.hide()` / `show()`, `Mouse.addListener()`
* `MovieClip`: `attachMovie`, `duplicateMovieClip`, `createEmptyMovieClip`,
  `createTextField`, `removeMovieClip`, `hitTest`, `getBounds`, `swapDepths`,
  `localToGlobal`… and the drawing API (`lineStyle`, `beginFill`, `lineTo`,
  `curveTo`…)
* properties `_x _y _xscale _yscale _rotation _alpha _visible _width _height
  _currentframe _totalframes _name _parent _root`
* `Math`, `String`, `Array`, `Number`, `Object`, `Color`, `Date` (from the PC
  clock), `setInterval` / `setTimeout`, `getTimer`, `Object.registerClass`
* `fscommand("quit")` returns to the prompt

When a movie uses ActionScript, FlashOS shows its own **pixelated black mouse
cursor** with a white outline. It turns into a pixel **hand** over buttons.

Not supported yet: ActionScript 3 (Flash 9+), JPEG images, sound, masks,
filters, morph shapes, typing into input text fields, and LZMA-compressed `ZWS` files.

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
The keyboard layout is US. Mice must be PS/2 or USB with the BIOS's PS/2
emulation (most PCs and all virtual machines). In VirtualBox, click inside the
window so it captures the mouse.

**Adding your own games:** drop `.swf` files into `swf/` and run `make`. They're
packed into the FlashFS archive on the disc. The total must stay under about
200 MB. FlashOS needs at least 64 MB of RAM plus the size of your files;
128–256 MB is recommended for ActionScript games.

## How it fits together

```
boot/mbr.asm        USB/HDD boot sector: loads the stage loader
boot/loader.asm     stage loader: A20, BIOS font, loads kernel + FlashFS
                    through unreal mode, picks a VESA 1024x768 mode,
                    enters 32-bit protected mode
kernel/entry.asm    kernel entry
kernel/main.c       boot animation + shell
kernel/video.c      framebuffer, back buffer, 16x32 text console
kernel/plat_pc.c    PS/2 keyboard + mouse, TSC/PIT timer, CMOS clock, power
kernel/fs.c         FlashFS read-only file archive
kernel/inflate.c    DEFLATE decoder
kernel/render.c     anti-aliased vector rasteriser, text, pixel cursor
kernel/player.c     SWF timeline, display tree, buttons, mouse/key events
kernel/avm1.c       ActionScript 1.0/2.0 virtual machine and built-in classes
tools/mkiso.py      writes the ISO 9660 + El Torito + MBR hybrid image
tools/mkfs.py       packs files into FlashFS
tools/make_demo_swf.py  builds the demo movies (it includes a small AVM1
                    assembler for catch.swf, an ActionScript game)
host/sim.c          runs the real kernel code on your PC and saves screenshots
                    (`make sim`): handy for testing without an emulator
```

Copyright (c) 2026 FlashOS. All rights reserved.
