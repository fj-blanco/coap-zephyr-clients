#!/bin/bash
# =============================================================================
# generate_certs.sh - Generate TLS/DTLS Certificates
# =============================================================================
# Generates certificates for CoAP over DTLS. Supports classical (RSA, ECC)
# and post-quantum (ML-DSA) algorithms.
#
# Usage:
#   ./generate_certs.sh [OPTIONS]
#
# Options:
#   --type <type>        Certificate type (default: ecc)
#   --output <dir>       Output directory (default: ./certs)
#   --install-openssl    Install OpenSSL 3.6 for ML-DSA support
#   -h, --help           Show this help
#
# Certificate Types:
#   rsa          RSA 2048-bit
#   ecc          ECDSA P-256 (default)
#   ed25519      Ed25519
#   ml-dsa-44    ML-DSA-44 (NIST Level 2) - requires OpenSSL 3.5+
#   ml-dsa-65    ML-DSA-65 (NIST Level 3) - requires OpenSSL 3.5+
#   ml-dsa-87    ML-DSA-87 (NIST Level 5) - requires OpenSSL 3.5+
#   all          Generate all supported types
#
# Examples:
#   ./generate_certs.sh                      # ECC P-256 (default)
#   ./generate_certs.sh --type rsa           # RSA 2048
#   ./generate_certs.sh --type ml-dsa-65     # Post-quantum ML-DSA
#   ./generate_certs.sh --type all           # All certificate types
#
# =============================================================================

set -e

# Configuration
CERT_TYPE="ecc"
OUTPUT_DIR="./certs"
INSTALL_OPENSSL="no"
OPENSSL_MIN_VERSION="3.5.0"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }
log_step() { echo -e "${BLUE}[STEP]${NC} $1"; }

usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Generate certificates for CoAP over DTLS.

Options:
  --type <type>        Certificate type to generate (default: ecc)
  --output <dir>       Output directory (default: ./certs)
  --install-openssl    Build and install OpenSSL 3.6 (required for ML-DSA)
  -h, --help           Show this help message

Certificate Types:
  rsa          RSA 2048-bit
  ecc          ECDSA P-256 (default)
  ed25519      Ed25519
  ml-dsa-44    ML-DSA-44 (NIST Level 2, ~AES-128)
  ml-dsa-65    ML-DSA-65 (NIST Level 3, ~AES-192) [recommended PQC]
  ml-dsa-87    ML-DSA-87 (NIST Level 5, ~AES-256)
  all          Generate all supported certificate types

Requirements:
  - Classical certs (rsa, ecc, ed25519): Any OpenSSL version
  - PQC certs (ml-dsa-*): OpenSSL 3.5+ (use --install-openssl)

Examples:
  $0                                    # ECC P-256 certificates
  $0 --type rsa                         # RSA certificates
  $0 --type ml-dsa-65                   # Post-quantum certificates
  $0 --type all --output ./my-certs     # All types to custom dir
  $0 --install-openssl --type ml-dsa-65 # Install OpenSSL 3.6 + generate
EOF
    exit 0
}

# Parse arguments
while [[ "$#" -gt 0 ]]; do
    case "$1" in
        --type)
            CERT_TYPE="$2"
            shift 2
            ;;
        --output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --install-openssl)
            INSTALL_OPENSSL="yes"
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

# Validate certificate type
VALID_TYPES="rsa ecc ed25519 ml-dsa-44 ml-dsa-65 ml-dsa-87 all"
if [[ ! " $VALID_TYPES " =~ " $CERT_TYPE " ]]; then
    log_error "Invalid certificate type: $CERT_TYPE"
    log_error "Valid types: $VALID_TYPES"
    exit 1
fi

# Version comparison
version_gte() {
    printf '%s\n%s' "$2" "$1" | sort -V -C
}

# Check OpenSSL version
check_openssl() {
    local openssl_cmd="${1:-openssl}"
    
    if ! command -v "$openssl_cmd" &> /dev/null; then
        return 1
    fi
    
    local version=$("$openssl_cmd" version 2>/dev/null | awk '{print $2}')
    
    if version_gte "$version" "$OPENSSL_MIN_VERSION"; then
        return 0
    fi
    
    return 1
}

