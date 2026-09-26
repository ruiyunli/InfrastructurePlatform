/**
 * ECC Digital Signature Demo
 *
 * 演示 ECC (secp256r1) 数字签名与验证，使用 mbedtls 4.1.0
 * 支持两种 API 实现，通过宏定义切换：
 *   - PK 高层 API（mbedtls_pk_sign/verify）
 *   - PSA Crypto 底层 API（psa_sign/verify_message）
 *
 * 参考: mbedtls 源码 programs/pkey/pk_sign.c, pk_verify.c
 *
 * 切换方式：修改下方的 #define 即可
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

/* ===== API 选择 ===== */
//#define USE_PK_API
#define USE_PSA_API

/* ===== mbedtls 头文件 ===== */
#include "mbedtls/pk.h"
#include "mbedtls/error.h"
#include "psa/crypto.h"

/* ===== 配置 ===== */
#define KEY_DIR "keys"
#define PRIVATE_KEY_FILE KEY_DIR "/private.pem"
#define PUBLIC_KEY_FILE  KEY_DIR "/public.pem"

/* ===== 通用工具函数 ===== */

// 设置控制台为 UTF-8 编码
void setConsoleUtf8() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}

/** 以十六进制打印缓冲区 */
static void print_hex(const char *label, const unsigned char *data, size_t len)
{
    printf("%s (", label);
    for (size_t i = 0; i < len; i++) {
        printf("%02x", data[i]);
    }
    printf(")\n");
}

/** 打印 mbedtls 错误码 */
static void print_error(const char *msg, int err)
{
    printf("  [错误] %s (错误码: -0x%04X)\n", msg, (unsigned int)(-err));
}

/** 打印 PSA 状态码 */
static void print_psa_error(const char *msg, psa_status_t status)
{
    printf("  [PSA 错误] %s (状态码: %d)\n", msg, (int)status);
}

/** 检查文件是否存在 */
static int file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return 0;
    }
    fclose(f);
    return 1;
}

/** 读取整个文件内容到缓冲区，返回读取的字节数 */
static size_t read_file(const char *path, unsigned char *buf, size_t buf_size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        printf("  [错误] 无法打开文件: %s\n", path);
        return 0;
    }
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if ((size_t)file_size > buf_size - 1) {
        fclose(f);
        printf("  [错误] 文件过大: %s (%ld 字节)\n", path, file_size);
        return 0;
    }
    size_t len = fread(buf, 1, buf_size - 1, f);
    fclose(f);
    return len;
}

/* ============================================================
 * PK 高层 API 实现
 * 使用: mbedtls_pk_parse_key, mbedtls_pk_sign, mbedtls_pk_verify
 *
 * 注意: mbedtls_pk_parse_key() 内部自动处理 PEM→DER 解码，
 *       不需要先调用 mbedtls_pem_read_buffer()。
 * ============================================================ */
#if defined(USE_PK_API)

/** 从 PEM 文件加载私钥（PK API） */
static int load_private_key_pk(mbedtls_pk_context *pk)
{
    int ret;
    unsigned char pem_buf[4096];
    size_t pem_len;
    const char *path = PRIVATE_KEY_FILE;

    printf("[1/4] 正在从 %s 加载私钥...\n", path);

    if (!file_exists(path)) {
        printf("  [错误] 文件不存在，请先生成密钥\n");
        return -1;
    }

    pem_len = read_file(path, pem_buf, sizeof(pem_buf) - 1);
    if (pem_len == 0) {
        return -1;
    }
    pem_buf[pem_len] = '\0';  /* 确保 null 终止 */

    /* mbedtls_pk_parse_key 自动识别 PEM 和 DER 格式
     * PEM 数据：keylen 必须包含 null 终止符（strlen(key) + 1）
     * DER 数据：keylen 为实际字节数 */
    mbedtls_pk_init(pk);
    ret = mbedtls_pk_parse_key(pk, pem_buf, pem_len + 1, NULL, 0);
    if (ret != 0) {
        print_error("mbedtls_pk_parse_key 失败", ret);
        mbedtls_pk_free(pk);
        return ret;
    }

    printf("  成功\n");
    return 0;
}

