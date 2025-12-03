#!/bin/bash
# =============================================================================
# build_libcoap.sh - Build libcoap with wolfSSL or OpenSSL backend
# =============================================================================
# Builds libcoap for CoAP server/client with DTLS support.
# Supports runtime PQC algorithm selection via environment variable.
#
# Usage:
#   ./build_libcoap.sh [OPTIONS]
#
# Options:
#   --backend <wolfssl|openssl>  DTLS backend (default: wolfssl)
#   --algorithm <alg>            Default key exchange algorithm
#   --install-dir <path>         Custom installation directory
#   --skip-clone                 Use existing libcoap directory
#   --clean                      Clean before building
#   -y, --yes                    Skip confirmation prompts
#   -h, --help                   Show this help
#
# Examples:
#   ./build_libcoap.sh                              # wolfSSL backend
#   ./build_libcoap.sh --backend openssl            # OpenSSL backend
#   ./build_libcoap.sh --algorithm P384_KYBER_LEVEL3  # With PQC default
#
# =============================================================================

set -e

# Configuration defaults
DTLS_BACKEND="wolfssl"
LIBCOAP_VERSION="develop"
DEFAULT_ALGORITHM=""
INSTALL_DIR=""
SKIP_CLONE="no"
DO_CLEAN="no"
AUTO_YES="no"

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

Build libcoap with DTLS support for CoAP server/client.

Options:
  --backend <wolfssl|openssl>  DTLS backend (default: wolfssl)
  --algorithm <alg>            Default key exchange algorithm for wolfSSL
  --install-dir <path>         Custom installation directory
  --skip-clone                 Use existing libcoap directory (don't clone)
  --clean                      Clean existing build before starting
  -y, --yes                    Skip confirmation prompts
  -h, --help                   Show this help message

Key Exchange Algorithms (wolfSSL backend only):
  Classical:    P-256, P-384, P-521, X25519
  Post-Quantum: KYBER_LEVEL1, KYBER_LEVEL3, KYBER_LEVEL5
  Hybrid:       P256_KYBER_LEVEL1, P384_KYBER_LEVEL3, P521_KYBER_LEVEL5

Runtime Algorithm Selection:
  With wolfSSL, you can override the algorithm at runtime:
  COAP_WOLFSSL_GROUPS=KYBER_LEVEL3 coap-client -m get coaps://server/resource

Examples:
  $0                                         # Basic wolfSSL build
  $0 --algorithm P384_KYBER_LEVEL3           # PQC default
  $0 --backend openssl                       # Use OpenSSL instead
  $0 --clean --skip-clone                    # Rebuild existing
EOF
    exit 0
}

# Parse arguments
while [[ "$#" -gt 0 ]]; do
    case "$1" in
        --backend)
            DTLS_BACKEND="$2"
            if [[ "$DTLS_BACKEND" != "wolfssl" && "$DTLS_BACKEND" != "openssl" ]]; then
                log_error "Invalid backend: $DTLS_BACKEND (use wolfssl or openssl)"
                exit 1
            fi
            shift 2
            ;;
        --algorithm)
            DEFAULT_ALGORITHM="$2"
            shift 2
            ;;
        --install-dir)
            INSTALL_DIR="$2"
            shift 2
            ;;
        --skip-clone)
            SKIP_CLONE="yes"
            shift
            ;;
        --clean)
            DO_CLEAN="yes"
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

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK_DIR="$(pwd)"
LIBCOAP_DIR="${WORK_DIR}/libcoap"

# Verify backend is installed
if [[ "$DTLS_BACKEND" == "wolfssl" ]]; then
    log_info "Checking wolfSSL installation..."
    if ! pkg-config --exists wolfssl 2>/dev/null; then
        log_error "wolfSSL not found! Run build_wolfssl.sh first."
        exit 1
    fi
    WOLFSSL_VERSION=$(pkg-config --modversion wolfssl 2>/dev/null || echo "unknown")
    log_info "Found wolfSSL $WOLFSSL_VERSION"
else
    log_info "Checking OpenSSL installation..."
    if ! pkg-config --exists openssl 2>/dev/null; then
        log_error "OpenSSL not found!"
        exit 1
    fi
    OPENSSL_VERSION=$(pkg-config --modversion openssl 2>/dev/null || echo "unknown")
    log_info "Found OpenSSL $OPENSSL_VERSION"
fi

log_info "=========================================="
log_info " libcoap Build Configuration"
log_info "=========================================="
log_info "DTLS Backend:      $DTLS_BACKEND"
log_info "Default Algorithm: ${DEFAULT_ALGORITHM:-<none>}"
log_info "Install Dir:       ${INSTALL_DIR:-system default}"
log_info "=========================================="

if [[ "$AUTO_YES" != "yes" ]]; then
    read -p "Continue? (y/n): " confirm
    [[ "$confirm" != "y" ]] && exit 0
fi

# Clean existing build
clean_build() {
    if [[ -d "$LIBCOAP_DIR" ]]; then
        log_info "Cleaning existing libcoap build..."
        cd "$LIBCOAP_DIR"
        make clean 2>/dev/null || true
        ./autogen.sh --clean 2>/dev/null || true
        sudo make uninstall 2>/dev/null || true
        cd "$WORK_DIR"
    fi
}