# Install OpenSSL 3.6
install_openssl() {
    local INSTALL_DIR="/opt/openssl-3.6"
    
    log_info "Installing OpenSSL 3.6 to $INSTALL_DIR..."
    
    sudo apt-get update -qq
    sudo apt-get install -y -qq build-essential checkinstall zlib1g-dev wget
    
    local BUILD_DIR="/tmp/openssl-build"
    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    log_info "Downloading OpenSSL 3.6.0..."
    wget -q https://github.com/openssl/openssl/releases/download/openssl-3.6.0/openssl-3.6.0.tar.gz
    tar xzf openssl-3.6.0.tar.gz
    cd openssl-3.6.0
    
    log_info "Configuring OpenSSL..."
    ./config --prefix="$INSTALL_DIR" --openssldir="$INSTALL_DIR/ssl"
    
    log_info "Building OpenSSL (this may take a while)..."
    make -j$(nproc)
    
    log_info "Installing OpenSSL..."
    sudo make install_sw
    
    # Create symlink
    sudo ln -sf "$INSTALL_DIR/bin/openssl" /usr/local/bin/openssl-pqc
    
    # Update library path
    echo "$INSTALL_DIR/lib64" | sudo tee /etc/ld.so.conf.d/openssl-3.6.conf > /dev/null
    sudo ldconfig
    
    log_info "OpenSSL 3.6 installed successfully"
    
    cd "$OLDPWD"
    rm -rf "$BUILD_DIR"
    
    OPENSSL_CMD="$INSTALL_DIR/bin/openssl"
}

# Find suitable OpenSSL
find_openssl() {
    # Check for OpenSSL 3.6 installation
    if [[ -x "/opt/openssl-3.6/bin/openssl" ]]; then
        if check_openssl "/opt/openssl-3.6/bin/openssl"; then
            OPENSSL_CMD="/opt/openssl-3.6/bin/openssl"
            return 0
        fi
    fi
    
    # Check for openssl-pqc symlink
    if command -v openssl-pqc &> /dev/null; then
        if check_openssl "openssl-pqc"; then
            OPENSSL_CMD="openssl-pqc"
            return 0
        fi
    fi
    
    # Check system OpenSSL
    if check_openssl "openssl"; then
        OPENSSL_CMD="openssl"
        return 0
    fi
    
    # System OpenSSL exists but may be older
    if command -v openssl &> /dev/null; then
        OPENSSL_CMD="openssl"
        return 0  # Will fail for ML-DSA but works for classical
    fi
    
    return 1
}

# Create configuration files
create_configs() {
    mkdir -p "$OUTPUT_DIR"
    
    cat > "$OUTPUT_DIR/root.conf" << 'EOF'
[ req ]
prompt                 = no
distinguished_name     = req_distinguished_name

[ req_distinguished_name ]
C                      = ES
ST                     = Madrid
L                      = Getafe
O                      = UC3M QURSA
OU                     = Research
CN                     = Root CA
emailAddress           = admin@qursa.uc3m.es

[ ca_extensions ]
subjectKeyIdentifier   = hash
authorityKeyIdentifier = keyid:always,issuer:always
keyUsage               = critical, keyCertSign, cRLSign
basicConstraints       = critical, CA:true
EOF

    cat > "$OUTPUT_DIR/entity.conf" << 'EOF'
[ req ]
prompt                 = no
distinguished_name     = req_distinguished_name

[ req_distinguished_name ]
C                      = ES
ST                     = Madrid
L                      = Getafe
O                      = UC3M QURSA
OU                     = Research
CN                     = CoAP Server
emailAddress           = server@qursa.uc3m.es

[ x509v3_extensions ]
subjectAltName         = IP:127.0.0.1,DNS:localhost
subjectKeyIdentifier   = hash
authorityKeyIdentifier = keyid:always,issuer:always
keyUsage               = critical, digitalSignature
extendedKeyUsage       = critical, serverAuth, clientAuth
basicConstraints       = critical, CA:false
EOF
}

