/*
 * net.c - 跨平台网络工具模块实现
 *
 * 平台差异全部集中在这里。未来接入 TLS 时：
 * - 在 conn 结构中加入 TLS 会话
 * - 在 net_connect / net_accept / net_read / net_write / net_close
 *   中替换为 TLS 对应调用
 * - 在 net_wait 中处理 TLS 内部缓冲（SSL_pending）
 * line_reader 和业务代码不需要修改。
 */
#include "net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#define INPUT_QUEUE_CAPACITY 32

struct conn {
    sock_t fd;
    /* 未来 TLS：TLS 会话对象可以放在这里 */
};

/* ------------------------------------------------------------------ */
/* 生命周期与错误处理                                                  */
/* ------------------------------------------------------------------ */
int net_init(void)
{
#if defined(_WIN32)
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed: %d\n", WSAGetLastError());
        return -1;
    }
    /* 让控制台按 UTF-8 显示中文 */
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#else
    /* 对端关闭后继续 send 不要终止进程 */
    signal(SIGPIPE, SIG_IGN);
#endif
    return 0;
}

void net_cleanup(void)
{
#if defined(_WIN32)
    WSACleanup();
#endif
}

void net_print_error(const char *what)
{
#if defined(_WIN32)
    fprintf(stderr, "%s failed: %d\n", what, WSAGetLastError());
#else
    fprintf(stderr, "%s failed: %s\n", what, strerror(errno));
#endif
}

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
    sock_t fd;
    struct sockaddr_in addr;
    int reuse = 1;
    conn_t *conn;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == SOCK_INVALID) {
        net_print_error("socket");
        return NULL;
    }

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)cfg->port);
    if (inet_pton(AF_INET, cfg->host, &addr.sin_addr) != 1) {
        fprintf(stderr, "invalid listen address: %s\n", cfg->host);
        sock_close(fd);
        return NULL;
    }

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        net_print_error("bind");
        sock_close(fd);
        return NULL;
    }

    if (listen(fd, NET_MAX_WAIT_CONNS) != 0) {
        net_print_error("listen");
        sock_close(fd);
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
    struct sockaddr_in peer;
#if defined(_WIN32)
    int peer_len = (int)sizeof(peer);
#else
    socklen_t peer_len = (socklen_t)sizeof(peer);
#endif
    sock_t fd;
    conn_t *conn;

    fd = accept(listener->fd, (struct sockaddr *)&peer, &peer_len);
    if (fd == SOCK_INVALID) {
        net_print_error("accept");
        return NULL;
    }

    conn = conn_create(fd);
    if (conn == NULL) {
        sock_close(fd);
        return NULL;
    }

    if (peer_ip != NULL && peer_ip_size > 0) {
#if defined(_WIN32)
        if (inet_ntop(AF_INET, &peer.sin_addr, peer_ip, peer_ip_size) == NULL) {
#else
        if (inet_ntop(AF_INET, &peer.sin_addr, peer_ip, (socklen_t)peer_ip_size) == NULL) {
#endif
            strncpy(peer_ip, "?", peer_ip_size - 1);
            peer_ip[peer_ip_size - 1] = '\0';
        }
    }
    if (peer_port != NULL) {
        *peer_port = (unsigned short)ntohs(peer.sin_port);
    }
    return conn;
}

conn_t *net_connect(const net_config_t *cfg)
{
    sock_t fd;
    struct sockaddr_in addr;
    conn_t *conn;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == SOCK_INVALID) {
        net_print_error("socket");
        return NULL;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)cfg->port);
    if (inet_pton(AF_INET, cfg->host, &addr.sin_addr) != 1) {
        fprintf(stderr, "invalid server address: %s\n", cfg->host);
        sock_close(fd);
        return NULL;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        net_print_error("connect");
        sock_close(fd);
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
    /* 未来 TLS：这里先执行 TLS 关闭通知（close_notify） */
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

#if defined(_WIN32)
    if (WSAGetLastError() == WSAEWOULDBLOCK || WSAGetLastError() == WSAEINTR) {
        return NET_RESULT_RETRY;
    }
#else
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        return NET_RESULT_RETRY;
    }
#endif
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

#if defined(_WIN32)
        if (WSAGetLastError() == WSAEWOULDBLOCK || WSAGetLastError() == WSAEINTR) {
            return NET_RESULT_RETRY;
        }
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return NET_RESULT_RETRY;
        }
#endif
        return NET_RESULT_ERROR;
    }
    return (int)sent;
}

int net_write_line(conn_t *conn, const char *text)
{
    char buffer[NET_LINE_SIZE + 2];
    int len = snprintf(buffer, sizeof(buffer), "%s\n", text);

    if (len < 0) {
        return NET_RESULT_ERROR;
    }
    if ((size_t)len >= sizeof(buffer)) {
        len = (int)sizeof(buffer) - 1;
    }
    return net_write(conn, buffer, (size_t)len);
}

