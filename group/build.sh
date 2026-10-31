#!/usr/bin/env bash
# Builds and self-signs ntpsyncd.sisx with GnuPoc on Linux.
# Requires: EPOCROOT (with trailing slash), SDK tools on PATH, openssl.
set -euo pipefail
: "${EPOCROOT:?Set EPOCROOT to your GnuPoc S60 3rd FP2 SDK root, with trailing slash}"

cd "$(dirname "$0")"

bldmake bldfiles
abld build gcce urel

# abld reports success even when a file does not compile
for bin in ntpsyncd.exe ntpsyncui.exe; do
    path="${EPOCROOT}epoc32/release/gcce/urel/$bin"
    if [ ! -f "$path" ] || [ "$path" -ot ../src/daemon.cpp ] || [ "$path" -ot ../src/ui/ntpuiappui.cpp ]; then
        echo "build.sh: $path was not built, see the errors above" >&2
        exit 1
    fi
done

# The icon: gfx/ntpsyncui.svg as it is into a MIF (GnuPoc's mifconv keeps the SVG text;
# the phone's SVG engine reads it).
mifconv "${EPOCROOT}epoc32/data/z/resource/apps/ntpsyncui_aif.mif" ../gfx/ntpsyncui.svg

# abld goes on when a resource file does not compile either
for res in resource/apps/ntpsyncui.rsc resource/apps/ntpsyncui_en.rsc \
           resource/apps/ntpsyncui_aif.mif private/10003a3f/apps/ntpsyncui_reg.rsc; do
    path="${EPOCROOT}epoc32/data/z/$res"
    if [ ! -f "$path" ] || [ "$path" -ot ../data/ntpsyncui.rss ]; then
        echo "build.sh: $path was not made, see the errors above" >&2
        exit 1
    fi
done

cd ../sis
sed -e "s#\$(EPOCROOT)#${EPOCROOT}#g" ntpsyncd.pkg > build.pkg

if [ ! -f dev.key ]; then
    # Old signsis wants SHA1 + a traditional "BEGIN RSA PRIVATE KEY" PEM.
    openssl req -new -x509 -sha1 -newkey rsa:1024 -nodes -days 3650 \
        -subj "/CN=ntpsyncd Dev" -keyout dev.key -out dev.cer
    openssl rsa -in dev.key -out dev.key -traditional 2>/dev/null || true
fi

makesis build.pkg ntpsyncd_unsigned.sis
signsis ntpsyncd_unsigned.sis ntpsyncd.sisx dev.cer dev.key
echo "Built $(pwd)/ntpsyncd.sisx"
