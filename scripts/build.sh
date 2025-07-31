#!/bin/bash
# ./scripts/build.sh
#
# Copyright (C) 2024-2025 Javier Blanco-Romero @fj-blanco (UC3M, QURSA project)
# Author: Javier Blanco-Romero
#
# Build script for CoAP Zephyr clients

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BOARD_TARGET="esp32_devkitc_wroom/esp32/procpu"

# Defaults
BACKEND=""
COAP_IP="134.102.218.18"
COAP_PATH="/hello"
COAP_PORT="5683"
WIFI_SSID=""
WIFI_PASS=""
USE_DTLS=false
DO_CLEAN=false
DO_INIT=false

usage() {
    echo "Usage: $0 --backend <wolfssl|mbedtls> [options]"
    echo ""
    echo "Required:"
    echo "  --backend <wolfssl|mbedtls>  TLS backend to use"
    echo "  --wifi-ssid <ssid>           WiFi network name"
    echo "  --wifi-pass <password>       WiFi password"
    echo ""
    echo "Optional:"
    echo "  --coap-ip <ip>               CoAP server IP (default: 134.102.218.18)"
    echo "  --coap-path <path>           CoAP server path (default: /hello)"
    echo "  --coap-port <port>           CoAP server port (default: 5683)"
    echo "  --use-dtls                   Enable DTLS (default: disabled)"
    echo "  --clean                      Clean build directory"
    echo "  --init                       Initialize workspace"
    echo ""
    echo "Example:"
    echo "  $0 --backend wolfssl --coap-ip \"your_ip\" --coap-port \"5684\" \\"
    echo "     --coap-path \"/time\" --wifi-ssid \"MyWiFi\" --wifi-pass \"password\" --use-dtls"
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --backend)
            BACKEND="$2"
            shift 2
            ;;
        --coap-ip)
            COAP_IP="$2"
            shift 2
            ;;
        --coap-path)
            COAP_PATH="$2"
            shift 2
            ;;
        --coap-port)
            COAP_PORT="$2"
            shift 2
            ;;
        --wifi-ssid)
            WIFI_SSID="$2"
            shift 2
            ;;
        --wifi-pass)
            WIFI_PASS="$2"
            shift 2
            ;;
        --use-dtls)
            USE_DTLS=true
            shift
            ;;
        --clean)
            DO_CLEAN=true
            shift
            ;;
        --init)
            DO_INIT=true
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

# Validate required arguments
if [[ -z "$BACKEND" ]]; then
    echo "ERROR: --backend is required"
    usage
    exit 1
fi

if [[ "$BACKEND" != "wolfssl" && "$BACKEND" != "mbedtls" ]]; then
    echo "ERROR: --backend must be 'wolfssl' or 'mbedtls'"
    exit 1
fi

if [[ -z "$WIFI_SSID" || -z "$WIFI_PASS" ]]; then
    echo "ERROR: --wifi-ssid and --wifi-pass are required"
    usage
    exit 1
fi

# Set backend-specific directory
cd "$PROJECT_ROOT/$BACKEND"

clean_build() {
    rm -rf build/
    find . -name "CMakeCache.txt" -delete 2>/dev/null || true
    find . -name "CMakeFiles" -type d -exec rm -rf {} + 2>/dev/null || true
}

init_workspace() {
    west init -l .
    west update
    west blobs fetch hal_espressif 2>/dev/null || true
}

if [ "$DO_INIT" = true ]; then
    echo "Initializing workspace..."
    init_workspace
    echo "Workspace initialized! Run without --init to build."
    exit 0
fi

if [ "$DO_CLEAN" = true ]; then
    clean_build
fi

# Initialize workspace if needed
if [ ! -f ".west/config" ] && [ ! -f "../.west/config" ]; then
    init_workspace
else
    west update
fi

west zephyr-export

# Set protocol based on DTLS flag
PROTOCOL="coap"
if [ "$USE_DTLS" = true ]; then
    PROTOCOL="coaps"
    if [ "$COAP_PORT" = "5683" ]; then
        COAP_PORT="5684"  # Default DTLS port
    fi
fi

echo "Building ${BACKEND} CoAP client"
echo "Target: ${PROTOCOL}://${COAP_IP}:${COAP_PORT}${COAP_PATH}"

# Export environment variables for CMake
export COAP_IP COAP_PATH COAP_PORT WIFI_SSID WIFI_PASS
if [ "$USE_DTLS" = true ]; then
    export USE_DTLS=1
fi

west build -p auto -b "$BOARD_TARGET" .

echo ""
echo "Build complete!"
echo "Flash: west flash"
echo "Monitor: west espressif monitor"