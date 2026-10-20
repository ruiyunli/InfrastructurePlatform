/*
 * socket server
 *
 * 跨平台（Windows / Linux）TCP 服务端：
 * - 最多支持 MAX_CLIENTS 个客户端同时连接
 * - 所有通信内容都由命令行输入驱动，连接建立后不自动收发消息
 * - 服务端标准输入的内容会广播给所有已连接客户端
 * - 输入 exit / quit / q 退出
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
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
#endif

/* 绑定地址：本机使用 127.0.0.1；局域网使用 "0.0.0.0" 或本机网卡 IP */
#define SERVER_BIND_IP "127.0.0.1"
#define SERVER_PORT 8080
#define MAX_CLIENTS 5
#define LINE_SIZE 512
#define QUEUE_CAPACITY 32

/* ------------------------------------------------------------------ */
/* 平台适配                                                            */
/* ------------------------------------------------------------------ */
#if defined(_WIN32)
typedef SOCKET sock_t;
typedef HANDLE thread_t;
typedef CRITICAL_SECTION mutex_t;
#define SOCK_INVALID INVALID_SOCKET
#define sock_close closesocket
#else
typedef int sock_t;
typedef pthread_t thread_t;
typedef pthread_mutex_t mutex_t;
#define SOCK_INVALID (-1)
#define sock_close close
#endif

/* ------------------------------------------------------------------ */
/* 输入队列：stdin 线程写入，主线程读取                                  */
/* ------------------------------------------------------------------ */
typedef struct {
    mutex_t lock;
    char lines[QUEUE_CAPACITY][LINE_SIZE];
    int head;
    int tail;
    int count;
} line_queue_t;

static void queue_init(line_queue_t *queue)
{
    memset(queue, 0, sizeof(*queue));
#if defined(_WIN32)
    InitializeCriticalSection(&queue->lock);
#else
    pthread_mutex_init(&queue->lock, NULL);
#endif
}

static void queue_push(line_queue_t *queue, const char *line)
{
#if defined(_WIN32)
    EnterCriticalSection(&queue->lock);
#else
    pthread_mutex_lock(&queue->lock);
#endif
    if (queue->count < QUEUE_CAPACITY) {
        strncpy(queue->lines[queue->tail], line, LINE_SIZE - 1);
        queue->lines[queue->tail][LINE_SIZE - 1] = '\0';
        queue->tail = (queue->tail + 1) % QUEUE_CAPACITY;
        queue->count++;
    }
    /* 队列满时直接丢弃最新输入，避免阻塞网络循环 */
#if defined(_WIN32)
    LeaveCriticalSection(&queue->lock);
#else
    pthread_mutex_unlock(&queue->lock);
#endif
}