# Generate RSA certificates
generate_rsa() {
    local dir="$OUTPUT_DIR/rsa"
    
    log_step "Generating RSA 2048 certificates..."
    mkdir -p "$dir"
    
    # CA
    $OPENSSL_CMD genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -out "$dir/ca_key.pem"
    $OPENSSL_CMD req -x509 -new -config "$OUTPUT_DIR/root.conf" -extensions ca_extensions \
        -days 3650 -key "$dir/ca_key.pem" -out "$dir/ca_cert.pem"
    
    # Server
    $OPENSSL_CMD genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -out "$dir/server_key.pem"
    $OPENSSL_CMD req -new -config "$OUTPUT_DIR/entity.conf" -key "$dir/server_key.pem" -out "$dir/server_csr.pem"
    $OPENSSL_CMD x509 -req -in "$dir/server_csr.pem" -CA "$dir/ca_cert.pem" -CAkey "$dir/ca_key.pem" \
        -CAcreateserial -extfile "$OUTPUT_DIR/entity.conf" -extensions x509v3_extensions \
        -days 365 -out "$dir/server_cert.pem"
    
    chmod 600 "$dir"/*_key.pem
    chmod 644 "$dir"/*_cert.pem
    rm -f "$dir/server_csr.pem" "$dir/ca_cert.srl"
    
    $OPENSSL_CMD verify -CAfile "$dir/ca_cert.pem" "$dir/server_cert.pem"
    log_info "RSA certificates created in $dir"
}

# Generate ECC P-256 certificates
generate_ecc() {
    local dir="$OUTPUT_DIR/ecc"
    
    log_step "Generating ECC P-256 certificates..."
    mkdir -p "$dir"
    
    # CA
    $OPENSSL_CMD genpkey -algorithm EC -pkeyopt ec_paramgen_curve:prime256v1 -out "$dir/ca_key.pem"
    $OPENSSL_CMD req -x509 -new -config "$OUTPUT_DIR/root.conf" -extensions ca_extensions \
        -days 3650 -key "$dir/ca_key.pem" -out "$dir/ca_cert.pem"
    
    # Server
    $OPENSSL_CMD genpkey -algorithm EC -pkeyopt ec_paramgen_curve:prime256v1 -out "$dir/server_key.pem"
    $OPENSSL_CMD req -new -config "$OUTPUT_DIR/entity.conf" -key "$dir/server_key.pem" -out "$dir/server_csr.pem"
    $OPENSSL_CMD x509 -req -in "$dir/server_csr.pem" -CA "$dir/ca_cert.pem" -CAkey "$dir/ca_key.pem" \
        -CAcreateserial -extfile "$OUTPUT_DIR/entity.conf" -extensions x509v3_extensions \
        -days 365 -out "$dir/server_cert.pem"
    
    chmod 600 "$dir"/*_key.pem
    chmod 644 "$dir"/*_cert.pem
    rm -f "$dir/server_csr.pem" "$dir/ca_cert.srl"
    
    $OPENSSL_CMD verify -CAfile "$dir/ca_cert.pem" "$dir/server_cert.pem"
    log_info "ECC P-256 certificates created in $dir"
}

# Generate Ed25519 certificates
generate_ed25519() {
    local dir="$OUTPUT_DIR/ed25519"
    
    log_step "Generating Ed25519 certificates..."
    mkdir -p "$dir"
    
    # CA
    $OPENSSL_CMD genpkey -algorithm Ed25519 -out "$dir/ca_key.pem"
    $OPENSSL_CMD req -x509 -new -config "$OUTPUT_DIR/root.conf" -extensions ca_extensions \
        -days 3650 -key "$dir/ca_key.pem" -out "$dir/ca_cert.pem"
    
    # Server
    $OPENSSL_CMD genpkey -algorithm Ed25519 -out "$dir/server_key.pem"
    $OPENSSL_CMD req -new -config "$OUTPUT_DIR/entity.conf" -key "$dir/server_key.pem" -out "$dir/server_csr.pem"
    $OPENSSL_CMD x509 -req -in "$dir/server_csr.pem" -CA "$dir/ca_cert.pem" -CAkey "$dir/ca_key.pem" \
        -CAcreateserial -extfile "$OUTPUT_DIR/entity.conf" -extensions x509v3_extensions \
        -days 365 -out "$dir/server_cert.pem"
    
    chmod 600 "$dir"/*_key.pem
    chmod 644 "$dir"/*_cert.pem
    rm -f "$dir/server_csr.pem" "$dir/ca_cert.srl"
    
    $OPENSSL_CMD verify -CAfile "$dir/ca_cert.pem" "$dir/server_cert.pem"
    log_info "Ed25519 certificates created in $dir"
}

# Generate ML-DSA certificates (OpenSSL 3.5+ required)
generate_ml_dsa() {
    local level="$1"  # 44, 65, or 87
    local algo="ML-DSA-$level"
    local dir="$OUTPUT_DIR/ml-dsa-$level"
    
    # Check for OpenSSL 3.5+
    local version=$($OPENSSL_CMD version 2>/dev/null | awk '{print $2}')
    if ! version_gte "$version" "$OPENSSL_MIN_VERSION"; then
        log_error "$algo requires OpenSSL 3.5+, found $version"
        log_error "Run with --install-openssl to install OpenSSL 3.6"
        return 1
    fi
    
    log_step "Generating $algo certificates..."
    mkdir -p "$dir"
    
    # CA
    log_info "  Creating CA key..."
    $OPENSSL_CMD genpkey -algorithm "$algo" -out "$dir/ca_key.pem"
    
    log_info "  Creating CA certificate..."
    $OPENSSL_CMD req -x509 -new \
        -config "$OUTPUT_DIR/root.conf" \
        -extensions ca_extensions \
        -days 3650 \
        -key "$dir/ca_key.pem" \
        -out "$dir/ca_cert.pem"
    
    # Server
    log_info "  Creating server key..."
    $OPENSSL_CMD genpkey -algorithm "$algo" -out "$dir/server_key.pem"
    
    log_info "  Creating server CSR..."
    $OPENSSL_CMD req -new \
        -config "$OUTPUT_DIR/entity.conf" \
        -key "$dir/server_key.pem" \
        -out "$dir/server_csr.pem"
    
    log_info "  Signing server certificate..."
    $OPENSSL_CMD x509 -req \
        -in "$dir/server_csr.pem" \
        -CA "$dir/ca_cert.pem" \
        -CAkey "$dir/ca_key.pem" \
        -CAcreateserial \
        -extfile "$OUTPUT_DIR/entity.conf" \
        -extensions x509v3_extensions \
        -days 365 \
        -out "$dir/server_cert.pem"
    
    chmod 600 "$dir"/*_key.pem
    chmod 644 "$dir"/*_cert.pem
    rm -f "$dir/server_csr.pem" "$dir/ca_cert.srl"
    
    log_info "  Verifying certificate chain..."
    $OPENSSL_CMD verify -CAfile "$dir/ca_cert.pem" "$dir/server_cert.pem"
    
    log_info "$algo certificates created in $dir"
}

# Main execution
main() {
    log_info "=========================================="
    log_info " Certificate Generator"
    log_info "=========================================="
    log_info "Type:   $CERT_TYPE"
    log_info "Output: $OUTPUT_DIR"
    log_info "=========================================="
    
    # Install OpenSSL if requested
    if [[ "$INSTALL_OPENSSL" == "yes" ]]; then
        install_openssl
    fi
    
    # Find suitable OpenSSL
    if ! find_openssl; then
        log_error "OpenSSL not found!"
        exit 1
    fi
    
    log_info "Using OpenSSL: $OPENSSL_CMD"
    $OPENSSL_CMD version
    
    # Create configuration files
    create_configs
    
    # Generate requested certificates
    case "$CERT_TYPE" in
        rsa)
            generate_rsa
            ;;
        ecc)
            generate_ecc
            ;;
        ed25519)
            generate_ed25519
            ;;
        ml-dsa-44)
            generate_ml_dsa 44
            ;;
        ml-dsa-65)
            generate_ml_dsa 65
            ;;
        ml-dsa-87)
            generate_ml_dsa 87
            ;;
        all)
            generate_rsa
            generate_ecc
            generate_ed25519
            
            # Try ML-DSA if OpenSSL supports it
            local version=$($OPENSSL_CMD version 2>/dev/null | awk '{print $2}')
            if version_gte "$version" "$OPENSSL_MIN_VERSION"; then
                generate_ml_dsa 44
                generate_ml_dsa 65
                generate_ml_dsa 87
            else
                log_warn "Skipping ML-DSA certificates (requires OpenSSL 3.5+)"
                log_warn "Run with --install-openssl to enable ML-DSA"
            fi
            ;;
    esac
    
    # Cleanup config files
    rm -f "$OUTPUT_DIR/root.conf" "$OUTPUT_DIR/entity.conf"
    
    log_info "=========================================="
    log_info " Certificates generated successfully!"
    log_info "=========================================="
    
    # Print summary
    echo ""
    echo "Generated certificates:"
    find "$OUTPUT_DIR" -name "*.pem" -type f 2>/dev/null | sort
    
    echo ""
    echo "Usage with CoAP server:"
    echo "  coap-server -A 0.0.0.0 -p 5684 \\"
    echo "    -c $OUTPUT_DIR/<type>/server_cert.pem \\"
    echo "    -j $OUTPUT_DIR/<type>/server_key.pem \\"
    echo "    -C $OUTPUT_DIR/<type>/ca_cert.pem"
}

main
