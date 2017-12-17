#!/bin/sh

rm -rf build
meson $@ . build
meson configure build
