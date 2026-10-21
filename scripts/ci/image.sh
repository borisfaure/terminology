#!/bin/sh
# Build, test and push the CI images, from the workflows or by hand:
#
#   image.sh build <target>        EFL_REF overrides the target's ref
#   image.sh test <target> [--logs DIR] [--date YYYYMMDD]
#   image.sh shell <target> [--date YYYYMMDD]
#   image.sh push <target>
#   image.sh build-deps
#   image.sh push-deps
#
# test and shell use the local image, or pull the published one, or
# build it as a last resort. CONTAINER=docker replaces podman.
set -eu

CONTAINER=${CONTAINER:-podman}
here=$(dirname "$0")
root=$(cd "$here/../.." && pwd)
git_url=${EFL_GIT_URL:-https://git.enlightenment.org/enlightenment/efl.git}

# image <name> [date]
image() { echo "ghcr.io/borisfaure/terminology-ci:$1-ubuntu24${2:+-$2}"; }
deps_image=$(image deps)

usage() { sed -n '3,9s/^# //p' "$0" >&2; exit 2; }

have_image() {
   $CONTAINER image inspect "$1" >/dev/null 2>&1 ||
      $CONTAINER pull "$1" >/dev/null 2>&1
}

# The commit a branch points to, or an annotated tag peels to.
efl_commit() {
   git ls-remote "$git_url" "refs/heads/$1" "refs/tags/$1" "refs/tags/$1^{}" |
      tail -n 1 | cut -f 1
}

build_deps() {
   $CONTAINER build --pull --no-cache --target deps -t "$deps_image" \
      -f "$here/Containerfile" "$root"
}

build() {
   info=$("$here/targets.sh" "$1")
   # shellcheck disable=SC2086
   set -- "$1" $info
   target=$1
   ref=${EFL_REF:-$2}
   shift 2
   have_image "$deps_image" || build_deps
   $CONTAINER build --target efl \
      --build-arg DEPS="$deps_image" \
      --build-arg EFL_REF="$ref" \
      --build-arg EFL_COMMIT="$(efl_commit "$ref")" \
      --build-arg EXPECT_FEATURES="$*" \
      -t "$(image "$target")" -f "$here/Containerfile" "$root"
   $CONTAINER tag "$(image "$target")" "$(image "$target" "$(date -u +%Y%m%d)")"
}

# run test|shell <target> [--logs DIR] [--date YYYYMMDD]
run() {
   mode=$1
   target=$2
   shift 2
   logs=
   date=
   while [ $# -gt 0 ]; do
      case $1 in
         --logs) logs=$2 ;;
         --date) date=$2 ;;
         *) usage ;;
      esac
      shift 2
   done

   img=$(image "$target" "$date")
   if ! have_image "$img"; then
      echo "no $img, building it" >&2
      build "$target"
      img=$(image "$target")
   fi

   set -- -v "$root:/src:ro,Z" -e COLUMNS=150 -e TERM=xterm-256color
   if [ -n "$logs" ]; then
      mkdir -p "$logs"
      set -- "$@" -v "$(cd "$logs" && pwd):/build:Z"
   fi
   if [ "$mode" = shell ]; then
      $CONTAINER run --rm -it "$@" "$img" /bin/sh
   else
      $CONTAINER run --rm "$@" "$img" /src/scripts/ci/build-and-test.sh
   fi
}

case ${1:-} in
   build) [ $# -eq 2 ] || usage; build "$2" ;;
   test|shell) [ $# -ge 2 ] || usage; run "$@" ;;
   push)
      [ $# -eq 2 ] || usage
      $CONTAINER push "$(image "$2")"
      $CONTAINER push "$(image "$2" "$(date -u +%Y%m%d)")"
      ;;
   build-deps) build_deps ;;
   push-deps) $CONTAINER push "$deps_image" ;;
   *) usage ;;
esac
