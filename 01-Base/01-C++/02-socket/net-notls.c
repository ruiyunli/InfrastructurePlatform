/*
 * net-notls.c - 纯 TCP 传输后端
 *
 * 实现 net-backend.h 约定的全部函数，不依赖任何第三方库。
 * 平台差异与裸 TCP 操作由 net-common.c 提供。
 */
#include "net-internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net-backend.h"
#include "net-common.h"

struct conn {
    sock_t fd;
};

/* ------------------------------------------------------------------ */
/* 连接管理                                                            */
/* ------------------------------------------------------------------ */
static conn_t *conn_create(sock_t fd)
{
    conn_t *conn = (conn_t *)malloc(sizeof(*conn));

    if (conn == NULL) {
        fprintf(stderr, "out of memory\n");
        return NULL;
    }
    conn->fd = fd;
    return conn;
}

conn_t *net_listen(const net_config_t *cfg)
{
    sock_t fd = net_tcp_listen(cfg->host, cfg->port);
    conn_t *conn;

    if (fd == SOCK_INVALID) {
        return NULL;
    }
    conn = conn_create(fd);
    if (conn == NULL) {
        sock_close(fd);
    }
    return conn;
}

conn_t *net_accept(conn_t *listener, char *peer_ip, size_t peer_ip_size,
                   unsigned short *peer_port)
{
    sock_t fd = net_tcp_accept(listener->fd, peer_ip, peer_ip_size, peer_port);
    conn_t *conn;

    if (fd == SOCK_INVALID) {
        return NULL;
    }
    conn = conn_create(fd);
    if (conn == NULL) {
        sock_close(fd);
    }
    return conn;
}

conn_t *net_connect(const net_config_t *cfg)
{
    sock_t fd = net_tcp_connect(cfg->host, cfg->port);
    conn_t *conn;

    if (fd == SOCK_INVALID) {
        return NULL;
    }
    conn = conn_create(fd);
    if (conn == NULL) {
        sock_close(fd);
    }
    return conn;
}

void net_close(conn_t *conn)
{
    if (conn == NULL) {
        return;
    }
    sock_close(conn->fd);
    free(conn);
}

/* ------------------------------------------------------------------ */
/* 读写                                                                */
/* ------------------------------------------------------------------ */
int net_read(conn_t *conn, void *buf, size_t len)
{
    int n;

    if (len > 0x7fffffffU) {
        len = 0x7fffffffU;
    }
    n = (int)recv(conn->fd, (char *)buf, (int)len, 0);
    if (n > 0) {
        return n;
    }
    if (n == 0) {
        return NET_RESULT_CLOSED;
    }
    if (net_error_is_retryable()) {
        return NET_RESULT_RETRY;
    }
    return NET_RESULT_ERROR;
}

int net_write(conn_t *conn, const void *buf, size_t len)
{
    size_t sent = 0;
    const char *data = (const char *)buf;

    while (sent < len) {
        size_t remaining = len - sent;
        int n;

        if (remaining > 0x7fffffffU) {
            remaining = 0x7fffffffU;
        }
        n = (int)send(conn->fd, data + sent, (int)remaining, 0);
        if (n > 0) {
            sent += (size_t)n;
            continue;
        }
        if (n == 0) {
            return NET_RESULT_CLOSED;
        }
        if (net_error_is_retryable()) {
            return NET_RESULT_RETRY;
        }
        return NET_RESULT_ERROR;
    }
    return (int)sent;
}

/* ------------------------------------------------------------------ */
/* 等待可读                                                            */
/* ------------------------------------------------------------------ */
int net_wait(net_wait_result_t *result, conn_t *listener,
             conn_t *const *conns, int conn_count, int timeout_ms)
{
    sock_t fds[NET_MAX_WAIT_CONNS];
    sock_t listener_fd;
    int ready;
    int i;

    memset(result, 0, sizeof(*result));
    if (conn_count > NET_MAX_WAIT_CONNS) {
        conn_count = NET_MAX_WAIT_CONNS;
    }
    if (conn_count < 0) {
        conn_count = 0;
    }

    listener_fd = (listener != NULL) ? listener->fd : SOCK_INVALID;
    for (i = 0; i < conn_count; i++) {
        fds[i] = (conns[i] != NULL) ? conns[i]->fd : SOCK_INVALID;
    }

    ready = net_select_fds(listener_fd, fds, conn_count, timeout_ms,
                           &result->listener_ready, result->ready);
    if (ready < 0) {
        return NET_RESULT_ERROR;
    }
    result->ready_count = ready;
    return ready;
}

/* ------------------------------------------------------------------ */
/* 后端钩子                                                            */
/* ------------------------------------------------------------------ */
int net_backend_init(void)
{
    return 0;
}

void net_backend_cleanup(void)
{
}

/* 纯 TCP 构建：接受 TLS 配置调用，但不做任何事 */
int net_tls_enable(const net_tls_config_t *cfg)
{
    (void)cfg;
    return 0;
}
