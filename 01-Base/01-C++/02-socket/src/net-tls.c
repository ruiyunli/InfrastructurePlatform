/*
 * net-tls.c - TLS 1.3 传输后端（mbedTLS 4.1.0）
 *
 * 实现 net-backend.h 约定的全部函数。与 net-notls.c 的差异：
 * - 建立 TCP 连接后同步完成 TLS 1.3 握手（带超时）
 * - 读写走 mbedtls_ssl_read / mbedtls_ssl_write
 * - net_wait 需要额外检查 mbedTLS 内部已解密的数据
 *
 * 密码学策略由 mbedTLS 的编译配置决定（TLS 1.3、P-256、ECDSA、无 RSA），
 * 本文件不设置 TLS 版本，也不设置套件列表。
 */
#include "net-internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <fcntl.h>
#include <time.h>
#endif

#include "mbedtls/error.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/pk.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
#include "psa/crypto.h"

#include "net-backend.h"
#include "net-common.h"

/* 握手默认超时（毫秒） */
#define TLS_DEFAULT_HANDSHAKE_TIMEOUT_MS 5000

struct conn {
    sock_t fd;
    mbedtls_ssl_context ssl;
    int ssl_ready; /* TLS 握手是否已完成 */
};

/* ------------------------------------------------------------------ */
/* 全局 TLS 状态                                                       */
/* ------------------------------------------------------------------ */
static struct {
    int enabled;
    int role; /* MBEDTLS_SSL_IS_SERVER / MBEDTLS_SSL_IS_CLIENT / 0 */

    /* 由 net_tls_enable 深拷贝 */
    char *ca_file;
    char *cert_file;
    char *key_file;
    char *server_name;
    int verify_peer;
    int handshake_timeout_ms;

    /* 只在首次 net_listen / net_connect 时初始化，两个端点共享 */
    int config_ready;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt own_cert;
    mbedtls_x509_crt ca_chain;
    mbedtls_pk_context own_key;
} g_tls;

/* ------------------------------------------------------------------ */
/* 工具函数                                                            */
/* ------------------------------------------------------------------ */
static char *tls_strdup(const char *text)
{
    size_t len;
    char *copy;

    if (text == NULL) {
        return NULL;
    }
    len = strlen(text) + 1;
    copy = (char *)malloc(len);
    if (copy != NULL) {
        memcpy(copy, text, len);
    }
    return copy;
}

static void tls_free_strings(void)
{
    free(g_tls.ca_file);
    free(g_tls.cert_file);
    free(g_tls.key_file);
    free(g_tls.server_name);
    g_tls.ca_file = NULL;
    g_tls.cert_file = NULL;
    g_tls.key_file = NULL;
    g_tls.server_name = NULL;
}

static void tls_print_error(const char *what, int ret)
{
    char text[256];

    mbedtls_strerror(ret, text, sizeof(text));
    fprintf(stderr, "[tls] %s failed: -0x%04x (%s)\n",
            what, (unsigned int)(-ret), text);
}

static uint64_t tls_now_ms(void)
{
#if defined(_WIN32)
    return (uint64_t)GetTickCount64();
#else
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000U + (uint64_t)(ts.tv_nsec / 1000000L);
#endif
}

static void tls_sleep_ms(int timeout_ms)
{
    if (timeout_ms <= 0) {
        return;
    }
#if defined(_WIN32)
    Sleep((DWORD)timeout_ms);
#else
    {
        struct timespec ts;

        ts.tv_sec = timeout_ms / 1000;
        ts.tv_nsec = (long)(timeout_ms % 1000) * 1000000L;
        nanosleep(&ts, NULL);
    }
#endif
}

static int tls_set_nonblocking(sock_t fd, int enabled)
{
#if defined(_WIN32)
    u_long mode = enabled ? 1UL : 0UL;

    return (ioctlsocket(fd, FIONBIO, &mode) == 0) ? 0 : -1;
#else
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags < 0) {
        return -1;
    }
    if (enabled) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    return (fcntl(fd, F_SETFL, flags) == 0) ? 0 : -1;
#endif
}

