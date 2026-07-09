#!/bin/sh
# Runs after the target rootfs is assembled, before the filesystem image is made.
set -e

# Mountpoint for the writable data partition (config + shaders)
mkdir -p "${TARGET_DIR}/data"

# Init scripts must be executable no matter what happened to the overlay
# files' permissions (editing them from Windows silently strips the x bit).
chmod +x "${TARGET_DIR}/etc/init.d/"S??*