/* ------------------------------------------------------------------ */
/* 等待可读                                                            */
/* ------------------------------------------------------------------ */
int net_wait(net_wait_result_t *result, conn_t *listener,
             conn_t *const *conns, int conn_count, int timeout_ms)
{
    fd_set readfds;
    struct timeval timeout;
    int ready;
    int i;
#if !defined(_WIN32)
    sock_t max_fd = 0;
#endif

    memset(result, 0, sizeof(*result));
    if (conn_count > NET_MAX_WAIT_CONNS) {
        conn_count = NET_MAX_WAIT_CONNS;
    }

    FD_ZERO(&readfds);
    if (listener != NULL) {
        FD_SET(listener->fd, &readfds);
#if !defined(_WIN32)
        max_fd = listener->fd;
#endif
    }
    for (i = 0; i < conn_count; i++) {
        if (conns[i] == NULL) {
            continue;
        }
        FD_SET(conns[i]->fd, &readfds);
#if !defined(_WIN32)
        if (conns[i]->fd > max_fd) {
            max_fd = conns[i]->fd;
        }
#endif
    }

    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

#if defined(_WIN32)
    ready = select(0, &readfds, NULL, NULL, &timeout);
#else
    ready = select((int)(max_fd + 1), &readfds, NULL, NULL, &timeout);
#endif
    if (ready < 0) {
#if defined(_WIN32)
        if (WSAGetLastError() == WSAEINTR) {
            return 0;
        }
#else
        if (errno == EINTR) {
            return 0;
        }
#endif
        net_print_error("select");
        return NET_RESULT_ERROR;
    }
    if (ready == 0) {
        return 0;
    }

    if (listener != NULL && FD_ISSET(listener->fd, &readfds)) {
        result->listener_ready = 1;
        result->ready_count++;
    }
    for (i = 0; i < conn_count; i++) {
        if (conns[i] != NULL && FD_ISSET(conns[i]->fd, &readfds)) {
            result->ready[i] = 1;
            result->ready_count++;
        }
    }

    /* 未来 TLS：如果 TLS 内部还有已解密数据，即使 select 未报可读，
     * 也应把这些连接标记为可读（SSL_pending）。 */
    return result->ready_count;
}

/* ------------------------------------------------------------------ */
/* 行缓冲                                                              */
/* ------------------------------------------------------------------ */
static void line_reader_consume(line_reader_t *reader, size_t count)
{
    if (count >= reader->len) {
        reader->len = 0;
        return;
    }
    memmove(reader->buf, reader->buf + count, reader->len - count);
    reader->len -= count;
}

static void copy_line_bytes(const char *src, size_t src_len,
                            char *out, size_t out_size)
{
    size_t copy_len = src_len;

    if (copy_len > out_size - 1) {
        copy_len = out_size - 1;
    }
    memcpy(out, src, copy_len);
    out[copy_len] = '\0';
    if (copy_len > 0 && out[copy_len - 1] == '\r') {
        out[copy_len - 1] = '\0';
    }
}

void line_reader_init(line_reader_t *reader)
{
    reader->len = 0;
    reader->discarding = 0;
    reader->eof = 0;
}

int line_reader_feed(line_reader_t *reader, conn_t *conn)
{
    int n;

    if (reader->eof) {
        return NET_RESULT_CLOSED;
    }
    if (reader->len >= sizeof(reader->buf)) {
        /* 调用方应先用 line_reader_next 消费缓冲 */
        return NET_RESULT_RETRY;
    }

    n = net_read(conn, reader->buf + reader->len, sizeof(reader->buf) - reader->len);
    if (n > 0) {
        reader->len += (size_t)n;
        return n;
    }
    if (n == NET_RESULT_CLOSED) {
        reader->eof = 1;
    }
    return n;
}

int line_reader_next(line_reader_t *reader, char *out, size_t out_size)
{
    if (out_size == 0) {
        return LINE_READER_NONE;
    }

    for (;;) {
        char *newline = (char *)memchr(reader->buf, '\n', reader->len);

        if (newline != NULL) {
            size_t line_len = (size_t)(newline - reader->buf);
            size_t consumed = line_len + 1;

            if (reader->discarding) {
                /* 超长行的结尾：丢弃整行，继续看缓冲里是否还有下一行 */
                reader->discarding = 0;
                line_reader_consume(reader, consumed);
                continue;
            }
            if (line_len == 0) {
                /* 忽略空行 */
                line_reader_consume(reader, consumed);
                continue;
            }

            /* 只复制当前行；输出缓冲不足时截断 */
            copy_line_bytes(reader->buf, line_len, out, out_size);
            line_reader_consume(reader, consumed);
            return LINE_READER_LINE;
        }

        if (reader->discarding) {
            /* 还没有遇到换行，丢弃已缓存部分，等待下一次 feed */
            reader->len = 0;
            return LINE_READER_NONE;
        }
        if (reader->len >= sizeof(reader->buf)) {
            /* 一行超出缓冲：开始丢弃，直到遇到换行 */
            reader->discarding = 1;
            reader->len = 0;
            return LINE_READER_NONE;
        }
        if (reader->eof && reader->len > 0) {
            /* 对端关闭时，把剩余内容作为最后一行返回 */
            copy_line_bytes(reader->buf, reader->len, out, out_size);
            reader->len = 0;
            return LINE_READER_LINE;
        }
        return LINE_READER_NONE;
    }
}

