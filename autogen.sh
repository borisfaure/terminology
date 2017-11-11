#!/bin/sh

rm -rf build
meson $@ . build
mesonconf build
