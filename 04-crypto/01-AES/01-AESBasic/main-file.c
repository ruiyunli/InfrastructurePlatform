#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <windows.h>
#include <psa/crypto.h>
#include <mbedtls/error.h>
#include <mbedtls/platform_util.h>

#define AES_KEY_LEN 32

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

/* AES-256-CBC 密钥：32 字节硬编码（仅供 demo 测试，请勿用于生产） */
static const unsigned char key_default[AES_KEY_LEN] = {
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

/** 写文件（二进制） */
static int write_file(const char *path, const unsigned char *data, size_t len)
{
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        fprintf(stderr, "fopen(%s, wb) failed: %s\n", path, strerror(errno));
        return -1;
    }
    size_t written = fwrite(data, 1, len, fp);
    fclose(fp);
    if (written != len) {
        fprintf(stderr, "write %s failed: %zu/%zu bytes\n", path, written, len);
        return -1;
    }
    return 0;
}

/** 读文件（二进制），文件大小不能超过 cap */
static int read_file(const char *path, unsigned char *buf, size_t cap, size_t *len)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "fopen(%s, rb) failed: %s\n", path, strerror(errno));
        return -1;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fprintf(stderr, "fseek(%s) failed\n", path);
        fclose(fp);
        return -1;
    }
    long size = ftell(fp);
    if (size < 0 || (size_t)size > cap) {
        fprintf(stderr, "read %s failed: size=%ld, cap=%zu\n", path, size, cap);
        fclose(fp);
        return -1;
    }
    rewind(fp);

    size_t n = fread(buf, 1, (size_t)size, fp);
    fclose(fp);
    if (n != (size_t)size) {
        fprintf(stderr, "read %s failed: %zu/%ld bytes\n", path, n, size);
        return -1;
    }

    *len = n;
    return 0;
}

/** 二进制转小写 hex 字符串（out 至少 2*len+1 字节） */
static void bytes_to_hex(const unsigned char *in, size_t len, char *out)
{
    static const char hex_digits[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        out[i * 2]     = hex_digits[in[i] >> 4];
        out[i * 2 + 1] = hex_digits[in[i] & 0x0F];
    }
    out[len * 2] = '\0';
}

/**
 * 用 PSA 计算 SHA-256，输出 64 字符小写 hex
 * @return 成功返回 0，失败返回 -1
 */
static int sha256_hex(const unsigned char *data, size_t len,
                      char *hex_out, size_t hex_out_size)
{
    unsigned char digest[32];
    size_t digest_len = 0;

    if (hex_out_size < sizeof(digest) * 2 + 1) {
        fprintf(stderr, "sha256_hex: output buffer too small\n");
        return -1;
    }

    psa_status_t status = psa_hash_compute(PSA_ALG_SHA_256, data, len,
                                           digest, sizeof(digest), &digest_len);
    if (status != PSA_SUCCESS) {
        fprintf(stderr, "psa_hash_compute failed: %ld\n", (long)status);
        return -1;
    }
    if (digest_len != sizeof(digest)) {
        fprintf(stderr, "unexpected SHA-256 length: %zu\n", digest_len);
        return -1;
    }

    bytes_to_hex(digest, digest_len, hex_out);
    return 0;
}

