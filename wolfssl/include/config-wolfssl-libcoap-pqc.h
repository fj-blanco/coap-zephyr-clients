/*
 * config-wolfssl-libcoap-pqc.h
 * wolfSSL configuration for libcoap ESP32 client with Post-Quantum Cryptography
 * 
 * This configuration enables ML-KEM (Kyber) key exchange for DTLS 1.3.
 * ML-KEM-768 is recommended for ESP32 (balance of security and performance).
 * 
 * Copyright (C) 2024-2025 Javier Blanco-Romero @fj-blanco (UC3M, QURSA project)
 * 
 * PQC Configuration Options:
 *   ENABLE_MLKEM         - Enable ML-KEM key exchange (requires DTLS 1.3)
 *   MLKEM_LEVEL          - Security level: 1 (512), 3 (768), 5 (1024)
 *   ENABLE_HYBRID_KEM    - Enable hybrid (ECC + ML-KEM) key exchange
 */

#ifndef WOLFSSL_SETTINGS_H
#define WOLFSSL_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WOLFSSL_USER_SETTINGS
#define WOLFSSL_USER_SETTINGS
#endif

/* ==========================================================================
 * PQC Configuration - Adjust based on your requirements
 * ========================================================================== */

/* Enable Post-Quantum ML-KEM (Kyber) support */
/* Uncomment to enable PQC key exchange */
// #define ENABLE_MLKEM

/* ML-KEM Security Level: 1=512, 3=768 (recommended), 5=1024 */
#define MLKEM_LEVEL 3

/* Enable Hybrid Key Exchange (ECC + ML-KEM) for transition security */
/* Recommended for production - protects against both classical and quantum attacks */
// #define ENABLE_HYBRID_KEM

/* ==========================================================================
 * Platform Configuration
 * ========================================================================== */

#ifndef WOLFSSL_ZEPHYR
#define WOLFSSL_ZEPHYR
#endif

/* Prevent wolfSSL from redefining min/max - Zephyr already defines them */
#define WOLFSSL_HAVE_MIN
#define WOLFSSL_HAVE_MAX

/* File system and I/O */
#define NO_STDIO_FILESYSTEM
#define NO_WRITEV
#define NO_DEV_RANDOM
#define HAVE_CUSTOM_RNG
#define NO_MAIN_DRIVER

/* Disable BIO file operations */
#define NO_BIO
#define WOLFSSL_NO_STDIO
#define NO_PWDBASED

/* Disable logging to files */
#define NO_ERROR_QUEUE
#define WOLFSSL_NO_STDIO_PRINTF

/* ==========================================================================
 * TLS/DTLS Configuration
 * ========================================================================== */

/* DTLS configuration */
#define WOLFSSL_DTLS
#define HAVE_SOCKADDR

#ifdef ENABLE_MLKEM
    /* ML-KEM requires TLS 1.3 / DTLS 1.3 */
    #define WOLFSSL_TLS13
    #define WOLFSSL_DTLS13
    #define HAVE_TLS13_KEYEXCHANGE
    #define HAVE_HKDF
#endif

/* TLS configuration */
#undef NO_TLS
#undef NO_WOLFSSL_CLIENT
#undef NO_WOLFSSL_SERVER

/* wolfSSL features for libcoap */
#define HAVE_TLS_EXTENSIONS
#define HAVE_SUPPORTED_CURVES
#define HAVE_SNI
#define HAVE_EXTENDED_MASTER

/* ==========================================================================
 * Post-Quantum Cryptography (ML-KEM / Kyber)
 * ========================================================================== */

