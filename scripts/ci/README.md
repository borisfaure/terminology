# CI scripts

Build and test Terminology against old EFL releases locally, the same way
CI does. Needs podman, or docker with `CONTAINER=docker`. The checkout is
mounted read-only at `/src` in the container.

## Test against an EFL version

    scripts/ci/image.sh test efl-1.26

Targets are listed in `targets` (EFL 1.26, 1.27, 1.28, master). The image
is pulled from ghcr.io if published, built locally otherwise — the first
build compiles EFL from source and takes a while. Inside it, Terminology
is configured, built and tested, and the build fails if the detected EFL
features do not match the target's expectations.

Add `--logs DIR` to keep the build directory and meson logs on the host:

    scripts/ci/image.sh test efl-1.26 --logs build/efl-1.26

## Build the images

`test` and `shell` pull the published image or build it as a last resort.
Build them yourself after changing `efl-deps.sh` or the `Containerfile`,
or to cache an image for repeated runs. `build-deps` first: `build`
pulls the published deps image when it has none locally.

    scripts/ci/image.sh build-deps
    scripts/ci/image.sh build efl-1.26

## Test another EFL ref

    EFL_REF=v1.26.0 scripts/ci/image.sh build efl-1.26
    scripts/ci/image.sh test efl-1.26

## Get a shell in the image

    scripts/ci/image.sh shell efl-1.26

The repo is at `/src`; the EFL commit built is in
`/usr/local/share/efl-commit`.

## Environment variables

- `EFL_REF` — build a different EFL ref into a target's image
- `EFL_GIT_URL` — clone EFL from a fork or mirror
- `EFL_MESON_OPTS` — extra meson options for the EFL build
