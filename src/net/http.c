/*
 * http.c - HTTP 客户端 (最小实现)
 */
#include "network.h"
#include "../string.h"
#include "../vga.h"
#include "../timer.h"

/* DNS 解析 (简单: 硬编码常见域名) */
static int dns_resolve(const char *host, uint8_t *ip)
{
    /* 简单硬编码: 127.0.0.1 和 10.0.2.2 */
    if (strcmp(host, "localhost") == 0) {
        ip[0] = 127; ip[1] = 0; ip[2] = 0; ip[3] = 1;
        return 1;
    }
    if (strcmp(host, "gateway") == 0) {
        ip[0] = 10; ip[1] = 0; ip[2] = 2; ip[3] = 2;
        return 1;
    }
    if (strcmp(host, "example.com") == 0) {
        ip[0] = 93; ip[1] = 184; ip[2] = 215; ip[3] = 14;
        return 1;
    }
    if (strcmp(host, "httpbin.org") == 0) {
        ip[0] = 18; ip[1] = 207; ip[2] = 88; ip[3] = 68;
        return 1;
    }
    return 0;
}

/* 解析 URL: 返回 host 和 path */
static int parse_url(const char *url, char *host, int host_max, char *path, int path_max)
{
    /* 跳过 http:// */
    const char *p = url;
    if (strncmp(p, "http://", 7) == 0) p += 7;

    /* 提取 host */
    int i = 0;
    while (*p && *p != '/' && *p != ':' && i < host_max - 1) {
        host[i++] = *p++;
    }
    host[i] = '\0';

    /* 提取 path */
    if (*p == 0) {
        path[0] = '/'; path[1] = '\0';
    } else {
        int j = 0;
        while (*p && j < path_max - 1) {
            path[j++] = *p++;
        }
        path[j] = '\0';
    }

    return 1;
}

int http_get(const char *url, char *response, int max_len)
{
    char host[128], path[256];

    if (!parse_url(url, host, sizeof(host), path, sizeof(path))) {
        return -1;
    }

    uint8_t ip[4];
    if (!dns_resolve(host, ip)) {
        return -2; /* DNS 解析失败 */
    }

    int conn = tcp_connect(ip, 80);
    if (conn < 0) {
        return -3; /* 连接失败 */
    }

    /* 构建 HTTP 请求 */
    char req[1024];
    int req_len = snprintf_simple(req, sizeof(req),
        "GET %s HTTP/1.0\r\n"
        "Host: %s\r\n"
        "User-Agent: basic-browser/1.0\r\n"
        "Accept: text/html,text/plain\r\n"
        "Connection: close\r\n"
        "\r\n",
        path, host);

    tcp_send(conn, (uint8_t *)req, req_len);

    /* 接收响应 */
    int total = 0;
    int timeout = 0;

    while (total < max_len - 1 && timeout < 100) {
        int n = tcp_recv(conn, (uint8_t *)(response + total), max_len - total - 1);
        if (n > 0) {
            total += n;
            timeout = 0;
        } else if (n == 0) {
            timer_sleep(100);
            timeout++;
        } else {
            break; /* 连接关闭 */
        }
    }

    response[total] = '\0';
    tcp_close(conn);
    return total;
}