#!/bin/sh
# Build dependencies on ubuntu:24.04: Ubuntu's build-dep list for EFL,
# which covers the other EFL versions too, and Terminology's build
# tools. git lets actions/checkout make a real clone in the container;
# ca-certificates, only recommended by git, lets it clone over https.
# Lua: edje needs it in every EFL version. 1.26/1.27 default to luajit,
# 1.28/master to lua5.1; Ubuntu's efl build-dep has neither.
set -eu

sed -i 's/^Types: deb$/Types: deb deb-src/' /etc/apt/sources.list.d/ubuntu.sources
apt-get update
apt-get install -y --no-install-recommends \
   build-essential meson ninja-build pkg-config gettext python3 gperf git \
   ca-certificates libluajit-5.1-dev liblua5.1-0-dev
apt-get build-dep -y efl
rm -rf /var/lib/apt/lists/*
