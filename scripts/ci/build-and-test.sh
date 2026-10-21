#!/bin/sh
# build-and-test.sh [--src DIR] [--build DIR]
#
# Build and test Terminology in a CI image. First check that configure
# detected exactly the EFL features in EXPECT_FEATURES, so that a check
# which silently stops matching fails here instead of testing the
# fallback.
set -eu

src=/src
build=/build
while [ $# -gt 0 ]; do
   case $1 in
      --src) src=$2 ;;
      --build) build=$2 ;;
      *) echo "unknown argument: $1" >&2; exit 2 ;;
   esac
   shift 2
done

echo "EFL commit: $(cat /usr/local/share/efl-commit 2>/dev/null || echo unknown)"
echo "Expected features: ${EXPECT_FEATURES:-none}"

if [ -f "$build/meson-private/coredata.dat" ]; then
   meson setup --reconfigure -Dtests=true "$build" "$src"
else
   meson setup -Dtests=true "$build" "$src"
fi

failed=
for check in URILIST:HAVE_ELM_SEL_FORMAT_URILIST \
             LIGATURES:HAVE_TEXTGRID_LIGATURES \
             OVERLINE:HAVE_TEXTGRID_OVERLINE \
             RGB:HAVE_TEXTGRID_RGB; do
   feature=${check%%:*}
   macro=${check#*:}
   case " ${EXPECT_FEATURES:-} " in
      *" $feature "*) expected=yes ;;
      *) expected=no ;;
   esac
   if grep -q "^#define $macro " "$build/terminology_config.h"; then
      detected=yes
   else
      detected=no
   fi
   if [ "$detected" != "$expected" ]; then
      echo "FAIL: $macro detected: $detected, expected: $expected" >&2
      failed=1
   fi
done
[ -z "$failed" ]

ninja -C "$build"
meson test -C "$build" --print-errorlogs
