/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "http.h"

#include <zephyr/kernel.h>
#include <zephyr/net/dns_resolve.h>
#include <zephyr/net/http/client.h>
#include <zephyr/net/http/status.h>
#include <zephyr/net/socket.h>

#ifndef CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_HTTP
#include <zephyr/net/tls_credentials.h>
#endif

#include "alloc.h"
#include "log.h"

ASTARTE_LOG_MODULE_REGISTER(astarte_http, CONFIG_ASTARTE_DEVICE_SDK_HTTP_LOG_LEVEL);

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

/** @brief Context struct for any HTTP request */
struct http_req_ctx
{
    /** @brief Flag to store the success or failure of the request */
    bool request_ok;
    /** @brief Buffer where to store the response */
    uint8_t *resp_buf;
    /** @brief Size of the response buffer */
    size_t resp_buf_size;
    /** @brief Number of bytes written in the response buffer */
    size_t bytes_written;
};

#define MAX_DNS_RESOLVED_ADDRESSES 8

/** @brief Context structure used for DNS resolution. */
struct dns_resolve_ctx
{
    /** @brief Semaphore to synchronize DNS resolution completion. */
    struct k_sem sem;
    /** @brief Array of resolved IPv4 addresses. */
    struct sockaddr_in resolved_addrs[MAX_DNS_RESOLVED_ADDRESSES];
    /** @brief Number of addresses successfully resolved and stored. */
    size_t addr_count;
};

/************************************************
 *       Checks over configuration values       *
 ***********************************************/

#ifdef CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_HTTP
#warning "TLS has been disabled (unsafe)!"
#endif /* defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_HTTP) */

BUILD_ASSERT(sizeof(CONFIG_ASTARTE_DEVICE_SDK_HOSTNAME) != 1, "Missing hostname in configuration");

/************************************************
 *       Callbacks declaration/definition       *
 ***********************************************/

static int http_response_cb(
    struct http_response *rsp, enum http_final_call final_data, void *user_data)
{
    int res = 0;
    struct http_req_ctx *ctx = (struct http_req_ctx *) user_data;

    ASTARTE_LOG_DBG("http_response_cb called. Status: %s (%d), Fragment len: %zu",
        rsp->http_status ? rsp->http_status : "N/A", rsp->http_status_code, rsp->body_frag_len);

    // Evaluate the status code if it has been parsed by Zephyr
    if ((rsp->http_status_code < HTTP_200_OK)
        || (rsp->http_status_code >= HTTP_300_MULTIPLE_CHOICES)) {
        ASTARTE_LOG_ERR(
            "HTTP request failed, response code: %s %d", rsp->http_status, rsp->http_status_code);
        ctx->request_ok = false;
    }

    // Accumulate the parsed body fragment into the output buffer
    if (rsp->body_frag_start && rsp->body_frag_len > 0) {
        ASTARTE_LOG_DBG("Processing body fragment of size %zu. Current bytes written: %zu",
            rsp->body_frag_len, ctx->bytes_written);

        if (ctx->resp_buf) {
            // Check if we have enough space (saving 1 byte for the null terminator)
            if (ctx->bytes_written + rsp->body_frag_len < ctx->resp_buf_size) {
                memcpy(
                    ctx->resp_buf + ctx->bytes_written, rsp->body_frag_start, rsp->body_frag_len);
                ctx->bytes_written += rsp->body_frag_len;
                ASTARTE_LOG_DBG("Fragment successfully copied to response buffer.");
            } else {
                ASTARTE_LOG_ERR("HTTP reply body exceeds provided buffer size (%zu). Needed at "
                                "least %zu bytes.",
                    ctx->resp_buf_size, ctx->bytes_written + rsp->body_frag_len + 1);
                ctx->request_ok = false;
                res = -1;
                goto exit;
            }
        } else {
            ASTARTE_LOG_DBG("No response buffer provided in context. Dropping fragment.");
        }
    }

    if (final_data == HTTP_DATA_FINAL) {
        ASTARTE_LOG_DBG(
            "All HTTP data received. Total payload size written: %zu bytes", ctx->bytes_written);
        // Force null-termination safely
        if (ctx->resp_buf && ctx->resp_buf_size > 0) {
            ctx->resp_buf[ctx->bytes_written] = '\0';
            ASTARTE_LOG_DBG("Response buffer successfully null-terminated.");
        }
    } else {
        ASTARTE_LOG_DBG("Awaiting more HTTP data...");
    }

exit:
    return res;
}

