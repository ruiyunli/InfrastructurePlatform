# Backend（C++17 + httplib）

登录后端：JWT 认证（`/api/login`、`/api/verify`、`/api/logout`）+ 背景图接口（`/api/background`）。

## 技术栈

- C++17，httplib（header-only）
- nlohmann/json（header-only）
- 自实现 HS256 JWT（`sha256` + `base64url`，无 OpenSSL 依赖）
- CMake 构建

## 目录结构

```
backend/
├── src/
│   ├── main.cpp                    # 入口：CORS + 路由注册 + 监听 8080
│   ├── router/ApiRouter.{h,cpp}    # 4 个 endpoint 的 handler
│   └── service/
│       ├── authservice.{h,cpp}     # 用户验证 + JWT 生成/校验
│       ├── backgroudmanager.{h,cpp}# 从 ./backgrounds 随机取一张图
│       ├── jwt_helper.{h,cpp}      # HS256 签名/验证
│       ├── sha256.{h,cpp}          # SHA-256 / HMAC-SHA256
│       └── base64url.{h,cpp}       # Base64URL 编解码
├── include/                        # httplib.h、nlohmann/json.hpp
├── release/
│   ├── backgrounds/                # 背景图（构建时安装到运行目录）
│   └── etc/users.json              # 用户配置示例
├── CMakeLists.txt
└── configure.py                    # 一键 cmake configure
```

## 构建

```bash
python configure.py                  # Windows → build/x64，Linux → build/linux
# 或手动：cmake -B build/x64 -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build/x64 --config Release
cmake --install build/x64 --config Release   # 安装到 install/x64
```

## 运行

```bash
cd install/x64
./server.exe        # Linux: ./server
```

服务器监听 8080，启动时从**当前工作目录**读取：

- `users.json`：用户列表（见下）
- `backgrounds/`：背景图目录（可选，放 `.jpg`/`.png`/`.webp`）

首次运行可复制示例配置：

```bash
cp etc/users.json users.json   # 启用完整用户列表（默认仅 admin/admin）
```

## 用户配置

运行目录下 `users.json` 格式：

```json
[
  {"username": "admin", "password": "admin"},
  {"username": "111", "password": "111"}
]
```

**若文件缺失，服务仅内置 `admin/admin`。**

改用户两步走：编辑 `release/etc/users.json`（源），重新 `cmake --install` 后再 `cp etc/users.json users.json`；或直接改运行目录里的 `users.json` 后重启。

## 背景图

把 `.jpg`/`.png`/`.webp` 放进运行目录的 `backgrounds/`，`/api/background` 会随机返回一张。

## API

| 接口 | 方法 | 说明 | 鉴权 |
|------|------|------|------|
| `/api/login` | POST | 登录，返回 token | - |
| `/api/verify` | GET | 校验 token | `Authorization: Bearer <token>` |
| `/api/logout` | POST | 注销 | - |
| `/api/background` | GET | 随机背景图 | - |

## 部署提示

Ubuntu 上建议 Nginx 反代 80 → 8080，systemd 托管 `server`。Nginx 将 `/LoginApp/api/` 反代到 `127.0.0.1:8080/api/`，静态资源用前端 `dist/`。
