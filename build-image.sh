#!/usr/bin/env bash
#
# Builds a flashable SD card image for a Raspberry Pi Zero 2 W using Buildroot.
# Works on Fedora and on WSL2 (Ubuntu/Debian or Fedora distros).
#
# Usage:
#   ./build-image.sh                normal (incremental) build
#   ./build-image.sh reconfigure    re-apply the defconfig + fragment, then build
#   ./build-image.sh menuconfig     open buildroot's menuconfig (for tinkering)
#   ./build-image.sh clean          delete build output (keeps downloads)
#
# Environment overrides:
#   OPENGLED_BUILD_DIR         where buildroot lives and builds (default: ~/opengled-buildroot)
#   OPENGLED_BUILDROOT_BRANCH  buildroot branch/tag to use (default: 2025.02.x LTS)
#   OPENGLED_BUILDROOT_GIT     buildroot git URL

set -euo pipefail

BUILDROOT_BRANCH="${OPENGLED_BUILDROOT_BRANCH:-2025.02.x}"
BUILDROOT_GIT="${OPENGLED_BUILDROOT_GIT:-https://gitlab.com/buildroot.org/buildroot.git}"

msg()  { printf '\033[1;32m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33mWARNING:\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[1;31mERROR:\033[0m %s\n' "$*" >&2; exit 1; }

# ---------------------------------------------------------------- environment

[[ "$(uname -s)" == "Linux" ]] \
    || die "This script must run on Linux. On Windows, run it inside WSL2 (e.g. 'wsl ./build-image.sh')."

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXTERNAL_DIR="$REPO_DIR/buildroot-external"
FRAGMENT="$EXTERNAL_DIR/configs/opengled.fragment"
[[ -f "$FRAGMENT" ]] || die "Can't find $FRAGMENT — is the repo checkout complete?"

IS_WSL=0
grep -qi microsoft /proc/version 2>/dev/null && IS_WSL=1

# Buildroot refuses to build from paths containing spaces
case "$REPO_DIR" in *" "*) die "The repository path ('$REPO_DIR') contains spaces; buildroot cannot handle that. Move/clone the repo to a path without spaces." ;; esac