static void dns_resolve_cb(
    enum dns_resolve_status status, struct dns_addrinfo *info, void *user_data)
{
    ASTARTE_LOG_DBG("DNS callback fired. Status: %d", status);
    struct dns_resolve_ctx *ctx = (struct dns_resolve_ctx *) user_data;

    // DNS_EAI_ALLDONE signals that the DNS resolver has finished iterating
    if (status == DNS_EAI_ALLDONE || status == DNS_EAI_CANCELED) {
        k_sem_give(&ctx->sem);
        return;
    }

    // Accumulate addresses up to MAX_DNS_RESOLVED_ADDRESSES
    if (status == DNS_EAI_INPROGRESS && info != NULL) {
        if (info->ai_family == AF_INET && ctx->addr_count < MAX_DNS_RESOLVED_ADDRESSES) {
            memcpy(
                &ctx->resolved_addrs[ctx->addr_count], &info->ai_addr, sizeof(struct sockaddr_in));
            ctx->addr_count++;
            ASTARTE_LOG_DBG("Successfully captured IPv4 address! (Count: %zu)", ctx->addr_count);
        }
    }
}

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief Create a new TCP socket and connect it to a server.
 *
 * @note The returned socket should be closed once its use has terminated.
 *
 * @return -1 upon failure, a file descriptor for the new socket otherwise.
 */
static int create_and_connect_socket(int32_t timeout_ms);

static astarte_result_t astarte_http_do_request(enum http_method method, int32_t timeout_ms,
    const char *url, const char **header_fields, const char *payload, struct http_req_ctx *ctx);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_http_post(int32_t timeout_ms, const char *url, const char **header_fields,
    const char *payload, uint8_t *resp_buf, size_t resp_buf_size)
{
    ASTARTE_LOG_DBG("Initiating HTTP POST request to URL: %s (Timeout: %d ms, Buffer size: %zu)",
        url, timeout_ms, resp_buf_size);

    struct http_req_ctx ctx = {
        .request_ok = true, .resp_buf = resp_buf, .resp_buf_size = resp_buf_size, .bytes_written = 0
    };

    return astarte_http_do_request(HTTP_POST, timeout_ms, url, header_fields, payload, &ctx);
}

