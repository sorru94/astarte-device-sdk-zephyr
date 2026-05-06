/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "http.h"

#include <zephyr/kernel.h>
#include <zephyr/net/http/client.h>
#include <zephyr/net/http/status.h>
#include <zephyr/net/socket.h>

#ifndef CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_HTTP
#include <zephyr/net/tls_credentials.h>
#endif

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

    // Evaluate the status code if it has been parsed by Zephyr
    if ((rsp->http_status_code < HTTP_200_OK)
        || (rsp->http_status_code >= HTTP_300_MULTIPLE_CHOICES)) {
        ASTARTE_LOG_ERR(
            "HTTP request failed, response code: %s %d", rsp->http_status, rsp->http_status_code);
        ctx->request_ok = false;
    }

    // Accumulate the parsed body fragment into the output buffer
    if (rsp->body_frag_start && rsp->body_frag_len > 0) {
        if (ctx->resp_buf) {
            // Check if we have enough space (saving 1 byte for the null terminator)
            if (ctx->bytes_written + rsp->body_frag_len < ctx->resp_buf_size) {
                memcpy(
                    ctx->resp_buf + ctx->bytes_written, rsp->body_frag_start, rsp->body_frag_len);
                ctx->bytes_written += rsp->body_frag_len;
            } else {
                ASTARTE_LOG_ERR("HTTP reply body exceeds provided buffer size.");
                ctx->request_ok = false;
                res = -1;
                goto exit;
            }
        }
    }

    if (final_data == HTTP_DATA_FINAL) {
        ASTARTE_LOG_DBG("All data received. Total payload size: %zu bytes", ctx->bytes_written);
        // Force null-termination safely
        if (ctx->resp_buf && ctx->resp_buf_size > 0) {
            ctx->resp_buf[ctx->bytes_written] = '\0';
        }
    }

exit:
    return res;
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
static int create_and_connect_socket(void);

#ifdef CONFIG_ASTARTE_DEVICE_SDK_HTTP_LOG_LEVEL_DBG
/**
 * @brief Print content of addrinfo struct.
 *
 * @param[in] input_addinfo addrinfo struct to print.
 */
static void dump_addrinfo(const struct zsock_addrinfo *input_addinfo);
#endif

static astarte_result_t astarte_http_do_request(enum http_method method, int32_t timeout_ms,
    const char *url, const char **header_fields, const char *payload, struct http_req_ctx *ctx);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_http_post(int32_t timeout_ms, const char *url, const char **header_fields,
    const char *payload, uint8_t *resp_buf, size_t resp_buf_size)
{
    struct http_req_ctx ctx = {
        .request_ok = true, .resp_buf = resp_buf, .resp_buf_size = resp_buf_size, .bytes_written = 0
    };

    return astarte_http_do_request(HTTP_POST, timeout_ms, url, header_fields, payload, &ctx);
}

