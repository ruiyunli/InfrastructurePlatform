/*
 * net-internal.h - 模块内部共享定义
 *
 * 只允许 net-common.c / net-notls.c / net-tls.c 包含。
 * 业务代码绝不能包含本文件，否则平台头文件会泄漏到业务编译单元。
 *
 * 内容：
 * 1. 平台头文件与 sock_t / mutex_t 等类型
 * 2. 平台初始化 / 清理
 * 3. 后端钩子（由 net-notls.c / net-tls.c 实现）
 * 4. 后端共用的裸 TCP 辅助函数与 select 封装
 */
#ifndef NET_INTERNAL_H
#define NET_INTERNAL_H

#include <stddef.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

typedef SOCKET sock_t;
typedef CRITICAL_SECTION mutex_t;
#define SOCK_INVALID INVALID_SOCKET
#define sock_close closesocket
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

typedef int sock_t;
typedef pthread_mutex_t mutex_t;
#define SOCK_INVALID (-1)
#define sock_close close
#endif

/* ------------------------------------------------------------------ */
/* 平台初始化 / 清理                                                   */
/* ------------------------------------------------------------------ */
int net_platform_init(void);
void net_platform_cleanup(void);

/* ------------------------------------------------------------------ */
/* 后端钩子：net_init / net_cleanup 会调用，业务层不可见               */
/* ------------------------------------------------------------------ */
int net_backend_init(void);
void net_backend_cleanup(void);

/* ------------------------------------------------------------------ */
/* 裸 TCP 辅助函数：两个后端共用                                       */
/* 失败返回 SOCK_INVALID，并已打印错误                                 */
/* ------------------------------------------------------------------ */
sock_t net_tcp_listen(const char *host, int port);
sock_t net_tcp_connect(const char *host, int port);
sock_t net_tcp_accept(sock_t listener, char *peer_ip, size_t peer_ip_size,
                      unsigned short *peer_port);

/* ------------------------------------------------------------------ */
/* select 封装                                                         */
/* - listener_fd 传 SOCK_INVALID 表示没有 listener                     */
/* - fds 中 SOCK_INVALID 表示跳过该项                                  */
/* - ready 与 fds 一一对应，调用方负责清零                             */
/* - 返回就绪数量；-1 表示错误；EINTR 返回 0                           */
/* ------------------------------------------------------------------ */
int net_select_fds(sock_t listener_fd, const sock_t *fds, int count,
                   int timeout_ms, int *listener_ready, int *ready);

/* 当前 socket 错误是否为可重试（EAGAIN / EWOULDBLOCK / EINTR） */
int net_error_is_retryable(void);

#endif /* NET_INTERNAL_H */
