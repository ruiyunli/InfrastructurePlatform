/*
 * net-tls.c - TLS 传输后端（占位实现）
 *
 * 真正的 mbedTLS 接入尚未完成。当前直接复用纯 TCP 后端，
 * 保证 -tls 构建可以完成配置、编译和运行；两种构建行为完全一致。
 *
 * 接入 mbedTLS 时替换本文件内容即可，net-backend.h 的契约不变。
 */
#include "net-notls.c"
