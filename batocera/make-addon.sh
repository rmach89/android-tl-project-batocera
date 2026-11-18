#!/bin/sh
# Package Lunaria builds as the Batocera / HippOS add-on.
#
#   sh batocera/make-addon.sh <out-dir> lunaria-<date>-linux-<arch>.tar.gz...
#
# Makes <out-dir>/lunaria-batocera-<arch>.zip for each build: the build, the
# installer, the service and the launcher, in one folder named
# lunaria-batocera/.  On the console: unzip it in /userdata/system and run
#   bash /userdata/system/lunaria-batocera/install.sh
set -eu

here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/.." && pwd)
[ $# -ge 2 ] || { echo "usage: $0 <out-dir> <lunaria-*-linux-ARCH.tar.gz>..." >&2; exit 1; }
out=$1
shift
mkdir -p "$out"
out=$(cd "$out" && pwd)

for build in "$@"; do
    [ -f "$build" ] || { echo "no such file: $build" >&2; exit 1; }
    case "$build" in
        *-linux-x86_64.tar.gz) arch=x86_64 ;;
        *-linux-aarch64.tar.gz) arch=aarch64 ;;
        *) echo "not a Linux x86_64/aarch64 build: $build" >&2; exit 1 ;;
    esac
    stage=$(mktemp -d)
    dir="$stage/lunaria-batocera"
    mkdir -p "$dir"
    for f in install.sh uninstall.sh service lunaria-launch.sh es_systems_android.cfg \
             hotkeys-lunaria.json lunaria.desktop README.md; do
        cp "$here/$f" "$dir/"
    done
    cp "$repo/scripts/install-framework.py" "$dir/"
    cp "$build" "$dir/"
    chmod 0755 "$dir/install.sh" "$dir/uninstall.sh" "$dir/service" "$dir/lunaria-launch.sh"
    zip="$out/lunaria-batocera-$arch.zip"
    rm -f "$zip"
    (cd "$stage" && python3 - "$zip" <<'PY'
import os, sys, zipfile
out = sys.argv[1]
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    for root, dirs, files in os.walk("lunaria-batocera"):
        dirs.sort()
        for name in sorted(files):
            path = os.path.join(root, name)
            info = zipfile.ZipInfo.from_file(path)
            info.compress_type = zipfile.ZIP_DEFLATED
            # keep the executable bits for unzip on the console
            info.external_attr = (os.stat(path).st_mode & 0xFFFF) << 16
            with open(path, "rb") as f:
                z.writestr(info, f.read())
PY
    )
    rm -r "$stage"
    echo "$zip"
done
