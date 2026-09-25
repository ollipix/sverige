#!/bin/bash
set -e

VERSION="0.7"
CXX="${CXX:-g++}"

echo "[saada] Building saada v${VERSION}..."

if ! command -v "$CXX" >/dev/null 2>&1; then
    echo "[saada] Fatal: C++ compiler not found: $CXX"
    exit 1
fi

if ! command -v install >/dev/null 2>&1; then
    echo "[saada] Fatal: install utility not found."
    exit 1
fi

"$CXX" -std=c++17 -O2 -Wall -Wextra -Wpedantic \
    -o saada saada.cpp

if [ -n "${PREFIX:-}" ]; then
    PREFIX_DIR="$PREFIX"
else
    PREFIX_DIR="/usr/local"
fi

BIN_DIR="${PREFIX_DIR}/bin"
SHARE_DIR="${PREFIX_DIR}/share/saada"

echo "[saada] Installing binary to ${BIN_DIR}/saada..."
install -Dm755 saada "${BIN_DIR}/saada"

echo "[saada] Installing manifest to ${SHARE_DIR}/manifest..."
install -Dm644 manifest "${SHARE_DIR}/manifest"

echo
echo "[saada] Saada v${VERSION} installed."
echo "[saada] Binary:   ${BIN_DIR}/saada"
echo "[saada] Manifest: ${SHARE_DIR}/manifest"
echo
echo "[saada] Try:"
echo "  saada --version"
echo "  saada doctor"
echo "  saada list"
