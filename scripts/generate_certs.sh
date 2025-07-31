#!/bin/bash

set -e

# Default to ECC P-256
CERT_TYPE="ecc"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -t|--type)
            CERT_TYPE="$2"
            shift 2
            ;;
        *)
            shift
            ;;
    esac
done

mkdir -p ./certs

if [[ "$CERT_TYPE" == "rsa" ]]; then
    # RSA 2048-bit
    echo "Generating RSA 2048-bit certificate..."
    cat > cert_config.conf << 'EOF'
[ req ]
prompt                 = no
distinguished_name     = req_distinguished_name

[ req_distinguished_name ]
CN                     = localhost

[ x509v3_extensions ]
subjectAltName = IP:127.0.0.1,DNS:localhost
keyUsage               = digitalSignature, keyEncipherment
extendedKeyUsage       = serverAuth
basicConstraints       = CA:false
EOF

    openssl genpkey -algorithm RSA -out ./certs/server.key -pkeyopt rsa_keygen_bits:2048
    openssl req -x509 -config cert_config.conf -extensions x509v3_extensions \
        -days 365 -key ./certs/server.key -out ./certs/server.crt
else
    # ECC P-256 (default)
    echo "Generating ECC P-256 certificate..."
    cat > cert_config.conf << 'EOF'
[ req ]
prompt                 = no
distinguished_name     = req_distinguished_name

[ req_distinguished_name ]
CN                     = localhost

[ x509v3_extensions ]
subjectAltName = IP:127.0.0.1,DNS:localhost
keyUsage               = digitalSignature, keyAgreement
extendedKeyUsage       = serverAuth
basicConstraints       = CA:false
EOF

    openssl genpkey -algorithm EC -out ./certs/server.key -pkeyopt ec_paramgen_curve:P-256
    openssl req -x509 -config cert_config.conf -extensions x509v3_extensions \
        -days 365 -key ./certs/server.key -out ./certs/server.crt
fi

chmod 600 ./certs/server.key
chmod 644 ./certs/server.crt
rm -f cert_config.conf

echo "Certificates ready in ./certs/"