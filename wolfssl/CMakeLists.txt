#
# wolfssl/CMakeLists.txt
#
# Copyright (C) 2024-2025 Javier Blanco-Romero @fj-blanco (UC3M, QURSA project)
#
# CMake configuration for CoAP client with wolfSSL
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(libcoap_wolfssl_client)

zephyr_include_directories(include)

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_sources(app PRIVATE src/main.c src/wifi.c)
target_link_libraries(app PRIVATE coap-3)

message(STATUS "...............................................") 
message(STATUS "ZEPHYR_EXTRA_MODULES after Zephyr package: ${ZEPHYR_EXTRA_MODULES}")
message(STATUS "wolfSSL configuration enabled")
message(STATUS "...............................................")