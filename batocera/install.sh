#!/bin/bash
# Install or update Lunaria on Batocera or HippOS.
#
#   bash install.sh [lunaria-<date>-linux-<arch>.tar.gz] [--no-framework]
#
# Run it from the unpacked add-on folder (on Batocera as root over SSH or in
# a terminal; on HippOS as the normal user or with sudo).  With no archive
# named, it takes the newest lunaria-*-linux-<this machine>.tar.gz beside it.
#
# What it does:
#   /userdata/system/lunaria/app          the Lunaria build (replaced on update)
#   /userdata/system/lunaria/*            launcher, Android system, settings
#   /userdata/system/services/lunaria     the service that links it all in
#   /userdata/roms/android                where .apk/.xapk files go
#   /userdata/saves/android               app data (kept across updates)
# and then enables and starts the service.  Nothing outside /userdata is
# written except the links the service makes.

set -u

HERE=$(cd "$(dirname "$0")" && pwd)
LUNARIA_HOME=/userdata/system/lunaria
SERVICES=/userdata/system/services
FRAMEWORK_URL=https://dl.google.com/android/repository/platform-31_r01.zip
FRAMEWORK_SHA1=ca5bcaa565cb37e9d287051d6dd0e49a5426ec29

say() { echo "==> $*"; }
warn() { echo "warning: $*" >&2; }
die() { echo "error: $*" >&2; exit 1; }

archive=""
want_framework=1
for arg in "$@"; do
    case "$arg" in
        --no-framework) want_framework=0 ;;
        -h|--help) sed -n '2,20p' "$0"; exit 0 ;;
        *) archive="$arg" ;;
    esac
done

# ---- where are we -----------------------------------------------------------
[ -d /userdata ] || die "/userdata not found: this is not Batocera or HippOS"
if command -v batocera-services >/dev/null 2>&1; then
    distro=batocera
elif command -v hippos-services >/dev/null 2>&1 || [ -d /usr/lib/hippos ]; then
    distro=hippos
else
    distro=unknown
    warn "neither Batocera nor HippOS detected; installing anyway"
fi
arch=$(uname -m)
case "$arch" in
    x86_64|aarch64) ;;
    *) die "Lunaria has no build for $arch (x86_64 and aarch64 only)" ;;
esac

# The user games run as: root on Batocera, the owner of the ROM folder on
# HippOS.  Folders made here must belong to them.
owner=root
[ -d /userdata/roms ] && owner=$(stat -c %U /userdata/roms 2>/dev/null || echo root)

# The build uses this system's C library; it was made on glibc 2.35.
glibc=$(getconf GNU_LIBC_VERSION 2>/dev/null | awk '{print $2}')
if [ -n "$glibc" ]; then
    major=${glibc%%.*}; minor=${glibc#*.}; minor=${minor%%.*}
    if [ "$major" -lt 2 ] || { [ "$major" -eq 2 ] && [ "$minor" -lt 35 ]; }; then
        die "glibc $glibc is too old (needs 2.35 or newer); update the system"
    fi
fi
say "$distro on $arch (glibc ${glibc:-unknown}), games run as $owner"

# ---- the build ----------------------------------------------------------------
if [ -z "$archive" ]; then
    archive=$(ls -t "$HERE"/lunaria-*-linux-"$arch".tar.gz 2>/dev/null | head -n 1)
fi
[ -n "$archive" ] && [ -f "$archive" ] ||
    die "no lunaria-*-linux-$arch.tar.gz found; name one: bash install.sh <archive>"
case "$archive" in
    *-linux-"$arch".tar.gz) ;;
    *) die "$(basename "$archive") is not a build for $arch" ;;
esac
say "installing $(basename "$archive")"

mkdir -p "$LUNARIA_HOME" || die "cannot create $LUNARIA_HOME"
work="$LUNARIA_HOME/.install"
rm -rf "$work"
mkdir -p "$work"
tar -xzf "$archive" -C "$work" || die "could not unpack $archive"
top=$(find "$work" -mindepth 1 -maxdepth 1 -type d | head -n 1)
[ -n "$top" ] && [ -x "$top/lunaria" ] || die "$(basename "$archive") holds no lunaria executable"

# Swap the new build in; the previous one stays as app.old until next time.
rm -rf "$LUNARIA_HOME/app.old"
[ -d "$LUNARIA_HOME/app" ] && mv "$LUNARIA_HOME/app" "$LUNARIA_HOME/app.old"
mv "$top" "$LUNARIA_HOME/app" || die "could not move the build into place"
rm -rf "$work"
app="$LUNARIA_HOME/app"

