# 前后端登录系统

一个基于 Vue3 + C++ 的前后端分离登录演示系统，包含 JWT 认证、状态持久化、必应每日图片背景等功能。

## 业务功能

### 用户认证
- 用户登录：支持三种预设账号（admin/admin、111/111、222/222）
- JWT Token 认证：24小时有效期，自动验证
- 注销功能：清除登录状态
- 登录状态持久化：刷新页面后自动恢复登录状态

### 主页功能
- 必应每日图片背景：自动获取 Bing 每日图片作为背景
- 仿谷歌搜索框：点击搜索弹出彩蛋提示
- 用户状态显示：右上角显示登录状态（未登录显示"登录"按钮，已登录显示用户名+"注销"）

## 技术选型

### 前端技术栈

| 技术 | 版本 | 说明 |
|------|------|------|
| Vue | 3.5.32 | 渐进式 JavaScript 框架 |
| Vite | 8.0.10 | 下一代前端构建工具 |
| Pinia | 3.0.4 | Vue3 官方推荐状态管理库 |
| Vue Router | 5.0.6 | Vue.js 官方路由管理器 |
| Axios | 1.16.0 | 基于 Promise 的 HTTP 客户端 |

**核心特性：**
- Vite 开发服务器代理配置，自动转发 `/api` 请求到后端
- Axios 拦截器自动注入 Authorization header
- 401 响应自动跳转登录页
- Pinia 状态管理配合 localStorage 实现持久化

### 后端技术栈

| 技术 | 版本 | 说明 |
|------|------|------|
| C++ | 17 | 后端编程语言 |
| httplib | header-only | 轻量级 HTTP 服务器库 |
| nlohmann/json | header-only | 现代 C++ JSON 库 |
| jwt_helper | 自实现 | HS256 JWT 组装与验证 |
| sha256 + base64url | 自实现 | SHA-256 / HMAC-SHA256 / Base64URL |

**核心特性：**
- header-only httplib/json + 自实现 SHA256/HMAC/Base64URL，零外部运行时依赖
- JWT HS256 算法签名，24小时有效期
- CORS 配置支持前端跨域请求
- OPTIONS 预检请求自动处理

## 项目结构

```
03-VUE/
├── frontend/                    # 前端项目
│   ├── src/
│   │   ├── api/auth.js         # API 封装（axios拦截器）
│   │   ├── stores/userStore.js # Pinia 状态管理
│   │   ├── router/index.js     # 路由配置
│   │   ├── views/
│   │   │   ├── LoginPage.vue   # 登录页
│   │   │   ├── HomePage.vue    # 主页
│   │   ├── App.vue             # 根组件
│   │   └── main.js             # 入口文件
│   ├── vite.config.js          # Vite 配置（含代理）
│   └── package.json            # 依赖配置
│
├── backend/                     # 后端项目
│   ├── src/
│   │   ├── main.cpp            # HTTP服务器主程序
│   │   ├── service/
│   │   │   ├── authservice.{h,cpp}        # AuthService（用户验证 + Token）
│   │   │   ├── backgroudmanager.{h,cpp}   # 背景图服务
│   │   │   ├── sha256.{h,cpp}             # SHA-256 + HMAC-SHA256
│   │   │   ├── base64url.{h,cpp}         # Base64URL 编解码
│   │   │   └── jwt_helper.{h,cpp}       # HS256 JWT 组装与验证
│   │   └── router/
│   │       └── ApiRouter.{h,cpp}         # 路由类
│   ├── include/
│   │   ├── httplib.h           # HTTP服务器库
│   │   └── nlohmann/json.hpp   # JSON处理库
│   ├── release/
│   │   ├── backgrounds/        # 背景图资源
│   │   └── etc/users.json      # 用户账号配置
│   ├── build/                  # 构建输出目录
│   ├── configure.py            # 一键 CMake configure 脚本
│   └── CMakeLists.txt          # CMake 配置
│
└── README.md                    # 本文档
```

## 启动方式

### 环境要求

- Node.js 18+
- pnpm（推荐）或 npm
- CMake 3.15+
- MSVC（Windows）或 GCC/Clang

> 后端依赖已自包含：httplib 和 nlohmann/json 是 header-only 入库；JWT 用自实现 HS256（基于 Brad Conte public domain SHA-256 + HMAC-SHA256 + Base64URL）。新开发机无需 vcpkg 或 OpenSSL。

### 1. 构建后端

```bash
# 进入后端目录
cd backend

# 方式一：一键 configure（推荐）
python configure.py

# 方式二：手动 cmake
# Windows:
cmake -B build/x64 -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build/x64 --config Release
# Linux/Mac:
cmake -B build/linux -S . -G "Unix Makefiles"
cmake --build build/linux

# 运行后端服务器
cd build/x64/Release    # Linux: cd build/linux
./server.exe            # Linux: ./server
```

后端服务启动在 http://localhost:8080

### 2. 启动前端

```bash
# 进入前端目录
cd frontend

# 安装依赖
pnpm install

# 启动开发服务器
pnpm dev
```

前端服务启动在 http://localhost:5173

### 3. 访问系统

打开浏览器访问 http://localhost:5173

## API 接口

| 接口 | 方法 | 说明 | 请求体 | 响应 |
|------|------|------|--------|------|
| `/api/login` | POST | 用户登录 | `{username, password}` | `{success, token, username}` |
| `/api/logout` | POST | 用户注销 | - | `{success, message}` |
| `/api/verify` | GET | 验证Token | Header: `Authorization: Bearer <token>` | `{success, username}` |
| `/api/background` | GET | 获取背景图 | - | 图片二进制数据 |

## 测试账号

| 用户名 | 密码 |
|--------|------|
| admin | admin |
| 111 | 111 |
| 222 | 222 |

## 开发说明

### 前端开发

```bash
# 开发模式
pnpm dev

# 生产构建
pnpm build

# 预览生产构建
pnpm preview
```

### 后端开发

修改后端代码后需要重新编译：

```bash
# Windows
cmake --build build/x64 --config Release
# Linux/Mac
cmake --build build/linux
```

### 添加新用户

编辑 `backend/release/etc/users.json` 添加用户：

```json
[
  {"username": "admin", "password": "admin"},
  {"username": "111", "password": "111"},
  {"username": "222", "password": "222"},
  {"username": "newuser", "password": "newpass"}
]
```

## 常见问题

### Q: 前端请求后端报 CORS 错误？

确保后端已启动，并且 Vite 代理配置正确。检查 `vite.config.js` 中的代理设置。

### Q: 登录后刷新页面状态丢失？

检查 localStorage 中是否正确保存了 token 和 username。可通过浏览器开发者工具查看 Application → Local Storage。

## 许可证

Apache2.0 License
