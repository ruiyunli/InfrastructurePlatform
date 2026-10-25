# socket（TLS 1.3 / 纯 TCP 示例）

跨平台（Windows / Linux）C 语言 TCP 通信示例工程。同一套业务代码通过编译开关
选择底层传输后端：

| 变体 | 后端文件 | 说明 |
|---|---|---|
| `notls` | `net-notls.c` | 明文 TCP，无第三方依赖 |
| `tls` | `net-tls.c` | mbedTLS 4.1.0，TLS 1.3，ECDSA P-256，单向认证 |

程序包含一个服务端和一个客户端：

- 服务端最多接受 5 个客户端，命令行输入的内容会广播给所有客户端
- 客户端连接后，命令行输入的内容发送给服务端，并显示服务端消息
- 输入 `exit` / `quit` / `q` 断开连接或退出程序

## 目录结构

```
02-socket/
├── CMakeLists.txt       构建脚本，NET_ENABLE_TLS 选择后端
├── configure.py         配置脚本：检查依赖、生成构建目录
├── net-backend.h        传输后端接口（连接、读写、等待、TLS 配置）
├── net-common.c/.h      跨平台公共代码（select、行读写、命令行输入线程）
├── net-internal.h       平台相关的内部定义
├── net-notls.c          纯 TCP 后端
├── net-tls.c            TLS 1.3 后端
├── main-server.c        服务端示例
├── main-client.c        客户端示例
├── certs/               TLS 证书（构建时随 install 一起部署）
│   ├── ca.crt.pem       客户端信任的 CA 证书
│   ├── server.crt.pem   服务器证书
│   └── server.key.pem   服务器私钥
├── build/               构建输出（已忽略）
├── install/             安装输出（已忽略）
└── 3rdparty/            mbedTLS 预编译库（已忽略，由 configure.py 下载）
```

`install/<platform>/<variant>/` 是唯一需要部署的目录：可执行文件与 `certs/` 都在这里。

## 环境要求

- CMake 3.15+
- Windows：Visual Studio（生成器见 `configure.py`，当前为 `Visual Studio 18 2026`）+ x64
- Linux：GCC/Clang + Make
- TLS 变体需要 mbedTLS 4.1.0：
  - Windows：`configure.py` 会从 SVN 导出到 `3rdparty/mbedtls-4.1.0/win32`
  - Linux：SVN 上暂无预编译包，需要自行编译并放到 `3rdparty/mbedtls-4.1.0/linux`
    （目录中需包含 `include/` 和 `lib/libmbedtls.a`、`libmbedx509.a`、`libtfpsacrypto.a`）
- 生成证书需要 OpenSSL 3.x（仅开发阶段使用，运行程序本身不需要）

## 构建

### Windows

```bat
python configure.py -tls
```

然后打开 `build/x64/tls/socket-tls-tls.slnx`
（Visual Studio 2026 使用新的 `.slnx` 解决方案格式；旧版生成器为 `.sln`）：

1. 生成 `ALL_BUILD`（Debug 或 Release）
2. 生成 `INSTALL` —— 可执行文件和 `certs/` 会同步到 `install/x64/tls/`

纯 TCP 变体：

```bat
python configure.py -notls
```

对应 `build/x64/notls/socket-tls-notls.slnx`，安装目录为 `install/x64/notls/`。

### Linux

```bash
python3 configure.py -tls
cmake --build build/linux/tls -j
cmake --install build/linux/tls
```

安装目录为 `install/linux/tls/`。

> `configure.py` 只负责生成构建文件；必须再执行 `INSTALL`（或 `cmake --install`），
> 可执行文件和证书才会出现在 `install/` 下。

## 运行

程序使用**相对工作目录**查找 `certs/`，因此请在安装目录中启动：

```bash
cd install/x64/tls     # Linux 为 install/linux/tls
./server.exe           # Linux 为 ./server
./client.exe           # 另开一个终端
```

Visual Studio 调试时无需手动切换目录，工程的调试工作目录已经设置为安装目录，
按 F5 直接运行 `server` 即可。

连接地址在源码中定义：

- `main-server.c`：`SERVER_BIND_IP` / `SERVER_PORT`
- `main-client.c`：`SERVER_IP` / `SERVER_PORT`

