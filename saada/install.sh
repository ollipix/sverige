#!/bin/bash

set -e

echo "[saada] Checking for C++ compiler..."

if ! command -v g++ >/dev/null 2>&1; then
    echo "[saada] Fatal: g++ is required to build Saada."
    exit 1
fi

echo "[saada] Compiling Saada..."

g++ \
    -std=c++17 \
    -O2 \
    -Wall \
    -Wextra \
    -o saada \
    saada.cpp

echo "[saada] Installing Saada..."

install -Dm755 saada /usr/local/bin/saada

echo "[saada] Saada installed successfully."
echo "[saada] Run 'saada --help' to get started."