# Patch libcoap for runtime algorithm selection (wolfSSL only)
patch_runtime_groups() {
    local src_file="$LIBCOAP_DIR/src/coap_wolfssl.c"
    
    if [[ ! -f "$src_file" ]]; then
        log_warn "coap_wolfssl.c not found, skipping patch"
        return
    fi

    # Check if already patched
    if grep -q 'getenv("COAP_WOLFSSL_GROUPS")' "$src_file" 2>/dev/null; then
        log_info "libcoap already patched for runtime algorithm selection"
        return
    fi

    log_info "Patching libcoap for runtime algorithm selection..."
    
    # Patch the coap_set_user_prefs function to check environment variable
    sed -i '/^static void$/,/^}$/{ 
        /coap_set_user_prefs/,/^}$/{
            /#ifdef COAP_WOLFSSL_GROUPS/,/#endif/{
                s/wolfSSL_CTX_set1_groups_list(ctx, COAP_WOLFSSL_GROUPS)/{\
    const char *env_groups = getenv("COAP_WOLFSSL_GROUPS");\
    const char *groups = (env_groups \&\& *env_groups) ? env_groups : COAP_WOLFSSL_GROUPS;\
    if (groups \&\& *groups) {\
        coap_log_debug("Using wolfSSL groups: %s\\n", groups);\
        wolfSSL_CTX_set1_groups_list(ctx, groups);\
    }\
}/
            }
        }
    }' "$src_file" 2>/dev/null || true
}

if [[ "$DO_CLEAN" == "yes" ]]; then
    clean_build
fi

# Clone or use existing
if [[ "$SKIP_CLONE" != "yes" ]]; then
    log_info "Removing existing libcoap directory..."
    sudo rm -rf "$LIBCOAP_DIR"
    
    log_info "Cloning libcoap ($LIBCOAP_VERSION)..."
    git clone https://github.com/obgm/libcoap "$LIBCOAP_DIR"
    cd "$LIBCOAP_DIR"
    git checkout "$LIBCOAP_VERSION"
else
    if [[ ! -d "$LIBCOAP_DIR" ]]; then
        log_error "libcoap directory not found. Run without --skip-clone first."
        exit 1
    fi
    log_info "Using existing libcoap directory..."
fi

cd "$LIBCOAP_DIR"

# Apply runtime groups patch for wolfSSL
if [[ "$DTLS_BACKEND" == "wolfssl" ]]; then
    patch_runtime_groups
fi

# Generate build system
log_info "Running autogen..."
./autogen.sh

# Build configure command
CONFIGURE_OPTS=""
CONFIGURE_OPTS+=" --enable-dtls"
CONFIGURE_OPTS+=" --disable-manpages"
CONFIGURE_OPTS+=" --disable-doxygen"
CONFIGURE_OPTS+=" --enable-tests"

if [[ "$DTLS_BACKEND" == "wolfssl" ]]; then
    CONFIGURE_OPTS+=" --with-wolfssl"
else
    CONFIGURE_OPTS+=" --with-openssl"
fi

if [[ -n "$INSTALL_DIR" ]]; then
    CONFIGURE_OPTS+=" --prefix=$INSTALL_DIR"
fi

# Set CPPFLAGS for wolfSSL algorithm defaults
if [[ "$DTLS_BACKEND" == "wolfssl" ]]; then
    if [[ -n "$DEFAULT_ALGORITHM" ]]; then
        export CPPFLAGS="-DCOAP_WOLFSSL_GROUPS=\"\\\"$DEFAULT_ALGORITHM\\\"\" -DDTLS_V1_3_ONLY=1"
    else
        export CPPFLAGS="-DCOAP_WOLFSSL_GROUPS=\\\"\\\" -DDTLS_V1_3_ONLY=1"
    fi
    log_info "CPPFLAGS: $CPPFLAGS"
fi

log_info "Configure options: $CONFIGURE_OPTS"
./configure $CONFIGURE_OPTS

log_info "Building libcoap..."
make -j$(nproc)

log_info "Installing libcoap..."
sudo make install
sudo ldconfig

log_info "=========================================="
log_info " libcoap installed successfully!"
log_info "=========================================="

# Print usage info
if [[ "$DTLS_BACKEND" == "wolfssl" ]]; then
    cat << EOF

Usage with wolfSSL backend:
===========================

Start server:
  coap-server -A 0.0.0.0 -p 5684 \\
    -c certs/server_cert.pem \\
    -j certs/server_key.pem \\
    -C certs/ca_cert.pem

Test client (default algorithm):
  coap-client -m get coaps://localhost:5684/time

Test with specific algorithm (runtime selection):
  COAP_WOLFSSL_GROUPS=P384_KYBER_LEVEL3 coap-client -m get coaps://server:5684/time
  COAP_WOLFSSL_GROUPS=KYBER_LEVEL3 coap-client -m get coaps://server:5684/time
  COAP_WOLFSSL_GROUPS=P-384 coap-client -m get coaps://server:5684/time

EOF
else
    cat << EOF

Usage with OpenSSL backend:
===========================

Start server:
  coap-server -A 0.0.0.0 -p 5684 \\
    -c certs/server_cert.pem \\
    -j certs/server_key.pem \\
    -C certs/ca_cert.pem

Test client:
  coap-client -m get coaps://localhost:5684/time

EOF
fi
