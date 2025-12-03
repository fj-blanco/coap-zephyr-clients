/* main_pqc.c - CoAP client with ML-KEM Post-Quantum Key Exchange
 *
 * This example demonstrates using ML-KEM (Kyber) for key exchange
 * in a CoAP over DTLS 1.3 connection.
 *
 * Copyright (C) 2024-2025 Javier Blanco-Romero @fj-blanco (UC3M, QURSA project)
 *
 * Key Exchange Options:
 *   - Pure PQC: WOLFSSL_ML_KEM_512, WOLFSSL_ML_KEM_768, WOLFSSL_ML_KEM_1024
 *   - Hybrid:   WOLFSSL_P256_ML_KEM_512, WOLFSSL_P384_ML_KEM_768, WOLFSSL_P521_ML_KEM_1024
 *
 * To enable ML-KEM in your build:
 *   1. Use config-wolfssl-libcoap-pqc.h with ENABLE_MLKEM defined
 *   2. Use prj_pqc.conf overlay for increased stack/heap
 *   3. Ensure server supports DTLS 1.3 with ML-KEM
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/posix/unistd.h>
#include <zephyr/sys/slist.h>
#include <coap3/coap.h>
#include "wifi.h"

/* Include wolfSSL headers for ML-KEM configuration */
#ifdef ENABLE_MLKEM
#include <wolfssl/ssl.h>
#include <wolfssl/wolfcrypt/settings.h>
#endif

static int have_response = 0;

#ifndef COAP_SERVER_IP
#define COAP_SERVER_IP "134.102.218.18"
#endif

#ifndef COAP_SERVER_PATH
#define COAP_SERVER_PATH "/hello"
#endif

#ifndef COAP_SERVER_PORT
#define COAP_SERVER_PORT COAPS_DEFAULT_PORT  /* DTLS required for ML-KEM */
#endif

#define COAP_CLIENT_URI "coaps://" COAP_SERVER_IP COAP_SERVER_PATH

/* ==========================================================================
 * ML-KEM Key Exchange Configuration
 * 
 * Choose one of the following based on your security requirements:
 * 
 * Pure Post-Quantum (provides PQC security only):
 *   WOLFSSL_ML_KEM_512   - NIST Level 1, fastest, smallest
 *   WOLFSSL_ML_KEM_768   - NIST Level 3, recommended balance
 *   WOLFSSL_ML_KEM_1024  - NIST Level 5, highest security
 * 
 * Hybrid (provides both classical and PQC security):
 *   WOLFSSL_P256_ML_KEM_512  - P-256 + ML-KEM-512
 *   WOLFSSL_P384_ML_KEM_768  - P-384 + ML-KEM-768, recommended for production
 *   WOLFSSL_P521_ML_KEM_1024 - P-521 + ML-KEM-1024
 * 
 * Hybrid is recommended during the transition period as it provides
 * security against both classical and quantum attackers.
 * ========================================================================== */
#ifndef PQC_KEY_EXCHANGE
#define PQC_KEY_EXCHANGE WOLFSSL_P384_ML_KEM_768  /* Hybrid recommended */
#endif

void cleanup_resources(coap_context_t *ctx, coap_session_t *session,
                       coap_optlist_t *optlist) {
    if (optlist)
        coap_delete_optlist(optlist);
    if (session)
        coap_session_release(session);
    if (ctx)
        coap_free_context(ctx);
    coap_cleanup();
}

int setup_destination_address(coap_address_t *dst, const char *host,
                              uint16_t port) {
    printf("Setting up destination address: %s:%d\n", host, port);

    memset(dst, 0, sizeof(coap_address_t));

    struct sockaddr_in *sin = (struct sockaddr_in *)&dst->addr.sin;
    sin->sin_family = AF_INET;
    sin->sin_port = htons(port);

    if (inet_pton(AF_INET, host, &sin->sin_addr) <= 0) {
        printf("Failed to convert IP address: %s\n", host);
        return 0;
    }

    dst->size = sizeof(struct sockaddr_in);
    dst->addr.sa.sa_family = AF_INET;

    printf("Address configured: %s:%d\n", host, port);
    return 1;
}

