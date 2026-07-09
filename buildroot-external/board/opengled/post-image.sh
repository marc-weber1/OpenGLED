#!/bin/sh
# Assembles the final SD card image:
#   p1 boot (FAT32, firmware + kernel), p2 rootfs (squashfs, read-only),
#   p3 data (ext4, config.yaml + shaders — the only writable partition).
set -e

BOARD_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "${BOARD_DIR}/../../.." && pwd)"

# Install our boot config over the rpi-firmware defaults
cp "${BOARD_DIR}/config.txt"  "${BINARIES_DIR}/rpi-firmware/config.txt"
cp "${BOARD_DIR}/cmdline.txt" "${BINARIES_DIR}/rpi-firmware/cmdline.txt"

# Populate and build the writable data partition
DATA_ROOT="${BUILD_DIR}/opengled-data-root"
rm -rf "${DATA_ROOT}"
mkdir -p "${DATA_ROOT}/shaders"
cp "${BOARD_DIR}/data-partition/config.yaml" "${DATA_ROOT}/config.yaml"
cp "${REPO_DIR}/shaders/"*.fs "${DATA_ROOT}/shaders/"

rm -f "${BINARIES_DIR}/data.ext4"
truncate -s 32M "${BINARIES_DIR}/data.ext4"
mkfs.ext4 -q -F -L gled-data -d "${DATA_ROOT}" "${BINARIES_DIR}/data.ext4"

support/scripts/genimage.sh -c "${BOARD_DIR}/genimage.cfg"
