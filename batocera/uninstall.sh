#!/bin/bash
# Remove Lunaria from Batocera or HippOS.  Games in /userdata/roms/android and
# app data in /userdata/saves/android are kept; delete them yourself if you
# want them gone.
#
#   bash /userdata/system/lunaria/uninstall.sh

LUNARIA_HOME=/userdata/system/lunaria
SERVICE=/userdata/system/services/lunaria

if [ -f "$SERVICE" ]; then
    bash "$SERVICE" stop
    if command -v batocera-services >/dev/null 2>&1; then
        batocera-services disable lunaria >/dev/null 2>&1
    elif command -v hippos-services >/dev/null 2>&1; then
        hippos-services disable lunaria >/dev/null 2>&1
    fi
    rm -f "$SERVICE"
fi
case "$LUNARIA_HOME" in
    /userdata/system/lunaria) rm -rf -- "$LUNARIA_HOME" ;;
esac
rm -rf -- /userdata/system/cache/lunaria
echo "Lunaria removed.  Kept: /userdata/roms/android and /userdata/saves/android."
echo "Restart EmulationStation to drop the Android system from the list."
