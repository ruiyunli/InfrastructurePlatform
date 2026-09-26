#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <windows.h>
#include <psa/crypto.h>
#include <mbedtls/error.h>

/* AES-256-CBC 密钥：32 字节硬编码（仅供 demo 测试，请勿用于生产） */
static const unsigned char key[32] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
    0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88,
    0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00
};

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

    const char *plaintext = "Hello, AES-256-CBC 加密 Demo! 跨平台兼容测试。";
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
    psa_set_key_bits(&attributes, 256);

    psa_key_id_t key_id = 0;
    status = psa_import_key(&attributes, key, sizeof(key), &key_id);
    psa_reset_key_attributes(&attributes);

    if (status != PSA_SUCCESS) {
        fprintf(stderr, "psa_import_key failed: %ld\n", (long)status);
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

    printf("\n===== 总体结果: %s =====\n", all_pass ? "PASS" : "FAIL");
    return all_pass ? 0 : 1;
}
