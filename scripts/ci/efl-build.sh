#!/bin/sh
# efl-build.sh <branch-or-tag> <prefix>
#
# Build EFL from git and install it into <prefix>, with the commit built
# in <prefix>/share/efl-commit. EFL_GIT_URL overrides the clone source,
# EFL_MESON_OPTS adds meson options.
set -eu

ref=$1
prefix=$2
url=${EFL_GIT_URL:-https://git.enlightenment.org/enlightenment/efl.git}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

git clone --depth 1 --branch "$ref" "$url" "$work/efl"

# Terminology needs none of these. pipewire and docs only exist from
# 1.28 on, and Ubuntu's build-dep list for 1.27 lacks what they need.
opts="-Dbuild-tests=false -Dbuild-examples=false -Dnls=false -Dbindings=
      -Dphysics=false -Dgstreamer=false -Dwl=false -Ddrm=false
      -Dsystemd=false"
for opt in pipewire docs; do
   if grep -q "^option('$opt'," "$work/efl/meson_options.txt"; then
      opts="$opts -D$opt=false"
   fi
done

# gnu17: EFL 1.26 to 1.28 do not build as C23, the default of gcc 15.
# --libdir=lib: /usr/local/lib is on Ubuntu's library and pkg-config paths.
# shellcheck disable=SC2086
meson setup "$work/build" "$work/efl" --prefix="$prefix" --libdir=lib \
   --buildtype=release -Dc_args=-std=gnu17 $opts ${EFL_MESON_OPTS:-}
ninja -C "$work/build" install

mkdir -p "$prefix/share"
git -C "$work/efl" rev-parse HEAD > "$prefix/share/efl-commit"
