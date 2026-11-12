#include <stdio.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#else
#include <errno.h>
#include <sys/random.h>
#endif

/* ------------------------------------------------------------------ */
/* 密钥文件格式                                                        */
/*                                                                     */
/*   偏移        长度   内容                                           */
/*   [0]          1     hi = 区 A 行号 (0..31)                         */
/*   [1, 15)     14     随机填充                                       */
/*   [15]         1     lo = 区 B 行号 (0..31)                         */
/*   [16, 528)   512    区 A：32 行 × 16B，第 hi 行 ← key[0..16)       */
/*   [528, 1040) 512    区 B：32 行 × 16B，第 lo 行 ← key[16..32)      */
/*                                                                     */
/*   除两个索引字节与命中的两行外，其余字节全部为随机数，整个文件      */
/*   与真随机文件在统计上不可区分。                                    */
/* ------------------------------------------------------------------ */

#define KEY_SIZE        32                          /* AES-256 密钥长度 */
#define HEADER_SIZE     16                          /* 头部：记录两个槽位索引 */
#define AREA_SIZE       512                         /* 每个区的字节数 */
#define ROW_SIZE        16                          /* 行宽 = AES block size */
#define ROWS_PER_AREA   (AREA_SIZE / ROW_SIZE)      /* 每区 32 行 */

#define AREA_A_OFFSET   HEADER_SIZE                             /* 16   */
#define AREA_B_OFFSET   (HEADER_SIZE + AREA_SIZE)               /* 528  */
#define FILE_SIZE       (HEADER_SIZE + AREA_SIZE * 2)           /* 1040 */

#define IDX_HI_OFFSET   0                           /* 高 16 字节所在行号 */
#define IDX_LO_OFFSET   15                          /* 低 16 字节所在行号 */

#define KEY_FILE        "aes.key"                   /* 输出文件名（当前路径） */

/* ------------------------------------------------------------------ */
/* 工具函数                                                            */
/* ------------------------------------------------------------------ */

// 设置控制台为 UTF-8 编码
void setConsoleUtf8(void)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

/** 以 hex 格式打印缓冲区内容 */
static void print_hex(const char *label, const unsigned char *data, size_t len)
{
    printf("%-16s [%4zu bytes] ", label, len);
    for (size_t i = 0; i < len; i++) {
        printf("%02X", data[i]);
        if ((i + 1) % ROW_SIZE == 0 && (i + 1) < len)
            printf("\n%30s", "");
    }
    printf("\n");
}

/* ------------------------------------------------------------------ */
/* 随机数：操作系统原生 CSPRNG                                         */
/* ------------------------------------------------------------------ */

#ifdef _WIN32
/*
 * 生成 len 字节密码学安全随机数（Windows CNG）
 * @return 0 成功，-1 失败
 */
static int random_bytes(unsigned char *buf, size_t len)
{
    NTSTATUS status = BCryptGenRandom(NULL, buf, (ULONG)len,
                                      BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return status == 0 ? 0 : -1;
}
#else
/*
 * 生成 len 字节密码学安全随机数（Linux getrandom(2)）
 * @return 0 成功，-1 失败
 */
static int random_bytes(unsigned char *buf, size_t len)
{
    size_t off = 0;
    while (off < len) {
        ssize_t n = getrandom(buf + off, len - off, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        off += (size_t)n;
    }
    return 0;
}
#endif

/* 生成 [0, bound) 区间的随机下标 */
static int random_index(int bound)
{
    uint32_t r;
    if (random_bytes((unsigned char *)&r, sizeof(r)) != 0)
        return -1;
    return (int)(r % (uint32_t)bound);
}

/* ------------------------------------------------------------------ */
/* 生成密钥文件                                                        */
/* ------------------------------------------------------------------ */

static int generate_key_file(void)
{
    unsigned char file[FILE_SIZE];
    unsigned char key[KEY_SIZE];
    int hi, lo;

    /* 1. 生成 32 字节随机密钥 */
    if (random_bytes(key, KEY_SIZE) != 0) {
        fprintf(stderr, "错误：密钥随机数生成失败\n");
        return 1;
    }

    /* 2. 整个文件先填满随机数 */
    if (random_bytes(file, FILE_SIZE) != 0) {
        fprintf(stderr, "错误：文件随机数生成失败\n");
        return 1;
    }

    /* 3. 随机选择两个槽位 */
    hi = random_index(ROWS_PER_AREA);
    lo = random_index(ROWS_PER_AREA);
    if (hi < 0 || lo < 0) {
        fprintf(stderr, "错误：槽位随机数生成失败\n");
        return 1;
    }

    /* 4. 高 16 字节写入区 A 第 hi 行 */
    memcpy(file + AREA_A_OFFSET + hi * ROW_SIZE, key, ROW_SIZE);
    /* 5. 低 16 字节写入区 B 第 lo 行 */
    memcpy(file + AREA_B_OFFSET + lo * ROW_SIZE, key + ROW_SIZE, ROW_SIZE);

    /* 6. 头部记录两个槽位索引 */
    file[IDX_HI_OFFSET] = (unsigned char)hi;
    file[IDX_LO_OFFSET] = (unsigned char)lo;

    /* 7. 落盘 */
    FILE *fp = fopen(KEY_FILE, "wb");
    if (fp == NULL) {
        fprintf(stderr, "错误：无法写入文件 %s\n", KEY_FILE);
        return 1;
    }
    if (fwrite(file, 1, FILE_SIZE, fp) != FILE_SIZE) {
        fprintf(stderr, "错误：写入文件失败 %s\n", KEY_FILE);
        fclose(fp);
        return 1;
    }
    fclose(fp);

    printf("已生成密钥文件: %s (%d 字节)\n", KEY_FILE, FILE_SIZE);
    print_hex("密钥 (key)", key, KEY_SIZE);
    printf("高 16 字节槽位: 区 A 第 %2d 行 (偏移 0x%04X)\n",
           hi, (unsigned)(AREA_A_OFFSET + hi * ROW_SIZE));
    printf("低 16 字节槽位: 区 B 第 %2d 行 (偏移 0x%04X)\n",
           lo, (unsigned)(AREA_B_OFFSET + lo * ROW_SIZE));
    return 0;
}

/* ------------------------------------------------------------------ */
/* 主函数                                                              */
/* ------------------------------------------------------------------ */

int main(void)
{
    setConsoleUtf8();
    return generate_key_file();
}
