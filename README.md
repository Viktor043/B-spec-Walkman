# SNRGY Walkman

Custom firmware for the Sony Walkman **NW-A820** (NW-A829/A828) and **NW-A910** (NW-A919) series: a music,
audiobook, book and game player with the SNRGY look (silver and black themes), running alongside Sony's own UI.

Based on [nw-a820-homebrew](https://github.com/manuaguirre9/nw-a820-homebrew) by Manuel Aguirre (GPL-2.0), which
did the original firmware hook, player, DSP and emulator work. This repository continues from it as its own project.

> **Status: experimental.** In daily use on an NW-A829, an NW-A828 and two NW-A919s.

## What it does

- **Drag-and-drop music**: MP3, FLAC (up to 24-bit, resampled to 44.1 kHz), AAC/M4A and non-DRM ATRAC straight from
  the drive. Gapless MP3, ReplayGain, cover art, lyrics (`.lrc`), equalizers and Sony's DSP effects.
- **Audiobooks & Pods**: one folder per book or show in `Audiobooks` (or `Podcasts`); resume per file, speed
  0.75–2× with pitch kept, silence trimming, voice boost, chapters, sleep timer.
- **Books**: EPUB and TXT reader, four text sizes, portrait or landscape.
- **Games**: Game Boy / Color (gnuboy), NES (agnes), GBA (gpSP), Doom (doomgeneric).
- **Tools**: Focus timer with background noise, alarm (through the headphones: the Walkmans have no speaker),
  calculator.
- **SNRGY interface**: status strip with an always-on mode, contextual OPTION menus, an (i) explanation for every
  setting, OPTION as the power key. English only.
- **NW-A919 notification light**: breathes while charging, steady when charged, blinks for the alarm, the end of a
  Focus session and low battery.
- **Bluetooth audio** (A2DP) on the A82x.
- **Dual boot**: Sony's UI is untouched; Settings > Switch to Sony's Walkman, and HOME + VOL- for 2 s in Sony's UI to
  come back. If the player ever fails to start, the next boot goes to Sony's UI by itself.
- **USB like Sony's UI**: plug in while the player runs and the Walkman shows up as a drive; eject, unplug, and the
  player continues where it was.

See [LEARNINGS.md](LEARNINGS.md) for what was found along the way and [FUTURE.md](FUTURE.md) for ideas.

## How it works

The Walkman runs Linux 2.6.10 on an ARM926 (MP201 SoC). The official 1.02 firmware is modified in exactly three places
(see [`firmware/build_hb.py`](firmware/build_hb.py)):

1. **A hook in `/etc/init.d/gemini`**: before Sony's UI starts, if `H:\HB\boot.sh` exists, run it. Without that
   file the Walkman boots exactly like stock.
2. **The version string**, which becomes `1.02-41` so you can tell the firmware apart.
3. **A one-instruction patch in Sony's UI** that hides the "Simple Mode" popup at startup (Japanese model only).

Everything else (the player, emulators, Bluetooth, networking) is ordinary programs in `H:\HB\bin`, built with
`zig cc` against the device's glibc. Read [`docs/`](docs/) and [`research/dsp/FINDINGS.md`](research/dsp/FINDINGS.md)
for the reverse-engineering notes.

**No Sony files are in this repository.** The firmware tools start from the official updater, which you download from
Sony, and patch it on your own computer.

## Installing

> ⚠️ This flashes a modified firmware. It has worked repeatedly on the developer's unit and Sony's official firmware
> can be put back at any time, but you do this at your own risk. Charge the battery first and don't unplug the
> Walkman while it updates.

On a Mac, use `firmware/install_mac.py` (A82x) or `firmware/install_mac_a910.py` (A91x): see
[macOS and the NW-A91x](#macos-and-the-nw-a91x-nw-a916a918a919). On Windows:

1. Build **`Walkman-Homebrew-Installer.exe`** with `python firmware\build_installer.py` (or use the original
   project's [releases](https://github.com/manuaguirre9/nw-a820-homebrew/releases), which predate the SNRGY work).
2. Open it. Windows will probably show a blue **"Windows protected your PC"** box, because the program isn't signed
   with a paid certificate: click **More info**, then **Run anyway**.
3. Connect the Walkman with its USB cable, in Sony's normal mode. The window finds it by itself and shows the model
   and firmware.
4. Tick the battery box and press the button. If your Walkman has an older firmware than Sony's last one (1.02), the
   button first says **Step 1: update to 1.02**; after that it says **Install**. About two minutes each.
5. When it says *Done*, unplug the Walkman. Hold **HOME for 3 seconds** to open the player (it asks for the
   language the first time); HOME for 3 seconds again goes back to Sony's Walkman.

The installer downloads Sony's official updater from Sony, takes the firmware out of it and patches it on your
computer; every step is checked against known MD5s (the patched firmware is byte-for-byte the one tested on the
developer's unit), so a changed or damaged download stops it before anything is flashed. It also builds the patched
Bluetooth helper from that firmware and fetches `cacert.pem` for HTTPS. Run a newer installer later to update the
homebrew (it says when your Walkman is already up to date), or any installer to uninstall it.

<details><summary>From the command line (developers)</summary>

Windows, Python 3 with `cryptography`, after `sh build_all.sh`:

```bat
python firmware\install.py --check      :: finds the Walkman, downloads Sony's updater, builds and checks everything
python firmware\install.py --firmware   :: an older firmware: Sony's official 1.02 first
python firmware\install.py              :: copies H:\HB, flashes the hook firmware if needed (about a minute)
python firmware\build_installer.py      :: builds dist\Walkman-Homebrew-Installer.exe (pip install pyinstaller)
```

By hand: `firmware/upgpack.py` unpacks the official `NW_WM_FW.UPG`, `firmware/build_hb.py` builds the hook firmware,
`firmware/fwtrig.py go <md5>` makes the Walkman flash `H:\NW_WM_FW.UPG`, and `build/HB` is copied to `H:\HB`.
</details>

### Uninstalling

- **Quick**: delete or rename `H:\HB\boot.sh`. The Walkman boots stock.
- **Complete**: open the installer and press **Uninstall…** (or `python firmware\install.py --uninstall`). It flashes
  Sony's official firmware back; then delete `H:\HB`.

### Where things go on the Walkman

| Folder | Content |
|---|---|
| `H:\Music` | Music (any folder structure) |
| `H:\Audiobooks` | Audiobooks and podcasts, one folder per book or show |
| `H:\Podcasts` | Optional: more podcast folders, listed under Audiobooks & Pods too |
| `H:\Books` | EPUB and TXT books |
| `H:\Games` | Game Boy games; `Games\GBA`, `Games\NES`, `Games\Doom` (`.wad`) for the others |
| `H:\HB` | The homebrew itself; `cacert.pem` here enables certificate checks for HTTPS |

> Without `H:\HB\cacert.pem`, podcast downloads use HTTPS **without** verifying certificates.

## Building

You need [Zig 0.15.2](https://ziglang.org/download/) (set `ZIG` to its path if it isn't on `PATH`), a POSIX shell
(Git Bash works) and Python 3.

```sh
ZIG=/path/to/zig sh build_all.sh     # builds everything and stages build/HB, ready to copy to H:\HB
```

Each program also has its own `build*.sh` next to its sources. The smaller programs under `homebrew/` (`hello`,
`keytest`, `keymap`, `lcdtest`, `meter`, `mp3play`, `hid`, `net/pantest`) are the experiments that led to the
player; they are kept for reference.

The binaries target `arm-linux-gnueabi` (ARM926EJ-S, glibc 2.4 symbols) and are post-processed by
`homebrew/hello/fixpthread.py` because the device has LinuxThreads in libc.

## Testing

```sh
ZIG=/path/to/zig python homebrew/tests/run_tests.py [path/to/gemini/res]
```

On the PC, no Walkman needed: unit tests of the player's logic (podcast directory parser, file names, battery curve,
keyboard layout), every screen rendered with a check that no non-English text is left, and the device build. The screen renders
need Sony's resources (`/usr/local/share/gemini/res` from the firmware); without them that part is skipped.

On the Walkman (connected as `H:`):

```sh
sh homebrew/hbplayer/build_test.sh          # hbtest: the player plus scripted keys and screenshots
python homebrew/tests/device/stage.py H:     # stages a one-time test run for the next boot
# eject, restart the Walkman, wait ~6 minutes, connect it again
python homebrew/tests/device/check.py H:     # checks the logs, turns the screenshots into PNGs
```

The device run plays an AAC then an MP3 (the DSP's direct mode must survive the change), goes through a first start
in English with an empty state, and searches a podcast over the phone's Bluetooth tethering on a copy of your state.
Nothing it does touches your positions or settings.

## International models (NWZ-A82x)

The international NWZ-A826/A828/A829 use the same hardware and almost the same firmware (same kernel, near-identical
root filesystem; the differences are the UI binary, USB mode (MTP) and the key the update package is encrypted with).
**Status: the homebrew runs on a real NWZ-A829** (installed by hand by [@GustavoAco](https://github.com/GustavoAco),
see [#2](https://github.com/manuaguirre9/nw-a820-homebrew/issues/2)), but the installer does not support these models
yet, so please wait for a release that says so. What we know:

- the Walkman only shows up as an MTP media device (no drive letter), so `fwtrig.py` and the drive copy can't be used;
  the `HB` folder can be copied over MTP;
- Sony's international updater doesn't detect the Walkman on Windows 10/11 (it looks for the `WUDFRd` driver, the
  device uses `WUDFWpd`); it works on Windows XP, or with a small patch to a copy of `FWUpdaterCom.dll`;
- `python build_hb.py --variant intl` builds the hook from the official NWZ-A820 1.02 (international key, no Simple
  Mode patch); this exact build has not been flashed on a device yet.

If you have one, or another Walkman you'd like to see supported, and want to help: open the installer with the Walkman
connected and press **Save report** (it only reads information and changes nothing). It saves `Walkman-report.txt`
on your Desktop and offers to open the [report form](https://github.com/manuaguirre9/nw-a820-homebrew/issues/new?template=walkman-report.yml);
drag the file into it.

## macOS and the NW-A91x (NW-A916/A918/A919)

**macOS**: `firmware/install_mac.py` does what `install.py` does, with the same MD5 checks: the SCSI commands go over
USB Bulk-Only Transport with libusb (so it runs with `sudo`), and the CAB is extracted with `tar`. It needs a Python
venv with `pyusb`, `libusb-package` and `cryptography`.

**NW-A91x** (2008 Japanese models with 1seg). **Status: experimental, works on one NW-A919**: the hook firmware
flashed from a Mac, and the player starts and plays music. `firmware/install_mac_a910.py`:

- downloads Sony's `NW-A910_V1_03.exe` and takes `NW_A91X.UPG` out of it (the A91x's updater looks for that name,
  not `NW_WM_FW.UPG`);
- decrypts it with key `b8442b44`, signature `5dfeff3c` (not in Rockbox's database; found with a known-plaintext
  search over the 9^8 DES-equivalent hex keys, about 2 minutes on 8 cores);
- builds the same hook as `build_hb.py` on the 1.03 rootfs (the device reports **1.03-41**) and checks that only
  `/etc/init.d/gemini` and `/etc/firmversion` change;
- stage 1 installs a diagnostic `boot.sh` (hardware info to `HB/out/diag.txt`), `--full` the whole homebrew.

Same MP201 SoC, kernel 2.6.10 and libc as the A82x, so the A82x binaries run as they are. The A91x has **no
Bluetooth** (the Bluetooth features and podcast downloads on the device don't work; `podsync` from the computer does)
and different DSP and kernel module builds. On the tested NW-A919 the DSP's direct MP3 mode does not start
(`dsp: helper did not start`), so the player falls back to decoding on the ARM: music plays, at some battery cost.
USB hand-over in the player works. The battery percentage uses the A82x curve and may be off.

```sh
sudo VENV/bin/python firmware/install_mac_a910.py --check      # read-only: model, firmware, MD5s
sudo VENV/bin/python firmware/install_mac_a910.py --firmware   # older than 1.03: Sony's official 1.03 first
sudo VENV/bin/python firmware/install_mac_a910.py              # hook firmware + diagnostics
sudo VENV/bin/python firmware/install_mac_a910.py --full       # the homebrew
sudo VENV/bin/python firmware/install_mac_a910.py --uninstall  # Sony's official 1.03 back
```

## Repository layout

| Path | Content |
|---|---|
| `firmware/` | UPG pack/unpack, hook firmware builder, SCSI update trigger and read-only probes |
| `homebrew/` | Player (`hbplayer`), boot scripts, emulator frontends, Bluetooth/network stack, small experiments |
| `homebrew/third_party/` | Upstream libraries (see [THIRD_PARTY.md](THIRD_PARTY.md)) |
| `research/` | DSP and UI reverse-engineering notes and tools |
| `tools/podsync/` | Podcast sync from the PC |
| `docs/` | Firmware notes and the project log (from the original project) |

## Credits

- **Manuel Aguirre** ([nw-a820-homebrew](https://github.com/manuaguirre9/nw-a820-homebrew)): the original project
  this one grew from: firmware hook, player, Sony DSP, Bluetooth, emulators and the Windows installer.
- [@GustavoAco](https://github.com/GustavoAco): first to run the homebrew on an international NWZ-A829, found the
  Sony updater's driver check and its patch, the international home icons and the wallpaper setting.
- [@keeper46](https://github.com/keeper46): device reports and testing on an NWZ-A829 (US/Canada).
- Everyone who sent a report from the installer.

## License

GPL-2.0-or-later (see [LICENSE](LICENSE)); third-party code keeps its own licence ([THIRD_PARTY.md](THIRD_PARTY.md)).
The `hbplayer` binary also contains sonic (Apache-2.0), so a built `hbplayer` as a whole is distributed under
GPL-3.0; gnuboy, gpSP and Doom are GPL-2.0.
Sony, Walkman, ATRAC and SonicStage are trademarks of Sony. This project is not affiliated with Sony.