/* 等待 fd 可读或可写：>0 就绪，0 超时，<0 错误 */
static int tls_wait_fd(sock_t fd, int for_write, int timeout_ms)
{
    fd_set readfds;
    fd_set writefds;
    struct timeval timeout;
    int ret;

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    if (for_write) {
        FD_SET(fd, &writefds);
    } else {
        FD_SET(fd, &readfds);
    }

    if (timeout_ms < 0) {
        timeout_ms = 0;
    }
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

#if defined(_WIN32)
    ret = select(0, for_write ? NULL : &readfds,
                 for_write ? &writefds : NULL, NULL, &timeout);
    if (ret < 0) {
        if (WSAGetLastError() == WSAEINTR) {
            return 0;
        }
        net_print_error("select");
        return -1;
    }
#else
    ret = select((int)(fd + 1), for_write ? NULL : &readfds,
                 for_write ? &writefds : NULL, NULL, &timeout);
    if (ret < 0) {
        if (errno == EINTR) {
            return 0;
        }
        net_print_error("select");
        return -1;
    }
#endif
    return ret;
}

/* ------------------------------------------------------------------ */
/* BIO 回调                                                            */
/* ------------------------------------------------------------------ */
static int tls_bio_send(void *ctx, const unsigned char *buf, size_t len)
{
    conn_t *conn = (conn_t *)ctx;
    int sent;

    if (len > 0x7fffffffU) {
        len = 0x7fffffffU;
    }
    sent = (int)send(conn->fd, (const char *)buf, (int)len, 0);
    if (sent >= 0) {
        return sent;
    }
    if (net_error_is_retryable()) {
        return MBEDTLS_ERR_SSL_WANT_WRITE;
    }
#if defined(_WIN32)
    if (WSAGetLastError() == WSAECONNRESET) {
        return MBEDTLS_ERR_NET_CONN_RESET;
    }
#else
    if (errno == EPIPE || errno == ECONNRESET) {
        return MBEDTLS_ERR_NET_CONN_RESET;
    }
#endif
    return MBEDTLS_ERR_NET_SEND_FAILED;
}

static int tls_bio_recv(void *ctx, unsigned char *buf, size_t len)
{
    conn_t *conn = (conn_t *)ctx;
    int received;

    if (len > 0x7fffffffU) {
        len = 0x7fffffffU;
    }
    received = (int)recv(conn->fd, (char *)buf, (int)len, 0);
    if (received >= 0) {
        /* 和 mbedtls_net_recv 一致：0 表示对端关闭 */
        return received;
    }
    if (net_error_is_retryable()) {
        return MBEDTLS_ERR_SSL_WANT_READ;
    }
#if defined(_WIN32)
    if (WSAGetLastError() == WSAECONNRESET) {
        return MBEDTLS_ERR_NET_CONN_RESET;
    }
#else
    if (errno == EPIPE || errno == ECONNRESET) {
        return MBEDTLS_ERR_NET_CONN_RESET;
    }
#endif
    return MBEDTLS_ERR_NET_RECV_FAILED;
}

/* ------------------------------------------------------------------ */
/* 证书与配置                                                          */
/* ------------------------------------------------------------------ */
static int tls_load_cert(mbedtls_x509_crt *chain, const char *path)
{
    int ret;

    if (path == NULL) {
        fprintf(stderr, "[tls] certificate file is not configured\n");
        return -1;
    }

    ret = mbedtls_x509_crt_parse_file(chain, path);
    if (ret < 0) {
        tls_print_error(path, ret);
        return -1;
    }
    if (ret > 0) {
        fprintf(stderr, "[tls] warning: %d certificate(s) in %s could not be parsed\n",
                ret, path);
    }
    return 0;
}

static int tls_load_own_credentials(void)
{
    int ret;

    if (tls_load_cert(&g_tls.own_cert, g_tls.cert_file) != 0) {
        return -1;
    }
    if (g_tls.key_file == NULL) {
        fprintf(stderr, "[tls] private key file is not configured\n");
        return -1;
    }

    ret = mbedtls_pk_parse_keyfile(&g_tls.own_key, g_tls.key_file, NULL);
    if (ret != 0) {
        tls_print_error(g_tls.key_file, ret);
        return -1;
    }

    ret = mbedtls_ssl_conf_own_cert(&g_tls.conf, &g_tls.own_cert, &g_tls.own_key);
    if (ret != 0) {
        tls_print_error("mbedtls_ssl_conf_own_cert", ret);
        return -1;
    }
    return 0;
}

