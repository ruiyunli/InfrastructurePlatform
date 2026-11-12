# AES-ECDSA 升级包签名与加密工具

用于在网络安全要求下生成和接收升级包：发送端加密并签名，接收端验签通过后再解密。

当前状态：命令行骨架已实现，加密/签名算法尚未实现（见文末"实现状态"）。

## 1. 目标与非目标

**目标**

- 生成端把原始升级包（如 `update.zip`）加密、签名，输出单一 `.package` 文件。
- 接收端对包做完整性和来源认证，验签通过后才解密还原原始文件。
- 处理 50MB–500MB 的大包时，内存占用保持恒定，不随文件大小增长。

**非目标（当前版本）**

- 不做密钥分发和密钥轮换。
- 不做防回滚（包版本校验）。
- 不压缩数据（输入通常已是 zip，重复压缩没有收益）。

## 2. 算法选型

| 用途 | 算法 | 说明 |
|------|------|------|
| 加密 | AES-256-GCM | AEAD，密文与认证标签一体，分块流式处理 |
| 签名 | ECDSA P-256 (SECP256R1) + SHA-256 | 私钥签名，公钥验签 |
| 密钥文件 | AES 密钥为原始 32 字节；ECDSA 为 DER 编码 | 私钥 PKCS#8 DER，公钥 SubjectPublicKeyInfo DER |

**处理顺序：Encrypt-then-Sign（先加密，再对密文签名）。**

接收端先验签、后解密，任何情况下都不会在认证完成前把数据交给解压或解析逻辑。

## 3. 命令行

```text
生成升级包:
  AES-ECDSA --pack   --crypt keys/aes256.key --sign keys/ecdsa_private.der \
             --in data/updata.zip --out data/updata.zip.package

接收升级包:
  AES-ECDSA --unpack --crypt keys/aes256.key --sign keys/ecdsa_public.der \
             --in data/updata.zip.package --out data/updata_out.zip
```

- `--crypt`：pack 端为加密密钥，unpack 端为解密密钥。当前设计两端使用同一份 AES-256 密钥。
- `--sign`：pack 端为 ECDSA 私钥，unpack 端为 ECDSA 公钥。
- `--in` / `--out`：输入文件、输出文件，均为必填。
- 支持 `--opt value` 和 `--opt=value` 两种写法。
- `-h` / `--help` 显示用法。
- 所有运行时提示使用英文，便于在无中文环境的终端中运行。

## 4. 包格式

### 4.1 整体布局

```text
+------------------+--------------------------+-------------+----------+-------------+
| header (28 B)    | ciphertext (N B)         | GCM tag     | sig_len  | signature   |
|                  |                          | (16 B)      | (2 B)    | (M B)       |
+------------------+--------------------------+-------------+----------+-------------+
^ 文件头                          主体（流式读写）                ^ 文件尾
```

- `N` = `plaintext_len`，即输入文件字节数。AES-GCM 密文长度与明文完全相同。
- `M` = ECDSA DER 签名长度，P-256 时约 70–72 字节，因此用 `sig_len` 显式记录。
- 所有多字节整数均为大端序（Big-Endian）。

### 4.2 头部字段

| 偏移 | 长度 | 字段 | 说明 |
|------|------|------|------|
| 0 | 4 | magic | ASCII `AEP1`，用于识别文件类型 |
| 4 | 1 | format_version | 包格式版本，当前为 `1` |
| 5 | 1 | crypt_alg | 加密算法，`1` = AES-256-GCM |
| 6 | 1 | sig_alg | 签名算法，`1` = ECDSA-P256-SHA256 |
| 7 | 1 | reserved | 保留，当前填 `0` |
| 8 | 12 | nonce | GCM 随机 nonce，每个包重新生成 |
| 20 | 8 | plaintext_len | 原始文件长度（uint64） |

头部共 28 字节。

### 4.3 签名覆盖范围

签名输入为**偏移 0 到 GCM tag 末尾**的全部字节：

```text
SHA-256( header || ciphertext || GCM tag )
```

`sig_len` 和 `signature` 本身不被签名覆盖。

接收端可以通过头部推导各字段位置：

```text
tag_offset        = 28 + plaintext_len
sig_len_offset    = 28 + plaintext_len + 16
signature_offset  = sig_len_offset + 2
```

### 4.4 头部完整性

