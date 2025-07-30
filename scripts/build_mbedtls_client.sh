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

# CoAP server configuration
COAP_IP=${COAP_IP:-"134.102.218.18"}
COAP_PATH=${COAP_PATH:-"/hello"}
COAP_PORT=${COAP_PORT:-"5683"}

# WiFi configuration
WIFI_SSID=${WIFI_SSID:-""}
WIFI_PASS=${WIFI_PASS:-""}

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
            echo ""
            echo "Environment variables:"
            echo "  COAP_IP    - CoAP server IP (default: 134.102.218.18)"
            echo "  COAP_PATH  - CoAP server path (default: /hello)"
            echo "  COAP_PORT  - CoAP server port (default: 5683)"
            echo "  WIFI_SSID  - WiFi network name (REQUIRED)"
            echo "  WIFI_PASS  - WiFi password (REQUIRED)"
            echo ""
            echo "Examples:"
            echo "  WIFI_SSID=\"your_ssid\" WIFI_PASS=\"your_password\" $0"
            echo "  COAP_IP=\"your_server_ip\" WIFI_SSID=\"your_ssid\" WIFI_PASS=\"your_password\" $0"
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

# Validate WiFi credentials are provided
if [ -z "$WIFI_SSID" ] || [ -z "$WIFI_PASS" ]; then
    echo ""
    echo "ERROR: WiFi credentials are required!"
    echo "Please set WIFI_SSID and WIFI_PASS environment variables."
    echo ""
    echo "Example:"
    echo "  WIFI_SSID=\"your_ssid\" WIFI_PASS=\"your_password\" $0"
    echo ""
    exit 1
fi

# Build
echo "Building mbedTLS CoAP client for board: $BOARD_TARGET"
echo "CoAP target: coap://${COAP_IP}${COAP_PATH}"
echo "WiFi network: ${WIFI_SSID}"

# Export environment variables for CMake
export COAP_IP COAP_PATH COAP_PORT WIFI_SSID WIFI_PASS

west build -p auto -b "$BOARD_TARGET" .

echo ""
echo "mbedTLS CoAP client build complete!"
echo "Configuration: coap://${COAP_IP}${COAP_PATH} via ${WIFI_SSID}"
echo "To flash: west flash"
echo "To monitor: west espressif monitor"