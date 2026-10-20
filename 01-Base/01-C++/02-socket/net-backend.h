/*
 * net-backend.h - 传输后端契约
 *
 * 由 net-notls.c（纯 TCP）和 net-tls.c（TLS 1.3）分别实现，
 * 两个后端必须实现完全相同的函数集合。
 *
 * 本文件不包含任何平台头文件，业务代码可以安全包含。
 */
#ifndef NET_BACKEND_H
#define NET_BACKEND_H

#include <stddef.h>

#define NET_MAX_WAIT_CONNS 16

/* net_read / net_write 返回值 */
#define NET_RESULT_CLOSED 0
#define NET_RESULT_RETRY (-1)
#define NET_RESULT_ERROR (-2)

/* 不透明连接类型 */
typedef struct conn conn_t;

typedef struct {
    const char *host;
    int port;
} net_config_t;

typedef struct {
    int listener_ready;
    int ready[NET_MAX_WAIT_CONNS];
    int ready_count;
} net_wait_result_t;

/* TLS 配置：纯 TCP 后端会忽略该结构 */
typedef struct {
    const char *ca_file;      /* 校验对端证书时使用 */
    const char *cert_file;    /* 本端证书链 */
    const char *key_file;     /* 本端私钥 */
    int verify_peer;          /* 0=不校验 1=校验 */
    int handshake_timeout_ms; /* 握手超时，<=0 使用默认值 */
} net_tls_config_t;

/* ------------------------------------------------------------------ */
/* 连接管理                                                            */
/* ------------------------------------------------------------------ */
conn_t *net_listen(const net_config_t *cfg);
conn_t *net_accept(conn_t *listener, char *peer_ip, size_t peer_ip_size,
                   unsigned short *peer_port);
conn_t *net_connect(const net_config_t *cfg);
void net_close(conn_t *conn);

/* ------------------------------------------------------------------ */
/* 读写                                                                */
/* 返回：>0 实际字节数；NET_RESULT_CLOSED；NET_RESULT_RETRY；NET_RESULT_ERROR */
/* ------------------------------------------------------------------ */
int net_read(conn_t *conn, void *buf, size_t len);
int net_write(conn_t *conn, const void *buf, size_t len);

/* ------------------------------------------------------------------ */
/* 等待可读：listener 可以为 NULL；返回就绪数量，<0 表示错误            */
/* ------------------------------------------------------------------ */
int net_wait(net_wait_result_t *result, conn_t *listener,
             conn_t *const *conns, int conn_count, int timeout_ms);

/* ------------------------------------------------------------------ */
/* 启用 TLS：必须在 net_listen / net_connect 之前调用                  */
/* 纯 TCP 构建中为空实现，返回 0                                       */
/* ------------------------------------------------------------------ */
int net_tls_enable(const net_tls_config_t *cfg);

#endif /* NET_BACKEND_H */