static int tls_setup_server(void)
{
    int ret;

    if (g_tls.cert_file == NULL || g_tls.key_file == NULL) {
        fprintf(stderr, "[tls] server requires cert_file and key_file\n");
        return -1;
    }

    ret = mbedtls_ssl_config_defaults(&g_tls.conf,
                                      MBEDTLS_SSL_IS_SERVER,
                                      MBEDTLS_SSL_TRANSPORT_STREAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        tls_print_error("mbedtls_ssl_config_defaults", ret);
        return -1;
    }

    if (tls_load_own_credentials() != 0) {
        return -1;
    }

    if (g_tls.verify_peer) {
        if (tls_load_cert(&g_tls.ca_chain, g_tls.ca_file) != 0) {
            return -1;
        }
        mbedtls_ssl_conf_ca_chain(&g_tls.conf, &g_tls.ca_chain, NULL);
        mbedtls_ssl_conf_authmode(&g_tls.conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    } else {
        /* 单向认证：不要求客户端证书 */
        mbedtls_ssl_conf_authmode(&g_tls.conf, MBEDTLS_SSL_VERIFY_NONE);
    }
    return 0;
}

static int tls_setup_client(void)
{
    int ret;

    ret = mbedtls_ssl_config_defaults(&g_tls.conf,
                                      MBEDTLS_SSL_IS_CLIENT,
                                      MBEDTLS_SSL_TRANSPORT_STREAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        tls_print_error("mbedtls_ssl_config_defaults", ret);
        return -1;
    }

    if (g_tls.verify_peer) {
        if (tls_load_cert(&g_tls.ca_chain, g_tls.ca_file) != 0) {
            return -1;
        }
        mbedtls_ssl_conf_ca_chain(&g_tls.conf, &g_tls.ca_chain, NULL);
        mbedtls_ssl_conf_authmode(&g_tls.conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    } else {
        mbedtls_ssl_conf_authmode(&g_tls.conf, MBEDTLS_SSL_VERIFY_NONE);
    }

    /* 可选：为将来的双向认证预留，单向认证时这两个字段为 NULL */
    if (g_tls.cert_file != NULL || g_tls.key_file != NULL) {
        if (g_tls.cert_file == NULL || g_tls.key_file == NULL) {
            fprintf(stderr, "[tls] client cert_file and key_file must be set together\n");
            return -1;
        }
        if (tls_load_own_credentials() != 0) {
            return -1;
        }
    }
    return 0;
}

static int tls_ensure_config(int role)
{
    int ret;

    if (!g_tls.enabled) {
        fprintf(stderr,
                "[tls] net_tls_enable() must be called before net_listen/net_connect\n");
        return -1;
    }
    if (g_tls.role == role && g_tls.config_ready) {
        return 0;
    }
    if (g_tls.role != 0) {
        fprintf(stderr, "[tls] cannot use both server and client TLS roles in one process\n");
        return -1;
    }

    g_tls.role = role;
    if (role == MBEDTLS_SSL_IS_SERVER) {
        ret = tls_setup_server();
    } else {
        ret = tls_setup_client();
    }
    if (ret != 0) {
        g_tls.role = 0;
        return -1;
    }

    g_tls.config_ready = 1;
    return 0;
}

/* ------------------------------------------------------------------ */
/* 连接管理                                                            */
/* ------------------------------------------------------------------ */
static conn_t *conn_create(sock_t fd)
{
    conn_t *conn = (conn_t *)calloc(1, sizeof(*conn));

    if (conn == NULL) {
        fprintf(stderr, "out of memory\n");
        return NULL;
    }
    conn->fd = fd;
    mbedtls_ssl_init(&conn->ssl);
    return conn;
}

static int tls_handshake(conn_t *conn, int server, const char *hostname)
{
    int timeout_ms = g_tls.handshake_timeout_ms > 0
                     ? g_tls.handshake_timeout_ms
                     : TLS_DEFAULT_HANDSHAKE_TIMEOUT_MS;
    uint64_t deadline;
    int ret;

    ret = mbedtls_ssl_setup(&conn->ssl, &g_tls.conf);
    if (ret != 0) {
        tls_print_error("mbedtls_ssl_setup", ret);
        return -1;
    }

    if (!server) {
        ret = mbedtls_ssl_set_hostname(&conn->ssl, hostname);
        if (ret != 0) {
            tls_print_error("mbedtls_ssl_set_hostname", ret);
            return -1;
        }
    }

    mbedtls_ssl_set_bio(&conn->ssl, conn, tls_bio_send, tls_bio_recv, NULL);

    /* 握手期间使用非阻塞 socket，配合 select 实现总超时 */
    if (tls_set_nonblocking(conn->fd, 1) != 0) {
        fprintf(stderr, "[tls] failed to switch socket to non-blocking mode\n");
        return -1;
    }

    deadline = tls_now_ms() + (uint64_t)timeout_ms;
    for (;;) {
        uint64_t now;
        int wait_ret;

        ret = mbedtls_ssl_handshake(&conn->ssl);
        if (ret == 0) {
            break;
        }
        if (ret != MBEDTLS_ERR_SSL_WANT_READ &&
            ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            tls_print_error("TLS handshake", ret);
            break;
        }

        now = tls_now_ms();
        if (now >= deadline) {
            fprintf(stderr, "[tls] handshake timed out after %d ms\n", timeout_ms);
            ret = MBEDTLS_ERR_SSL_TIMEOUT;
            break;
        }

        wait_ret = tls_wait_fd(conn->fd,
                               ret == MBEDTLS_ERR_SSL_WANT_WRITE,
                               (int)(deadline - now));
        if (wait_ret < 0) {
            ret = MBEDTLS_ERR_NET_POLL_FAILED;
            break;
        }
        /* wait_ret == 0：超时或被信号打断，回到循环重新判断 deadline */
    }

    tls_set_nonblocking(conn->fd, 0);

    if (ret != 0) {
        return -1;
    }

    conn->ssl_ready = 1;

    if (!server && g_tls.verify_peer) {
        uint32_t flags = mbedtls_ssl_get_verify_result(&conn->ssl);

        if (flags != 0) {
            char info[512];

            mbedtls_x509_crt_verify_info(info, sizeof(info), "  ", flags);
            fprintf(stderr, "[tls] certificate verification failed (0x%08x):\n%s",
                    (unsigned int)flags, info);
            return -1;
        }
    }

    printf("[tls] handshake ok: %s / %s\n",
           mbedtls_ssl_get_version(&conn->ssl),
           mbedtls_ssl_get_ciphersuite(&conn->ssl));
    return 0;
}

conn_t *net_listen(const net_config_t *cfg)
{
    sock_t fd;
    conn_t *conn;

    if (tls_ensure_config(MBEDTLS_SSL_IS_SERVER) != 0) {
        return NULL;
    }

    fd = net_tcp_listen(cfg->host, cfg->port);
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
    sock_t fd;
    conn_t *conn;

    fd = net_tcp_accept(listener->fd, peer_ip, peer_ip_size, peer_port);
    if (fd == SOCK_INVALID) {
        return NULL;
    }

    conn = conn_create(fd);
    if (conn == NULL) {
        sock_close(fd);
        return NULL;
    }

    if (tls_handshake(conn, 1, NULL) != 0) {
        fprintf(stderr, "[tls] rejected connection from %s:%u\n",
                peer_ip != NULL ? peer_ip : "?",
                peer_port != NULL ? (unsigned int)*peer_port : 0U);
        net_close(conn);
        return NULL;
    }
    return conn;
}

conn_t *net_connect(const net_config_t *cfg)
{
    sock_t fd;
    conn_t *conn;
    const char *hostname;

    if (tls_ensure_config(MBEDTLS_SSL_IS_CLIENT) != 0) {
        return NULL;
    }

    fd = net_tcp_connect(cfg->host, cfg->port);
    if (fd == SOCK_INVALID) {
        return NULL;
    }

    conn = conn_create(fd);
    if (conn == NULL) {
        sock_close(fd);
        return NULL;
    }

    hostname = (g_tls.server_name != NULL) ? g_tls.server_name : cfg->host;
    if (tls_handshake(conn, 0, hostname) != 0) {
        net_close(conn);
        return NULL;
    }
    return conn;
}

void net_close(conn_t *conn)
{
    if (conn == NULL) {
        return;
    }

    if (conn->ssl_ready) {
        /* best effort：对端可能已经关闭，不关心返回值 */
        (void)mbedtls_ssl_close_notify(&conn->ssl);
    }
    mbedtls_ssl_free(&conn->ssl);
    sock_close(conn->fd);
    free(conn);
}

/* ------------------------------------------------------------------ */
/* 读写                                                                */
/* ------------------------------------------------------------------ */
int net_read(conn_t *conn, void *buf, size_t len)
{
    int ret;

    if (conn == NULL || !conn->ssl_ready) {
        return NET_RESULT_ERROR;
    }
    if (len == 0) {
        return 0;
    }
    if (len > 0x7fffffffU) {
        len = 0x7fffffffU;
    }

    ret = mbedtls_ssl_read(&conn->ssl, (unsigned char *)buf, len);
    if (ret > 0) {
        return ret;
    }
    if (ret == 0 || ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY ||
        ret == MBEDTLS_ERR_SSL_CONN_EOF) {
        return NET_RESULT_CLOSED;
    }
    if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
        return NET_RESULT_RETRY;
    }

    tls_print_error("TLS read", ret);
    return NET_RESULT_ERROR;
}

int net_write(conn_t *conn, const void *buf, size_t len)
{
    const unsigned char *data = (const unsigned char *)buf;
    size_t sent = 0;

    if (conn == NULL || !conn->ssl_ready) {
        return NET_RESULT_ERROR;
    }

    while (sent < len) {
        size_t remaining = len - sent;
        int ret;

        if (remaining > 0x7fffffffU) {
            remaining = 0x7fffffffU;
        }

        ret = mbedtls_ssl_write(&conn->ssl, data + sent, remaining);
        if (ret > 0) {
            sent += (size_t)ret;
            continue;
        }
        if (ret == 0) {
            return NET_RESULT_CLOSED;
        }
        if (ret == MBEDTLS_ERR_SSL_WANT_READ ||
            ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            /* 阻塞 socket 下极少出现；调用方按失败处理 */
            return NET_RESULT_RETRY;
        }

        tls_print_error("TLS write", ret);
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
    int buffered = 0;
    int has_fd = 0;
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
    if (listener_fd != SOCK_INVALID) {
        has_fd = 1;
    }

    for (i = 0; i < conn_count; i++) {
        fds[i] = SOCK_INVALID;
        if (conns[i] == NULL) {
            continue;
        }

        /*
         * mbedTLS 可能已经把数据留在内部缓冲区（已解密的记录、待处理的
         * 握手消息等），此时 socket 上可能没有新数据，select 不会通知。
         * check_pending 覆盖这些情况，有数据就直接标记可读，并从 select
         * 集合中排除，避免重复计数。
         */
        if (conns[i]->ssl_ready &&
            mbedtls_ssl_check_pending(&conns[i]->ssl) != 0) {
            result->ready[i] = 1;
            buffered++;
            continue;
        }

        fds[i] = conns[i]->fd;
        has_fd = 1;
    }

    if (!has_fd) {
        if (buffered > 0) {
            result->ready_count = buffered;
            return buffered;
        }
        /* 没有任何可等待的对象：按超时时间休眠，避免调用方忙等 */
        tls_sleep_ms(timeout_ms);
        return 0;
    }

    ready = net_select_fds(listener_fd, fds, conn_count, timeout_ms,
                           &result->listener_ready, result->ready);
    if (ready < 0) {
        return NET_RESULT_ERROR;
    }

    result->ready_count = ready + buffered;
    return result->ready_count;
}

/* ------------------------------------------------------------------ */
/* 后端钩子                                                            */
/* ------------------------------------------------------------------ */
int net_backend_init(void)
{
    psa_status_t status = psa_crypto_init();

    if (status != PSA_SUCCESS) {
        fprintf(stderr, "[tls] psa_crypto_init failed: %d\n", (int)status);
        return -1;
    }

    memset(&g_tls, 0, sizeof(g_tls));
    mbedtls_ssl_config_init(&g_tls.conf);
    mbedtls_x509_crt_init(&g_tls.own_cert);
    mbedtls_x509_crt_init(&g_tls.ca_chain);
    mbedtls_pk_init(&g_tls.own_key);
    return 0;
}

void net_backend_cleanup(void)
{
    mbedtls_ssl_config_free(&g_tls.conf);
    mbedtls_x509_crt_free(&g_tls.own_cert);
    mbedtls_x509_crt_free(&g_tls.ca_chain);
    mbedtls_pk_free(&g_tls.own_key);
    tls_free_strings();
    memset(&g_tls, 0, sizeof(g_tls));

    mbedtls_psa_crypto_free();
}

int net_tls_enable(const net_tls_config_t *cfg)
{
    char *ca_file;
    char *cert_file;
    char *key_file;
    char *server_name;

    if (cfg == NULL) {
        fprintf(stderr, "[tls] net_tls_enable: cfg is NULL\n");
        return -1;
    }

    ca_file = tls_strdup(cfg->ca_file);
    cert_file = tls_strdup(cfg->cert_file);
    key_file = tls_strdup(cfg->key_file);
    server_name = tls_strdup(cfg->server_name);

    if ((cfg->ca_file != NULL && ca_file == NULL) ||
        (cfg->cert_file != NULL && cert_file == NULL) ||
        (cfg->key_file != NULL && key_file == NULL) ||
        (cfg->server_name != NULL && server_name == NULL)) {
        free(ca_file);
        free(cert_file);
        free(key_file);
        free(server_name);
        fprintf(stderr, "[tls] out of memory\n");
        return -1;
    }

    tls_free_strings();
    g_tls.ca_file = ca_file;
    g_tls.cert_file = cert_file;
    g_tls.key_file = key_file;
    g_tls.server_name = server_name;
    g_tls.verify_peer = cfg->verify_peer;
    g_tls.handshake_timeout_ms = cfg->handshake_timeout_ms;
    g_tls.enabled = 1;

    return 0;
}