static int queue_pop(line_queue_t *queue, char *out, size_t out_size)
{
    int got = 0;

#if defined(_WIN32)
    EnterCriticalSection(&queue->lock);
#else
    pthread_mutex_lock(&queue->lock);
#endif
    if (queue->count > 0) {
        strncpy(out, queue->lines[queue->head], out_size - 1);
        out[out_size - 1] = '\0';
        queue->head = (queue->head + 1) % QUEUE_CAPACITY;
        queue->count--;
        got = 1;
    }
#if defined(_WIN32)
    LeaveCriticalSection(&queue->lock);
#else
    pthread_mutex_unlock(&queue->lock);
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

static int is_quit_command(const char *text)
{
    return iequals(text, "exit") || iequals(text, "quit") || iequals(text, "q");
}

#if defined(_WIN32)
static DWORD WINAPI stdin_thread_proc(LPVOID arg)
#else
static void *stdin_thread_proc(void *arg)
#endif
{
    line_queue_t *queue = (line_queue_t *)arg;
    char line[LINE_SIZE];

    while (fgets(line, sizeof(line), stdin) != NULL) {
        trim_line_end(line);
        if (line[0] != '\0') {
            queue_push(queue, line);
        }
    }

#if defined(_WIN32)
    return 0;
#else
    return NULL;
#endif
}

static int start_stdin_thread(thread_t *thread, line_queue_t *queue)
{
#if defined(_WIN32)
    *thread = CreateThread(NULL, 0, stdin_thread_proc, queue, 0, NULL);
    return (*thread == NULL) ? -1 : 0;
#else
    return (pthread_create(thread, NULL, stdin_thread_proc, queue) == 0) ? 0 : -1;
#endif
}

/* ------------------------------------------------------------------ */
/* socket 辅助函数                                                     */
/* ------------------------------------------------------------------ */
static int socket_system_init(void)
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

static void socket_system_cleanup(void)
{
#if defined(_WIN32)
    WSACleanup();
#endif
}

static void print_socket_error(const char *what)
{
#if defined(_WIN32)
    fprintf(stderr, "%s failed: %d\n", what, WSAGetLastError());
#else
    fprintf(stderr, "%s failed: %s\n", what, strerror(errno));
#endif
}

static int send_all(sock_t fd, const char *data, size_t len)
{
    size_t sent = 0;

    while (sent < len) {
        int n = (int)send(fd, data + sent, (int)(len - sent), 0);
        if (n <= 0) {
            return -1;
        }
        sent += (size_t)n;
    }
    return 0;
}

static int send_line(sock_t fd, const char *text)
{
    char buffer[LINE_SIZE + 2];
    int len = snprintf(buffer, sizeof(buffer), "%s\n", text);

    if (len < 0) {
        return -1;
    }
    return send_all(fd, buffer, (size_t)len);
}

/* ------------------------------------------------------------------ */
/* 客户端列表                                                          */
/* ------------------------------------------------------------------ */
typedef struct {
    sock_t fd;
    int id;
} client_t;

static client_t g_clients[MAX_CLIENTS];
static int g_client_count = 0;
static int g_next_client_id = 1;

static void clients_init(void)
{
    int i;

    for (i = 0; i < MAX_CLIENTS; i++) {
        g_clients[i].fd = SOCK_INVALID;
        g_clients[i].id = 0;
    }
}

static int find_free_slot(void)
{
    int i;

    for (i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].fd == SOCK_INVALID) {
            return i;
        }
    }
    return -1;
}

static void drop_client(int slot)
{
    sock_close(g_clients[slot].fd);
    printf("[server] client#%d disconnected\n", g_clients[slot].id);
    g_clients[slot].fd = SOCK_INVALID;
    g_clients[slot].id = 0;
    g_client_count--;
}

