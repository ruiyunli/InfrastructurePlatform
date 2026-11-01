/*
 * socket client
 *
 * 跨平台（Windows / Linux）TCP 客户端：
 * - 连接建立后不自动收发消息，全部由命令行输入驱动
 * - 标准输入的内容发送给服务端
 * - 接收服务端消息并显示
 * - 输入 exit / quit / q 退出
 *
 * 网络与平台相关代码在 net.c 中实现。
 */
#include <stdio.h>

#include "net-backend.h"
#include "net-common.h"

/* 服务端地址：本机使用 127.0.0.1；跨机器时改为服务端的局域网 IP */
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080

#if defined(NET_ENABLE_TLS)
/* 相对工作目录（VS 调试时为 install 目录） */
#define TLS_CA_FILE "certs/ca.crt.pem"

/*
 * 证书校验名。NULL 表示直接使用 SERVER_IP，让"连哪个地址"和"证书写了什么名字"
 * 保持一致：
 * - SERVER_IP 是 IP：证书 SAN 需要包含 IP:127.0.0.1
 * - SERVER_IP 是域名：证书 SAN 需要包含 DNS:该域名
 * 只有"用 IP 连接、证书却只签给域名"时，才需要在这里单独填写域名。
 */
#define TLS_SERVER_NAME NULL
#endif

int main(void)
{
    net_config_t cfg = { SERVER_IP, SERVER_PORT };
    conn_t *server_conn;
    input_reader_t *input;
    line_reader_t reader;
    int running = 1;

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    if (net_init() != 0) {
        return 1;
    }

#if defined(NET_ENABLE_TLS)
    {
        /* 单向认证：只校验服务器证书；cert_file / key_file 留空 */
        net_tls_config_t tls_cfg = {
            TLS_CA_FILE,
            NULL,            /* cert_file：单向认证不需要客户端证书 */
            NULL,            /* key_file */
            TLS_SERVER_NAME,
            1,               /* verify_peer：校验服务器证书 */
            5000             /* handshake_timeout_ms */
        };

        if (net_tls_enable(&tls_cfg) != 0) {
            net_cleanup();
            return 1;
        }
        printf("[client] TLS 1.3 enabled, verifying %s\n",
               TLS_SERVER_NAME != NULL ? TLS_SERVER_NAME : SERVER_IP);
    }
#endif

    input = input_reader_start();
    if (input == NULL) {
        net_cleanup();
        return 1;
    }

    server_conn = net_connect(&cfg);
    if (server_conn == NULL) {
        net_cleanup();
        return 1;
    }

    line_reader_init(&reader);
    printf("[client] connected to %s:%d\n", SERVER_IP, SERVER_PORT);
    printf("[client] 输入内容发送给服务端；输入 exit / quit / q 退出\n");

    while (running) {
        conn_t *conns[1];
        net_wait_result_t wait_result;
        int rc;

        conns[0] = server_conn;
        if (net_wait(&wait_result, NULL, conns, 1, 200) < 0) {
            break;
        }

        /* 接收服务端消息 */
        if (wait_result.ready[0]) {
            rc = line_reader_feed(&reader, server_conn);

            if (rc == NET_RESULT_ERROR) {
                printf("[client] receive error\n");
                running = 0;
            } else {
                for (;;) {
                    char line[NET_LINE_SIZE];

                    if (line_reader_next(&reader, line, sizeof(line)) != LINE_READER_LINE) {
                        break;
                    }
                    printf("[server] %s\n", line);
                }

                if (rc == NET_RESULT_CLOSED) {
                    printf("[client] server closed the connection\n");
                    running = 0;
                }
            }
        }

        /* 处理标准输入：发送给服务端 */
        if (running) {
            char line[NET_LINE_SIZE];

            while (input_reader_pop(input, line, sizeof(line))) {
                if (is_quit_command(line)) {
                    printf("[client] disconnecting\n");
                    running = 0;
                    break;
                }
                if (net_write_line(server_conn, line) <= 0) {
                    printf("[client] send failed\n");
                    running = 0;
                    break;
                }
                printf("[me] %s\n", line);
            }
        }
    }

    net_close(server_conn);
    net_cleanup();
    printf("[client] disconnected\n");

    return 0;
}