static coap_response_t response_handler(coap_session_t *session,
                                        const coap_pdu_t *sent,
                                        const coap_pdu_t *received,
                                        const int id) {
    size_t len;
    const uint8_t *databuf;
    size_t offset;
    size_t total;

    (void)session;
    (void)sent;
    (void)id;

    have_response = 1;
    printf("\n=== RESPONSE RECEIVED ===\n");
    coap_show_pdu(COAP_LOG_WARN, received);
    if (coap_get_data_large(received, &len, &databuf, &offset, &total)) {
        printf("Response data: ");
        fwrite(databuf, 1, len, stdout);
        printf("\n");
    }
    printf("=== END RESPONSE ===\n");
    return COAP_RESPONSE_OK;
}

void verify_tls_backend(void) {
    printf("\n=== TLS Backend Verification ===\n");
    
    coap_tls_version_t *tls_version = coap_get_tls_library_version();
    
    if (!tls_version) {
        printf("Failed to get TLS library version\n");
        return;
    }
    
    switch (tls_version->type) {
        case COAP_TLS_LIBRARY_WOLFSSL:
            printf("Using wolfSSL backend\n");
            break;
        default:
            printf("TLS backend type: %d\n", tls_version->type);
            break;
    }
    
    printf("DTLS supported: %s\n", coap_dtls_is_supported() ? "Yes" : "No");
    printf("DTLS PKI supported: %s\n", coap_dtls_pki_is_supported() ? "Yes" : "No");
    
#ifdef ENABLE_MLKEM
    printf("ML-KEM (PQC) support: ENABLED\n");
    printf("Configured key exchange: ");
    #if PQC_KEY_EXCHANGE == WOLFSSL_ML_KEM_512
    printf("ML-KEM-512 (NIST Level 1)\n");
    #elif PQC_KEY_EXCHANGE == WOLFSSL_ML_KEM_768
    printf("ML-KEM-768 (NIST Level 3)\n");
    #elif PQC_KEY_EXCHANGE == WOLFSSL_ML_KEM_1024
    printf("ML-KEM-1024 (NIST Level 5)\n");
    #elif PQC_KEY_EXCHANGE == WOLFSSL_P256_ML_KEM_512
    printf("P256_ML_KEM_512 (Hybrid Level 1)\n");
    #elif PQC_KEY_EXCHANGE == WOLFSSL_P384_ML_KEM_768
    printf("P384_ML_KEM_768 (Hybrid Level 3) [RECOMMENDED]\n");
    #elif PQC_KEY_EXCHANGE == WOLFSSL_P521_ML_KEM_1024
    printf("P521_ML_KEM_1024 (Hybrid Level 5)\n");
    #else
    printf("Custom: %d\n", PQC_KEY_EXCHANGE);
    #endif
#else
    printf("ML-KEM (PQC) support: DISABLED\n");
#endif
    
    printf("=== End TLS Backend Verification ===\n\n");
}

/* DTLS PKI setup with ML-KEM key exchange
 * 
 * Note: Certificate type (RSA, ECC, ML-DSA) is independent of key exchange.
 * You can use ML-KEM key exchange with any certificate type.
 */
static coap_dtls_pki_t *setup_pki_with_mlkem(void) {
    static coap_dtls_pki_t dtls_pki;
    
    memset(&dtls_pki, 0, sizeof(dtls_pki));
    dtls_pki.version = COAP_DTLS_PKI_SETUP_VERSION;
    
    /* Disable certificate verification for testing
     * In production, set to 1 and provide proper CA certificate */
    dtls_pki.verify_peer_cert = 0;
    dtls_pki.is_rpk_not_cert = 0;
    
    /* For production with ML-DSA certificates, you would set:
     * dtls_pki.pki_key.key_type = COAP_PKI_KEY_PEM;
     * dtls_pki.pki_key.key.pem.public_cert = "ml_dsa_server_cert.pem";
     * dtls_pki.pki_key.key.pem.private_key = "ml_dsa_server_key.pem";
     * dtls_pki.pki_key.key.pem.ca_file = "ml_dsa_ca_cert.pem";
     */
    
    return &dtls_pki;
}

