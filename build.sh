#!/bin/sh
# Cross-compile clink.app for PocketBook Verse Pro Color (B300).
# Reuses pb-clink-builder, or pb-rsvp-builder if that image already exists.
set -e
IMAGE=pb-clink-builder
if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
  if docker image inspect pb-rsvp-builder >/dev/null 2>&1; then
    IMAGE=pb-rsvp-builder
  else
    echo "Building $IMAGE (first time, downloads SDK-B300)..."
    docker build --platform linux/amd64 -t "$IMAGE" .
  fi
fi
docker run --rm --platform linux/amd64 -v "$(pwd):/project" "$IMAGE" \
  -c 'mkdir -p build && cd build && cmake -DCMAKE_TOOLCHAIN_FILE=/SDK/share/cmake/arm_conf.cmake .. && cmake --build .'
echo "OK  build/clink.app"
