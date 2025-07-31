#
# mbedtls/CMakeLists.txt
#
# Copyright (C) 2024-2025 Javier Blanco-Romero @fj-blanco (UC3M, QURSA project)
#
# CMake configuration for CoAP client
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(libcoap_mbedtls_client)

# CoAP server configuration
set(COAP_SERVER_IP_VALUE $ENV{COAP_IP})
set(COAP_SERVER_PATH_VALUE $ENV{COAP_PATH})
set(COAP_SERVER_PORT_VALUE $ENV{COAP_PORT})

if(DEFINED COAP_IP)
    set(COAP_SERVER_IP_VALUE ${COAP_IP})
endif()

if(DEFINED COAP_PATH)
    set(COAP_SERVER_PATH_VALUE ${COAP_PATH})
endif()

if(DEFINED COAP_PORT)
    set(COAP_SERVER_PORT_VALUE ${COAP_PORT})
endif()

if(NOT COAP_SERVER_IP_VALUE)
    set(COAP_SERVER_IP_VALUE "134.102.218.18")
endif()

if(NOT COAP_SERVER_PATH_VALUE)
    set(COAP_SERVER_PATH_VALUE "/hello")
endif()

if(NOT COAP_SERVER_PORT_VALUE)
    set(COAP_SERVER_PORT_VALUE "5683")
endif()

# DTLS configuration
set(USE_DTLS_VALUE $ENV{USE_DTLS})
if(DEFINED USE_DTLS)
    set(USE_DTLS_VALUE ${USE_DTLS})
endif()

# Handle WiFi configuration
set(WIFI_SSID_VALUE $ENV{WIFI_SSID})
set(WIFI_PASS_VALUE $ENV{WIFI_PASS})

if(DEFINED WIFI_SSID)
    set(WIFI_SSID_VALUE ${WIFI_SSID})
endif()

if(DEFINED WIFI_PASS)
    set(WIFI_PASS_VALUE ${WIFI_PASS})
endif()

if(NOT WIFI_SSID_VALUE)
    set(WIFI_SSID_VALUE "WIFI_SSID_NOT_SET")
endif()

if(NOT WIFI_PASS_VALUE)
    set(WIFI_PASS_VALUE "WIFI_PASS_NOT_SET")
endif()

zephyr_include_directories(include)

if(CONFIG_MBEDTLS)
    target_link_libraries(coap-3 PUBLIC mbedTLS)
endif()

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_sources(app PRIVATE src/main.c src/wifi.c)

target_compile_definitions(app PRIVATE
    COAP_SERVER_IP="${COAP_SERVER_IP_VALUE}"
    COAP_SERVER_PATH="${COAP_SERVER_PATH_VALUE}"
    COAP_SERVER_PORT=${COAP_SERVER_PORT_VALUE}
    WIFI_SSID="${WIFI_SSID_VALUE}"
    WIFI_PASS="${WIFI_PASS_VALUE}"
)

message(STATUS "...............................................")
message(STATUS "ZEPHYR_EXTRA_MODULES after Zephyr package: ${ZEPHYR_EXTRA_MODULES}")
message(STATUS "CoAP Server: ${COAP_SERVER_IP_VALUE}${COAP_SERVER_PATH_VALUE}:${COAP_SERVER_PORT_VALUE}")
message(STATUS "WiFi Network: ${WIFI_SSID_VALUE}")
message(STATUS "...............................................")