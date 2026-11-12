#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

/* ===== mbedtls 头文件 ===== */
#include "mbedtls/pk.h"
#include "mbedtls/error.h"
#include "psa/crypto.h"

/* ===== 通用工具函数 ===== */

// 设置控制台为 UTF-8 编码
void setConsoleUtf8() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}

/* ===== 运行模式 ===== */

typedef enum {
    MODE_NONE = 0,
    MODE_PACK,      // 生成升级包（加密 + 签名）
    MODE_UNPACK     // 接收升级包（验签 + 解密）
} run_mode_t;

/* ===== 命令行参数 ===== */

typedef struct {
    run_mode_t mode;
    const char *crypt_key;   // --crypt：加密（en.key）/ 解密（de.key）密钥文件
    const char *sign_key;    // --sign ：签名（private.key）/ 验签（public.key）密钥文件
    const char *in_path;     // --in   ：原始包（pack）/ 升级包（unpack）
    const char *out_path;    // --out  ：升级包（pack）/ 原始包（unpack）
} cli_options_t;

static void print_usage(const char *prog)
{
    printf("Usage:\n");
    printf("  Create package: %s --pack   --crypt <en.key>     --sign <private.key>\n", prog);
    printf("                  --in <a.zip>        --out <a.zip.package>\n");
    printf("  Unpack package: %s --unpack --crypt <de.key>     --sign <public.key>\n", prog);
    printf("                  --in <a.zip.package> --out <a.zip>\n");
    printf("\n");
    printf("Options:\n");
    printf("  --pack            Create an upgrade package\n");
    printf("  --unpack          Verify and unpack an upgrade package\n");
    printf("  --crypt <file>    Encryption/decryption key file\n");
    printf("  --sign  <file>    Signing/verification key file\n");
    printf("  --in    <file>    Input file\n");
    printf("  --out   <file>    Output file\n");
    printf("  -h, --help        Show this help\n");
}

// 取出形如 "--crypt value" 或 "--crypt=value" 的参数值
static const char *take_value(int argc, char *argv[], int *idx, const char *name)
{
    const char *arg = argv[*idx];
    size_t name_len = strlen(name);

    if (arg[name_len] == '=') {
        return arg + name_len + 1;
    }
    if (*idx + 1 >= argc) {
        fprintf(stderr, "Error: option %s requires a value\n", name);
        return NULL;
    }

    (*idx)++;
    return argv[*idx];
}

static int parse_args(int argc, char *argv[], cli_options_t *opts)
{
    memset(opts, 0, sizeof(*opts));

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            print_usage(argv[0]);
            exit(EXIT_SUCCESS);
        } else if (strcmp(arg, "--pack") == 0) {
            if (opts->mode != MODE_NONE) {
                fprintf(stderr, "Error: --pack and --unpack cannot be used together\n");
                return -1;
            }
            opts->mode = MODE_PACK;
        } else if (strcmp(arg, "--unpack") == 0) {
            if (opts->mode != MODE_NONE) {
                fprintf(stderr, "Error: --pack and --unpack cannot be used together\n");
                return -1;
            }
            opts->mode = MODE_UNPACK;
        } else if (strcmp(arg, "--crypt") == 0 || strncmp(arg, "--crypt=", 8) == 0) {
            opts->crypt_key = take_value(argc, argv, &i, "--crypt");
            if (opts->crypt_key == NULL) {
                return -1;
            }
        } else if (strcmp(arg, "--sign") == 0 || strncmp(arg, "--sign=", 7) == 0) {
            opts->sign_key = take_value(argc, argv, &i, "--sign");
            if (opts->sign_key == NULL) {
                return -1;
            }
        } else if (strcmp(arg, "--in") == 0 || strncmp(arg, "--in=", 5) == 0) {
            opts->in_path = take_value(argc, argv, &i, "--in");
            if (opts->in_path == NULL) {
                return -1;
            }
        } else if (strcmp(arg, "--out") == 0 || strncmp(arg, "--out=", 6) == 0) {
            opts->out_path = take_value(argc, argv, &i, "--out");
            if (opts->out_path == NULL) {
                return -1;
            }
        } else {
            fprintf(stderr, "Error: unknown option %s\n", arg);
            return -1;
        }
    }

    if (opts->mode == MODE_NONE) {
        fprintf(stderr, "Error: either --pack or --unpack is required\n");
        return -1;
    }
    if (opts->crypt_key == NULL) {
        fprintf(stderr, "Error: missing required option --crypt\n");
        return -1;
    }
    if (opts->sign_key == NULL) {
        fprintf(stderr, "Error: missing required option --sign\n");
        return -1;
    }
    if (opts->in_path == NULL) {
        fprintf(stderr, "Error: missing required option --in\n");
        return -1;
    }
    if (opts->out_path == NULL) {
        fprintf(stderr, "Error: missing required option --out\n");
        return -1;
    }

    return 0;
}

/* ===== 业务骨架（暂未实现） ===== */

static int do_pack(const cli_options_t *opts)
{
    printf("[pack] mode: create upgrade package\n");
    printf("[pack] crypt_key: %s\n", opts->crypt_key);
    printf("[pack] sign_key : %s\n", opts->sign_key);
    printf("[pack] in       : %s\n", opts->in_path);
    printf("[pack] out      : %s\n", opts->out_path);

    // TODO: 读取原始包 -> AES-256-GCM 加密 -> ECDSA 对密文签名 -> 输出升级包
    return 0;
}

static int do_unpack(const cli_options_t *opts)
{
    printf("[unpack] mode: receive upgrade package\n");
    printf("[unpack] crypt_key: %s\n", opts->crypt_key);
    printf("[unpack] sign_key : %s\n", opts->sign_key);
    printf("[unpack] in       : %s\n", opts->in_path);
    printf("[unpack] out      : %s\n", opts->out_path);

    // TODO: 读取升级包 -> ECDSA 验签 -> AES-256-GCM 解密 -> 输出原始包
    return 0;
}

/* ===== 入口 ===== */

int main(int argc, char *argv[])
{
    setConsoleUtf8();

    cli_options_t opts;
    if (parse_args(argc, argv, &opts) != 0) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    switch (opts.mode) {
    case MODE_PACK:
        return do_pack(&opts);
    case MODE_UNPACK:
        return do_unpack(&opts);
    default:
        return EXIT_FAILURE;
    }
}
