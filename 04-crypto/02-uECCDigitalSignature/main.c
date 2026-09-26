#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <windows.h>
#include <bcrypt.h>

#include "uECC.h"

/* ===== SHA-256 实现 ===== */

#define SHA256_BLOCK_SIZE 64
#define SHA256_HASH_SIZE  32

typedef struct {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t  data[SHA256_BLOCK_SIZE];
    uint32_t datalen;
} SHA256_CTX;

static const uint32_t k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
static uint32_t choose_(uint32_t e, uint32_t f, uint32_t g) { return (e & f) ^ (~e & g); }
static uint32_t majority_(uint32_t e, uint32_t f, uint32_t g) { return (e & f) ^ (e & g) ^ (f & g); }
static uint32_t sig0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
static uint32_t sig1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
static uint32_t theta0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
static uint32_t theta1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

static void sha256_transform(SHA256_CTX *ctx, const uint8_t *data)
{
    uint32_t m[64];
    uint32_t a, b, c, d, e, f, g, h, t1, t2;

    for (int i = 0, j = 0; i < 16; i++, j += 4)
        m[i] = ((uint32_t)data[j] << 24) | ((uint32_t)data[j+1] << 16) |
               ((uint32_t)data[j+2] << 8) | data[j+3];

    for (int i = 16; i < 64; i++)
        m[i] = theta1(m[i-2]) + m[i-7] + theta0(m[i-15]) + m[i-16];

    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];

    for (int i = 0; i < 64; i++) {
        t1 = h + sig1(e) + choose_(e, f, g) + k[i] + m[i];
        t2 = sig0(a) + majority_(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static void sha256_init(SHA256_CTX *ctx)
{
    ctx->datalen = 0;
    ctx->bitlen = 0;
    ctx->state[0] = 0x6a09e667; ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372; ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f; ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab; ctx->state[7] = 0x5be0cd19;
}

static void sha256_update(SHA256_CTX *ctx, const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        ctx->data[ctx->datalen++] = data[i];
        if (ctx->datalen == SHA256_BLOCK_SIZE) {
            sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

static void sha256_final(SHA256_CTX *ctx, uint8_t *hash)
{
    size_t i = ctx->datalen;
    ctx->data[i++] = 0x80;
    while (i < 56) ctx->data[i++] = 0x00;
    if (ctx->datalen >= 56) {
        sha256_transform(ctx, ctx->data);
        memset(ctx->data, 0, 56);
    }
    ctx->bitlen += ctx->datalen * 8;
    ctx->data[63] = (uint8_t)(ctx->bitlen & 0xff);
    ctx->data[62] = (uint8_t)((ctx->bitlen >> 8) & 0xff);
    ctx->data[61] = (uint8_t)((ctx->bitlen >> 16) & 0xff);
    ctx->data[60] = (uint8_t)((ctx->bitlen >> 24) & 0xff);
    ctx->data[59] = (uint8_t)((ctx->bitlen >> 32) & 0xff);
    ctx->data[58] = (uint8_t)((ctx->bitlen >> 40) & 0xff);
    ctx->data[57] = (uint8_t)((ctx->bitlen >> 48) & 0xff);
    ctx->data[56] = (uint8_t)((ctx->bitlen >> 56) & 0xff);
    sha256_transform(ctx, ctx->data);
    for (i = 0; i < 8; i++) {
        hash[i*4]   = (uint8_t)(ctx->state[i] >> 24);
        hash[i*4+1] = (uint8_t)(ctx->state[i] >> 16);
        hash[i*4+2] = (uint8_t)(ctx->state[i] >> 8);
        hash[i*4+3] = (uint8_t)ctx->state[i];
    }
}

static void sha256_hash(const uint8_t *data, size_t len, uint8_t *hash)
{
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, hash);
}

/* ===== 工具函数 ===== */

static int default_RNG(uint8_t *dest, unsigned size)
{
    return BCryptGenRandom(NULL, dest, size, BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0;
}

static void print_hex(const char *label, const uint8_t *data, size_t len)
{
    printf("%s (len=%zu):\n  ", label, len);
    for (size_t i = 0; i < len; i++) {
        printf("%02x", data[i]);
        if ((i + 1) % 32 == 0 && i + 1 < len) printf("\n  ");
    }
    printf("\n");
}

/* ===== secp256r1 密钥对（硬编码） =====
 * 私钥: 32 字节 | 公钥: 64 字节 (非压缩格式 X || Y)
 * 由 uECC 预先生成，可替换为你自己的密钥对。
 */

static const uint8_t s_priv_key[32] = {
    0x98,0x77,0x56,0xc3,0x2a,0x7c,0x3f,0x75,
    0x41,0xaa,0xe1,0x9e,0x1c,0xd8,0x3d,0x16,
    0x29,0x45,0x71,0x97,0x90,0x4c,0x02,0xd8,
    0x5e,0xd8,0x3c,0x94,0x6b,0xe0,0x0e,0x14
};

static const uint8_t s_pub_key[64] = {
    0x2e,0xdd,0x4e,0x73,0x2f,0x6c,0xb6,0xea,
    0xae,0x2c,0xab,0xac,0x03,0xe1,0x74,0x70,
    0x21,0x7a,0x59,0x93,0xd7,0xfc,0xab,0x6b,
    0x44,0xaf,0xf2,0xd6,0x83,0x8c,0x99,0x33,
    0x46,0xfa,0x22,0xdf,0xd3,0x6e,0x0a,0x63,
    0xdb,0xa1,0xb9,0x9a,0xbf,0x3c,0xcf,0x24,
    0x33,0x28,0x2e,0x7d,0x71,0xbd,0x4f,0x60,
    0xe0,0xd1,0x53,0x05,0x13,0x34,0x27,0xed
};

/* ===== 主函数 ===== */

static void setConsoleUtf8(void)
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}

int main(void)
{
    uECC_Curve curve = uECC_secp256r1();
    const char *message = "51423597415624876154+0x41578953+mixstring";
    size_t message_len = strlen(message);

    uint8_t hash[SHA256_HASH_SIZE];
    uint8_t signature[64];
    int ret;

    setConsoleUtf8();

    printf("==============================================\n");
    printf("  ECC 数字签名演示程序 (uECC)\n");
    printf("  曲线: secp256r1 (P-256)\n");
    printf("  哈希: SHA-256\n");
    printf("==============================================\n\n");

    print_hex("私钥", s_priv_key, sizeof(s_priv_key));
    print_hex("公钥 (X||Y)", s_pub_key, sizeof(s_pub_key));
    printf("\n");

    /* 计算消息哈希 */
    sha256_hash((const uint8_t *)message, message_len, hash);
    print_hex("消息哈希 SHA-256", hash, SHA256_HASH_SIZE);
    printf("\n");

    /* 签名 */
    uECC_set_rng(default_RNG);
    ret = uECC_sign(s_priv_key, hash, SHA256_HASH_SIZE, signature, curve);
    if (ret != 1) {
        printf("[失败] 签名失败!\n");
        return 1;
    }
    print_hex("签名 (r||s)", signature, sizeof(signature));
    printf("\n");

    /* 验证签名 */
    ret = uECC_verify(s_pub_key, hash, SHA256_HASH_SIZE, signature, curve);
    if (ret == 1) {
        printf("[成功] 签名验证通过!\n");
    } else {
        printf("[失败] 签名验证失败!\n");
        return 1;
    }

    /* 篡改检测 */
    printf("\n--- 篡改检测测试 ---\n");
    uint8_t bad_hash[SHA256_HASH_SIZE];
    sha256_hash((const uint8_t *)"Tampered message!", strlen("Tampered message!"), bad_hash);

    ret = uECC_verify(s_pub_key, bad_hash, SHA256_HASH_SIZE, signature, curve);
    if (ret == 1) {
        printf("[异常] 篡改后的消息竟然验证通过! (不应出现)\n");
        return 1;
    } else {
        printf("[正常] 篡改后的消息验证失败, 符合预期。\n");
    }

    printf("\n==============================================\n");
    printf("  演示完成\n");
    printf("==============================================\n");

    return 0;
}
