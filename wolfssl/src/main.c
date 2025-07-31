/* minimal CoAP client
 *
 * Copyright (C) 2018-2024 Olaf Bergmann <bergmann@tzi.org>
 * Copyright (C) 2024-2025 Javier Blanco-Romero @fj-blanco (UC3M, QURSA project)
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

static int have_response = 0;

#ifndef COAP_SERVER_IP
#define COAP_SERVER_IP "134.102.218.18"
#endif

#ifndef COAP_SERVER_PATH
#define COAP_SERVER_PATH "/hello"
#endif

#ifndef COAP_SERVER_PORT
#ifdef USE_DTLS
#define COAP_SERVER_PORT COAPS_DEFAULT_PORT
#else
#define COAP_SERVER_PORT COAP_DEFAULT_PORT
#endif
#endif

#ifdef USE_DTLS
#define COAP_CLIENT_URI "coaps://" COAP_SERVER_IP COAP_SERVER_PATH
#else
#define COAP_CLIENT_URI "coap://" COAP_SERVER_IP COAP_SERVER_PATH
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

    dst->size = sizeof(struct sockaddr_in); // This is 8 bytes in Zephyr
    dst->addr.sa.sa_family = AF_INET;

    printf("Address size set to: %u (sizeof(struct sockaddr_in))\n", dst->size);
    printf("Address family: %d\n", dst->addr.sa.sa_family);
    printf("Target: %s:%d\n", host, port);

    printf("Verification - sin_family: %d, sin_port: 0x%x, sin_addr: 0x%x\n",
           sin->sin_family, sin->sin_port, sin->sin_addr.s_addr);

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
    
    printf("TLS Library Type: %d\n", tls_version->type);
    
    switch (tls_version->type) {
        case COAP_TLS_LIBRARY_NOTLS:
            printf("No TLS support\n");
            break;
        case COAP_TLS_LIBRARY_TINYDTLS:
            printf("Using TinyDTLS backend\n");
            break;
        case COAP_TLS_LIBRARY_OPENSSL:
            printf("Using OpenSSL backend\n");
            break;
        case COAP_TLS_LIBRARY_GNUTLS:
            printf("Using GnuTLS backend\n");
            break;
        case COAP_TLS_LIBRARY_MBEDTLS:
            printf("Using mbedTLS backend\n");
            break;
        case COAP_TLS_LIBRARY_WOLFSSL:
            printf("Using wolfSSL backend\n");
            break;
        default:
            printf("Unknown TLS backend (type: %d)\n", tls_version->type);
            break;
    }
    
    printf("DTLS supported: %s\n", coap_dtls_is_supported() ? "Yes" : "No");
    printf("DTLS PSK supported: %s\n", coap_dtls_psk_is_supported() ? "Yes" : "No");
    printf("DTLS PKI supported: %s\n", coap_dtls_pki_is_supported() ? "Yes" : "No");
    
    printf("=== End TLS Backend Verification ===\n\n");
}

#ifdef USE_DTLS
/* Minimal PKI setup - disables certificate verification */
static coap_dtls_pki_t *setup_minimal_pki(void) {
    static coap_dtls_pki_t dtls_pki;
    
    memset(&dtls_pki, 0, sizeof(dtls_pki));
    dtls_pki.version = COAP_DTLS_PKI_SETUP_VERSION;
    dtls_pki.verify_peer_cert = 0;  // Disable certificate verification
    dtls_pki.is_rpk_not_cert = 0;
    
    return &dtls_pki;
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
    int is_mcast;
#define BUFSIZE 100
    unsigned char scratch[BUFSIZE];

    printf("=== CoAP Client Configuration ===\n");
    printf("Target URI: %s\n", coap_uri);
    printf("Server IP: %s\n", COAP_SERVER_IP);
    printf("Server Path: %s\n", COAP_SERVER_PATH);
    printf("Server Port: %d\n", COAP_SERVER_PORT);
#ifdef USE_DTLS
    printf("DTLS Mode: ENABLED\n");
#else
    printf("DTLS Mode: DISABLED\n");
#endif
    printf("================================\n\n");

    printf("Starting CoAP client......\n");

    /* Initialize libcoap library */
    coap_startup();

    /* Verify which TLS backend is being used */
    verify_tls_backend();

    /* Set logging level */
    coap_set_log_level(COAP_LOG_WARN);

    /* Parse the URI */
    len = coap_split_uri((const unsigned char *)coap_uri, strlen(coap_uri), &uri);
    if (len != 0) {
        coap_log_warn("Failed to parse uri %s\n", coap_uri);
        goto finish;
    } else {
        printf("URI parsed successfully......\n");
        printf("Parsed - Scheme: %d, Host: %.*s, Port: %d, Path: %.*s\n", 
               uri.scheme, (int)uri.host.length, uri.host.s,
               uri.port, (int)uri.path.length, uri.path.s);
    }

    wifi_init(NULL);

    /* WiFi connection with retries */
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
                k_sleep(K_MSEC(2000)); // Wait 2 seconds before retry
            }
        }
    }

    if (!wifi_connected) {
        printf("Failed to connect to WiFi after 3 attempts\n");
        goto finish;
    }

    /* Add delay to ensure network stack is ready */
    k_sleep(K_MSEC(1000));

    /* Extract host string from URI for address setup */
    char host_str[64];
    if (uri.host.length < sizeof(host_str)) {
        memcpy(host_str, uri.host.s, uri.host.length);
        host_str[uri.host.length] = '\0';
    } else {
        printf("Host string too long\n");
        goto finish;
    }

    /* Setup destination address with correct size */
    uint16_t port = uri.port ? uri.port : COAP_SERVER_PORT;
    if (!setup_destination_address(&dst, host_str, port)) {
        printf("Failed to setup destination address\n");
        goto finish;
    } else {
        printf("Address resolved......\n");
    }

    is_mcast = 0;
    printf("CoAP creating new context....\n");
    /* create CoAP context and a client session */
    if (!(ctx = coap_new_context(NULL))) {
        coap_log_emerg("cannot create libcoap context\n");
        goto finish;
    } else {
        printf("CoAP context created......\n");
    }

    /* Support large responses */
    coap_context_set_block_mode(ctx, COAP_BLOCK_USE_LIBCOAP |
                                         COAP_BLOCK_SINGLE_BODY);

    /* Create session based on URI scheme */
    if (uri.scheme == COAP_URI_SCHEME_COAP) {
        session = coap_new_client_session(ctx, NULL, &dst, COAP_PROTO_UDP);
    } else if (uri.scheme == COAP_URI_SCHEME_COAP_TCP) {
        session = coap_new_client_session(ctx, NULL, &dst, COAP_PROTO_TCP);
#ifdef USE_DTLS
    } else if (uri.scheme == COAP_URI_SCHEME_COAPS) {
        /* DTLS session with minimal PKI (no cert verification) */
        coap_dtls_pki_t *dtls_pki = setup_minimal_pki();
        session = coap_new_client_session_pki(ctx, NULL, &dst, COAP_PROTO_DTLS, dtls_pki);
#endif
    }
    if (!session) {
        coap_log_emerg("cannot create client session\n");
        goto finish;
    } else {
        printf("CoAP session created......\n");
    }

    coap_register_response_handler(ctx, response_handler);

    /* construct CoAP message */
    pdu = coap_pdu_init(is_mcast ? COAP_MESSAGE_NON : COAP_MESSAGE_CON,
                        COAP_REQUEST_CODE_GET, coap_new_message_id(session),
                        coap_session_max_pdu_size(session));
    if (!pdu) {
        coap_log_emerg("cannot create PDU\n");
        goto finish;
    }

    /* Add option list (which will be sorted) to the PDU */
    len = coap_uri_into_options(&uri, &dst, &optlist, 1, scratch,
                                sizeof(scratch));
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

    printf("About to send CoAP packet...\n");
    /* and send the PDU */
    if (coap_send(session, pdu) == COAP_INVALID_MID) {
        coap_log_err("cannot send CoAP pdu\n");
        goto finish;
    } else {
        printf("CoAP packet sent successfully!\n");
    }

    wait_ms = (coap_session_get_default_leisure(session).integer_part + 1) * 1000;

    printf("Waiting for response...\n");
    while (have_response == 0 || is_mcast) {
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
        printf("SUCCESS: Response received!\n");
        result = EXIT_SUCCESS;
        goto finish;
    } else {
        printf("FAILED: No response received\n");
    }

    result = EXIT_SUCCESS;
finish:
    printf("Cleaning up resources...\n");
    cleanup_resources(ctx, session, optlist);
    wifi_disconnect();
    printf("CLIENT FINISHED.\n");

    return result;
}