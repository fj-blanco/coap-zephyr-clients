# CoAP Zephyr Clients

**ESP32 Wi-Fi CoAP client** implementation using [libcoap](https://libcoap.net/) for [Zephyr](https://www.zephyrproject.org/) RTOS with [mbedTLS](https://mbed-tls.readthedocs.io/en/latest/) and wolfSSL DTLS backends. The client connects to Wi-Fi networks and communicates with the CoAP example server at `coap.me/hello` via its IP address (134.102.218.18) over UDP, as DNS resolution has not been tested. This implementation takes ideas from the [libcoap Zephyr examples](https://github.com/obgm/libcoap/tree/develop/examples/zephyr) (which are tested on `native_sim`), but is specifically adapted and tested for **ESP32 hardware**.

The mbedtls client is set to use a libcoap [fork](https://github.com/fj-blanco/libcoap/tree/zephyr_pr) in the [mbedtls/west.yml](mbedtls/west.yml) file. This fork extended libcoap's Zephyr support via [POSIX API](https://docs.zephyrproject.org/latest/services/portability/posix/index.html#posix-support), and this branch has been merged into libcoap `develop` through [PR #1704](https://github.com/obgm/libcoap/pull/1704). The wolfssl client is set to use this [branch](https://github.com/fj-blanco/libcoap/tree/zephyr_wolfssl_pr) of the fork in the [wolfssl/west.yml](wolfssl/west.yml) file, that has also been merged into `develop` with this [PR #1717](https://github.com/obgm/libcoap/pull/1717). So you can set the `revision` to `develop` for both clients in the `west.yml` file if you want to use the latest changes in libcoap.

## Tested Environment

This client has been succesfully tested with the following:

### Hardware

- [ESP32 DevKit-C v4 by Espressif](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32/esp32-devkitc/user_guide.html) (targeting `esp32_devkitc/esp32/procpu`)

### Software

These are the versions that have been tested:

- **Base OS**: Ubuntu 22.04.5 LTS (kernel 6.8.0-60-generic)
- **Python**: 3.12.11
- **Zephyr SDK**: 0.17.4
- **Zephyr**: v4.3.0
- **west**: 1.5.0
- **libcoap**: [v4.3.5-187](https://github.com/obgm/libcoap) (official, pinned to commit `0bf6a2d7`)
- **wolfSSL**: v5.8.4-stable

## Setup

### Python Environment & West

```bash
conda create -n coap-zephyr-clients python=3.12
conda activate coap-zephyr-clients
pip install -r requirements.txt  # Installs west
```

### Zephyr SDK

Initialize one of the backends (this will run `west init`, `west update`, and install Zephyr's Python requirements automatically):

```bash
./scripts/build.sh --backend mbedtls --init  # or --backend wolfssl
```

Install the SDK from within the backend directory:

```bash
cd mbedtls  # or cd wolfssl
west sdk install
```

### udev Rules

First find your SDK path with:

```bash
west sdk list
```

and then, if your path is, for example, `zephyr-sdk-0.17.0` run:

```bash
sudo cp ~/zephyr-sdk-0.17.0/sysroots/x86_64-pokysdk-linux/usr/share/openocd/contrib/60-openocd.rules /etc/udev/rules.d
sudo udevadm control --reload
```

## Configuration

The build script accepts the following command-line arguments:

Required:

- `--backend <wolfssl|mbedtls>`: TLS backend to use
- `--wifi-ssid <ssid>`: WiFi network name
- `--wifi-pass <password>`: WiFi password

Optional:

- `--coap-ip <ip>`: CoAP server IP address (default: 134.102.218.18 - coap.me)
- `--coap-path <path>`: CoAP server path (default: /hello)
- `--coap-port <port>`: CoAP server port (default: 5683)
- `--use-dtls`: Enable DTLS mode (uses `coaps://` scheme and port 5684)
- `--clean`: Clean build directory before building
- `--init`: Initialize workspace only

## Basic Usage

Build with wolfSSL backend:

```bash
./scripts/build.sh --backend wolfssl --wifi-ssid "your_ssid" --wifi-pass "your_password"
```

Build with mbedTLS backend:

```bash
./scripts/build.sh --backend mbedtls --wifi-ssid "your_ssid" --wifi-pass "your_password"
```

### Flash and Monitor

After building, flash and monitor from the backend directory:

```bash
cd wolfssl  # or cd mbedtls
west flash
west espressif monitor
```

The client will connect to the Wi-Fi network and send a CoAP request to [coap.me/hello](https://coap.me/hello). You should see the response in the monitor output.

## Testing with a local server

Install libcoap:

```bash
./install_libcoap.sh
```

### Over UDP

Run the local server:

```bash
 ./libcoap/build/bin/coap-server -A 0.0.0.0 -v 8 -V 7 -d 10
```

To ensure the server is running, you can test with a local client first:

```bash
./libcoap/build/bin/coap-client -m get coap://localhost/time
```

Then build the client with the local server configuration:

```bash
./scripts/build.sh --backend wolfssl --coap-ip "your_ip" --coap-path "/time" \
  --wifi-ssid "your_ssid" --wifi-pass "your_password"
```

Finally, flash and monitor the client:

```bash
cd wolfssl
west flash
west espressif monitor
```

You should see the client connecting to the local CoAP server and receiving the response.

### Over DTLS

Generate the certificates:

```bash
./generate_certs.sh           # ECC P-256 (default)
./generate_certs.sh -t ecc    # ECC P-256  
./generate_certs.sh -t rsa    # RSA 2048-bit
```

Run the local server with DTLS support:

```bash
./libcoap/build/bin/coap-server -A 0.0.0.0 -c ./certs/server.crt -j ./certs/server.key -n -v 8 -V 7 -d 10
```

You can again test with the local client with server certificate verification disabled (for self-signed certs):

```bash
./libcoap/build/bin/coap-client -m get coaps://localhost:5684/time -v 6 -n
```

or with the embedded client:

```bash
./scripts/build.sh --backend wolfssl --coap-ip "your_ip" --coap-path "/time" \
  --wifi-ssid "your_ssid" --wifi-pass "your_password" --use-dtls
```

Finally, flash and monitor the client:

```bash
cd wolfssl
west flash
west espressif monitor
```

## Server Setup with PQC Support

The `scripts/server/` folder contains unified scripts for building a CoAP server with optional **Post-Quantum Cryptography (PQC)** support. These scripts can be used to build wolfSSL and libcoap with ML-KEM (Kyber) key exchange and ML-DSA (Dilithium) certificate authentication.

### Requirements

- **wolfSSL v5.8.4-stable** or later (native ML-KEM/ML-DSA support, no liboqs needed)
- **OpenSSL 3.6.0** or later (native ML-DSA support for certificate generation)
- Standard build tools: `git`, `cmake`, `autotools`, `pkg-config`

### Building wolfSSL

```bash
# Classical cryptography only
./scripts/server/build_wolfssl.sh

# With PQC support (ML-KEM + ML-DSA)
./scripts/server/build_wolfssl.sh --pqc

# Custom version and install directory
./scripts/server/build_wolfssl.sh --pqc --version v5.8.4-stable --install-dir /opt/wolfssl
```

Available flags:
- `--pqc`: Enable ML-KEM (Kyber) and ML-DSA (Dilithium) support
- `--version <tag>`: wolfSSL version to build (default: v5.8.4-stable)
- `--install-dir <path>`: Installation directory (default: ./deps/wolfssl)
- `--build-dir <path>`: Build directory (default: ./deps/wolfssl-build)
- `--clean`: Clean build directory before building
- `--debug`: Enable debug symbols

### Building libcoap

```bash
# With wolfSSL backend (classical)
./scripts/server/build_libcoap.sh --backend wolfssl

# With wolfSSL backend and PQC support
./scripts/server/build_libcoap.sh --backend wolfssl --pqc

# Set default key exchange algorithm
./scripts/server/build_libcoap.sh --backend wolfssl --pqc --groups P384_KYBER_LEVEL3
```

Available flags:
- `--backend <wolfssl|openssl>`: TLS backend to use (default: wolfssl)
- `--pqc`: Enable PQC support with DTLS 1.3
- `--groups <algorithm>`: Default key exchange algorithm (only with --pqc)
- `--wolfssl-dir <path>`: wolfSSL installation directory
- `--install-dir <path>`: Installation directory (default: ./deps/libcoap)
- `--skip-clone`: Use existing libcoap source
- `--clean`: Clean build directory before building

### Generating Certificates

```bash
# RSA certificates (2048-bit)
./scripts/server/generate_certs.sh --type rsa

# ECC certificates (P-256)
./scripts/server/generate_certs.sh --type ecc

# ECC with P-384
./scripts/server/generate_certs.sh --type ecc --variant p384

# ECC with Ed25519
./scripts/server/generate_certs.sh --type ecc --variant ed25519

# ML-DSA certificates (ML-DSA-65, requires OpenSSL 3.6+)
./scripts/server/generate_certs.sh --type mldsa

# ML-DSA with specific security level
./scripts/server/generate_certs.sh --type mldsa --variant mldsa87

# Check OpenSSL version and capabilities
./scripts/server/generate_certs.sh --check-openssl
```

Available flags:
- `--type <rsa|ecc|mldsa>`: Certificate type (default: ecc)
- `--variant <variant>`: Algorithm variant
  - RSA: `rsa2048`, `rsa4096`
  - ECC: `p256`, `p384`, `ed25519`
  - ML-DSA: `mldsa44`, `mldsa65`, `mldsa87`
- `--output-dir <path>`: Output directory (default: ./certs)
- `--openssl-bin <path>`: OpenSSL binary path
- `--check-openssl`: Check OpenSSL version and capabilities

### Running the Server with PQC

After building wolfSSL and libcoap with PQC support:

```bash
# Run server with default algorithm
./deps/libcoap/bin/coap-server -A 0.0.0.0 -c ./certs/server_cert.pem \
  -j ./certs/server_key.pem -R ./certs/ca_cert.pem -n -v 8 -d 10

# Override key exchange algorithm at runtime
COAP_WOLFSSL_GROUPS=P384_KYBER_LEVEL3 ./deps/libcoap/bin/coap-server \
  -A 0.0.0.0 -c ./certs/server_cert.pem -j ./certs/server_key.pem \
  -R ./certs/ca_cert.pem -n -v 8 -d 10
```

### Supported Key Exchange Algorithms

When built with `--pqc`, the following algorithms are available via `COAP_WOLFSSL_GROUPS`:

| Algorithm | Description | Security Level |
|-----------|-------------|----------------|
| `KYBER_LEVEL1` | Pure ML-KEM-512 | NIST Level 1 |
| `KYBER_LEVEL3` | Pure ML-KEM-768 | NIST Level 3 |
| `KYBER_LEVEL5` | Pure ML-KEM-1024 | NIST Level 5 |
| `P256_KYBER_LEVEL1` | ECDH P-256 + ML-KEM-512 hybrid | Level 1 |
| `P384_KYBER_LEVEL3` | ECDH P-384 + ML-KEM-768 hybrid | Level 3 |
| `P521_KYBER_LEVEL5` | ECDH P-521 + ML-KEM-1024 hybrid | Level 5 |

### Quick Start: Full PQC Pipeline

```bash
# 1. Build wolfSSL with PQC
./scripts/server/build_wolfssl.sh --pqc

# 2. Build libcoap with wolfSSL and PQC
./scripts/server/build_libcoap.sh --backend wolfssl --pqc --groups P384_KYBER_LEVEL3

# 3. Generate ML-DSA certificates (requires OpenSSL 3.6+)
./scripts/server/generate_certs.sh --type mldsa --variant mldsa65

# 4. Run the server
./deps/libcoap/bin/coap-server -A 0.0.0.0 -c ./certs/server_cert.pem \
  -j ./certs/server_key.pem -R ./certs/ca_cert.pem -n -v 8 -d 10

# 5. Test with client (override algorithm if needed)
COAP_WOLFSSL_GROUPS=P384_KYBER_LEVEL3 ./deps/libcoap/bin/coap-client \
  -m get coaps://localhost:5684/time -v 6 -n
```

### Notes on PQC

- **ML-KEM (Kyber)** is used for key exchange (KEX) in TLS/DTLS 1.3
- **ML-DSA (Dilithium)** is used for certificate signatures
- KEX and signature algorithms are independent - you can use ML-KEM with RSA/ECC certificates
- wolfSSL 5.8.4+ has native PQC support (no liboqs dependency)
- OpenSSL 3.6+ has native ML-DSA support (no oqsprovider needed)
- The `COAP_WOLFSSL_GROUPS` environment variable allows runtime algorithm selection

## Contributing

Contributions are welcome! If you have suggestions for improvements or find bugs, please open an issue or submit a pull request.