astarte_result_t astarte_http_get(int32_t timeout_ms, const char *url, const char **header_fields,
    uint8_t *resp_buf, size_t resp_buf_size)
{
    struct http_req_ctx ctx = {
        .request_ok = true, .resp_buf = resp_buf, .resp_buf_size = resp_buf_size, .bytes_written = 0
    };

    return astarte_http_do_request(HTTP_GET, timeout_ms, url, header_fields, NULL, &ctx);
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static int create_and_connect_socket(void)
{
    char hostname[] = CONFIG_ASTARTE_DEVICE_SDK_HOSTNAME;
#ifdef CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_HTTP
    char port[] = "80";
#else
    char port[] = "443";
#endif
    struct zsock_addrinfo hints = { 0 };
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct zsock_addrinfo *broker_addrinfo = NULL;
    int getaddrinfo_rc = zsock_getaddrinfo(hostname, port, &hints, &broker_addrinfo);
    if (getaddrinfo_rc != 0) {
        ASTARTE_LOG_ERR("Unable to resolve address (%d) %s", getaddrinfo_rc,
            zsock_gai_strerror(getaddrinfo_rc));
        if (getaddrinfo_rc == DNS_EAI_SYSTEM) {
            ASTARTE_LOG_ERR("Errno: (%d) %s", errno, strerror(errno));
        }
        return -1;
    }

#ifdef CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_HTTP
    int proto = IPPROTO_TCP;
#else
    int proto = IPPROTO_TLS_1_2;
#endif

    int sock = -1;
    struct zsock_addrinfo *curr_addr = NULL;

    // Iterate through the linked list of resolved addresses
    for (curr_addr = broker_addrinfo; curr_addr != NULL; curr_addr = curr_addr->ai_next) {
#ifdef CONFIG_ASTARTE_DEVICE_SDK_HTTP_LOG_LEVEL_DBG
        dump_addrinfo(curr_addr);
#endif

        sock = zsock_socket(curr_addr->ai_family, curr_addr->ai_socktype, proto);
        if (sock == -1) {
            ASTARTE_LOG_DBG("Socket creation failed, trying next address...");
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

        int connect_rc = zsock_connect(sock, curr_addr->ai_addr, curr_addr->ai_addrlen);
        if (connect_rc == -1) {
            ASTARTE_LOG_DBG(
                "Connection failed (%d -  %s), trying next address...", errno, strerror(errno));
            zsock_close(sock);
            sock = -1;
            continue;
        }

        // If we reach here, we have successfully connected
        break;
    }

    // Free the linked list after we are done iterating
    zsock_freeaddrinfo(broker_addrinfo);

    // Check if we exhausted the list without a successful connection
    if (sock == -1) {
        ASTARTE_LOG_ERR("Failed to connect to any resolved address.");
    }

    return sock;
}

#ifdef CONFIG_ASTARTE_DEVICE_SDK_HTTP_LOG_LEVEL_DBG
#define ADDRINFO_IP_ADDR_SIZE 16U
static void dump_addrinfo(const struct zsock_addrinfo *input_addinfo)
{
    char ip_addr[ADDRINFO_IP_ADDR_SIZE] = { 0 };
    zsock_inet_ntop(AF_INET, &((struct sockaddr_in *) input_addinfo->ai_addr)->sin_addr, ip_addr,
        sizeof(ip_addr));
    ASTARTE_LOG_DBG("addrinfo @%p: ai_family=%d, ai_socktype=%d, ai_protocol=%d, "
                    "sa_family=%d, sin_port=%x, ip_addr=%s ai_addrlen=%zu",
        input_addinfo, input_addinfo->ai_family, input_addinfo->ai_socktype,
        input_addinfo->ai_protocol, input_addinfo->ai_addr->sa_family,
        ((struct sockaddr_in *) input_addinfo->ai_addr)->sin_port, ip_addr,
        input_addinfo->ai_addrlen);
}
#endif

static astarte_result_t astarte_http_do_request(enum http_method method, int32_t timeout_ms,
    const char *url, const char **header_fields, const char *payload, struct http_req_ctx *ctx)
{
    int sock = create_and_connect_socket();
    if (sock < 0) {
        return ASTARTE_RESULT_SOCKET_ERROR;
    }

    struct http_request req = { 0 };
    // TODO: This buffer is still allocated on the stack. Consider moving this to the heap to avoid
    // the stack overflow risk.
    uint8_t recv_buf[CONFIG_ASTARTE_DEVICE_SDK_ADVANCED_HTTP_RCV_BUFFER_SIZE];
    memset(recv_buf, 0, sizeof(recv_buf));

    req.method = method;
    req.host = CONFIG_ASTARTE_DEVICE_SDK_HOSTNAME;
#ifdef CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_HTTP
    req.port = "80";
#else
    req.port = "443";
#endif
    req.url = url;
    req.header_fields = header_fields;
    req.protocol = "HTTP/1.1";
    req.response = http_response_cb;
    if (payload) {
        req.content_type_value = "application/json";
        req.payload = payload;
        req.payload_len = strlen(payload);
    }
    req.recv_buf = recv_buf;
    req.recv_buf_len = sizeof(recv_buf);

    // Pass context struct as the user_data parameter
    int http_rc = http_client_req(sock, &req, timeout_ms, ctx);
    if ((http_rc < 0) || !ctx->request_ok) {
        ASTARTE_LOG_ERR("HTTP request failed: %d", http_rc);
        ASTARTE_LOG_ERR("Content: %s", (char *) recv_buf);
        zsock_close(sock);
        return ASTARTE_RESULT_HTTP_REQUEST_ERROR;
    }

    zsock_close(sock);
    return ASTARTE_RESULT_OK;
}
