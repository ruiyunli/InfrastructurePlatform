#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <windows.h>
#include <psa/crypto.h>
#include <mbedtls/error.h>
#include <mbedtls/platform_util.h>

#define AES_KEY_LEN 32

/* ------------------------------------------------------------------ */
/* 密钥来源开关：二选一                                                */
/* ------------------------------------------------------------------ */
#define KEY_SOURCE_DEFAULT 0        /* 内置 key_default（demo，开箱即用） */
#define KEY_SOURCE_FILE    1        /* 从运行目录的 aes.key 加载         */
#define KEY_SOURCE         KEY_SOURCE_FILE

#if KEY_SOURCE != KEY_SOURCE_DEFAULT && KEY_SOURCE != KEY_SOURCE_FILE
#error "KEY_SOURCE must be KEY_SOURCE_DEFAULT or KEY_SOURCE_FILE"
#endif

/* 密钥文件：由 04-AESKeyGen 生成，使用者手动拷贝到运行目录 */
#define KEY_FILE "aes.key"

/* aes.key 格式（与 04-AESKeyGen 对称）：
 *   [0]        1     hi = 区 A 行号 (0..31)
 *   [1, 15)   14     随机填充
 *   [15]       1     lo = 区 B 行号 (0..31)
 *   [16, 528) 512    区 A：32 行 × 16B，第 hi 行 ← key[0..16)
 *   [528,1040)512    区 B：32 行 × 16B，第 lo 行 ← key[16..32) */
#define KEY_FILE_SIZE     1040
#define KEY_HEADER_SIZE   16
#define KEY_AREA_SIZE     512
#define KEY_ROW_SIZE      16
#define KEY_ROWS_PER_AREA (KEY_AREA_SIZE / KEY_ROW_SIZE)      /* 32  */
#define KEY_AREA_A_OFFSET KEY_HEADER_SIZE                     /* 16  */
#define KEY_AREA_B_OFFSET (KEY_HEADER_SIZE + KEY_AREA_SIZE)   /* 528 */
#define KEY_IDX_HI_OFFSET 0
#define KEY_IDX_LO_OFFSET 15

#if KEY_SOURCE == KEY_SOURCE_DEFAULT
/* AES-256-CBC 密钥：32 字节硬编码（仅供 demo 测试，请勿用于生产） */
static const unsigned char key_default[AES_KEY_LEN] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
    0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88,
    0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00
};
#endif

/* ------------------------------------------------------------------ */
/* 工具函数                                                            */
/* ------------------------------------------------------------------ */

// 设置控制台为 UTF-8 编码
void setConsoleUtf8() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}

/** 以 hex 格式打印缓冲区内容 */
static void print_hex(const char *label, const unsigned char *data, size_t len)
{
    printf("%-24s [%4zu bytes] ", label, len);
    for (size_t i = 0; i < len; i++) {
        printf("%02X", data[i]);
        if ((i + 1) % 16 == 0 && (i + 1) < len)
            printf("\n%24s  ", "");
    }
    printf("\n");
}

/**
 * PKCS7 填充
 * @return 填充后的长度，失败返回 0
 */
static size_t pkcs7_pad(unsigned char *out, size_t out_size,
                        const unsigned char *in, size_t in_len)
{
    const size_t block_size = 16;
    size_t pad_len = block_size - (in_len % block_size);
    if (out_size < in_len + pad_len)
        return 0;
    memcpy(out, in, in_len);
    memset(out + in_len, (unsigned char)pad_len, pad_len);
    return in_len + pad_len;
}

/**
 * PKCS7 去填充
 * @return 去填充后的长度，失败返回 0
 */
static size_t pkcs7_unpad(const unsigned char *in, size_t in_len)
{
    if (in_len == 0 || in_len % 16 != 0)
        return 0;
    unsigned char pad_val = in[in_len - 1];
    if (pad_val < 1 || pad_val > 16)
        return 0;
    for (size_t i = in_len - pad_val; i < in_len; i++) {
        if (in[i] != pad_val)
            return 0;
    }
    return in_len - pad_val;
}

