# FlashOS 1.0

**FlashOS** is an operating system that boots straight into a red retro
terminal and can only run `.swf` (Flash) files.

It runs on **FlashKernel**, its own proprietary kernel written from scratch.
There's no Linux, BusyBox or GRUB. Everything from the boot sector up is in
this repository. The only outside code is the MP3 decoder
([minimp3](https://github.com/lieff/minimp3), public domain / CC0, so it can be
used without any conditions).

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
| `INSTALL`          | install FlashOS on the hard disk (only when started from the ISO) |
| `RESCAN`           | read game CDs / ISO images again               |
| `VOL [0-100/MUTE]` | sound volume                                   |
| `TIME`             | count 10 seconds, to check the clock speed     |
| `NET`              | connect to the wired network, show IP / status |
| `PING <host>`      | test the connection                            |
| `GET <http://url>` | download a SWF from the internet               |
| `CLS`, `ABOUT`, `HELP`, `REBOOT`, `SHUTDOWN` | the usual           |

**Up / Down arrows** bring back earlier commands, as in a normal terminal.

Anything that isn't a real SWF file is refused with **ACCESS DENIED**. FlashOS
checks the file extension and also the file's header (`FWS`/`CWS`/`ZWS`).

While a movie plays, **F12** or **Ctrl+Q** returns to the prompt, and
**F9** mutes, **F10** turns the volume down and **F11** turns it up (with a
pixel volume bar on screen). Esc also works
for plain animations; in ActionScript games Esc goes to the game.

## Games on USB sticks and extra disks

While booting, FlashOS checks every USB stick and extra disk the PC's BIOS can
see. On each FAT16 or FAT32 drive it loads the `.swf` files in the top folder
and in folders one level down, and lists them in `LS` with a **USB** tag.
Long file names work, and the hidden `._` files macOS creates are ignored.

* **Real PC:** format the stick as FAT32 (macOS Disk Utility: "MS-DOS (FAT)"),
  copy your games onto it, plug it in, then boot FlashOS.
* **VirtualBox:** VirtualBox's BIOS can't see USB sticks, so use a small
  virtual disk instead. On a Mac:
  `tools/make-games-disk-mac.sh ~/Desktop/MyGames` makes `FlashGames.vdi`.
  Add it in *Settings → Storage → IDE controller → Add hard disk*.

Sticks plugged in after start-up are not seen. Files are read-only, and they
have to fit in RAM.

## Game CDs / ISO images

Put SWF files in a plain data ISO and insert it as a second CD: FlashOS reads
every CD drive at start-up and lists the games with a **CD** tag (folders one
level down and long names included). After swapping the disc, type `RESCAN`.

On a Mac: `hdiutil makehybrid -iso -joliet -o MyGames.iso ~/Desktop/MyGames`.
In VirtualBox, put that ISO on the **IDE controller** (FlashOS reads CD drives
through IDE).

## Installing on the hard disk

Boot from the ISO once and type `INSTALL`. FlashOS lists the hard disks, asks
you to type `YES` (the disk is erased), copies itself, and checks the copy.
Then remove the CD / ISO and reboot: FlashOS now starts from the disk.
On an installed system the `INSTALL` command no longer exists.

INSTALL uses the IDE interface. VirtualBox's default IDE controller and QEMU
work. On real PCs, set the SATA mode to *IDE / Legacy / Compatibility* in the
BIOS setup.

## Internet (wired only)

`NET` connects with a network cable (DHCP) and shows the address. `PING` tests
the connection. `GET http://site/game.swf` downloads a SWF into memory; it shows
up in `LS` with a **NET** tag and plays like any other file. Downloads last
until you reboot.

* Network cards: **Intel PRO/1000** family (VirtualBox's and QEMU's default
  adapter, some Intel PCs) and **Realtek RTL8139**. Other cards (for example
  the common Realtek RTL8111 or Intel I219) aren't supported yet.
* **Only `http://`**: HTTPS needs encryption FlashOS doesn't have, so
  HTTPS-only sites (including GitHub) can't be used. Wi-Fi isn't supported.

## The built-in Flash player

FlashKernel has its own SWF player:

* vector shapes with straight and curved edges, anti-aliased
* solid, linear-gradient, radial-gradient and bitmap fills, transparency, outlines
* the timeline, sprites (movie clips) with their own timelines, colour effects
* lossless bitmaps, and compressed `CWS` files (FlashKernel's own zlib decoder)
* the movie is scaled to fit the screen
* images: JPEG (baseline), PNG and GIF, with transparency, and lossless bitmaps
* masks (nested too)
* text: static text drawn from the movie's own fonts, and dynamic and input
  text fields drawn in the PC's pixel font. Click a text box to type into it.
* **sound** through an Intel AC'97 sound card: MP3, ADPCM and uncompressed
  sound, event sounds, music streamed with the animation, button sounds and the
  `Sound` object (`attachSound`, `start`, `stop`, `setVolume`)

In VirtualBox turn sound on in *Settings → Audio* and pick **ICH AC97** as the
controller.

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

Not supported yet: ActionScript 3 (Flash 9+), progressive JPEGs, filters
(blur, glow…), morph shapes, Nellymoser/Speex sound, Intel HD Audio sound cards,
and LZMA-compressed `ZWS` files.

## Flash Notepad

Flash Notepad is an app for FlashOS, made as a SWF in `apps/notepad/`. CI
publishes it on the Releases page as **`Flash-notepad.iso`**, a data CD
labelled *Flash notepad* with `FlashNotepad.swf` and a sample text, picture
and video. Insert it as a second CD and type `flashnotepad`.

* **Text**: a multi-line editor with word wrap, a scroll bar, selection
  with the mouse or Shift+arrows, cut/copy/paste, undo, find, and time and date.
  It opens and saves `.txt` files.
* **Draw**: pencil, eraser, fill bucket, line, rectangle and circle, with 4
  sizes and 16 colours. Drawings are saved as `.bmp` pictures.
* **Pictures**: view `.jpg`, `.png`, `.gif` and `.bmp` files.
* **Video**: play `.flv` videos with the Sorenson Spark codec and MP3, PCM or
  ADPCM sound. To make one:
  `ffmpeg -i in.mp4 -c:v flv -q:v 5 -c:a libmp3lame -ar 44100 out.flv`
* **Calculator**.

The UI uses Comic Neue (SIL Open Font License, see
`apps/notepad/ComicNeue-OFL.txt`), a free font made to look like Comic Sans.
It has cyan borders, and the File, Edit and Help menus sit in the top border.

Files are saved in a documents area on the hard disk where FlashOS is
installed (from 16 MB in; reinstalling keeps it). When FlashOS runs from a CD,
saved files stay in memory until restart. CDs and USB sticks are read-only;
FlashOS now also loads `.txt .jpg .jpeg .png .gif .bmp .flv` files from them.

### Writing your own apps

`tools/asc.py` compiles ActionScript 1.0 into a SWF. It can embed a TrueType
font for text fields:

```sh
python3 tools/asc.py app.as app.swf --size 1024x768 --fps 30 --font my.ttf "My Font"
```

Besides the standard AS1/AS2 classes, FlashOS apps can use:

| API | |
|---|---|
| `FlashOS.files(ext)` | list files, e.g. `FlashOS.files("jpg,png")`: `[{name, size, where}]` with `where` = `disk`, `memory`, `cd`, `usb`, `net` or `system` |
| `FlashOS.read(name)` / `FlashOS.write(name, text)` | text files (UTF-8 on disk) |
| `FlashOS.remove(name)`, `FlashOS.exists(name)` | |
| `FlashOS.storage()`, `FlashOS.freeKB()` | `"disk"` or `"memory"`, free space |
| `FlashOS.loadBitmap(name)` / `FlashOS.saveBitmap(name, bmp)` | pictures as `BitmapData` |
| `FlashOS.openVideo(name)` | `.flv` video: `play() pause() stop() seek(s) close()`, `time duration playing width height bitmap` |
| `FlashOS.getClipboard()`, `System.setClipboard(s)` | clipboard |
| `FlashOS.exit()` | back to the prompt |
| `flash.display.BitmapData` | `fillRect setPixel(32) getPixel(32) floodFill copyPixels clone dispose`, plus `drawLine(x0, y0, x1, y1, colour, size)` |
| `clip.attachBitmap(bmp, depth, snap, smooth)` | show a BitmapData |
| text fields | `multiline wordWrap embedFonts border background scroll maxscroll`, `TextFormat` (font, size, colour, align), `replaceSel`, `Selection.setSelection`, `copy() cut() paste() undo() selectAll()` |

## Build

You need `nasm`, `gcc` with 32-bit support, `make` and `python3`:

```sh
sudo apt install nasm gcc-multilib make python3 qemu-system-x86
make            # -> build/FlashOS.iso  (about 300 KB)
make apps       # -> build/apps/FlashNotepad.swf (needs python3-fonttools)
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
boot/stage2.c       real-mode C (gcc -m16): finds .swf files on USB sticks /
                    disks through the BIOS (FAT16/FAT32 reader)
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
kernel/ata.c        IDE disk driver (INSTALL)
kernel/net.c        network drivers and DHCP / DNS / TCP / HTTP
kernel/cdfs.c       game CDs: ISO 9660 / Joliet reader
kernel/audio.c      AC'97 driver, mixer, ADPCM; MP3 through minimp3.h
kernel/image.c      JPEG, PNG and GIF decoders
tools/mkiso.py      writes the ISO 9660 + El Torito + MBR hybrid image
tools/mkfs.py       packs files into FlashFS
tools/make_demo_swf.py  builds the demo movies (it includes a small AVM1
                    assembler for catch.swf, an ActionScript game)
host/sim.c          runs the real kernel code on your PC and saves screenshots
                    (`make sim`): handy for testing without an emulator
```

Copyright (c) 2026 FlashOS. All rights reserved.