/* ------------------------------------------------------------------ */
/* 标准输入读取                                                        */
/* ------------------------------------------------------------------ */
struct input_reader {
    mutex_t lock;
    char lines[INPUT_QUEUE_CAPACITY][NET_LINE_SIZE];
    int head;
    int tail;
    int count;
};

static void input_reader_push(input_reader_t *reader, const char *line)
{
#if defined(_WIN32)
    EnterCriticalSection(&reader->lock);
#else
    pthread_mutex_lock(&reader->lock);
#endif
    if (reader->count < INPUT_QUEUE_CAPACITY) {
        strncpy(reader->lines[reader->tail], line, NET_LINE_SIZE - 1);
        reader->lines[reader->tail][NET_LINE_SIZE - 1] = '\0';
        reader->tail = (reader->tail + 1) % INPUT_QUEUE_CAPACITY;
        reader->count++;
    }
    /* 队列满时丢弃最新输入，避免阻塞网络循环 */
#if defined(_WIN32)
    LeaveCriticalSection(&reader->lock);
#else
    pthread_mutex_unlock(&reader->lock);
#endif
}

int input_reader_pop(input_reader_t *reader, char *out, size_t out_size)
{
    int got = 0;

    if (out_size == 0) {
        return 0;
    }

#if defined(_WIN32)
    EnterCriticalSection(&reader->lock);
#else
    pthread_mutex_lock(&reader->lock);
#endif
    if (reader->count > 0) {
        strncpy(out, reader->lines[reader->head], out_size - 1);
        out[out_size - 1] = '\0';
        reader->head = (reader->head + 1) % INPUT_QUEUE_CAPACITY;
        reader->count--;
        got = 1;
    }
#if defined(_WIN32)
    LeaveCriticalSection(&reader->lock);
#else
    pthread_mutex_unlock(&reader->lock);
#endif
    return got;
}

static void trim_line_end(char *text)
{
    size_t len = strlen(text);

    while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r')) {
        text[--len] = '\0';
    }
}

static int iequals(const char *a, const char *b)
{
#if defined(_WIN32)
    return _stricmp(a, b) == 0;
#else
    return strcasecmp(a, b) == 0;
#endif
}

int is_quit_command(const char *text)
{
    return iequals(text, "exit") || iequals(text, "quit") || iequals(text, "q");
}

#if defined(_WIN32)
static DWORD WINAPI input_thread_proc(LPVOID arg)
#else
static void *input_thread_proc(void *arg)
#endif
{
    input_reader_t *reader = (input_reader_t *)arg;
    char line[NET_LINE_SIZE];

    while (fgets(line, sizeof(line), stdin) != NULL) {
        trim_line_end(line);
        if (line[0] != '\0') {
            input_reader_push(reader, line);
        }
    }

#if defined(_WIN32)
    return 0;
#else
    return NULL;
#endif
}

input_reader_t *input_reader_start(void)
{
    input_reader_t *reader;

    reader = (input_reader_t *)malloc(sizeof(*reader));
    if (reader == NULL) {
        fprintf(stderr, "out of memory\n");
        return NULL;
    }
    memset(reader, 0, sizeof(*reader));

#if defined(_WIN32)
    InitializeCriticalSection(&reader->lock);
    {
        HANDLE handle = CreateThread(NULL, 0, input_thread_proc, reader, 0, NULL);

        if (handle == NULL) {
            fprintf(stderr, "CreateThread failed: %lu\n", (unsigned long)GetLastError());
            DeleteCriticalSection(&reader->lock);
            free(reader);
            return NULL;
        }
        /* 线程分离：进程退出时自动结束 */
        CloseHandle(handle);
    }
#else
    if (pthread_mutex_init(&reader->lock, NULL) != 0) {
        free(reader);
        return NULL;
    }
    {
        pthread_t thread;

        if (pthread_create(&thread, NULL, input_thread_proc, reader) != 0) {
            fprintf(stderr, "pthread_create failed\n");
            pthread_mutex_destroy(&reader->lock);
            free(reader);
            return NULL;
        }
        pthread_detach(thread);
    }
#endif

    return reader;
}
