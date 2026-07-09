#!/bin/sh
# Runs after the target rootfs is assembled, before the filesystem image is made.
set -e

# Mountpoint for the writable data partition (config + shaders)
mkdir -p "${TARGET_DIR}/data"