#if KEY_SOURCE == KEY_SOURCE_FILE
/*
 * 从 aes.key 读取 AES-256 密钥（格式见 04-AESKeyGen）
 * @param key 输出缓冲区，长度至少 AES_KEY_LEN 字节
 * @return 成功返回 0，失败返回 -1
 */
static int load_key_from_file(unsigned char *key)
{
    unsigned char file[KEY_FILE_SIZE];

    FILE *fp = fopen(KEY_FILE, "rb");
    if (fp == NULL) {
        fprintf(stderr, "load_key: cannot open key file '%s': %s\n",
                KEY_FILE, strerror(errno));
        return -1;
    }

    size_t n = fread(file, 1, KEY_FILE_SIZE, fp);
    fclose(fp);
    if (n != KEY_FILE_SIZE) {
        fprintf(stderr, "load_key: key file '%s' size mismatch "
                        "(expected %d bytes, got %zu)\n",
                KEY_FILE, KEY_FILE_SIZE, n);
        return -1;
    }

    unsigned char hi = file[KEY_IDX_HI_OFFSET];
    unsigned char lo = file[KEY_IDX_LO_OFFSET];
    if (hi >= KEY_ROWS_PER_AREA || lo >= KEY_ROWS_PER_AREA) {
        fprintf(stderr, "load_key: key file '%s' is corrupt "
                        "(row index out of range: hi=%u lo=%u)\n",
                KEY_FILE, hi, lo);
        return -1;
    }

    /* 密钥 = 区 A 第 hi 行 + 区 B 第 lo 行，各 16 字节 */
    memcpy(key, file + KEY_AREA_A_OFFSET + (size_t)hi * KEY_ROW_SIZE,
           KEY_ROW_SIZE);
    memcpy(key + KEY_ROW_SIZE,
           file + KEY_AREA_B_OFFSET + (size_t)lo * KEY_ROW_SIZE,
           KEY_ROW_SIZE);

    return 0;
}
#endif  /* KEY_SOURCE == KEY_SOURCE_FILE */

/*
 * 加载 AES-256 密钥
 * @param key 输出缓冲区，长度至少 AES_KEY_LEN 字节
 * @param len 缓冲区长度
 * @return 成功返回 0，失败返回 -1
 */
static int load_key(unsigned char *key, size_t len)
{
    if (key == NULL || len < AES_KEY_LEN) {
        fprintf(stderr, "load_key: invalid arg (key=%p, len=%zu)\n",
                (void *)key, len);
        return -1;
    }

#if KEY_SOURCE == KEY_SOURCE_FILE
    printf("KEYMODE: FILE\n");
    /* 文件缺失 / 大小异常 / 行号越界时由 load_key_from_file 报错并失败 */
    if (load_key_from_file(key) != 0)
        return -1;
#else
    printf("KEYMODE: DEFAULT\n");
    memcpy(key, key_default, AES_KEY_LEN);
#endif

    return 0;
}

/* ------------------------------------------------------------------ */
/* AES-256-CBC 加密 / 解密（PSA Crypto API + 手动 PKCS7）     */
/* ------------------------------------------------------------------ */

/**
 * AES-256-CBC 加密
 * 输出格式：output = IV(16字节) + 密文
 */