/** 从 PEM 文件加载公钥（PK API） */
static int load_public_key_pk(mbedtls_pk_context *pk)
{
    int ret;
    unsigned char pem_buf[4096];
    size_t pem_len;
    const char *path = PUBLIC_KEY_FILE;

    printf("[3/4] 正在从 %s 加载公钥...\n", path);

    if (!file_exists(path)) {
        printf("  [错误] 文件不存在，请先生成密钥\n");
        return -1;
    }

    pem_len = read_file(path, pem_buf, sizeof(pem_buf) - 1);
    if (pem_len == 0) {
        return -1;
    }
    pem_buf[pem_len] = '\0';  /* 确保 null 终止 */

    /* mbedtls_pk_parse_public_key 自动识别 PEM 和 DER 格式
     * PEM 数据：keylen 必须包含 null 终止符（strlen(key) + 1） */
    mbedtls_pk_init(pk);
    ret = mbedtls_pk_parse_public_key(pk, pem_buf, pem_len + 1);
    if (ret != 0) {
        print_error("mbedtls_pk_parse_public_key 失败", ret);
        mbedtls_pk_free(pk);
        return ret;
    }

    printf("  成功\n");
    return 0;
}

/** 使用 PK API 对数据签名（先哈希，再签名） */
static int sign_data_pk(mbedtls_pk_context *pk,
                        const unsigned char *data, size_t data_len,
                        unsigned char *sig, size_t sig_buf_len, size_t *sig_len)
{
    int ret;
    unsigned char hash[PSA_HASH_LENGTH(PSA_ALG_SHA_256)];
    size_t hash_len;
    psa_status_t status;

    printf("[2/4] 正在签名数据（%zu 字节）...\n", data_len);

    /* 第一步：用 SHA-256 计算数据哈希 */
    status = psa_hash_compute(PSA_ALG_SHA_256, data, data_len, hash, sizeof(hash), &hash_len);
    if (status != PSA_SUCCESS) {
        print_psa_error("psa_hash_compute 失败", status);
        return -1;
    }

    /* 第二步：用私钥对哈希签名 */
    ret = mbedtls_pk_sign(pk, MBEDTLS_PK_SIGALG_ECDSA,
                          hash, hash_len, sig, sig_buf_len, sig_len);
    if (ret != 0) {
        print_error("mbedtls_pk_sign 失败", ret);
        return ret;
    }

    printf("  成功\n");
    print_hex("  签名", sig, *sig_len);
    return 0;
}

/** 使用 PK API 验证签名（先哈希，再验证） */
static int verify_signature_pk(mbedtls_pk_context *pk,
                               const unsigned char *data, size_t data_len,
                               const unsigned char *sig, size_t sig_len)
{
    int ret;
    unsigned char hash[PSA_HASH_LENGTH(PSA_ALG_SHA_256)];
    size_t hash_len;
    psa_status_t status;

    printf("[4/4] 正在验证签名...\n");

    /* 第一步：用 SHA-256 计算数据哈希 */
    status = psa_hash_compute(PSA_ALG_SHA_256, data, data_len, hash, sizeof(hash), &hash_len);
    if (status != PSA_SUCCESS) {
        print_psa_error("psa_hash_compute 失败", status);
        return -1;
    }

    /* 第二步：用公钥验证签名 */
    ret = mbedtls_pk_verify(pk, MBEDTLS_PK_SIGALG_ECDSA,
                             hash, hash_len, sig, sig_len);
    if (ret != 0) {
        print_error("mbedtls_pk_verify 失败", ret);
        return ret;
    }

    printf("  成功\n  结果：验证通过\n");
    return 0;
}

#endif /* USE_PK_API */

/* ============================================================
 * PSA Crypto 底层 API 实现
 * 使用: mbedtls_pk_parse_key + mbedtls_pk_import_into_psa,
 *       psa_sign_message, psa_verify_message
 *
 * 参考 mbedtls_pk_import_into_psa() 的示例流程：
 *   mbedtls_pk_parse_key() 先解析（自动处理 PEM→DER），
 *   再用 mbedtls_pk_import_into_psa() 导入到 PSA。
 * ============================================================ */
#if defined(USE_PSA_API)

/**
 * 从 PEM 文件加载私钥并导入 PSA
 *
 * 流程:
 *   1. mbedtls_pk_parse_key() 解析 PEM（自动处理 PEM→DER 解码）
 *   2. mbedtls_pk_get_psa_attributes() 获取 PSA 属性
 *   3. mbedtls_pk_import_into_psa() 导入到 PSA key store
 */