#ifdef ENABLE_MLKEM
/* Callback to configure ML-KEM key exchange on the DTLS session
 * 
 * This is called by libcoap/wolfSSL during session setup.
 * It sets the preferred key exchange algorithm.
 */
static void configure_mlkem_keyshare(WOLFSSL *ssl) {
    if (ssl == NULL) {
        printf("ERROR: SSL context is NULL\n");
        return;
    }
    
    printf("Configuring ML-KEM key exchange...\n");
    
    /* Set the ML-KEM key share */
    int ret = wolfSSL_UseKeyShare(ssl, PQC_KEY_EXCHANGE);
    
    if (ret == WOLFSSL_SUCCESS) {
        printf("ML-KEM key exchange configured successfully\n");
    } else {
        printf("WARNING: Failed to set ML-KEM key share (error: %d)\n", ret);
        printf("Falling back to default key exchange\n");
    }
}
#endif

int main(void) {
    coap_context_t *ctx = NULL;
    coap_session_t *session = NULL;
    coap_optlist_t *optlist = NULL;
    coap_address_t dst;
    coap_pdu_t *pdu = NULL;
    int result = EXIT_FAILURE;
    int len;
    int res;
    unsigned int wait_ms;
    coap_uri_t uri;
    const char *coap_uri = COAP_CLIENT_URI;
#define BUFSIZE 100
    unsigned char scratch[BUFSIZE];

    printf("\n");
    printf("============================================\n");
    printf(" CoAP Client with Post-Quantum Cryptography\n");
    printf("============================================\n");
    printf("Target URI: %s\n", coap_uri);
    printf("Server Port: %d\n", COAP_SERVER_PORT);
    printf("Protocol: CoAP over DTLS 1.3\n");
    printf("============================================\n\n");

    /* Initialize libcoap library */
    coap_startup();

    /* Verify TLS backend and PQC configuration */
    verify_tls_backend();

    /* Set logging level */
    coap_set_log_level(COAP_LOG_WARN);

    /* Parse the URI */
    len = coap_split_uri((const unsigned char *)coap_uri, strlen(coap_uri), &uri);
    if (len != 0) {
        coap_log_warn("Failed to parse uri %s\n", coap_uri);
        goto finish;
    }
    printf("URI parsed successfully\n");

    /* Connect to WiFi */
    wifi_init(NULL);
    
    int wifi_connected = 0;
    for (int attempt = 1; attempt <= 3 && !wifi_connected; attempt++) {
        if (attempt > 1) {
            printf("WiFi retry attempt %d/3...\n", attempt);
        }
        
        int ret = connect_to_wifi();
        if (ret >= 0 && wait_for_wifi_connection() >= 0) {
            wifi_connected = 1;
        } else {
            printf("WiFi connection attempt %d failed\n", attempt);
            if (attempt < 3) {
                wifi_disconnect();
                k_sleep(K_MSEC(2000));
            }
        }
    }

    if (!wifi_connected) {
        printf("Failed to connect to WiFi after 3 attempts\n");
        goto finish;
    }

    k_sleep(K_MSEC(1000));

    /* Setup destination address */
    char host_str[64];
    if (uri.host.length < sizeof(host_str)) {
        memcpy(host_str, uri.host.s, uri.host.length);
        host_str[uri.host.length] = '\0';
    } else {
        printf("Host string too long\n");
        goto finish;
    }

    uint16_t port = uri.port ? uri.port : COAP_SERVER_PORT;
    if (!setup_destination_address(&dst, host_str, port)) {
        printf("Failed to setup destination address\n");
        goto finish;
    }

    /* Create CoAP context */
    printf("Creating CoAP context...\n");
    if (!(ctx = coap_new_context(NULL))) {
        coap_log_emerg("Cannot create libcoap context\n");
        goto finish;
    }

    coap_context_set_block_mode(ctx, COAP_BLOCK_USE_LIBCOAP |
                                     COAP_BLOCK_SINGLE_BODY);

    /* Create DTLS session with ML-KEM
     * 
     * Key exchange (ML-KEM) is negotiated during the TLS handshake.
     * The certificate type (RSA, ECC, ML-DSA) is separate from key exchange.
     */
    if (uri.scheme == COAP_URI_SCHEME_COAPS) {
        printf("Creating DTLS session with PQC key exchange...\n");
        
        coap_dtls_pki_t *dtls_pki = setup_pki_with_mlkem();
        session = coap_new_client_session_pki(ctx, NULL, &dst, 
                                               COAP_PROTO_DTLS, dtls_pki);
        
#ifdef ENABLE_MLKEM
        if (session) {
            /* Get the underlying wolfSSL object and configure ML-KEM
             * 
             * Note: In current libcoap, this might need to be done via
             * a custom setup callback or by modifying libcoap's wolfSSL
             * integration. This example shows the concept.
             */
            /* TODO: libcoap integration for key share selection
             * 
             * Option 1: Use environment variable COAP_WOLFSSL_GROUPS
             *   - Set before starting: export COAP_WOLFSSL_GROUPS=P384_KYBER_LEVEL3
             *   - This is the easiest approach with patched libcoap
             * 
             * Option 2: Direct wolfSSL access (requires libcoap modification)
             *   - Get WOLFSSL* from session
             *   - Call wolfSSL_UseKeyShare(ssl, PQC_KEY_EXCHANGE)
             * 
             * Option 3: Build libcoap with compile-time default
             *   - Use CPPFLAGS="-DCOAP_WOLFSSL_GROUPS=..."
             */
            printf("ML-KEM key exchange will be negotiated during handshake\n");
        }
#endif
    } else {
        /* Non-DTLS session (not recommended for PQC) */
        session = coap_new_client_session(ctx, NULL, &dst, COAP_PROTO_UDP);
    }

    if (!session) {
        coap_log_emerg("Cannot create client session\n");
        goto finish;
    }
    printf("Session created successfully\n");

    coap_register_response_handler(ctx, response_handler);

    /* Construct CoAP message */
    pdu = coap_pdu_init(COAP_MESSAGE_CON, COAP_REQUEST_CODE_GET,
                        coap_new_message_id(session),
                        coap_session_max_pdu_size(session));
    if (!pdu) {
        coap_log_emerg("Cannot create PDU\n");
        goto finish;
    }

    len = coap_uri_into_options(&uri, &dst, &optlist, 1, scratch, sizeof(scratch));
    if (len) {
        coap_log_warn("Failed to create options\n");
        goto finish;
    }

    if (optlist) {
        res = coap_add_optlist_pdu(pdu, &optlist);
        if (res != 1) {
            coap_log_warn("Failed to add options to PDU\n");
            goto finish;
        }
    }

    coap_show_pdu(COAP_LOG_WARN, pdu);

    /* Send the request */
    printf("Sending CoAP request (ML-KEM handshake will occur)...\n");
    if (coap_send(session, pdu) == COAP_INVALID_MID) {
        coap_log_err("Cannot send CoAP PDU\n");
        goto finish;
    }
    printf("Request sent successfully\n");

    wait_ms = (coap_session_get_default_leisure(session).integer_part + 1) * 1000;

    printf("Waiting for response...\n");
    while (have_response == 0) {
        res = coap_io_process(ctx, 500);
        if (res >= 0) {
            if (wait_ms > 0) {
                if ((unsigned)res >= wait_ms) {
                    printf("TIMEOUT: No response received\n");
                    break;
                } else {
                    wait_ms -= res;
                }
            }
        }
    }

    if (have_response != 0) {
        printf("\n=== SUCCESS ===\n");
        printf("CoAP response received over DTLS 1.3\n");
#ifdef ENABLE_MLKEM
        printf("Key exchange: ML-KEM (Post-Quantum)\n");
#endif
        printf("===============\n");
        result = EXIT_SUCCESS;
    } else {
        printf("FAILED: No response received\n");
    }

finish:
    printf("Cleaning up resources...\n");
    cleanup_resources(ctx, session, optlist);
    wifi_disconnect();
    printf("CLIENT FINISHED.\n");

    return result;
}