astarte_result_t astarte_http_get(int32_t timeout_ms, const char *url, const char **header_fields,
    uint8_t *resp_buf, size_t resp_buf_size)
{
    ASTARTE_LOG_DBG("Initiating HTTP GET request to URL: %s (Timeout: %d ms, Buffer size: %zu)",
        url, timeout_ms, resp_buf_size);

    struct http_req_ctx ctx = {
        .request_ok = true, .resp_buf = resp_buf, .resp_buf_size = resp_buf_size, .bytes_written = 0
    };

    return astarte_http_do_request(HTTP_GET, timeout_ms, url, header_fields, NULL, &ctx);
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static int create_and_connect_socket(int32_t timeout_ms)
{
    char hostname[] = CONFIG_ASTARTE_DEVICE_SDK_HOSTNAME;

    ASTARTE_LOG_DBG("Attempting DNS resolution for %s", hostname);

    struct dns_resolve_ctx dns_ctx = {
        .addr_count = 0,
    };
    k_sem_init(&dns_ctx.sem, 0, 1);

    uint16_t dns_id = 0;
    int resolve_rc = dns_get_addr_info(
        hostname, DNS_QUERY_TYPE_A, &dns_id, dns_resolve_cb, &dns_ctx, timeout_ms);
    if (resolve_rc == 0) {
        // Block until the callback gives the semaphore
        k_sem_take(&dns_ctx.sem, K_FOREVER);
    } else {
        ASTARTE_LOG_ERR("Failed to initiate DNS resolution (err: %d)", resolve_rc);
        return -1;
    }

    if (dns_ctx.addr_count == 0) {
        ASTARTE_LOG_ERR("DNS resolution failed to find any IPv4 addresses for %s", hostname);
        return -1;
    }

    ASTARTE_LOG_DBG("DNS resolution successful. Found %zu addresses.", dns_ctx.addr_count);

#ifdef CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_HTTP
    int proto = IPPROTO_TCP;
    // NOLINTNEXTLINE(hicpp-signed-bitwise)
    uint16_t port = htons(80U);
    ASTARTE_LOG_DBG("Using cleartext TCP (IPPROTO_TCP) on port 80");
#else
    int proto = IPPROTO_TLS_1_2;
    // NOLINTNEXTLINE(hicpp-signed-bitwise)
    uint16_t port = htons(443U);
    ASTARTE_LOG_DBG("Using secure TLS (IPPROTO_TLS_1_2) on port 443");
#endif

    // Loop through all resolved addresses
    int sock = -1;
    for (size_t i = 0; i < dns_ctx.addr_count; i++) {
        dns_ctx.resolved_addrs[i].sin_port = port;

        sock = zsock_socket(AF_INET, SOCK_STREAM, proto);
        if (sock == -1) {
            ASTARTE_LOG_ERR("Socket creation failed for address index %zu.", i);
            continue;
        }

#ifndef CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_HTTP
        sec_tag_t sec_tag_opt[] = {
            CONFIG_ASTARTE_DEVICE_SDK_HTTPS_CA_CERT_TAG,
        };

        int sockopt_rc
            = zsock_setsockopt(sock, SOL_TLS, TLS_SEC_TAG_LIST, sec_tag_opt, sizeof(sec_tag_opt));
        if (sockopt_rc == -1) {
            ASTARTE_LOG_ERR("Socket options error (TLS_SEC_TAG_LIST): %d", sockopt_rc);
            zsock_close(sock);
            sock = -1;
            continue;
        }

        sockopt_rc = zsock_setsockopt(sock, SOL_TLS, TLS_HOSTNAME, hostname, strlen(hostname));
        if (sockopt_rc == -1) {
            ASTARTE_LOG_ERR("Socket options error (TLS_HOSTNAME): %d", sockopt_rc);
            zsock_close(sock);
            sock = -1;
            continue;
        }
#endif

        ASTARTE_LOG_DBG("Attempting to connect socket %d to remote address index %zu.", sock, i);

        int connect_rc = zsock_connect(
            sock, (struct sockaddr *) &dns_ctx.resolved_addrs[i], sizeof(struct sockaddr_in));

        if (connect_rc == 0) {
            ASTARTE_LOG_DBG("Successfully connected socket %d.", sock);
            break; // Connection succeeded, break out of the fallback loop
        }

        ASTARTE_LOG_ERR("Connection failed to address index %zu (%d - %s), closing socket...", i,
            errno, strerror(errno));
        zsock_close(sock);
        sock = -1;
    }

    if (sock == -1) {
        ASTARTE_LOG_ERR("Failed to connect to any resolved addresses.");
        return -1;
    }

    return sock;
}

static astarte_result_t astarte_http_do_request(enum http_method method, int32_t timeout_ms,
    const char *url, const char **header_fields, const char *payload, struct http_req_ctx *ctx)
{
    ASTARTE_LOG_DBG("Entering astarte_http_do_request. Method: %d, URL: %s", method, url);

    int sock = create_and_connect_socket(timeout_ms);
    if (sock < 0) {
        ASTARTE_LOG_ERR("Aborting HTTP request due to socket creation/connection failure.");
        return ASTARTE_RESULT_SOCKET_ERROR;
    }

    struct http_request req = { 0 };
    size_t buf_size = CONFIG_ASTARTE_DEVICE_SDK_ADVANCED_HTTP_RCV_BUFFER_SIZE;
    uint8_t *recv_buf = (uint8_t *) astarte_calloc(buf_size, sizeof(uint8_t));
    if (recv_buf == NULL) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        zsock_close(sock);
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }

    req.method = method;
    req.host = CONFIG_ASTARTE_DEVICE_SDK_HOSTNAME;
    req.port = NULL;
    req.url = url;
    req.header_fields = header_fields;
    req.protocol = "HTTP/1.1";
    req.response = http_response_cb;

    if (payload) {
        req.content_type_value = "application/json";
        req.payload = payload;
        req.payload_len = strlen(payload);
        ASTARTE_LOG_DBG("Payload attached. Length: %zu", req.payload_len);
    } else {
        ASTARTE_LOG_DBG("No payload attached to this request.");
    }

    req.recv_buf = recv_buf;
    req.recv_buf_len = buf_size;

    ASTARTE_LOG_DBG("Executing http_client_req on socket %d...", sock);

    // Pass context struct as the user_data parameter
    int http_rc = http_client_req(sock, &req, timeout_ms, ctx);

    ASTARTE_LOG_DBG("http_client_req returned with code: %d", http_rc);

    if ((http_rc < 0) || !ctx->request_ok) {
        ASTARTE_LOG_ERR("HTTP request failed (http_client_req code: %d, context flag ok: %d)",
            http_rc, ctx->request_ok);
        zsock_close(sock);
        astarte_free(recv_buf);
        return ASTARTE_RESULT_HTTP_REQUEST_ERROR;
    }

    ASTARTE_LOG_DBG("HTTP request completed successfully. Closing socket %d.", sock);
    zsock_close(sock);

    astarte_free(recv_buf);
    return ASTARTE_RESULT_OK;
}