除了签名保护，头部的 28 字节同时作为 AES-GCM 的 **AAD**。这样即使跳过验签直接解密，头部被篡改也会导致 GCM 校验失败，形成双重保护。

## 5. 处理流程

### 5.1 pack

1. 打开输入文件，获取文件大小 `N`。
2. 生成 12 字节随机 nonce，组装 28 字节头部。
3. 写头部；初始化 SHA-256 和 AES-GCM（AAD = 头部）。
4. 循环读取明文块（建议 1–4MB）：
   - GCM 加密，密文写入输出；
   - 密文字节增量喂给 SHA-256。
5. GCM 结束，得到 16 字节 tag；写 tag，并把 tag 喂给 SHA-256。
6. 用 ECDSA 私钥对最终 SHA-256 摘要签名。
7. 写 `sig_len`（2 字节）和 DER 签名。
8. 先写临时文件，全部成功后重命名到目标路径，避免产生半成品包。

### 5.2 unpack

1. 读取并校验头部：magic、`format_version`、算法标识、`plaintext_len`。
2. **第一遍扫描（只读，不解密）**：对 `header + ciphertext + tag` 增量计算 SHA-256，读取末尾 `sig_len` 和签名，用 ECDSA 公钥验签。
   - 验签失败：直接报错退出，**不产生任何输出文件**。
3. **第二遍扫描（解密）**：定位到密文起点，初始化 AES-GCM（AAD = 头部），分块解密并写入输出；GCM 结束时校验 tag。
4. 解密失败（tag 不匹配）：删除临时文件，报错退出。
5. 全部成功后重命名临时文件到 `--out` 指定路径。

两遍扫描都是顺序读，500MB 包在本地磁盘上开销可接受；内存中同时只保留一个数据块。

## 6. 密钥与调试数据

`configure-test.py` 在文件缺失时生成以下调试数据，已存在则跳过：

| 路径 | 内容 |
|------|------|
| `data/updata.zip` | 含 `a.txt`、`b.txt` 的测试包，每个文件写入 10 个随机字符 |
| `keys/aes256.key` | 原始 32 字节 AES-256 密钥 |
| `keys/ecdsa_private.der` | ECDSA P-256 私钥，PKCS#8 DER |
| `keys/ecdsa_public.der` | ECDSA P-256 公钥，SubjectPublicKeyInfo DER |

说明：

- ECDSA 公私钥按一组判断，只有两个文件都存在时才跳过；只剩一个时会重新生成整组。
- 这些文件属于本地调试数据，已在 `.gitignore` 中排除，不提交到仓库。
- `configure-test.py` 需要 Python 包 `cryptography`。

## 7. 错误处理

- 参数错误、文件打开失败、格式校验失败、验签失败、解密失败均返回非 0。
- 接收端在验签失败时不解密、不产生输出。
- 面向设备的版本如担心侧信道区分，可把"验签失败"和"解密失败"统一为同一条对外错误信息；命令行调试阶段保留可区分的详细日志。

## 8. 已知限制

- **AES 密钥固定**：所有包共用一份 AES-256 密钥，密钥随设备分发。设备被攻破会泄露保密性，但不会破坏来源认证（签名私钥不在设备上）。
- **无防回滚**：旧版本的有效 `.package` 可以被重放。后续建议把包版本、目标设备型号、时间戳放入被签名的头部或独立 manifest，由接收端对比当前版本。
- **签名算法标识未绑定曲线**：当前固定 P-256；如未来支持多曲线，需要在头部增加曲线标识。
- **单文件非流式输出**：pack 必须先写完再重命名；对超大包需要预留目标磁盘空间。

## 9. 构建

```text
python configure.py        # 检查依赖并生成 CMake 工程
cmake --build build/x64 --config Debug
python configure-test.py   # 生成调试用的包和密钥（可选）
```

依赖：mbedTLS 4.1.0（`3rdparty/` 下）、CMake、MSVC；生成调试数据额外需要 Python 的 `cryptography` 包。

## 10. 实现状态

- [x] 命令行参数解析（`--pack` / `--unpack` / `--crypt` / `--sign` / `--in` / `--out`）
- [x] 调试数据生成脚本 `configure-test.py`
- [ ] AES-256-GCM 分块加解密
- [ ] ECDSA P-256 签名与验签
- [ ] 包格式读写与字段校验
- [ ] 临时文件 + 原子重命名
- [ ] 端到端测试（pack → unpack 还原一致）
