# Lunaria on Batocera and HippOS

This add-on adds an **Android (Lunaria)** system to EmulationStation on
[Batocera](https://batocera.org) and [HippOS](https://github.com/hippos-linux/hippos-linux).
Put `.apk`, `.xapk`, `.apks` or `.apkm` files in `/userdata/roms/android` and
start them like any other game.

- **Controllers** reach the game as a real Android controller (buttons, sticks,
  triggers, d-pad). Games with built-in controller support use them as on a
  phone. The pads you configured in the front end are recognised through
  their SDL mappings.
- **Leaving a game:** Hotkey (Select) + Start, like every other system.
- **Touchscreen** devices (handhelds, touch monitors) work as multi-touch.
- **App data** is kept in `/userdata/saves/android` and survives updates.

## Install

1. Get the add-on for your machine: `lunaria-batocera-x86_64.zip` for a PC,
   `lunaria-batocera-aarch64.zip` for an ARM board or handheld. To build it
   yourself, see [Building the add-on](#building-the-add-on).
2. Copy the zip to the `system` folder of the console's network share
   (`\\BATOCERA\share\system` on Windows, `smb://batocera/share/system` on a
   Mac or Linux), or to `/userdata/system` with a USB stick.
3. On the console, open a terminal (on Batocera, SSH in as `root`, password
   `linux`, or use the terminal from the F1 applications menu) and run:

   ```bash
   cd /userdata/system
   unzip -o lunaria-batocera-x86_64.zip
   bash lunaria-batocera/install.sh
   ```

   On HippOS, run it as your normal user. Use `sudo` only if you want the
   `/usr/bin/lunaria` shortcut as well.

4. Restart EmulationStation (Main menu > Quit > Restart EmulationStation).
   **Android (Lunaria)** appears once there is at least one game in
   `/userdata/roms/android`.

The installer:

- puts the build in `/userdata/system/lunaria/app`,
- uses the system's own graphics, audio and C++ libraries where it has them,
- downloads Google's Android framework resources (about 60 MB, once). If
  there is no network, games still run; run the installer again later to
  get them.
- installs and enables the **lunaria** service.

To update, run `install.sh` again with the new zip. Your settings and app
data are kept, and the previous build stays in `app.old` until the next
update.

## The lunaria service

Everything lives in `/userdata/system/lunaria`. The service script,
`/userdata/system/services/lunaria`, links it into the places the system
looks:

| Link | Points to | What it does |
|---|---|---|
| `/userdata/system/configs/emulationstation/es_systems_android.cfg` | `lunaria/es_systems_android.cfg` | The Android system in the game list |
| `/usr/bin/lunaria`, `/usr/bin/lunaria-launch` | `lunaria/app/lunaria`, `lunaria/lunaria-launch.sh` | Run it from a terminal |
| `/usr/share/applications/lunaria.desktop` | `lunaria/lunaria.desktop` | Desktop / applications menu entry |
| `/usr/share/hippos/hotkeys/contexts/lunaria.json` | `lunaria/hotkeys-lunaria.json` | HippOS hotkeys while a game runs |

`start` creates the links, and `stop` removes them. It only removes links
that point into `/userdata/system/lunaria`, so files of your own are never
deleted. Links under `/usr` are lost at every reboot on Batocera, so the
service recreates them each time it starts. On a system where `/usr` is
read-only, those links are skipped; the game list does not need them.

Switch the service on or off in **Main menu > System settings > Services**,
or from a terminal:

```bash
batocera-services start lunaria     # or: hippos-services start lunaria
batocera-services stop lunaria
batocera-services status lunaria
```

Turning it off hides the Android system. Games, saves and the build stay
where they are.

## Settings

`/userdata/system/lunaria/lunaria-batocera.conf` (shell syntax):

| Setting | Default | |
|---|---|---|
| `LUNARIA_EXIT_COMBO` | `BACK+START` | Buttons that close the game, pressed together. Empty turns it off. |
| `LUNARIA_KEYMAP` | `off` | `genshin`, `crossworlds` or a keymap file: the controller drives touches instead of being a real controller. |
| `LUNARIA_FULLSCREEN` | `1` | `0` for a window. |
| `LUNARIA_DATA_ROOT` | `/userdata/saves/android` | App data. |

To use mappings for an unusual pad, put an SDL `gamecontrollerdb.txt` in
`/userdata/system/lunaria/`.

## When something does not work

The log of the last game is `/userdata/system/logs/lunaria.log`. Include it,
the game's name and version, and your machine when you report a problem.

- **The system does not show up.** There must be at least one game in
  `/userdata/roms/android`. Run `batocera-services status lunaria`; it
  should say "started". Then restart EmulationStation.
- **Black screen or no sound.** The installer set aside some bundled
  libraries in `app/lib-unused` in favour of the system's own. Moving one
  back into `app/lib` tests whether it was the cause.
- **The controller does nothing.** Make sure no keymap is set (`LUNARIA_KEYMAP=off`).
  The log shows `[input] gamepad: <name>` when the pad is found.

To remove Lunaria: `bash /userdata/system/lunaria/uninstall.sh`. This keeps
your games and app data.

## Building the add-on

The console runs a build made on another Linux machine:

```bash
make dynarmic-build
make -j"$(nproc)" lunaria
make syslib || true
make dist                                  # lunaria-<date>-linux-<arch>.tar.gz
sh batocera/make-addon.sh out lunaria-*-linux-*.tar.gz
```

Build on Ubuntu 22.04 or another system with glibc 2.35 or older; see the main
README for the build dependencies. On GitHub, the **batocera** workflow
(Actions > batocera > Run workflow) builds both zips for x86_64 and aarch64.
