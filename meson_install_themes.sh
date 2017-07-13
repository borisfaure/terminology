#!/bin/sh

for x in "$@" ; do
    chmod 644 "$MESON_INSTALL_DESTDIR_PREFIX/$x"
done
