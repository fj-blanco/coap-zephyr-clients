#!/bin/bash

# Default values
WOLFSSL_VERSION_TAG="master"
DEFAULT_RELEASE_TAG="v5.8.2-stable"
DTLS_VERSION="1.3"  # Can be "1.2" or "1.3"
DEBUG_MODE="yes"
WOLFSSL_BUILD_DIR="/tmp/wolfssl"

# Function to display usage
usage() {
    echo "Usage: $0 [--fork | --release <version>]"
    echo "  --release [ver]  Clone from wolfSSL/wolfssl.git with specified version (default: v5.8.2-stable)"
    echo "  -h, --help       Show this help message"
    exit 1
}

# Parse command-line arguments
while [[ "$#" -gt 0 ]]; do
  case "$1" in
    --release)
      WOLFSSL_VERSION_TAG="${2:-$DEFAULT_RELEASE_TAG}"
      shift 2
      ;;
    -h|--help)
      usage
      ;;
    *)
      echo "Unknown option: $1"
      exit 1
      ;;
  esac
done

# Installing missing packages for Raspberry Pi
sudo apt-get update
sudo apt-get install -y autoconf automake libtool coreutils bsdmainutils

# Prompt user for removal of existing installation
read -p "Do you want to remove existing wolfSSL installation? (y/n): " remove_existing
if [ "$remove_existing" == "y" ]; then
    echo "Removing existing wolfSSL libraries..."
    sudo find /usr/local/lib -type f \( -name 'libwolfssl.*' \) -exec rm {} + 2>/dev/null
    sudo find /usr/lib /usr/local/lib -name 'libcoap-3-wolfssl.so*' -exec rm {} + 2>/dev/null
    sudo find /usr/lib /usr/local/lib -name 'libwolfssl.so*' -exec rm {} + 2>/dev/null
    sudo find /usr/local/include -type d -name 'wolfssl' -exec rm -rf {} + 2>/dev/null
    echo "Existing installation removed."
else
    echo "Keeping existing installation..."
fi

echo "Removing existing wolfSSL build directory in /tmp..."
rm -rf $WOLFSSL_BUILD_DIR
echo "Cloning wolfSSL to $WOLFSSL_BUILD_DIR..."
git clone --branch $WOLFSSL_VERSION_TAG --depth 1 https://github.com/wolfSSL/wolfssl.git $WOLFSSL_BUILD_DIR
cd $WOLFSSL_BUILD_DIR

# Build wolfSSL
./autogen.sh
mkdir build
cd build

#WOLFSSL_FLAGS="--enable-all --enable-dtls --enable-opensslall --enable-opensslextra --enable-experimental --with-liboqs --enable-kyber=ml-kem --disable-rpk"
WOLFSSL_FLAGS="--enable-all --enable-dtls --enable-kyber --enable-dilithium --disable-rpk"

if [ "$DEBUG_MODE" == "yes" ]; then
    WOLFSSL_FLAGS="$WOLFSSL_FLAGS --enable-debug"
fi

if [ "$DTLS_VERSION" == "1.3" ]; then
    echo "Installing with DTLS 1.3 support"
    WOLFSSL_FLAGS="$WOLFSSL_FLAGS --enable-dtls13 --enable-dtls-frag-ch"
else
    echo "Installing with DTLS 1.2 support"
fi

../configure $WOLFSSL_FLAGS
make all -j$(nproc)

sudo make install
sudo ldconfig

echo "wolfSSL installed successfully!"
echo "Build directory: $WOLFSSL_BUILD_DIR (will be cleaned up automatically on reboot)"