本机测试保持 `127.0.0.1:8080`；跨机器时把客户端 `SERVER_IP` 改为服务端 IP
（也可以是 `localhost` 这类域名，内部通过 `getaddrinfo` 解析），
服务端 `SERVER_BIND_IP` 改为 `0.0.0.0` 或网卡 IP。

## TLS 说明

### 认证模式

当前实现为**单向认证**：客户端校验服务器证书，服务端不要求客户端证书。

- 服务端：`verify_peer = 0`，加载 `server.crt.pem` + `server.key.pem`
- 客户端：`verify_peer = 1`，用 `ca.crt.pem` 校验服务器，并校验证书名字

后端已经支持双向认证：把服务端 `verify_peer` 设为 1 并提供 `ca_file`，
客户端再提供 `cert_file` / `key_file` 即可。

### 证书校验名（server_name）

`SERVER_IP` 决定"连接哪个地址"，`server_name` 决定"证书应该写哪个名字"，
两者在 TLS 中是独立的。示例中：

```c
#define TLS_SERVER_NAME NULL   /* NULL -> 使用 SERVER_IP 作为校验名 */
```

这样"连哪个地址"和"证书写了什么名字"天然一致：

- `SERVER_IP` 是 IP（如 `127.0.0.1`）→ 证书 SAN 需要包含 `IP:127.0.0.1`
- `SERVER_IP` 是域名（如 `server.example.com`）→ SAN 需要包含 `DNS:server.example.com`

只有一种情况需要显式设置 `TLS_SERVER_NAME`：**用 IP 连接，但证书只签给了域名**
（例如内网用 `192.168.1.10` 访问、证书是 `server.example.com` 签发的）。
此时把 `TLS_SERVER_NAME` 填为域名即可，但它与"实际场景中两者一致"的直觉相反，
不推荐在正式环境长期使用。

### 配置项

`net_tls_config_t`（见 `net-backend.h`）：

| 字段 | 说明 |
|---|---|
| `ca_file` | 信任的 CA 链，PEM 中可拼接多张证书 |
| `cert_file` | 本端证书；服务端必填，客户端仅在双向认证时使用 |
| `key_file` | 本端私钥，需与 `cert_file` 配套 |
| `server_name` | 客户端校验证书用的名字；`NULL` 表示使用 `net_connect` 的 host |
| `verify_peer` | 客户端：1=校验服务器；服务端：1=要求客户端证书 |
| `handshake_timeout_ms` | 握手总超时，`<=0` 使用默认 5000ms |

### PEM 与 DER

推荐 PEM：

- mbedTLS 自动识别格式，PEM / DER 都能加载
- 一个 PEM 文件可以按顺序拼接多张证书，适合部署证书链
- `cert_file` 也可以写成 `server.crt.pem + intermediate.crt.pem` 拼接后的文件

DER 一个文件只能放一张证书，部署证书链时需要拆成多个文件，因此本工程默认使用 PEM。

## 生成证书

以下命令使用 OpenSSL 3.x、P-256（`prime256v1`），签发的证书可用于 TLS 1.3。
在工作目录 `certs/` 中执行；如果使用 Git Bash，先关闭路径转换，避免
`/CN=...` 被转换成 Windows 路径：

```bash
export MSYS_NO_PATHCONV=1
cd certs
```

### 1. 生成 CA

```bash
openssl genpkey -algorithm EC -pkeyopt ec_paramgen_curve:P-256 -out ca.key.pem

openssl req -new -x509 -key ca.key.pem -sha256 -days 3650 \
    -out ca.crt.pem \
    -subj "/CN=Socket Test CA/O=InfrastructurePlatform"
```

`ca.key.pem` 是 CA 私钥，只用于签发证书，**不要提交到仓库，也不要部署到客户端**。
仓库中只保留 `ca.crt.pem`。

### 2. 生成服务器私钥与证书请求

```bash
openssl genpkey -algorithm EC -pkeyopt ec_paramgen_curve:P-256 -out server.key.pem

openssl req -new -key server.key.pem -out server.csr.pem \
    -subj "/CN=localhost"
```

