# CoAP Zephyr Clients

**ESP32 Wi-Fi CoAP client** implementation using [libcoap](https://libcoap.net/) for [Zephyr](https://www.zephyrproject.org/) RTOS with [mbedTLS](https://mbed-tls.readthedocs.io/en/latest/) security. The client connects to Wi-Fi networks and communicates with the CoAP example server at `coap.me/hello` via its IP address (134.102.218.18) over UDP, as DNS resolution has not been tested. This implementation takes ideas from the [libcoap Zephyr examples](https://github.com/obgm/libcoap/tree/develop/examples/zephyr) (which are tested on `native_sim`), but is specifically adapted and tested for **real ESP32 hardware**.

The client uses a libcoap [fork](https://github.com/fj-blanco/libcoap/tree/zephyr_wolfssl). Previously, the fork extended libcoap's Zephyr support via [POSIX API](https://docs.zephyrproject.org/latest/services/portability/posix/index.html#posix-support), and this branch has been merged into libcoap `develop` through [PR #1704](https://github.com/obgm/libcoap/pull/1704). The libcoap fork used in this project now specifically extends Zephyr support for wolfSSL backend.

## Tested Environment

This client has been succesfully tested with the following:

### Hardware

- [ESP32 DevKit-C v4 by Espressif](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32/esp32-devkitc/user_guide.html) (targeting `esp32_devkitc_wroom/esp32/procpu`)

### Software

- **Base OS**: Ubuntu 22.04.5 LTS (kernel 6.8.0-60-generic)
- **Python**: 3.12.11
- **Zephyr**: v4.1.0
- **west**: 1.4.0
- **libcoap**: This [fork](https://github.com/fj-blanco/libcoap/tree/zephyr_wolfssl) with minimal changes extending libcoap's Zephyr support using the POSIX API
- **wolfSSL**: v5.8.2-stable

## Setup

### Python Environment & West

```bash
conda create -n coap-zephyr-clients python=3.12
conda activate coap-zephyr-clients
pip install -r requirements.txt  # Installs west~=1.4.0
```

### Zephyr SDK

Initialize Zephyr workspace:

### Initialize wolfSSL client

```bash
./scripts/build_wolfssl_client.sh init
```

Install the SDK:

```bash
cd wolfssl
west sdk install
```

### Initialize mbedTLS client

```bash
./scripts/build_mbedtls_client.sh init
```

Install the SDK:

```bash
cd mbedtls
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

Edit Wi-Fi credentials in `mbedtls/src/wifi.c`:

```c
#define WIFI_SSID "your_ssid"
#define WIFI_PASS "your_password"
```

## Build & Flash

### wolfSSL client

Build the client:

```bash
./scripts/build_wolfssl_client.sh
```

Flash and monitor the board:

```bash
cd wolfssl
west flash
west espressif monitor
```

### mbedTLS client

Build the client:

```bash
./scripts/build_mbedtls_client.sh
```

Flash and monitor the board:

```bash
cd mbedtls
west flash
west espressif monitor
```