static void broadcast_line(const char *line)
{
    char buffer[LINE_SIZE + 2];
    int len = snprintf(buffer, sizeof(buffer), "%s\n", line);
    int i;

    if (len < 0) {
        return;
    }

    for (i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].fd == SOCK_INVALID) {
            continue;
        }
        if (send_all(g_clients[i].fd, buffer, (size_t)len) != 0) {
            printf("[server] failed to send to client#%d\n", g_clients[i].id);
            drop_client(i);
        }
    }
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */
int main(void)
{
    line_queue_t input_queue;
    thread_t stdin_thread;
    sock_t listen_fd;
    struct sockaddr_in addr;
    int reuse = 1;
    int running = 1;

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    if (socket_system_init() != 0) {
        return 1;
    }

    queue_init(&input_queue);
    if (start_stdin_thread(&stdin_thread, &input_queue) != 0) {
        fprintf(stderr, "failed to start stdin thread\n");
        socket_system_cleanup();
        return 1;
    }

    clients_init();

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == SOCK_INVALID) {
        print_socket_error("socket");
        socket_system_cleanup();
        return 1;
    }

    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_BIND_IP, &addr.sin_addr) != 1) {
        fprintf(stderr, "invalid bind address: %s\n", SERVER_BIND_IP);
        sock_close(listen_fd);
        socket_system_cleanup();
        return 1;
    }

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        print_socket_error("bind");
        sock_close(listen_fd);
        socket_system_cleanup();
        return 1;
    }

    if (listen(listen_fd, MAX_CLIENTS) != 0) {
        print_socket_error("listen");
        sock_close(listen_fd);
        socket_system_cleanup();
        return 1;
    }

    printf("[server] listening on %s:%d, max %d clients\n", SERVER_BIND_IP, SERVER_PORT, MAX_CLIENTS);
    printf("[server] 输入 exit / quit / q 退出；输入其他内容会广播给所有客户端\n");

    while (running) {
        fd_set readfds;
        sock_t max_fd = listen_fd;
        struct timeval timeout;
        int ready;
        int i;

        FD_ZERO(&readfds);
        FD_SET(listen_fd, &readfds);
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (g_clients[i].fd != SOCK_INVALID) {
                FD_SET(g_clients[i].fd, &readfds);
                if (g_clients[i].fd > max_fd) {
                    max_fd = g_clients[i].fd;
                }
            }
        }

        timeout.tv_sec = 0;
        timeout.tv_usec = 200 * 1000;

        ready = select((int)(max_fd + 1), &readfds, NULL, NULL, &timeout);
        if (ready < 0) {
#if defined(_WIN32)
            if (WSAGetLastError() == WSAEINTR) {
                continue;
            }
#else
            if (errno == EINTR) {
                continue;
            }
#endif
            print_socket_error("select");
            break;
        }

        /* 接受新连接 */
        if (ready > 0 && FD_ISSET(listen_fd, &readfds)) {
            struct sockaddr_in peer;
#if defined(_WIN32)
            int peer_len = (int)sizeof(peer);
#else
            socklen_t peer_len = (socklen_t)sizeof(peer);
#endif
            sock_t client_fd = accept(listen_fd, (struct sockaddr *)&peer, &peer_len);

            if (client_fd == SOCK_INVALID) {
                print_socket_error("accept");
            } else if (g_client_count >= MAX_CLIENTS) {
                send_line(client_fd, "SERVER FULL");
                sock_close(client_fd);
                printf("[server] rejected connection: server is full\n");
            } else {
                int slot = find_free_slot();
                char peer_ip[INET_ADDRSTRLEN] = "?";

                inet_ntop(AF_INET, &peer.sin_addr, peer_ip, sizeof(peer_ip));
                g_clients[slot].fd = client_fd;
                g_clients[slot].id = g_next_client_id++;
                g_client_count++;
                printf("[server] client#%d connected from %s:%u\n",
                       g_clients[slot].id, peer_ip, (unsigned)ntohs(peer.sin_port));
            }
        }

        /* 读取已连接客户端的数据 */
        if (ready > 0) {
            for (i = 0; i < MAX_CLIENTS; i++) {
                char buffer[LINE_SIZE];
                int n;

                if (g_clients[i].fd == SOCK_INVALID) {
                    continue;
                }
                if (!FD_ISSET(g_clients[i].fd, &readfds)) {
                    continue;
                }

                n = (int)recv(g_clients[i].fd, buffer, (int)(sizeof(buffer) - 1), 0);
                if (n <= 0) {
                    drop_client(i);
                    continue;
                }

                buffer[n] = '\0';
                trim_line_end(buffer);
                printf("[client#%d] %s\n", g_clients[i].id, buffer);
            }
        }

        /* 处理标准输入 */
        {
            char line[LINE_SIZE];

            while (queue_pop(&input_queue, line, sizeof(line))) {
                if (is_quit_command(line)) {
                    printf("[server] shutting down\n");
                    running = 0;
                    break;
                }
                if (g_client_count == 0) {
                    printf("[server] no clients connected, dropped: %s\n", line);
                    continue;
                }
                printf("[broadcast] %s\n", line);
                broadcast_line(line);
            }
        }
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].fd != SOCK_INVALID) {
            sock_close(g_clients[i].fd);
            g_clients[i].fd = SOCK_INVALID;
        }
    }
    sock_close(listen_fd);
    socket_system_cleanup();
    printf("[server] stopped\n");

    return 0;
}