static psa_status_t aes256_cbc_encrypt(unsigned char *out, size_t *out_len,
                                       const unsigned char *in, size_t in_len,
                                       psa_key_id_t key_id)
{
    psa_status_t status;
    psa_cipher_operation_t op = PSA_CIPHER_OPERATION_INIT;

    /* 手动 PKCS7 填充 */
    unsigned char padded[1024];
    size_t padded_len = pkcs7_pad(padded, sizeof(padded), in, in_len);
    if (padded_len == 0) {
        fprintf(stderr, "pkcs7_pad failed\n");
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    /* 设置加密 */
    status = psa_cipher_encrypt_setup(&op, key_id, PSA_ALG_CBC_NO_PADDING);
    if (status != PSA_SUCCESS) {
        fprintf(stderr, "psa_cipher_encrypt_setup failed: %ld\n", (long)status);
        return status;
    }

    /* 生成随机 IV */
    size_t iv_len;
    status = psa_cipher_generate_iv(&op, out, 16, &iv_len);
    if (status != PSA_SUCCESS) {
        fprintf(stderr, "psa_cipher_generate_iv failed: %ld\n", (long)status);
        psa_cipher_abort(&op);
        return status;
    }

    /* 加密填充后的数据 */
    size_t ct_len;
    status = psa_cipher_update(&op, padded, padded_len,
                                out + 16, 1024 - 16, &ct_len);
    if (status != PSA_SUCCESS) {
        fprintf(stderr, "psa_cipher_update (encrypt) failed: %ld\n", (long)status);
        psa_cipher_abort(&op);
        return status;
    }

    /* 完成加密 */
    size_t finish_len;
    status = psa_cipher_finish(&op, out + 16 + ct_len, 1024 - 16 - ct_len, &finish_len);
    if (status != PSA_SUCCESS) {
        fprintf(stderr, "psa_cipher_finish (encrypt) failed: %ld\n", (long)status);
        psa_cipher_abort(&op);
        return status;
    }

    *out_len = 16 + ct_len + finish_len;
    return PSA_SUCCESS;
}

/**
 * AES-256-CBC 解密
 * 输入格式：input = IV(16字节) + 密文
 */
static psa_status_t aes256_cbc_decrypt(unsigned char *out, size_t *out_len,
                                       const unsigned char *in, size_t in_len,
                                       psa_key_id_t key_id)
{
    if (in_len <= 16) {
        fprintf(stderr, "aes256_cbc_decrypt: input too short\n");
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    psa_status_t status;
    psa_cipher_operation_t op = PSA_CIPHER_OPERATION_INIT;

    const unsigned char *iv = in;
    const unsigned char *ciphertext = in + 16;
    size_t ciphertext_len = in_len - 16;

    /* 设置解密 */
    status = psa_cipher_decrypt_setup(&op, key_id, PSA_ALG_CBC_NO_PADDING);
    if (status != PSA_SUCCESS) {
        fprintf(stderr, "psa_cipher_decrypt_setup failed: %ld\n", (long)status);
        return status;
    }

    /* 设置 IV */
    status = psa_cipher_set_iv(&op, iv, 16);
    if (status != PSA_SUCCESS) {
        fprintf(stderr, "psa_cipher_set_iv failed: %ld\n", (long)status);
        psa_cipher_abort(&op);
        return status;
    }

    /* 解密（输出含 PKCS7 填充） */
    size_t pt_len;
    status = psa_cipher_update(&op, ciphertext, ciphertext_len,
                                out, 1024, &pt_len);
    if (status != PSA_SUCCESS) {
        fprintf(stderr, "psa_cipher_update (decrypt) failed: %ld\n", (long)status);
        psa_cipher_abort(&op);
        return status;
    }

    /* 完成解密 */
    size_t finish_len;
    status = psa_cipher_finish(&op, out + pt_len, 1024 - pt_len, &finish_len);
    if (status != PSA_SUCCESS) {
        fprintf(stderr, "psa_cipher_finish (decrypt) failed: %ld\n", (long)status);
        psa_cipher_abort(&op);
        return status;
    }

    /* 手动 PKCS7 去填充 */
    size_t total_len = pt_len + finish_len;
    size_t unpadded_len = pkcs7_unpad(out, total_len);
    if (unpadded_len == 0) {
        fprintf(stderr, "pkcs7_unpad failed\n");
        return PSA_ERROR_INVALID_PADDING;
    }

    *out_len = unpadded_len;
    return PSA_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* 测试场景                                                            */
/* ------------------------------------------------------------------ */

static int test_basic_scenario(psa_key_id_t key_id)
{
    int pass = 1;

    const char *plaintext = "Hello, AES-256-CBC 加密 Demo! 文本测试。";
    size_t plaintext_len = strlen(plaintext);

    printf("=== AES-256-CBC 基本测试 (PSA Crypto API) ===\n\n");
    printf("明文: %s\n", plaintext);
    print_hex("明文 (hex)", (const unsigned char *)plaintext, plaintext_len);
    printf("\n");

    /* 加密 */
    unsigned char encrypted[1024];
    size_t encrypted_len = 0;

    psa_status_t status = aes256_cbc_encrypt(encrypted, &encrypted_len,
                                               (const unsigned char *)plaintext,
                                               plaintext_len, key_id);
    if (status != PSA_SUCCESS) {
        fprintf(stderr, "加密失败 (status=%ld)\n", (long)status);
        return 0;
    }

    print_hex("IV", encrypted, 16);
    print_hex("密文", encrypted + 16, encrypted_len - 16);
    printf("\n");

    /* 解密 */
    unsigned char decrypted[1024];
    size_t decrypted_len = 0;

    status = aes256_cbc_decrypt(decrypted, &decrypted_len,
                               encrypted, encrypted_len, key_id);
    if (status != PSA_SUCCESS) {
        fprintf(stderr, "解密失败 (status=%ld)\n", (long)status);
        return 0;
    }

    if (decrypted_len < sizeof(decrypted))
        decrypted[decrypted_len] = '\0';
    else
        decrypted[sizeof(decrypted) - 1] = '\0';

    print_hex("解密结果 (hex)", decrypted, decrypted_len);
    printf("解密结果 (string): %s\n\n", (const char *)decrypted);

    /* 验证 */
    if (decrypted_len != plaintext_len) {
        fprintf(stderr, "长度不匹配: decrypted_len=%zu, plaintext_len=%zu\n",
                decrypted_len, plaintext_len);
        pass = 0;
    } else if (memcmp(decrypted, plaintext, plaintext_len) != 0) {
        fprintf(stderr, "内容不匹配!\n");
        pass = 0;
    }

    if (pass) {
        printf("[PASS] 加解密验证通过\n");
    } else {
        printf("[FAIL] 加解密验证失败\n");
    }

    return pass;
}

/* ------------------------------------------------------------------ */
/* 主函数                                                              */
/* ------------------------------------------------------------------ */

int main(void)
{
    int all_pass = 1;
    unsigned char key[AES_KEY_LEN];

    setConsoleUtf8();

    /* 初始化 PSA Crypto */
    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        fprintf(stderr, "psa_crypto_init failed: %ld\n", (long)status);
        return 1;
    }

    /* 导入密钥 */
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT |
                                            PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&attributes, PSA_ALG_CBC_NO_PADDING);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, AES_KEY_LEN * 8);

    /* 加载密钥 */
    if (load_key(key, sizeof(key)) != 0) {
        mbedtls_psa_crypto_free();
        return 1;
    }

    psa_key_id_t key_id = 0;
    status = psa_import_key(&attributes, key, sizeof(key), &key_id);
    psa_reset_key_attributes(&attributes);

    /* 密钥已导入 PSA，清除栈上的明文副本 */
    mbedtls_platform_zeroize(key, sizeof(key));

    if (status != PSA_SUCCESS) {
        fprintf(stderr, "psa_import_key failed: %ld\n", (long)status);
        mbedtls_psa_crypto_free();
        return 1;
    }

    printf("AES-256-CBC 加解密 Demo (PSA Crypto API)\n");
    printf("============================================\n\n");

    /* 运行测试场景 */
    if (!test_basic_scenario(key_id)) {
        all_pass = 0;
    }

    /* 清理 */
    psa_destroy_key(key_id);
    mbedtls_psa_crypto_free();

    printf("\n===== 总体结果: %s =====\n", all_pass ? "PASS" : "FAIL");
    return all_pass ? 0 : 1;
}
