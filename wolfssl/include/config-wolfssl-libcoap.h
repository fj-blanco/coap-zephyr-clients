/*
 * config-wolfssl-libcoap.h
 * wolfSSL configuration for libcoap ESP32 client
 * 
 * Copyright (C) 2024-2025 Javier Blanco-Romero @fj-blanco (UC3M, QURSA project)
 */

#ifndef WOLFSSL_SETTINGS_H
#define WOLFSSL_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WOLFSSL_USER_SETTINGS
#define WOLFSSL_USER_SETTINGS
#endif

/* Platform specific */
#ifndef WOLFSSL_ZEPHYR
#define WOLFSSL_ZEPHYR
#endif

/* File system and I/O */
#define NO_STDIO_FILESYSTEM
#define NO_WRITEV
#define NO_DEV_RANDOM
#define HAVE_CUSTOM_RNG
#define NO_MAIN_DRIVER

/* Disable BIO file operations that cause the fprintf/vfprintf errors */
#define NO_BIO
#define WOLFSSL_NO_STDIO
#define NO_PWDBASED

/* Disable logging to files - this fixes the fprintf errors */
#define NO_ERROR_QUEUE
#define WOLFSSL_NO_STDIO_PRINTF

/* DTLS configuration */
#define WOLFSSL_DTLS
#define HAVE_SOCKADDR

/* TLS configuration */
#undef NO_TLS
#undef NO_WOLFSSL_CLIENT
#undef NO_WOLFSSL_SERVER

/* wolfSSL features for libcoap */
#define HAVE_TLS_EXTENSIONS
#define HAVE_SUPPORTED_CURVES
#define HAVE_SNI
#define HAVE_EXTENDED_MASTER

/* Session management */
#define SMALL_SESSION_CACHE

/* Certificate and X.509 support */
#undef NO_CERTS
#define WOLFSSL_CERT_VERIFY
#define WOLFSSL_CERT_GEN
#define KEEP_PEER_CERT

/* ASN.1 support */
#define WOLFSSL_ASN_TEMPLATE

/* Buffer-based certificate loading */
#define WOLFSSL_DER_LOAD
#define WOLFSSL_PEM_TO_DER

/* Error handling */
#undef NO_ERROR_STRINGS
#define WOLFSSL_ERROR_CODE_OPENSSL

/* OpenSSL compatibility layer */
#define OPENSSL_EXTRA
#define OPENSSL_ALL
#define WOLFSSL_CERT_EXT
#define WOLFSSL_MULTI_INSTALL_DIR

/* Additional OpenSSL compatibility */
#define WOLFSSL_HMAC
#define HAVE_HMAC
#define WOLFSSL_KEY_GEN
#define WOLFSSL_CERT_REQ
#define WOLFSSL_ALT_NAMES
#define HAVE_OID_ENCODING
#define WOLFSSL_CERT_NAME_ALL

/* RSA functionality */
#define OPENSSL_EXTRA_X509_SMALL
#define WOLFSSL_CERT_EXT
#define WOLFSSL_CERT_GEN_CACHE

/* X.509 and certificate verification */
#define WOLFSSL_X509_NAME_AVAILABLE
#define HAVE_CRL
#define WOLFSSL_CRL_ALLOW_MISSING_CDP

/* Remove the problematic RSA_PUBLIC_ONLY - we need full RSA support for libcoap */
/* #define WOLFSSL_RSA_PUBLIC_ONLY */

/* Callback support */
#define HAVE_EX_DATA

/* Hash algorithms */
#define WOLFSSL_SHA256
#define WOLFSSL_SHA384
#define WOLFSSL_SHA512
#define HAVE_HKDF

/* ECC */
#define HAVE_ECC
#define ECC_TIMING_RESISTANT
#define HAVE_ECC_VERIFY
#define HAVE_ECC_SIGN

/* AES */
#define HAVE_AES
#define HAVE_AES_CBC
#define HAVE_AESGCM
#define WOLFSSL_AES_128
#define WOLFSSL_AES_256

/* RSA */
#undef NO_RSA
#define WC_RSA_BLINDING

/* RSA key functions */
#define WOLFSSL_KEY_GEN
#define WOLFSSL_RSA_KEY_CHECK
#define WOLFSSL_RSA_VERIFY_INLINE

/* DER conversion */
#define WOLFSSL_DER_TO_PEM
#define HAVE_PKCS8

/* Full RSA support */
#define WOLFSSL_RSA_GENERATE_EXTRA

/* HMAC */
#undef NO_HMAC

/* DH */
#undef NO_DH
#define WOLFSSL_DH_CONST

/* Random number generation */
#define HAVE_HASHDRBG
#define WC_RNG_SEED_CB

/* Memory and stack optimization */
#define WOLFSSL_SMALL_STACK
#define NO_BENCH
#define WOLFSSL_NO_BENCH

/* Disabled features */
#define NO_DSA
#define NO_RC4
#define NO_MD4
#define NO_MD5
#define NO_OLD_TLS

/* Math library */
#define WOLFSSL_SP_MATH
#define WOLFSSL_SP_SMALL
#define WOLFSSL_HAVE_SP_ECC
#define WOLFSSL_HAVE_SP_RSA

/* Threading */
#define SINGLE_THREADED

/* Debugging - prevents redefinition warnings */
#ifdef CONFIG_WOLFSSL_DEBUG
    #ifndef DEBUG_WOLFSSL
        #define DEBUG_WOLFSSL
    #endif
    #undef NO_ERROR_STRINGS
#else
    #undef DEBUG_WOLFSSL
    #define NO_ERROR_STRINGS
#endif

#ifdef __cplusplus
}
#endif

#endif /* WOLFSSL_SETTINGS_H */