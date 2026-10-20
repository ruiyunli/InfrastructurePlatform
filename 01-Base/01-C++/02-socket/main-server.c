/*
 * socket server
 *
 * 跨平台（Windows / Linux）TCP 服务端：
 * - 最多支持 MAX_CLIENTS 个客户端同时连接
 * - 所有通信内容都由命令行输入驱动，连接建立后不自动收发消息
 * - 服务端标准输入的内容会广播给所有已连接客户端
 * - 输入 exit / quit / q 退出
 *
 * 网络与平台相关代码在 net.c 中实现。
 */
#include <stdio.h>

#include "net-backend.h"
#include "net-common.h"

/* 绑定地址：本机使用 127.0.0.1；局域网使用 "0.0.0.0" 或本机网卡 IP */
#define SERVER_BIND_IP "127.0.0.1"
#define SERVER_PORT 8080
#define MAX_CLIENTS 5

typedef struct {
    conn_t *conn;
    int id;
    line_reader_t reader;
} client_t;

static client_t g_clients[MAX_CLIENTS];
static int g_client_count = 0;
static int g_next_client_id = 1;

static void drop_client(int slot)
{
    net_close(g_clients[slot].conn);
    printf("[server] client#%d disconnected\n", g_clients[slot].id);
    g_clients[slot].conn = NULL;
    g_clients[slot].id = 0;
    g_client_count--;
}

static void broadcast_line(const char *line)
{
    int i;

    for (i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].conn == NULL) {
            continue;
        }
        if (net_write_line(g_clients[i].conn, line) <= 0) {
            printf("[server] failed to send to client#%d\n", g_clients[i].id);
            drop_client(i);
        }
    }
}

int main(void)
{
    net_config_t cfg = { SERVER_BIND_IP, SERVER_PORT };
    conn_t *listener;
    input_reader_t *input;
    int running = 1;

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    if (net_init() != 0) {
        return 1;
    }

    input = input_reader_start();
    if (input == NULL) {
        net_cleanup();
        return 1;
    }

    listener = net_listen(&cfg);
    if (listener == NULL) {
        net_cleanup();
        return 1;
    }

    printf("[server] listening on %s:%d, max %d clients\n",
           SERVER_BIND_IP, SERVER_PORT, MAX_CLIENTS);
    printf("[server] 输入 exit / quit / q 退出；输入其他内容会广播给所有客户端\n");

    while (running) {
        conn_t *conns[MAX_CLIENTS];
        int slots[MAX_CLIENTS];
        int conn_count = 0;
        net_wait_result_t wait_result;
        int i;

        for (i = 0; i < MAX_CLIENTS; i++) {
            if (g_clients[i].conn != NULL) {
                conns[conn_count] = g_clients[i].conn;
                slots[conn_count] = i;
                conn_count++;
            }
        }

        if (net_wait(&wait_result, listener, conns, conn_count, 200) < 0) {
            break;
        }

        /* 接受新连接 */
        if (wait_result.listener_ready) {
            char peer_ip[64];
            unsigned short peer_port = 0;
            conn_t *new_conn = net_accept(listener, peer_ip, sizeof(peer_ip), &peer_port);

            if (new_conn != NULL) {
                if (g_client_count >= MAX_CLIENTS) {
                    net_write_line(new_conn, "SERVER FULL");
                    net_close(new_conn);
                    printf("[server] rejected connection: server is full\n");
                } else {
                    int slot = -1;

                    for (i = 0; i < MAX_CLIENTS; i++) {
                        if (g_clients[i].conn == NULL) {
                            slot = i;
                            break;
                        }
                    }
                    g_clients[slot].conn = new_conn;
                    g_clients[slot].id = g_next_client_id++;
                    line_reader_init(&g_clients[slot].reader);
                    g_client_count++;
                    printf("[server] client#%d connected from %s:%u\n",
                           g_clients[slot].id, peer_ip, (unsigned)peer_port);
                }
            }
        }

        /* 读取已连接客户端的数据 */
        for (i = 0; i < conn_count; i++) {
            int slot = slots[i];
            int rc;

            if (g_clients[slot].conn == NULL || !wait_result.ready[i]) {
                continue;
            }

            rc = line_reader_feed(&g_clients[slot].reader, g_clients[slot].conn);
            if (rc == NET_RESULT_ERROR) {
                drop_client(slot);
                continue;
            }

            for (;;) {
                char line[NET_LINE_SIZE];

                if (line_reader_next(&g_clients[slot].reader, line, sizeof(line)) != LINE_READER_LINE) {
                    break;
                }
                printf("[client#%d] %s\n", g_clients[slot].id, line);
            }

            if (rc == NET_RESULT_CLOSED) {
                drop_client(slot);
            }
        }

        /* 处理标准输入：广播给所有客户端 */
        {
            char line[NET_LINE_SIZE];

            while (input_reader_pop(input, line, sizeof(line))) {
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
        if (g_clients[i].conn != NULL) {
            net_close(g_clients[i].conn);
            g_clients[i].conn = NULL;
        }
    }
    net_close(listener);
    net_cleanup();
    printf("[server] stopped\n");

    return 0;
}