static int load_private_key_psa(psa_key_id_t *key_id)
{
    int ret;
    unsigned char pem_buf[4096];
    size_t pem_len;
    mbedtls_pk_context pk;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_status_t status;
    const char *path = PRIVATE_KEY_FILE;

    printf("[1/4] 正在从 %s 加载私钥（PSA API）...\n", path);

    if (!file_exists(path)) {
        printf("  [错误] 文件不存在，请先生成密钥\n");
        return -1;
    }

    pem_len = read_file(path, pem_buf, sizeof(pem_buf) - 1);
    if (pem_len == 0) {
        return -1;
    }
    pem_buf[pem_len] = '\0';  /* 确保 null 终止 */

    /* 第一步：用 PK API 解析 PEM（自动处理 PEM→DER 解码）
     * PEM 数据 keylen 须含 null 终止符 */
    mbedtls_pk_init(&pk);
    ret = mbedtls_pk_parse_key(&pk, pem_buf, pem_len + 1, NULL, 0);
    if (ret != 0) {
        print_error("mbedtls_pk_parse_key 失败", ret);
        mbedtls_pk_free(&pk);
        return ret;
    }

    /* 第二步：获取 PSA 属性 */
    ret = mbedtls_pk_get_psa_attributes(&pk, PSA_KEY_USAGE_SIGN_MESSAGE, &attributes);
    if (ret != 0) {
        print_error("mbedtls_pk_get_psa_attributes 失败", ret);
        mbedtls_pk_free(&pk);
        return ret;
    }

    /* 第三步：导入到 PSA key store */
    status = mbedtls_pk_import_into_psa(&pk, &attributes, key_id);
    mbedtls_pk_free(&pk);
    if (status != 0) {
        print_psa_error("mbedtls_pk_import_into_psa 失败", status);
        return (int)status;
    }

    printf("  成功（PSA 密钥 ID: %lu）\n", (unsigned long)*key_id);
    return 0;
}

/**
 * 从 PEM 文件加载公钥并导入 PSA
 *
 * 流程同上，但使用 PSA_KEY_USAGE_VERIFY_MESSAGE 用法标志。
 * 公钥使用 mbedtls_pk_parse_public_key() 解析。
 */
static int load_public_key_psa(psa_key_id_t *key_id)
{
    int ret;
    unsigned char pem_buf[4096];
    size_t pem_len;
    mbedtls_pk_context pk;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_status_t status;
    const char *path = PUBLIC_KEY_FILE;

    printf("[3/4] 正在从 %s 加载公钥（PSA API）...\n", path);

    if (!file_exists(path)) {
        printf("  [错误] 文件不存在，请先生成密钥\n");
        return -1;
    }

    pem_len = read_file(path, pem_buf, sizeof(pem_buf) - 1);
    if (pem_len == 0) {
        return -1;
    }
    pem_buf[pem_len] = '\0';  /* 确保 null 终止 */

    /* 第一步：用 PK API 解析公钥 PEM（自动处理 PEM→DER 解码）
     * PEM 数据 keylen 须含 null 终止符 */
    mbedtls_pk_init(&pk);
    ret = mbedtls_pk_parse_public_key(&pk, pem_buf, pem_len + 1);
    if (ret != 0) {
        print_error("mbedtls_pk_parse_public_key 失败", ret);
        mbedtls_pk_free(&pk);
        return ret;
    }

    /* 第二步：获取 PSA 属性 */
    ret = mbedtls_pk_get_psa_attributes(&pk, PSA_KEY_USAGE_VERIFY_MESSAGE, &attributes);
    if (ret != 0) {
        print_error("mbedtls_pk_get_psa_attributes 失败", ret);
        mbedtls_pk_free(&pk);
        return ret;
    }

    /* 第三步：导入到 PSA key store */
    status = mbedtls_pk_import_into_psa(&pk, &attributes, key_id);
    mbedtls_pk_free(&pk);
    if (status != 0) {
        print_psa_error("mbedtls_pk_import_into_psa 失败", status);
        return (int)status;
    }

    printf("  成功（PSA 密钥 ID: %lu）\n", (unsigned long)*key_id);
    return 0;
}

/** 使用 PSA Crypto API 对数据签名（自动计算哈希） */
static int sign_data_psa(psa_key_id_t priv_key_id,
                          const unsigned char *data, size_t data_len,
                          unsigned char *sig, size_t sig_buf_len, size_t *sig_len)
{
    psa_status_t status;

    printf("[2/4] 正在签名数据（%zu 字节）...\n", data_len);

    /* PSA 内部自动处理 SHA-256 哈希（ECDSA + SHA-256） */
    status = psa_sign_message(priv_key_id,
                              PSA_ALG_ECDSA(PSA_ALG_SHA_256),
                              data, data_len,
                              sig, sig_buf_len, sig_len);
    if (status != PSA_SUCCESS) {
        print_psa_error("psa_sign_message 失败", status);
        return (int)status;
    }

    printf("  成功\n");
    print_hex("  签名", sig, *sig_len);
    return 0;
}

