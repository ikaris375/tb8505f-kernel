#!/usr/bin/env bash
#
# Lenovo TB8505F (MT6761) Kernel Build Script
# Version : 1.0
# Author  : ikaris375
#

set -e

############################################
# Configuration
############################################

REPO_OWNER="ikaris375"
REPO_NAME="tb8505f-kernel"

RELEASE_TAG="v1.0-clean"

TOOLCHAIN_ARCHIVE="arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu.tar.xz"

TOOLCHAIN_URL="https://github.com/${REPO_OWNER}/${REPO_NAME}/releases/download/${RELEASE_TAG}/${TOOLCHAIN_ARCHIVE}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

TOOLCHAIN_ROOT="${SCRIPT_DIR}/toolchains"

TOOLCHAIN_DIR="${TOOLCHAIN_ROOT}/arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu"

TOOLCHAIN_BIN="${TOOLCHAIN_DIR}/bin"

export ARCH=arm64

export CROSS_COMPILE="${TOOLCHAIN_BIN}/aarch64-none-linux-gnu-"

JOBS="$(nproc)"
DEFCONFIG="tb8766p1_64_bsp_defconfig"

KCFLAGS="-Wno-error=array-bounds -Wno-error=maybe-uninitialized -Wno-error=stringop-truncation -Wno-error=packed-not-aligned -Wno-error=address-of-packed-member -Wno-error=misleading-indentation -Wno-error=restrict -Wno-error=stringop-overflow -Wno-error=memset-elt-size -Wno-error=missing-attributes -Wno-error=zero-length-bounds"

############################################
# Colors
############################################

GREEN="\e[32m"
RED="\e[31m"
BLUE="\e[34m"
YELLOW="\e[33m"
RESET="\e[0m"

############################################
# Logging
############################################

info() {
    echo -e "${BLUE}[*]${RESET} $1"
}

success() {
    echo -e "${GREEN}[✓]${RESET} $1"
}

error() {
    echo -e "${RED}[✗]${RESET} $1"
    exit 1
}

############################################
# Dependency Check
############################################

check_command() {

    command -v "$1" >/dev/null 2>&1 || error "$1 is not installed."

}

check_dependencies() {

    info "Checking dependencies..."

    check_command make
    check_command tar
    check_command xz
    check_command curl

    success "Dependencies OK"

}

############################################
# Toolchain
############################################

download_toolchain() {

    mkdir -p "${TOOLCHAIN_ROOT}"

    if [ -f "${TOOLCHAIN_BIN}/aarch64-none-linux-gnu-gcc" ]; then

        success "Toolchain found."

        return

    fi

    info "Downloading toolchain..."

    curl -L "${TOOLCHAIN_URL}" \
        -o "${TOOLCHAIN_ROOT}/${TOOLCHAIN_ARCHIVE}"

    success "Download complete."

    info "Extracting toolchain..."

    tar -xf \
        "${TOOLCHAIN_ROOT}/${TOOLCHAIN_ARCHIVE}" \
        -C "${TOOLCHAIN_ROOT}"

    rm -f "${TOOLCHAIN_ROOT}/${TOOLCHAIN_ARCHIVE}"

    success "Toolchain installed."

}

############################################
# Build
############################################

build_kernel() {

    START=$(date +%s)

    info "Checking kernel configuration..."

    if [ ! -f ".config" ]; then
        info "No .config found. Generating ${DEFCONFIG}..."

        make \
            ARCH=arm64 \
            CROSS_COMPILE="${CROSS_COMPILE}" \
            "${DEFCONFIG}" || error "Failed to generate .config"

        success "Configuration generated."
    fi

    info "Starting kernel build..."

    make \
        ARCH=arm64 \
        CROSS_COMPILE="${CROSS_COMPILE}" \
        -j1 V=1 \
        KCFLAGS="${KCFLAGS}"

    END=$(date +%s)

    ELAPSED=$((END-START))

    OUTPUT="arch/arm64/boot/Image.gz-dtb"

    if [ -f "${OUTPUT}" ]; then
        success "Kernel compiled successfully."

        echo
        echo "Image:"
        echo "${OUTPUT}"
    else
        error "Build completed, but ${OUTPUT} was not found."
    fi

    echo
    echo "Build Time: ${ELAPSED} seconds"

}

############################################
# Main
############################################

echo

echo "=============================================="

echo " Lenovo TB8505F Kernel Build System v1.0"

echo "=============================================="

echo

check_dependencies

download_toolchain

build_kernel