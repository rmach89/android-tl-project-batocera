#!/bin/bash
# Lunaria launcher for Batocera and HippOS: EmulationStation's Android system
# runs this
# with the game's .apk/.xapk/.apks/.apkm as its only argument.
#
# Settings live in /userdata/system/lunaria/lunaria-batocera.conf (shell
# syntax); anything set there, or already in the environment, wins over the
# defaults below.  The log of the last run is /userdata/system/logs/lunaria.log.

LUNARIA_HOME=/userdata/system/lunaria
CONF="$LUNARIA_HOME/lunaria-batocera.conf"
LOG=/userdata/system/logs/lunaria.log

rom="$1"
mkdir -p "$(dirname "$LOG")" 2>/dev/null
# A log folder this user cannot write (made by root): log to /tmp instead.
if ! { : >>"$LOG"; } 2>/dev/null; then
    LOG="/tmp/lunaria-$(id -un).log"
fi
exec >"$LOG" 2>&1
echo "[lunaria-launch] $(date '+%F %T') launching: $rom"

if [ -z "$rom" ] || [ ! -e "$rom" ]; then
    echo "[lunaria-launch] no such game: $rom"
    exit 1
fi
if [ ! -x "$LUNARIA_HOME/app/lunaria" ]; then
    echo "[lunaria-launch] Lunaria is not installed in $LUNARIA_HOME/app; run install.sh"
    exit 1
fi

# shellcheck source=/dev/null
[ -f "$CONF" ] && . "$CONF"

# Where Android apps keep their data, the unpacked-package cache, and
# Lunaria's own settings file.
export LUNARIA_DATA_ROOT="${LUNARIA_DATA_ROOT:-/userdata/saves/android}"
export LUNARIA_CACHE_DIR="${LUNARIA_CACHE_DIR:-/userdata/system/cache/lunaria}"
export LUNARIA_CONF="${LUNARIA_CONF:-$LUNARIA_HOME/lunaria.conf}"
mkdir -p "$LUNARIA_DATA_ROOT" "$LUNARIA_CACHE_DIR"

# A console: fill the screen, and leave with Hotkey+Start like every other
# Batocera system (set LUNARIA_EXIT_COMBO= in the conf file to turn it off).
export LUNARIA_FULLSCREEN="${LUNARIA_FULLSCREEN:-1}"
export LUNARIA_EXIT_COMBO="${LUNARIA_EXIT_COMBO-BACK+START}"

# Controllers are passed to the game as a real Android controller unless a
# keymap is chosen (LUNARIA_KEYMAP=genshin, crossworlds, or a file path).
export LUNARIA_KEYMAP="${LUNARIA_KEYMAP:-off}"

# Controller mappings for pads GLFW's own list does not know: SDL's
# gamecontrollerdb format, from the first of these that exists.
if [ -z "$SDL_GAMECONTROLLERCONFIG_FILE" ]; then
    for db in "$LUNARIA_HOME/gamecontrollerdb.txt" \
              /userdata/system/configs/sdl2/gamecontrollerdb.txt \
              /tmp/gamecontrollerdb.txt \
              /usr/share/sdl3/gamecontrollerdb.txt \
              /usr/share/sdl2/gamecontrollerdb.txt \
              /usr/share/gamecontrollerdb/gamecontrollerdb.txt; do
        if [ -f "$db" ]; then
            export SDL_GAMECONTROLLERCONFIG_FILE="$db"
            break
        fi
    done
fi

# The bundled GLFW talks to X11.  On a Batocera build that runs Wayland with
# no X11 server, use the system's GLFW (built for Wayland) instead.
if [ -z "$DISPLAY" ] && [ -n "$WAYLAND_DISPLAY" ]; then
    for glfw in /usr/lib/libglfw.so.3 /usr/lib64/libglfw.so.3; do
        if [ -f "$glfw" ]; then
            export LD_PRELOAD="$glfw${LD_PRELOAD:+:$LD_PRELOAD}"
            break
        fi
    done
elif [ -z "$DISPLAY" ]; then
    export DISPLAY=:0.0
fi

# HippOS: its hotkey daemon acts per game; give it Lunaria's (Hotkey+Start
# ends the game) for this run and hand back the front end's afterwards.
HOTKEYS=/usr/lib/hippos/hippos-hotkeys
if [ -x "$HOTKEYS" ] && [ -f "$LUNARIA_HOME/hotkeys-lunaria.json" ]; then
    keys=$(python3 -c 'import json,sys; print(json.dumps(json.load(open(sys.argv[1]))["keys"]))' \
           "$LUNARIA_HOME/hotkeys-lunaria.json" 2>/dev/null)
    [ -n "$keys" ] && "$HOTKEYS" --new-context lunaria "$keys" 2>/dev/null
    trap '"$HOTKEYS" --default-context 2>/dev/null' EXIT
fi

echo "[lunaria-launch] DISPLAY=$DISPLAY WAYLAND_DISPLAY=$WAYLAND_DISPLAY keymap=$LUNARIA_KEYMAP" \
     "exit=$LUNARIA_EXIT_COMBO mappings=${SDL_GAMECONTROLLERCONFIG_FILE:-built-in}"

# Lunaria looks for its fonts and runtime next to itself and in the working
# directory, so start it from its own folder.
cd "$LUNARIA_HOME/app" || exit 1
./lunaria "$rom"
status=$?
echo "[lunaria-launch] lunaria exited with status $status"
exit $status
