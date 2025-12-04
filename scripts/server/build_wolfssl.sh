#!/bin/bash
# =============================================================================
# build_wolfssl.sh - Build wolfSSL with optional Post-Quantum Cryptography
# =============================================================================
# Builds wolfSSL for use with libcoap. Supports both classical crypto and
# native PQC (ML-KEM/Kyber, ML-DSA/Dilithium) without requiring liboqs.
#
# Usage:
#   ./build_wolfssl.sh [OPTIONS]
#
# Options:
#   --version <tag>   wolfSSL version (default: v5.8.4-stable)
#   --dtls <1.2|1.3>  DTLS version (default: 1.3)
#   --pqc             Enable Post-Quantum Cryptography (ML-KEM, ML-DSA)
#   --debug           Enable debug mode
#   --clean           Remove existing installation first
#   -y, --yes         Skip confirmation prompts
#   -h, --help        Show this help
#
# Examples:
#   ./build_wolfssl.sh                    # Classical crypto, DTLS 1.3
#   ./build_wolfssl.sh --pqc              # With PQC support
#   ./build_wolfssl.sh --dtls 1.2         # DTLS 1.2 only
#   ./build_wolfssl.sh --pqc --debug      # PQC with debug
#
# =============================================================================

set -e

# Configuration defaults
WOLFSSL_VERSION="v5.8.4-stable"
DTLS_VERSION="1.3"
DEBUG_MODE="no"
ENABLE_PQC="no"
CLEAN_INSTALL="no"
AUTO_YES="no"
WOLFSSL_BUILD_DIR="${WOLFSSL_BUILD_DIR:-/tmp/wolfssl-build}"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Build wolfSSL with optional Post-Quantum Cryptography support.

Options:
  --version <tag>   wolfSSL version tag (default: $WOLFSSL_VERSION)
  --dtls <1.2|1.3>  DTLS version (default: $DTLS_VERSION)
  --pqc             Enable Post-Quantum Cryptography (ML-KEM, ML-DSA)
  --debug           Enable wolfSSL debug mode
  --clean           Remove existing wolfSSL installation before building
  -y, --yes         Skip confirmation prompts
  -h, --help        Show this help message

Post-Quantum Algorithms (enabled with --pqc):
  Key Exchange:  ML-KEM-512, ML-KEM-768, ML-KEM-1024 (Kyber)
  Signatures:    ML-DSA-44, ML-DSA-65, ML-DSA-87 (Dilithium)
  Hybrids:       P256_KYBER_LEVEL1, P384_KYBER_LEVEL3, P521_KYBER_LEVEL5

Note: ML-KEM key exchange requires DTLS 1.3 (--dtls 1.3)

Examples:
  $0                          # Classical crypto only
  $0 --pqc                    # Enable PQC support
  $0 --pqc --dtls 1.3 --debug # PQC with debug
  $0 --clean --pqc -y         # Clean install with PQC, no prompts
EOF
    exit 0
}

# Parse arguments
while [[ "$#" -gt 0 ]]; do
    case "$1" in
        --version)
            WOLFSSL_VERSION="$2"
            shift 2
            ;;
        --dtls)
            DTLS_VERSION="$2"
            if [[ "$DTLS_VERSION" != "1.2" && "$DTLS_VERSION" != "1.3" ]]; then
                log_error "Invalid DTLS version: $DTLS_VERSION (use 1.2 or 1.3)"
                exit 1
            fi
            shift 2
            ;;
        --pqc)
            ENABLE_PQC="yes"
            shift
            ;;
        --debug)
            DEBUG_MODE="yes"
            shift
            ;;
        --clean)
            CLEAN_INSTALL="yes"
            shift
            ;;
        -y|--yes)
            AUTO_YES="yes"
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            log_error "Unknown option: $1"
            usage
            ;;
    esac
done

# Warn about DTLS 1.2 + PQC limitation
if [[ "$ENABLE_PQC" == "yes" && "$DTLS_VERSION" == "1.2" ]]; then
    log_warn "ML-KEM key exchange requires DTLS 1.3."
    log_warn "With DTLS 1.2, only ML-DSA signatures will be available."
    if [[ "$AUTO_YES" != "yes" ]]; then
        read -p "Continue anyway? (y/n): " confirm
        [[ "$confirm" != "y" ]] && exit 0
    fi
fi

log_info "=========================================="
log_info " wolfSSL Build Configuration"
log_info "=========================================="
log_info "Version:     $WOLFSSL_VERSION"
log_info "DTLS:        $DTLS_VERSION"
log_info "PQC Support: $ENABLE_PQC"
log_info "Debug:       $DEBUG_MODE"
log_info "Build Dir:   $WOLFSSL_BUILD_DIR"
log_info "=========================================="