`server.key.pem` 默认是 PKCS#8 格式（`-----BEGIN PRIVATE KEY-----`），mbedTLS 可以直接读取。

### 3. 定义扩展项（SAN）

新建 `server.ext`：

```
basicConstraints=critical,CA:FALSE
keyUsage=critical,digitalSignature
extendedKeyUsage=serverAuth
subjectAltName=DNS:localhost,IP:127.0.0.1
```

- `DNS:localhost`：`SERVER_IP` 使用域名或 `server_name=localhost` 时需要
- `IP:127.0.0.1`：`SERVER_IP` 使用 IP 时需要
- `extendedKeyUsage=serverAuth`：明确证书用途
- 如果服务端有多个访问地址，在 SAN 中继续追加，例如
  `subjectAltName=DNS:localhost,DNS:server.example.com,IP:127.0.0.1,IP:192.168.1.10`

TLS 1.3 要求签名算法支持 P-256 + SHA-256，上述配置满足要求。

### 4. 用 CA 签发服务器证书

```bash
openssl x509 -req -in server.csr.pem \
    -CA ca.crt.pem -CAkey ca.key.pem -CAcreateserial \
    -out server.crt.pem -days 3650 -sha256 \
    -extfile server.ext
```

完成后 `certs/` 中保留运行时需要的三个文件：

```
ca.crt.pem
server.crt.pem
server.key.pem
```

`server.csr.pem`、`server.ext`、`ca.crt.srl` 属于中间产物，可以删除；
`ca.key.pem` 请离线保存好，后续用它给新证书签名。

### 5. 检查证书

```bash
openssl x509 -in server.crt.pem -noout -subject -issuer -dates
openssl x509 -in server.crt.pem -noout -ext subjectAltName
openssl verify -CAfile ca.crt.pem server.crt.pem
```

### 6. 证书链

如果服务器证书由中间 CA 签发：

```bash
cat server.crt.pem intermediate.crt.pem > server-chain.crt.pem
```

把 `server-chain.crt.pem` 作为 `cert_file`，客户端 `ca_file` 中放入根 CA
（也可以按 `根 CA + 中间 CA` 的顺序拼接）。

## 部署证书

证书源目录是工程根目录的 `certs/`，构建 `INSTALL` 时会整体复制到
`install/<platform>/tls/certs/`。

替换成自己的证书时：

1. 用同名文件覆盖 `certs/` 下的 `ca.crt.pem`、`server.crt.pem`、`server.key.pem`
2. 重新生成 `INSTALL`，或手动复制到 `install/x64/tls/certs/`
3. 如果证书的 DNS 名或 IP 与源码中的 `SERVER_IP` 不一致，同步调整 `main-client.c`

> 仓库中的证书是**开发测试用自签名材料**，仅用于本地联调。
> 正式环境请重新生成，并妥善保管私钥，不要把生产私钥提交到仓库。

## 常见问题

**启动时报证书加载失败**

确认工作目录是 `install/x64/tls`（或 `install/linux/tls`），且 `certs/`
位于该目录下。程序使用的是相对路径 `certs/...`。

**客户端报 `Certificate verification failed`**

- CA 不匹配：客户端 `ca.crt.pem` 必须能验证服务器证书
- 名字不匹配：`SERVER_IP` / `TLS_SERVER_NAME` 必须在证书 SAN 中
  （IP 对应 `IP:` 项，域名对应 `DNS:` 项）

**TLS 1.2 客户端连不上**

本工程的 mbedTLS 配置只启用 TLS 1.3。使用旧客户端测试时会收到
`protocol version` 告警，这是预期行为。

**服务端在建立连接时短暂卡住**

TLS 握手在服务端主循环中同步完成，最长阻塞 `handshake_timeout_ms`（默认 5 秒）。
客户端主动放弃连接时，服务端需要等超时后才继续处理其他连接。

## 已知限制

- 仅支持 IPv4：`net_tcp_connect` 使用 `getaddrinfo(AF_INET)`，监听地址必须是 IPv4 字面量
- 服务端为单线程 select 模型，握手期间无法处理其他连接
- 未实现会话恢复、OCSP、证书吊销检查
- DER 证书链需要拆分为多个文件；推荐直接使用 PEM
