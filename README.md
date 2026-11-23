# FlashOS

A tiny Linux distribution that boots straight into a red retro terminal and
**can only run `.swf` (Flash) files**.

```
 ___ _         _    ___  ___
| __| |__ _ __| |_ / _ \/ __|
| _|| / _` (_-< ' \ (_) \__ \
|_| |_\__,_/__/_||_\___/|___/
Welcome to FlashOS,Insert An Command,


 > _
```

## How it works

| Layer        | What                                                             |
|--------------|------------------------------------------------------------------|
| Kernel       | Mainline Linux, no Tux logo, big Terminus 16x32 console font     |
| Userland     | BusyBox (init, mount, sh for scripts only); built with Buildroot |
| Shell        | `flashsh`: the only program on tty1; it shows the boot animation and the prompt |
| Player       | [Ruffle](https://ruffle.rs) (Flash Player emulator) on a bare Xorg + Mesa |
| Window mgr   | `flashwm`: forces the SWF fullscreen; **F12 / Ctrl+Q** quits back to the prompt |

**Boot animation:** the logo loads like an old PC drawing an image: each
scanline fills in left-to-right behind a solid beam, top to bottom, while a
`LOADING FLASHOS.IMG [####....]` bar counts up. The screen flickers like a CRT
settling, then the welcome line types itself out.

**SWF only:** `flashsh` refuses anything that isn't a `.swf` file whose header
is really `FWS`/`CWS`/`ZWS`. There is no general shell. `flash-run` checks the
extension again before starting Ruffle.

## Commands

| Command            | Action                                       |
|--------------------|----------------------------------------------|
| `LS`               | list SWF files (`/swf` and USB drives)       |
| `RUN <name or #>`  | play a SWF (or just type `game.swf`)         |
| `USB`              | rescan USB sticks                            |
| `CLS`, `ABOUT`, `HELP`, `REBOOT`, `SHUTDOWN` | the usual          |

## Build

On a Debian/Ubuntu machine (around 10 GB free disk; the first build takes 30–90 min):

```sh
sudo apt install build-essential git curl wget cpio unzip rsync bc file \
                 python3 libncurses-dev libelf-dev syslinux-utils qemu-system-x86
./build.sh
./run-qemu.sh                 # boot it
./run-qemu.sh ~/my-swf-files  # boot with a folder as a USB stick
```

Output: `output/images/rootfs.iso9660`, a hybrid ISO. Burn it to a CD or
`dd` it to a USB stick to boot real PCs (BIOS boot).

Add your own games: drop `.swf` files into `board/flashos/swf/` before building
(they're baked into `/swf`), or put them on any FAT/exFAT USB stick.

## Layout

```
build.sh, run-qemu.sh            build + test helpers
configs/flashos_defconfig        Buildroot config (x86_64, glibc, Xorg, Mesa, ALSA)
board/flashos/linux.fragment     kernel tweaks (no logo, big font, fb, sound, USB)
board/flashos/isolinux.cfg       quiet boot, straight into FlashOS
board/flashos/rootfs-overlay/    inittab, flash-run, flash-mount, xorg.conf
board/flashos/post-build.sh      installs Ruffle + SWFs into the image
package/flashos-tools/           Buildroot package for flashsh + flashwm
src/flashsh/                     flashsh.c (shell + animation), flashwm.c
tools/make_demo_swf.py           generates the bundled bounce.swf demo
```

## Tuning

* `/etc/flashos.conf`: Ruffle options (default `--fullscreen --graphics gl`).
* Graphics use Mesa's software renderer so it boots anywhere. For speed on real
  hardware, enable `BR2_PACKAGE_MESA3D_LLVM` (llvmpipe) or a GPU driver
  (`BR2_PACKAGE_MESA3D_GALLIUM_DRIVER_IRIS`, `..._RADEONSI`) and switch
  `xorg.conf` to the `modesetting` driver.
* Debug shell: uncomment the `tty3` line in `rootfs-overlay/etc/inittab`, then
  press Alt+F3.
* Try just the shell on any Linux: `cd src/flashsh && make flashsh && ./flashsh`
  (set `FLASHOS_SWF_DIRS=/path/to/swfs`).