# ---- libraries ----------------------------------------------------------------
# The build carries the libraries of the machine it was made on.  This
# system's own copies match its GPU drivers, its audio server and the C++
# runtime its Mesa drivers were built with, and it is newer than that
# machine, so where it has a library (same soname, same ABI) its copy is
# used and the bundled one is set aside in app/lib-unused.  GLFW stays: the
# bundled build is the one Lunaria was tested with.
system_has() {
    if command -v ldconfig >/dev/null 2>&1 &&
       ldconfig -p 2>/dev/null | awk '{print $1}' | grep -qxF "$1"; then
        return 0
    fi
    for d in /usr/lib /lib /usr/lib64 /lib64 "/usr/lib/$arch-linux-gnu" "/lib/$arch-linux-gnu"; do
        [ -e "$d/$1" ] && return 0
    done
    return 1
}
if [ -d "$app/lib" ]; then
    mkdir -p "$app/lib-unused"
    moved=0
    for lib in "$app"/lib/*.so*; do
        [ -e "$lib" ] || continue
        name=$(basename "$lib")
        case "$name" in libglfw.so*) continue ;; esac
        if system_has "$name"; then
            mv "$lib" "$app/lib-unused/" && moved=$((moved + 1))
        fi
    done
    say "using the system's copy of $moved bundled libraries (set aside in app/lib-unused)"
    # Anything the system turns out not to resolve goes back.
    missing=$(cd "$app" && ldd ./lunaria 2>/dev/null | awk '/not found/{print $1}')
    for name in $missing; do
        if [ -e "$app/lib-unused/$name" ]; then
            mv "$app/lib-unused/$name" "$app/lib/" && say "restored bundled $name"
        else
            warn "$name is needed and found nowhere"
        fi
    done
fi

# ---- launcher, Android system, service --------------------------------------------
for f in lunaria-launch.sh es_systems_android.cfg hotkeys-lunaria.json lunaria.desktop \
         install-framework.py uninstall.sh; do
    [ -f "$HERE/$f" ] && cp -f "$HERE/$f" "$LUNARIA_HOME/$f"
done
chmod 0755 "$LUNARIA_HOME/lunaria-launch.sh" "$LUNARIA_HOME/uninstall.sh" 2>/dev/null
mkdir -p "$SERVICES"
cp -f "$HERE/service" "$SERVICES/lunaria" && chmod 0755 "$SERVICES/lunaria"

if [ ! -f "$LUNARIA_HOME/lunaria-batocera.conf" ]; then
    cat > "$LUNARIA_HOME/lunaria-batocera.conf" <<'EOF'
# Lunaria settings for the Android system (shell syntax; remove the # to use).
#
# Buttons that close the game, pressed together.  Names: A B X Y LB RB LT RT
# BACK (Select) START GUIDE (Home/Hotkey) L3 R3 UP DOWN LEFT RIGHT.
# Empty turns it off.
#LUNARIA_EXIT_COMBO="BACK+START"
#
# Controller as touches instead of a real Android controller:
# genshin, crossworlds, or a keymap file of your own.
#LUNARIA_KEYMAP=off
#
# Window instead of full screen.
#LUNARIA_FULLSCREEN=0
#
# Where app data lives.
#LUNARIA_DATA_ROOT=/userdata/saves/android
EOF
fi

made_logs=0
[ -d /userdata/system/logs ] || made_logs=1
mkdir -p /userdata/roms/android /userdata/saves/android /userdata/system/cache/lunaria \
         /userdata/system/logs
if [ ! -f /userdata/roms/android/_info.txt ]; then
    cat > /userdata/roms/android/_info.txt <<'EOF'
Android (Lunaria)

Put Android games and apps here: .apk, .xapk, .apks or .apkm files.
App data is kept in /userdata/saves/android.
Settings: /userdata/system/lunaria/lunaria-batocera.conf
Log of the last game: /userdata/system/logs/lunaria.log
Leave a game with Hotkey (Select) + Start.
EOF
fi

# ---- Android framework resources (themes for apps with Android UI) -------------
# Google's SDK Platform 31, unmodified.  Kept beside the build so updates do
# not download it again.
fw="$LUNARIA_HOME/framework-res"
if [ "$want_framework" = 1 ] && [ ! -f "$fw/resources.arsc" ]; then
    zip="$LUNARIA_HOME/platform-31_r01.zip"
    if [ ! -f "$zip" ]; then
        say "downloading Android framework resources (about 60 MB)"
        if command -v curl >/dev/null 2>&1; then
            curl -fL --retry 2 -o "$zip.part" "$FRAMEWORK_URL" && mv "$zip.part" "$zip"
        elif command -v wget >/dev/null 2>&1; then
            wget -O "$zip.part" "$FRAMEWORK_URL" && mv "$zip.part" "$zip"
        fi
    fi
    if [ -f "$zip" ] && command -v python3 >/dev/null 2>&1 &&
       python3 "$LUNARIA_HOME/install-framework.py" "$zip" "$fw" >/dev/null; then
        rm -f "$zip"
        say "framework resources installed"
    else
        rm -f "$zip.part"
        warn "framework resources not installed (no network or no python3);"
        warn "games work without them; apps with Android menus may look plain."
        warn "Run the installer again later to retry."
    fi
fi
if [ -f "$fw/resources.arsc" ]; then
    mkdir -p "$app/runtime"
    rm -rf "$app/runtime/framework-res"
    ln -sfn "$fw" "$app/runtime/framework-res" 2>/dev/null || cp -R "$fw" "$app/runtime/framework-res"
fi

# ---- ownership ----------------------------------------------------------------
if [ "$(id -u)" = 0 ] && [ "$owner" != root ]; then
    chown -R "$owner": "$LUNARIA_HOME" /userdata/roms/android /userdata/saves/android \
        /userdata/system/cache/lunaria 2>/dev/null
    chown "$owner": "$SERVICES" "$SERVICES/lunaria" 2>/dev/null
    [ "$made_logs" = 1 ] && chown "$owner": /userdata/system/logs 2>/dev/null
fi

# ---- switch it on -------------------------------------------------------------------
case "$distro" in
    batocera)
        batocera-services enable lunaria >/dev/null 2>&1
        batocera-services start lunaria ;;
    hippos)
        hippos-services enable lunaria >/dev/null 2>&1
        hippos-services start lunaria ;;
    *)
        bash "$SERVICES/lunaria" start ;;
esac

say "done.  Put .apk/.xapk files in /userdata/roms/android, then restart"
say "EmulationStation (Main menu > Quit > Restart EmulationStation) or update"
say "the game lists, and open the \"Android (Lunaria)\" system."