/** 使用 PSA Crypto API 验证签名（自动计算哈希） */
static int verify_signature_psa(psa_key_id_t pub_key_id,
                                const unsigned char *data, size_t data_len,
                                const unsigned char *sig, size_t sig_len)
{
    psa_status_t status;

    printf("[4/4] 正在验证签名...\n");

    /* PSA 内部自动处理 SHA-256 哈希验证 */
    status = psa_verify_message(pub_key_id,
                                 PSA_ALG_ECDSA(PSA_ALG_SHA_256),
                                 data, data_len,
                                 sig, sig_len);
    if (status != PSA_SUCCESS) {
        print_psa_error("psa_verify_message 失败", status);
        return (int)status;
    }

    printf("  成功\n  结果：验证通过\n");
    return 0;
}

#endif /* USE_PSA_API */

/* ============================================================
 * 主函数
 * ============================================================ */

int main(void)
{
    int ret = 0;
    const char *message = "Hello, World!";
    size_t message_len = strlen(message);
    unsigned char signature[PSA_SIGNATURE_MAX_SIZE];
    size_t sig_len = 0;

    setConsoleUtf8();

    printf("==============================================\n");
    printf("  ECC 数字签名演示程序\n");
#if defined(USE_PK_API)
    printf("  接口: PK 高层 API (mbedtls_pk_sign/verify)\n");
#elif defined(USE_PSA_API)
    printf("  接口: PSA Crypto (psa_sign/verify_message)\n");
#endif
    printf("  曲线: secp256r1 (P-256)\n");
    printf("  哈希: SHA-256\n");
    printf("==============================================\n\n");

    /* 初始化 PSA Crypto（mbedtls 4.x 中 PK 和 PSA 都需要） */
    if (psa_crypto_init() != PSA_SUCCESS) {
        printf("[错误] psa_crypto_init 失败\n");
        return 1;
    }

#if defined(USE_PK_API)
    {
        mbedtls_pk_context priv_key;
        mbedtls_pk_context pub_key;

        /* 第一步：加载私钥 */
        ret = load_private_key_pk(&priv_key);
        if (ret != 0) goto cleanup_pk;

        /* 第二步：签名数据 */
        ret = sign_data_pk(&priv_key, (const unsigned char *)message, message_len,
                           signature, sizeof(signature), &sig_len);
        if (ret != 0) goto cleanup_pk;

        /* 第三步：加载公钥 */
        ret = load_public_key_pk(&pub_key);
        if (ret != 0) goto cleanup_pk;

        /* 第四步：验证签名 */
        ret = verify_signature_pk(&pub_key, (const unsigned char *)message, message_len,
                                  signature, sig_len);

    cleanup_pk:
        mbedtls_pk_free(&priv_key);
        mbedtls_pk_free(&pub_key);
    }

#elif defined(USE_PSA_API)
    {
        psa_key_id_t priv_key_id = 0;
        psa_key_id_t pub_key_id = 0;

        /* 第一步：发布者-加载私钥到 PSA */
        ret = load_private_key_psa(&priv_key_id);
        if (ret != 0) goto cleanup_psa;

        /* 第二步：发布者-签名数据 */
        ret = sign_data_psa(priv_key_id, (const unsigned char *)message, message_len,
                            signature, sizeof(signature), &sig_len);
        if (ret != 0) goto cleanup_psa;

        /* 第三步：接收者-加载公钥到 PSA */
        ret = load_public_key_psa(&pub_key_id);
        if (ret != 0) goto cleanup_psa;

        /* 第四步：接收者-验证签名 */
        ret = verify_signature_psa(pub_key_id, (const unsigned char *)message, message_len,
                                    signature, sig_len);

    cleanup_psa:
        if (priv_key_id != 0) {
            psa_destroy_key(priv_key_id);
        }
        if (pub_key_id != 0) {
            psa_destroy_key(pub_key_id);
        }
    }

#else
    #error "请定义 USE_PK_API 或 USE_PSA_API"
#endif

    printf("\n");
    if (ret == 0) {
        printf("演示成功完成。\n");
    } else {
        printf("演示失败（错误码: %d）。\n", ret);
    }

    mbedtls_psa_crypto_free();
    return (ret == 0) ? 0 : 1;
}
