#!/bin/bash
# ./scripts/build_client.sh
#
# Copyright (C) 2024-2025 Javier Blanco-Romero @fj-blanco (UC3M, QURSA project)
# Author: Javier Blanco-Romero
#
# Build script for CoAP Zephyr clients

set -e

clean_build() {
    rm -rf build/
    find . -name "CMakeCache.txt" -delete 2>/dev/null || true
    find . -name "CMakeFiles" -type d -exec rm -rf {} + 2>/dev/null || true
}

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BOARD_TARGET="esp32_devkitc_wroom/esp32/procpu"
DO_CLEAN=false
DO_INIT=false

# Parse arguments
for arg in "$@"; do
    case "$arg" in
        clean)
            DO_CLEAN=true
            ;;
        init)
            DO_INIT=true
            ;;
        *)
            echo "Usage: $0 [clean] [init]"
            echo "  clean   - Clean build directory before building"
            echo "  init    - Initialize and update west workspace"
            exit 1
            ;;
    esac
done

# Go to the mbedtls client directory
cd "$PROJECT_ROOT/mbedtls"
echo "Working in: $PWD"

if [ "$DO_CLEAN" = true ]; then
    echo "Cleaning build directory..."
    clean_build
fi

init_workspace() {
    echo "Initializing workspace..."
    west init -l .
    west update
    echo "Fetching ESP32 blobs..."
    west blobs fetch hal_espressif 2>/dev/null || true
}

# Initialize workspace if needed (standard Zephyr way)
if [ ! -f ".west/config" ] && [ ! -f "../.west/config" ]; then
    init_workspace
elif [ "$DO_INIT" = true ]; then
    echo "Re-initializing and updating workspace..."
    init_workspace
    echo ""
    echo "Workspace initialized!"
    echo "Run './scripts/build_client.sh' to build the client"
    exit 0
else
    echo "Updating workspace..."
    west update
fi

# Export Zephyr environment
echo "Exporting Zephyr environment..."
west zephyr-export

# Build
echo "Building for board: $BOARD_TARGET"
west build -p auto -b "$BOARD_TARGET" .

echo ""
echo "Build complete!"
echo "To flash: west flash"
echo "To monitor: west espressif monitor"