# The build tree must live OUTSIDE the repo: the open-gled package rsyncs the
# whole repo as its source, and on WSL2 building on /mnt/* (Windows drives) is
# both extremely slow and breaks symlinks/permissions.
BUILD_DIR="${OPENGLED_BUILD_DIR:-$HOME/opengled-buildroot}"
case "$BUILD_DIR" in *" "*) die "OPENGLED_BUILD_DIR ('$BUILD_DIR') contains spaces; buildroot cannot handle that." ;; esac
case "$BUILD_DIR" in "$REPO_DIR"/*) die "OPENGLED_BUILD_DIR must not be inside the repository (the build would recursively copy itself). Default is ~/opengled-buildroot." ;; esac
if [[ $IS_WSL -eq 1 && "$BUILD_DIR" == /mnt/* ]]; then
    die "OPENGLED_BUILD_DIR is on a Windows drive ($BUILD_DIR). Buildroot needs a native Linux filesystem (symlinks, permissions, speed). Use the default (~/opengled-buildroot) or another path inside the WSL2 filesystem."
fi
if [[ $IS_WSL -eq 1 && "$REPO_DIR" == /mnt/* ]]; then
    warn "The repo is on a Windows drive ($REPO_DIR). That works (sources are copied), but cloning it inside the WSL2 filesystem would build faster."
fi

# WSL2 imports the Windows PATH, whose entries contain spaces/parentheses
# ('Program Files (x86)') — buildroot refuses to run with those in PATH.
CLEAN_PATH=""
IFS=':' read -ra _path_parts <<< "$PATH"
for p in "${_path_parts[@]}"; do
    [[ "$p" == *" "* || "$p" == *"("* ]] && continue
    CLEAN_PATH="${CLEAN_PATH:+$CLEAN_PATH:}$p"
done
export PATH="$CLEAN_PATH"

if [[ ${EUID} -eq 0 ]]; then
    warn "Running as root; setting FORCE_UNSAFE_CONFIGURE=1 so buildroot's tar build doesn't abort."
    export FORCE_UNSAFE_CONFIGURE=1
fi

# -------------------------------------------------------------- dependencies

declare -A CMD_DESC=(
    [git]="git"
    [make]="GNU make"
    [gcc]="C compiler"
    [g++]="C++ compiler"
    [patch]="patch"
    [perl]="perl"
    [python3]="python3"
    [rsync]="rsync (used to copy this repo into the build)"
    [bc]="bc (needed by the kernel build)"
    [wget]="wget (buildroot's downloader)"
    [cpio]="cpio"
    [unzip]="unzip"
    [tar]="tar"
    [file]="file"
    [bzip2]="bzip2"
    [gzip]="gzip"
    [cmp]="cmp (diffutils)"
    [find]="find (findutils)"
    [which]="which"
)

missing=()
for cmd in "${!CMD_DESC[@]}"; do
    command -v "$cmd" >/dev/null 2>&1 || missing+=("$cmd (${CMD_DESC[$cmd]})")
done

# Perl modules that minimal perl installs (notably Fedora's) often lack,
# but buildroot's host tools need. Buildroot fails these late with a cryptic
# "your perl installation is not complete enough", so check them up front.
# (This is the set buildroot's autoconf/automake/host-libtool machinery pulls
# in beyond bare perl.)
PERL_MODULES=(
    Thread::Queue
    FindBin
    ExtUtils::MakeMaker
    Data::Dumper
    IPC::Cmd
    open
    English
    POSIX
    File::Copy
    File::Compare
    Getopt::Long
    Digest::SHA
)
for mod in "${PERL_MODULES[@]}"; do
 +  # Load the module file with require rather than `use`/-M: pragmas like
    # `open` fail on an empty import list even when installed, so we just
    # confirm the .pm is loadable (turn Foo::Bar into Foo/Bar.pm).
    modpath="${mod//:://}.pm"
    perl -e "require '$modpath'" >/dev/null 2>&1 || missing+=("perl module $mod")
done

if [[ ${#missing[@]} -gt 0 ]]; then
    echo
    echo "The following build dependencies are missing:" >&2
    printf '  - %s\n' "${missing[@]}" >&2
    echo >&2

    . /etc/os-release 2>/dev/null || true
    like="${ID:-} ${ID_LIKE:-}"
    if [[ "$like" == *fedora* || "$like" == *rhel* ]]; then
        echo "On Fedora, install everything with:" >&2
        echo "  sudo dnf install -y git make gcc gcc-c++ patch perl-core perl-Thread-Queue perl-FindBin perl-ExtUtils-MakeMaker python3 rsync bc wget cpio unzip tar file bzip2 gzip diffutils findutils which ncurses-devel" >&2
    elif [[ "$like" == *debian* || "$like" == *ubuntu* ]]; then
        echo "On Debian/Ubuntu (typical WSL2 distro), install everything with:" >&2
        echo "  sudo apt-get update && sudo apt-get install -y build-essential git patch perl python3 rsync bc wget cpio unzip file bzip2 libncurses-dev" >&2
    else
        echo "Install the equivalents of: build-essential/gcc-c++, git, patch, perl (with Thread::Queue), python3, rsync, bc, wget, cpio, unzip, file, bzip2, diffutils, findutils" >&2
    fi
    die "Install the missing dependencies and re-run."
fi

NPROC="$(nproc)"
msg "All host dependencies found. Building with all $NPROC CPU cores."

# ----------------------------------------------------------------- buildroot

BR_DIR="$BUILD_DIR/buildroot"
OUT_DIR="$BUILD_DIR/output"
export BR2_DL_DIR="$BUILD_DIR/dl"
mkdir -p "$BUILD_DIR" "$BR2_DL_DIR"

ACTION="${1:-build}"

if [[ "$ACTION" == "clean" ]]; then
    msg "Removing $OUT_DIR (downloads in $BR2_DL_DIR are kept)"
    rm -rf "$OUT_DIR"
    exit 0
fi

if [[ ! -d "$BR_DIR/.git" ]]; then
    msg "Cloning buildroot ($BUILDROOT_BRANCH) into $BR_DIR"
    git clone --depth 1 --branch "$BUILDROOT_BRANCH" "$BUILDROOT_GIT" "$BR_DIR" \
        || die "Failed to clone buildroot from $BUILDROOT_GIT (branch $BUILDROOT_BRANCH). Check network access, or set OPENGLED_BUILDROOT_GIT to a mirror such as https://github.com/buildroot/buildroot.git"
else
    msg "Using existing buildroot checkout in $BR_DIR"
fi

# ---------------------------------------------------------------- configure

configure() {
    msg "Applying raspberrypizero2w_defconfig + opengled.fragment"
    make -C "$BR_DIR" O="$OUT_DIR" BR2_EXTERNAL="$EXTERNAL_DIR" raspberrypizero2w_defconfig \
        || die "Failed to apply raspberrypizero2w_defconfig. Your buildroot checkout may be too old for this board; try removing $BR_DIR and re-running."
    ( cd "$BR_DIR" && ./support/kconfig/merge_config.sh -m -O "$OUT_DIR" "$OUT_DIR/.config" "$FRAGMENT" ) \
        || die "Failed to merge $FRAGMENT into the buildroot config."
    make -C "$BR_DIR" O="$OUT_DIR" olddefconfig \
        || die "Failed to finalize the merged buildroot config."

    # Sanity-check that the important fragment options survived the merge
    for opt in BR2_PACKAGE_OPEN_GLED BR2_TARGET_ROOTFS_SQUASHFS BR2_PACKAGE_MESA3D_GALLIUM_DRIVER_VC4; do
        grep -q "^${opt}=y" "$OUT_DIR/.config" \
            || die "$opt did not make it into the final config — the fragment merge failed (check $OUT_DIR/.config and $FRAGMENT)."
    done
}

if [[ "$ACTION" == "reconfigure" || ! -f "$OUT_DIR/.config" || "$FRAGMENT" -nt "$OUT_DIR/.config" ]]; then
    configure
fi

if [[ "$ACTION" == "menuconfig" ]]; then
    exec make -C "$BR_DIR" O="$OUT_DIR" menuconfig
fi

# -------------------------------------------------------------------- build

# The open-gled package copies this repo at extract time, so force it to
# pick up source changes on every run (cheap compared to the full build).
if [[ -d "$OUT_DIR/build/open-gled-local" ]]; then
    msg "Refreshing open-gled sources from $REPO_DIR"
    make -C "$BR_DIR" O="$OUT_DIR" open-gled-dirclean
fi

msg "Building (first build compiles a cross-toolchain + kernel and takes 1-2 hours; later builds are incremental)"
# Parallelism comes from BR2_JLEVEL=0 in the fragment: every package builds
# with all available cores. Buildroot's top-level make must stay serial.
make -C "$BR_DIR" O="$OUT_DIR" \
    || die "Build failed. Scroll up for the first error; re-running './build-image.sh' resumes where it stopped."

IMG="$OUT_DIR/images/sdcard.img"
[[ -f "$IMG" ]] || die "Build finished but $IMG was not produced — check the output of the post-image step above."

cp "$IMG" "$REPO_DIR/opengled-sdcard.img"

msg "Done!"
echo
echo "SD card image: $REPO_DIR/opengled-sdcard.img"
echo
echo "Flash it with Raspberry Pi Imager (choose 'Use custom image'), or:"
echo "  sudo dd if=opengled-sdcard.img of=/dev/sdX bs=4M conv=fsync status=progress"
if [[ $IS_WSL -eq 1 && "$REPO_DIR" != /mnt/* ]]; then
    echo
    echo "From Windows, the image is at: \\\\wsl\$\\<your-distro>$REPO_DIR\\opengled-sdcard.img"
fi
echo
echo "The image boots straight into open_gled. Partition 3 ('gled-data', ext4) holds"
echo "/data/config.yaml and /data/shaders — edit those to reconfigure without rebuilding."
