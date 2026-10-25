/*
 * net-common.c - 公共模块实现
 *
 * 与传输方式无关：
 * - 平台初始化 / 清理
 * - 后端共用的裸 TCP 辅助函数与 select 封装
 * - 行缓冲（解决 TCP 粘包 / 半包）
 * - 标准输入读取线程
 */
#include "net-internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net-common.h"

#define INPUT_QUEUE_CAPACITY 32

/* ------------------------------------------------------------------ */
/* 生命周期与错误处理                                                  */
/* ------------------------------------------------------------------ */
int net_platform_init(void)
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

void net_platform_cleanup(void)
{
#if defined(_WIN32)
    WSACleanup();
#endif
}

int net_init(void)
{
    if (net_platform_init() != 0) {
        return -1;
    }
    if (net_backend_init() != 0) {
        net_platform_cleanup();
        return -1;
    }
    return 0;
}

void net_cleanup(void)
{
    net_backend_cleanup();
    net_platform_cleanup();
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
/* 裸 TCP 辅助函数                                                     */
/* ------------------------------------------------------------------ */
sock_t net_tcp_listen(const char *host, int port)
{
    sock_t fd;
    struct sockaddr_in addr;
    int reuse = 1;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == SOCK_INVALID) {
        net_print_error("socket");
        return SOCK_INVALID;
    }

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        fprintf(stderr, "invalid listen address: %s\n", host);
        sock_close(fd);
        return SOCK_INVALID;
    }

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        net_print_error("bind");
        sock_close(fd);
        return SOCK_INVALID;
    }

    if (listen(fd, NET_MAX_WAIT_CONNS) != 0) {
        net_print_error("listen");
        sock_close(fd);
        return SOCK_INVALID;
    }

    return fd;
}

sock_t net_tcp_accept(sock_t listener, char *peer_ip, size_t peer_ip_size,
                      unsigned short *peer_port)
{
    struct sockaddr_in peer;
#if defined(_WIN32)
    int peer_len = (int)sizeof(peer);
#else
    socklen_t peer_len = (socklen_t)sizeof(peer);
#endif
    sock_t fd;

    fd = accept(listener, (struct sockaddr *)&peer, &peer_len);
    if (fd == SOCK_INVALID) {
        net_print_error("accept");
        return SOCK_INVALID;
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
    return fd;
}

sock_t net_tcp_connect(const char *host, int port)
{
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct addrinfo *it;
    char port_text[16];
    sock_t fd = SOCK_INVALID;

    snprintf(port_text, sizeof(port_text), "%d", port);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;          /* 当前仅支持 IPv4 */
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    /* host 既可以是点分十进制 IP，也可以是域名 */
    if (getaddrinfo(host, port_text, &hints, &result) != 0 || result == NULL) {
        fprintf(stderr, "cannot resolve server address: %s\n", host);
        return SOCK_INVALID;
    }

    for (it = result; it != NULL; it = it->ai_next) {
        fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (fd == SOCK_INVALID) {
            continue;
        }
#if defined(_WIN32)
        if (connect(fd, it->ai_addr, (int)it->ai_addrlen) == 0) {
#else
        if (connect(fd, it->ai_addr, it->ai_addrlen) == 0) {
#endif
            break;
        }
        sock_close(fd);
        fd = SOCK_INVALID;
    }

    freeaddrinfo(result);

    if (fd == SOCK_INVALID) {
        net_print_error("connect");
        return SOCK_INVALID;
    }

    return fd;
}

int net_error_is_retryable(void)
{
#if defined(_WIN32)
    int code = WSAGetLastError();
    return code == WSAEWOULDBLOCK || code == WSAEINTR;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
#endif
}

/* ------------------------------------------------------------------ */
/* select 封装                                                         */
/* ------------------------------------------------------------------ */
int net_select_fds(sock_t listener_fd, const sock_t *fds, int count,
                   int timeout_ms, int *listener_ready, int *ready)
{
    fd_set readfds;
    struct timeval timeout;
    int total = 0;
    int i;
#if !defined(_WIN32)
    sock_t max_fd = 0;
    int have_fd = 0;
#endif

    FD_ZERO(&readfds);

    if (listener_fd != SOCK_INVALID) {
        FD_SET(listener_fd, &readfds);
#if !defined(_WIN32)
        max_fd = listener_fd;
        have_fd = 1;
#endif
    }
    for (i = 0; i < count; i++) {
        if (fds[i] == SOCK_INVALID) {
            continue;
        }
        FD_SET(fds[i], &readfds);
#if !defined(_WIN32)
        if (!have_fd || fds[i] > max_fd) {
            max_fd = fds[i];
            have_fd = 1;
        }
#endif
    }

    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

#if defined(_WIN32)
    if (select(0, &readfds, NULL, NULL, &timeout) < 0) {
        if (WSAGetLastError() == WSAEINTR) {
            return 0;
        }
        net_print_error("select");
        return -1;
    }
#else
    if (select((int)(max_fd + 1), &readfds, NULL, NULL, &timeout) < 0) {
        if (errno == EINTR) {
            return 0;
        }
        net_print_error("select");
        return -1;
    }
#endif

    if (listener_fd != SOCK_INVALID && FD_ISSET(listener_fd, &readfds)) {
        *listener_ready = 1;
        total++;
    }
    for (i = 0; i < count; i++) {
        if (fds[i] != SOCK_INVALID && FD_ISSET(fds[i], &readfds)) {
            ready[i] = 1;
            total++;
        }
    }
    return total;
}

/* ------------------------------------------------------------------ */
/* 按行发送                                                            */
/* ------------------------------------------------------------------ */
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