if [[ "$AUTO_YES" != "yes" ]]; then
    read -p "Continue? (y/n): " confirm
    [[ "$confirm" != "y" ]] && exit 0
fi

# Install dependencies
log_info "Installing build dependencies..."
sudo apt-get update -qq
sudo apt-get install -y -qq autoconf automake libtool coreutils bsdmainutils

# Clean existing installation if requested
if [[ "$CLEAN_INSTALL" == "yes" ]]; then
    log_info "Removing existing wolfSSL installation..."
    sudo find /usr/local/lib -type f \( -name 'libwolfssl.*' \) -exec rm {} + 2>/dev/null || true
    sudo find /usr/lib /usr/local/lib -name 'libwolfssl.so*' -exec rm {} + 2>/dev/null || true
    sudo find /usr/local/include -type d -name 'wolfssl' -exec rm -rf {} + 2>/dev/null || true
    sudo ldconfig
fi

# Clone repository
rm -rf "$WOLFSSL_BUILD_DIR"
log_info "Cloning wolfSSL $WOLFSSL_VERSION..."
git clone --branch "$WOLFSSL_VERSION" --depth 1 https://github.com/wolfSSL/wolfssl.git "$WOLFSSL_BUILD_DIR"
cd "$WOLFSSL_BUILD_DIR"

# Generate build system
log_info "Running autogen..."
./autogen.sh

mkdir -p build
cd build

# Build configure flags
WOLFSSL_FLAGS=""

# Core TLS/DTLS support
WOLFSSL_FLAGS+=" --enable-dtls"
WOLFSSL_FLAGS+=" --enable-tls13"
WOLFSSL_FLAGS+=" --enable-hkdf"

# DTLS version specific
if [[ "$DTLS_VERSION" == "1.3" ]]; then
    WOLFSSL_FLAGS+=" --enable-dtls13"
    WOLFSSL_FLAGS+=" --enable-dtls-frag-ch"
fi

# Post-Quantum Cryptography (native wolfCrypt implementations, no liboqs)
# Using standardized NIST names: ML-KEM (FIPS 203) and ML-DSA (FIPS 204)
if [[ "$ENABLE_PQC" == "yes" ]]; then
    WOLFSSL_FLAGS+=" --enable-mlkem"        # ML-KEM (formerly Kyber) - FIPS 203
    WOLFSSL_FLAGS+=" --enable-mldsa"        # ML-DSA (formerly Dilithium) - FIPS 204
    WOLFSSL_FLAGS+=" --enable-experimental" # Required for some PQC TLS features
fi

# Additional features
WOLFSSL_FLAGS+=" --enable-all"
WOLFSSL_FLAGS+=" --enable-opensslextra"
WOLFSSL_FLAGS+=" --enable-opensslall"
WOLFSSL_FLAGS+=" --enable-curve25519"
WOLFSSL_FLAGS+=" --enable-ed25519"

# Disable problematic features
WOLFSSL_FLAGS+=" --disable-rpk"

# Debug mode
if [[ "$DEBUG_MODE" == "yes" ]]; then
    WOLFSSL_FLAGS+=" --enable-debug"
fi

log_info "Configure flags: $WOLFSSL_FLAGS"
log_info "Running configure..."
../configure $WOLFSSL_FLAGS

log_info "Building wolfSSL..."
make all -j$(nproc)

log_info "Installing wolfSSL..."
sudo make install
sudo ldconfig

log_info "=========================================="
log_info " wolfSSL installed successfully!"
log_info "=========================================="

# Show what's available
if [[ "$ENABLE_PQC" == "yes" ]]; then
    cat << EOF

Post-Quantum Algorithms Available:
==================================
Key Exchange (ML-KEM/Kyber):
  KYBER_LEVEL1, KYBER_LEVEL3, KYBER_LEVEL5
  P256_KYBER_LEVEL1, P384_KYBER_LEVEL3, P521_KYBER_LEVEL5

Digital Signatures (ML-DSA/Dilithium):
  DILITHIUM_LEVEL2, DILITHIUM_LEVEL3, DILITHIUM_LEVEL5

Note: ML-KEM requires DTLS 1.3
EOF
else
    cat << EOF

Classical Algorithms Available:
===============================
Key Exchange: ECDHE (P-256, P-384, P-521), X25519, DHE
Signatures:   RSA, ECDSA, Ed25519
EOF
fi

echo ""
log_info "Build directory: $WOLFSSL_BUILD_DIR"
