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

#ifndef COAP_CLIENT_URI
#define COAP_CLIENT_URI "coap://134.102.218.18/hello"
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
    coap_show_pdu(COAP_LOG_WARN, received);
    if (coap_get_data_large(received, &len, &databuf, &offset, &total)) {
        fwrite(databuf, 1, len, stdout);
        fwrite("\n", 1, 1, stdout);
    }
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

    printf("Starting CoAP client......\n");

    /* Initialize libcoap library */
    coap_startup();

    /* Verify which TLS backend is being used */
    verify_tls_backend();

    /* Set logging level */
    coap_set_log_level(COAP_LOG_WARN);

    /* Parse the URI */
    len =
        coap_split_uri((const unsigned char *)coap_uri, strlen(coap_uri), &uri);
    if (len != 0) {
        coap_log_warn("Failed to parse uri %s\n", coap_uri);
        goto finish;
    } else {
        printf("URI parsed......\n");
    }

    wifi_init(NULL);

    int ret = connect_to_wifi();

    printf("Waiting for Wi-Fi connection...\n");
    if (wait_for_wifi_connection() < 0) {
        printf("Failed to connect to Wi-Fi within the timeout period\n");
        goto finish;
    }

    printf("Wi-Fi connected. Proceeding...\n");
    if (ret < 0) {
        printf("Wi-Fi connection failed\n");
        goto finish;
    } else {
        printf("Wi-Fi connection in progress\n");
    }

    /* Add delay to ensure network stack is ready */
    k_sleep(K_MSEC(1000));

    /* Setup destination address with correct size */
    if (!setup_destination_address(&dst, "134.102.218.18", uri.port)) {
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

    if (uri.scheme == COAP_URI_SCHEME_COAP) {
        session = coap_new_client_session(ctx, NULL, &dst, COAP_PROTO_UDP);
    } else if (uri.scheme == COAP_URI_SCHEME_COAP_TCP) {
        session = coap_new_client_session(ctx, NULL, &dst, COAP_PROTO_TCP);
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

    printf(
        "About to send CoAP packet using default libcoap socket function...\n");
    /* and send the PDU */
    if (coap_send(session, pdu) == COAP_INVALID_MID) {
        coap_log_err("cannot send CoAP pdu\n");
        goto finish;
    } else {
        printf("CoAP packet sent successfully using default libcoap!\n");
    }

    wait_ms =
        (coap_session_get_default_leisure(session).integer_part + 1) * 1000;

    while (have_response == 0 || is_mcast) {
        res = coap_io_process(ctx, 500);
        if (res >= 0) {
            if (wait_ms > 0) {
                if ((unsigned)res >= wait_ms) {
                    fprintf(stdout, "timeout\n");
                    break;
                } else {
                    wait_ms -= res;
                }
            }
        }
    }

    if (have_response != 0) {
        printf("SUCCESS: Response received using default libcoap!\n");
        result = EXIT_SUCCESS;
        goto finish;
    }

    result = EXIT_SUCCESS;
finish:
    printf("Cleaning up resources...\n");
    cleanup_resources(ctx, session, optlist);
    wifi_disconnect();
    printf("CLIENT FINISHED.\n");

    return result;
}