/** 通过 cmd.exe 执行命令，成功返回 0 */
static int run_command(const char *command)
{
    printf("  > %s\n", command);
    int rc = system(command);
    if (rc != 0) {
        fprintf(stderr, "command failed (rc=%d): %s\n", rc, command);
        return -1;
    }
    return 0;
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

    memcpy(key, key_default, AES_KEY_LEN);
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

#define ORIGIN_FILE    "origin.txt"
#define ENCRYPT_FILE   "encrypt.bin"
#define SHA_FILE       "sha.txt"
#define ZIP_FILE       "public.zip"
#define EXTRACT_DIR    "extract"
#define RESTORED_FILE  "origin_restored.txt"

#define SHA256_HEX_LEN 64

static int test_basic_scenario(psa_key_id_t key_id)
{
    /* origin.txt 内容：第一行 0-9，第二行 A-Z */
    static const char origin_content[] =
        "0123456789\n"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ\n";

    int pass = 1;
    unsigned char plain[1024];
    size_t plain_len = 0;
    unsigned char encrypted[1024];
    size_t encrypted_len = 0;
    unsigned char extracted[1024];
    size_t extracted_len = 0;
    unsigned char decrypted[1024];
    size_t decrypted_len = 0;
    unsigned char stored_buf[128];
    size_t stored_len = 0;
    char expected_hash[SHA256_HEX_LEN + 1];
    char actual_hash[SHA256_HEX_LEN + 1];
    char path[256];
    psa_status_t status;

    printf("=== 文件加密 + SHA-256 校验 + ZIP 打包测试 ===\n\n");

    /* 1. 生成 origin.txt */
    printf("[1/6] 生成 %s\n", ORIGIN_FILE);
    if (write_file(ORIGIN_FILE, (const unsigned char *)origin_content,
                   sizeof(origin_content) - 1) != 0)
        return 0;
    if (read_file(ORIGIN_FILE, plain, sizeof(plain), &plain_len) != 0)
        return 0;
    printf("  内容 (%zu 字节):\n", plain_len);
    fwrite(plain, 1, plain_len, stdout);
    print_hex("  hex", plain, plain_len);
    printf("\n");

    /* 2. 加密 → encrypt.bin */
    printf("[2/6] AES-256-CBC 加密 → %s\n", ENCRYPT_FILE);
    status = aes256_cbc_encrypt(encrypted, &encrypted_len,
                                plain, plain_len, key_id);
    if (status != PSA_SUCCESS) {
        fprintf(stderr, "加密失败 (status=%ld)\n", (long)status);
        return 0;
    }
    if (write_file(ENCRYPT_FILE, encrypted, encrypted_len) != 0)
        return 0;
    print_hex("  IV", encrypted, 16);
    print_hex("  密文", encrypted + 16, encrypted_len - 16);
    printf("\n");

    /* 3. 计算 SHA-256 → sha.txt */
    printf("[3/6] SHA-256 计算 → %s\n", SHA_FILE);
    if (sha256_hex(encrypted, encrypted_len,
                   expected_hash, sizeof(expected_hash)) != 0)
        return 0;
    {
        char line[SHA256_HEX_LEN + 2];
        int len = snprintf(line, sizeof(line), "%s\n", expected_hash);
        if (write_file(SHA_FILE, (const unsigned char *)line, (size_t)len) != 0)
            return 0;
    }
    printf("  SHA-256: %s\n\n", expected_hash);

    /* 4. 打包 public.zip（使用系统自带 bsdtar） */
    printf("[4/6] 打包 → %s\n", ZIP_FILE);
    if (run_command("tar -a -c -f " ZIP_FILE " "
                    ENCRYPT_FILE " " SHA_FILE) != 0)
        return 0;
    printf("\n");

    /* 5. 解包 public.zip → extract/ */
    printf("[5/6] 解包 %s → %s/\n", ZIP_FILE, EXTRACT_DIR);
    if (!CreateDirectoryA(EXTRACT_DIR, NULL) &&
        GetLastError() != ERROR_ALREADY_EXISTS) {
        fprintf(stderr, "创建目录 %s 失败\n", EXTRACT_DIR);
        return 0;
    }
    if (run_command("tar -x -f " ZIP_FILE " -C " EXTRACT_DIR) != 0)
        return 0;

    /* 对解包出的 encrypt.bin 重新计算 SHA-256，与 sha.txt 比对 */
    snprintf(path, sizeof(path), "%s/%s", EXTRACT_DIR, ENCRYPT_FILE);
    if (read_file(path, extracted, sizeof(extracted), &extracted_len) != 0)
        return 0;
    if (sha256_hex(extracted, extracted_len,
                   actual_hash, sizeof(actual_hash)) != 0)
        return 0;

    snprintf(path, sizeof(path), "%s/%s", EXTRACT_DIR, SHA_FILE);
    if (read_file(path, stored_buf, sizeof(stored_buf) - 1, &stored_len) != 0)
        return 0;
    stored_buf[stored_len] = '\0';

    printf("  解包密文 SHA-256: %s\n", actual_hash);
    if (stored_len < SHA256_HEX_LEN) {
        fprintf(stderr, "  sha.txt 内容过短 (%zu 字节)\n", stored_len);
        pass = 0;
    } else if (_strnicmp((const char *)stored_buf, actual_hash,
                         SHA256_HEX_LEN) != 0) {
        fprintf(stderr, "  完整性校验失败: sha.txt 与解包密文不匹配\n");
        pass = 0;
    } else {
        printf("  完整性校验通过\n");
    }
    printf("\n");

    /* 6. 解密还原 origin.txt */
    printf("[6/6] 解密 → %s\n", RESTORED_FILE);
    status = aes256_cbc_decrypt(decrypted, &decrypted_len,
                                extracted, extracted_len, key_id);
    if (status != PSA_SUCCESS) {
        fprintf(stderr, "解密失败 (status=%ld)\n", (long)status);
        return 0;
    }
    if (write_file(RESTORED_FILE, decrypted, decrypted_len) != 0)
        return 0;

    printf("  还原内容 (%zu 字节):\n", decrypted_len);
    fwrite(decrypted, 1, decrypted_len, stdout);
    print_hex("  hex", decrypted, decrypted_len);

    /* 与 origin.txt 的原始内容比对 */
    if (decrypted_len != plain_len) {
        fprintf(stderr, "长度不匹配: decrypted_len=%zu, plain_len=%zu\n",
                decrypted_len, plain_len);
        pass = 0;
    } else if (memcmp(decrypted, plain, plain_len) != 0) {
        fprintf(stderr, "内容不匹配!\n");
        pass = 0;
    }

    if (pass) {
        printf("[PASS] 加密 → 校验 → 打包 → 解包 → 还原 全流程通过\n");
    } else {
        printf("[FAIL] 流程验证失败\n");
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