#ifdef ENABLE_MLKEM
    /* Enable wolfSSL native ML-KEM implementation (no liboqs needed) */
    #define WOLFSSL_HAVE_MLKEM
    #define WOLFSSL_WC_MLKEM
    
    /* Required hash algorithms for ML-KEM */
    #define WOLFSSL_SHA3
    #define WOLFSSL_SHAKE128
    #define WOLFSSL_SHAKE256
    
    /* Enable experimental features */
    #define WOLFSSL_EXPERIMENTAL_SETTINGS
    
    /* Select ML-KEM parameter set based on security level */
    #if MLKEM_LEVEL == 1
        /* ML-KEM-512: NIST Level 1 (~AES-128) - Smallest, fastest */
        #define WOLFSSL_NO_ML_KEM_768
        #define WOLFSSL_NO_ML_KEM_1024
    #elif MLKEM_LEVEL == 3
        /* ML-KEM-768: NIST Level 3 (~AES-192) - Recommended balance */
        #define WOLFSSL_NO_ML_KEM_512
        #define WOLFSSL_NO_ML_KEM_1024
    #elif MLKEM_LEVEL == 5
        /* ML-KEM-1024: NIST Level 5 (~AES-256) - Highest security */
        #define WOLFSSL_NO_ML_KEM_512
        #define WOLFSSL_NO_ML_KEM_768
    #else
        #error "Invalid MLKEM_LEVEL: must be 1, 3, or 5"
    #endif
    
    /* Note: Actual key share selection is done in application code using:
     * wolfSSL_UseKeyShare(ssl, WOLFSSL_ML_KEM_768);
     * or for hybrid:
     * wolfSSL_UseKeyShare(ssl, WOLFSSL_P384_ML_KEM_768);
     */
#endif

/* ==========================================================================
 * Session Management
 * ========================================================================== */

#define SMALL_SESSION_CACHE

/* ==========================================================================
 * Certificate and X.509 Support
 * ========================================================================== */

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

/* ==========================================================================
 * OpenSSL Compatibility Layer
 * ========================================================================== */

#define OPENSSL_EXTRA
#define OPENSSL_ALL
#define WOLFSSL_CERT_EXT
#define WOLFSSL_MULTI_INSTALL_DIR

#define WOLFSSL_HMAC
#define HAVE_HMAC
#define WOLFSSL_KEY_GEN
#define WOLFSSL_CERT_REQ
#define WOLFSSL_ALT_NAMES
#define HAVE_OID_ENCODING
#define WOLFSSL_CERT_NAME_ALL

/* RSA functionality */
#define OPENSSL_EXTRA_X509_SMALL
#define WOLFSSL_CERT_GEN_CACHE

/* X.509 and certificate verification */
#define WOLFSSL_X509_NAME_AVAILABLE
#define HAVE_CRL
#define WOLFSSL_CRL_ALLOW_MISSING_CDP

/* Callback support */
#define HAVE_EX_DATA

/* ==========================================================================
 * Cryptographic Algorithms
 * ========================================================================== */

/* Hash algorithms */
#define WOLFSSL_SHA256
#define WOLFSSL_SHA384
#define WOLFSSL_SHA512
#define HAVE_HKDF

/* ECC - Required for hybrid key exchange */
#define HAVE_ECC
#define ECC_TIMING_RESISTANT
#define HAVE_ECC_VERIFY
#define HAVE_ECC_SIGN

#ifdef ENABLE_HYBRID_KEM
    /* Enable specific curves for hybrid key exchange */
    #define HAVE_ECC256     /* P-256 for P256_ML_KEM_512 */
    #define HAVE_ECC384     /* P-384 for P384_ML_KEM_768 */
    #define HAVE_ECC521     /* P-521 for P521_ML_KEM_1024 */
#endif

/* AES */
#define HAVE_AES
#define HAVE_AES_CBC
#define HAVE_AESGCM
#define WOLFSSL_AES_128
#define WOLFSSL_AES_256

/* RSA */
#undef NO_RSA
#define WC_RSA_BLINDING
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

/* ==========================================================================
 * Memory and Stack Optimization
 * ========================================================================== */

#define WOLFSSL_SMALL_STACK
#define NO_BENCH
#define WOLFSSL_NO_BENCH

/* 
 * IMPORTANT: ML-KEM requires larger stack sizes!
 * Ensure prj.conf has at least:
 *   CONFIG_MAIN_STACK_SIZE=16384
 *   CONFIG_HEAP_MEM_POOL_SIZE=65536
 */

/* ==========================================================================
 * Disabled Features
 * ========================================================================== */

#define NO_DSA
#define NO_RC4
#define NO_MD4
#define NO_MD5
#define NO_OLD_TLS

/* ==========================================================================
 * Math Library
 * ========================================================================== */

#define WOLFSSL_SP_MATH
#define WOLFSSL_SP_SMALL
#define WOLFSSL_HAVE_SP_ECC
#define WOLFSSL_HAVE_SP_RSA

/* ==========================================================================
 * Threading
 * ========================================================================== */

#define SINGLE_THREADED

/* ==========================================================================
 * Debugging
 * ========================================================================== */

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
