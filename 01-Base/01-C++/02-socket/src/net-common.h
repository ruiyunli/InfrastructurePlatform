/*
 * net-common.h - 公共模块接口
 *
 * 与传输方式无关的部分：生命周期、行缓冲、标准输入读取。
 * 依赖 net-backend.h 中的 conn_t 与返回值常量。
 */
#ifndef NET_COMMON_H
#define NET_COMMON_H

#include <stddef.h>

#include "net-backend.h"

#define NET_LINE_SIZE 512

/* line_reader_next 返回值 */
#define LINE_READER_LINE 1
#define LINE_READER_NONE 0

/* ------------------------------------------------------------------ */
/* 生命周期                                                            */
/* ------------------------------------------------------------------ */
int net_init(void);
void net_cleanup(void);
void net_print_error(const char *what);

/* ------------------------------------------------------------------ */
/* 按行发送                                                            */
/* ------------------------------------------------------------------ */
int net_write_line(conn_t *conn, const char *text);

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

#endif /* NET_COMMON_H */
