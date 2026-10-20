/*
 * net.h - 跨平台网络工具模块
 *
 * 职责：
 * 1. 屏蔽 Windows Winsock / Linux POSIX socket 差异
 * 2. 提供 conn_t 连接抽象，未来可在此层接入 TLS
 * 3. 提供行缓冲（line_reader）解决 TCP 粘包 / 半包
 * 4. 提供跨平台的标准输入读取线程
 *
 * conn_t 是不透明类型，业务代码不需要包含任何平台头文件。
 */
#ifndef NET_H
#define NET_H

#include <stddef.h>

#define NET_LINE_SIZE 512
#define NET_MAX_WAIT_CONNS 16

/* net_read / net_write 返回值 */
#define NET_RESULT_CLOSED 0
#define NET_RESULT_RETRY (-1)
#define NET_RESULT_ERROR (-2)

/* line_reader_next 返回值 */
#define LINE_READER_LINE 1
#define LINE_READER_NONE 0

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

/* ------------------------------------------------------------------ */
/* 生命周期                                                            */
/* ------------------------------------------------------------------ */
int net_init(void);
void net_cleanup(void);
void net_print_error(const char *what);

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
int net_write_line(conn_t *conn, const char *text);

/* ------------------------------------------------------------------ */
/* 等待可读：listener 可以为 NULL；返回就绪数量，<0 表示错误            */
/* ------------------------------------------------------------------ */
int net_wait(net_wait_result_t *result, conn_t *listener,
             conn_t *const *conns, int conn_count, int timeout_ms);

/* ------------------------------------------------------------------ */
/* 行缓冲：调用 feed 读取一次数据，再循环调用 next 取出完整行           */
/* ------------------------------------------------------------------ */
typedef struct {
    char buf[NET_LINE_SIZE];
    size_t len;
    int discarding; /* 正在丢弃超长行的剩余部分 */
    int eof;        /* 对端已关闭 */
} line_reader_t;

void line_reader_init(line_reader_t *reader);

/* 从连接读取一次数据到缓冲区 */
int line_reader_feed(line_reader_t *reader, conn_t *conn);

/* 从缓冲区取出一个完整行：1=取到一行；0=没有完整行 */
int line_reader_next(line_reader_t *reader, char *out, size_t out_size);

/* ------------------------------------------------------------------ */
/* 标准输入读取（后台线程 + 加锁队列）                                  */
/* ------------------------------------------------------------------ */
typedef struct input_reader input_reader_t;

input_reader_t *input_reader_start(void);
int input_reader_pop(input_reader_t *reader, char *out, size_t out_size);
int is_quit_command(const char *text);

#endif /* NET_H */
