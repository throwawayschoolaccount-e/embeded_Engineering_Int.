// http_server.cpp
#include "http_server.h"
#include "lwip/tcp.h"
#include "pico/cyw43_arch.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>

namespace gateway {

namespace {

constexpr size_t MAX_FIRMWARE_SIZE = 512 * 1024; // must match STM32 bank size
constexpr size_t MAX_HEADER_SIZE   = 1024;

UartLink*       g_link   = nullptr;
TransferStatus* g_status = nullptr;

struct ConnState {
    char     header_buf[MAX_HEADER_SIZE];
    size_t   header_len = 0;
    bool     headers_done = false;

    uint8_t* body_buf = nullptr;
    size_t   body_expected = 0;
    size_t   body_received = 0;

    uint8_t  signature[64] = {0};
    bool     is_post_update = false;
    bool     is_get_status  = false;
};

// Parses "X-Signature: <128 hex chars>" out of the header block.
bool parse_signature(const char* headers, uint8_t out_sig[64]) {
    const char* p = strstr(headers, "X-Signature:");
    if (!p) return false;
    p += strlen("X-Signature:");
    while (*p == ' ') ++p;

    for (int i = 0; i < 64; ++i) {
        auto hex_val = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        int hi = hex_val(p[i * 2]);
        int lo = hex_val(p[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        out_sig[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return true;
}

size_t parse_content_length(const char* headers) {
    const char* p = strstr(headers, "Content-Length:");
    if (!p) return 0;
    return static_cast<size_t>(strtoul(p + strlen("Content-Length:"), nullptr, 10));
}

void send_response(struct tcp_pcb* tpcb, int code, const char* status_text,
                    const char* content_type, const char* body) {
    char header[256];
    size_t body_len = body ? strlen(body) : 0;
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",
        code, status_text, content_type, body_len);

    tcp_write(tpcb, header, header_len, TCP_WRITE_FLAG_COPY);
    if (body_len) {
        tcp_write(tpcb, body, body_len, TCP_WRITE_FLAG_COPY);
    }
    tcp_output(tpcb);
}

void finish_connection(struct tcp_pcb* tpcb, ConnState* cs) {
    if (cs->body_buf) {
        free(cs->body_buf);
    }
    tcp_arg(tpcb, nullptr);
    tcp_recv(tpcb, nullptr);
    tcp_close(tpcb);
    delete cs;
}

err_t on_recv(void* arg, struct tcp_pcb* tpcb, struct pbuf* p, err_t err) {
    auto* cs = static_cast<ConnState*>(arg);

    if (!p) {
        // Remote closed the connection.
        finish_connection(tpcb, cs);
        return ERR_OK;
    }

    tcp_recved(tpcb, p->tot_len);

    size_t offset = 0;
    struct pbuf* walker = p;
    while (walker) {
        const char* chunk = static_cast<const char*>(walker->payload);
        size_t chunk_len = walker->len;
        size_t pos = 0;

        // --- Header parsing phase ---
        if (!cs->headers_done) {
            while (pos < chunk_len && cs->header_len < MAX_HEADER_SIZE - 1) {
                cs->header_buf[cs->header_len++] = chunk[pos++];
                if (cs->header_len >= 4 &&
                    memcmp(&cs->header_buf[cs->header_len - 4], "\r\n\r\n", 4) == 0) {
                    cs->header_buf[cs->header_len] = '\0';
                    cs->headers_done = true;

                    cs->is_post_update = strncmp(cs->header_buf, "POST /update", 12) == 0;
                    cs->is_get_status  = strncmp(cs->header_buf, "GET /status", 11) == 0;

                    if (cs->is_post_update) {
                        cs->body_expected = parse_content_length(cs->header_buf);
                        if (cs->body_expected == 0 || cs->body_expected > MAX_FIRMWARE_SIZE ||
                            !parse_signature(cs->header_buf, cs->signature)) {
                            send_response(tpcb, 400, "Bad Request", "text/plain",
                                          "missing/invalid Content-Length or X-Signature");
                            pbuf_free(p);
                            finish_connection(tpcb, cs);
                            return ERR_OK;
                        }
                        cs->body_buf = static_cast<uint8_t*>(malloc(cs->body_expected));
                        if (!cs->body_buf) {
                            send_response(tpcb, 507, "Insufficient Storage", "text/plain", "no memory");
                            pbuf_free(p);
                            finish_connection(tpcb, cs);
                            return ERR_OK;
                        }
                    } else if (cs->is_get_status) {
                        char json[160];
                        snprintf(json, sizeof(json),
                            "{\"in_progress\":%s,\"sent\":%u,\"total\":%u,\"ok\":%s,\"message\":\"%s\"}",
                            g_status->in_progress ? "true" : "false",
                            g_status->bytes_sent, g_status->bytes_total,
                            g_status->last_result_ok ? "true" : "false",
                            g_status->last_message);
                        send_response(tpcb, 200, "OK", "application/json", json);
                        pbuf_free(p);
                        finish_connection(tpcb, cs);
                        return ERR_OK;
                    } else {
                        send_response(tpcb, 404, "Not Found", "text/plain", "unknown route");
                        pbuf_free(p);
                        finish_connection(tpcb, cs);
                        return ERR_OK;
                    }
                    break;
                }
            }
        }

        // --- Body accumulation phase ---
        if (cs->headers_done && cs->is_post_update) {
            while (pos < chunk_len && cs->body_received < cs->body_expected) {
                cs->body_buf[cs->body_received++] = chunk[pos++];
            }

            if (cs->body_received == cs->body_expected) {
                g_status->in_progress = true;
                g_status->bytes_total = cs->body_expected;

                auto progress_cb = [](uint32_t sent, uint32_t total) {
                    g_status->bytes_sent = sent;
                };

                UartResult result = g_link->send_firmware(
                    cs->body_buf, cs->body_expected, cs->signature, progress_cb);

                g_status->in_progress = false;
                g_status->last_result_ok = (result == UartResult::OK);
                snprintf(g_status->last_message, sizeof(g_status->last_message),
                         result == UartResult::OK ? "update delivered" :
                         result == UartResult::REJECTED ? "STM32 rejected update" :
                         "UART link error");

                if (result == UartResult::OK) {
                    send_response(tpcb, 200, "OK", "text/plain", "firmware delivered and accepted");
                } else {
                    send_response(tpcb, 502, "Bad Gateway", "text/plain", "STM32 did not accept firmware");
                }
                pbuf_free(p);
                finish_connection(tpcb, cs);
                return ERR_OK;
            }
        }

        walker = walker->next;
    }

    pbuf_free(p);
    return ERR_OK;
}

err_t on_accept(void* arg, struct tcp_pcb* newpcb, err_t err) {
    auto* cs = new ConnState();
    tcp_arg(newpcb, cs);
    tcp_recv(newpcb, on_recv);
    return ERR_OK;
}

} 

void http_server_start(UartLink* link, TransferStatus* status) {
    g_link = link;
    g_status = status;

    struct tcp_pcb* pcb = tcp_new();
    tcp_bind(pcb, IP_ADDR_ANY, 80);
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, on_accept);
}

} 
