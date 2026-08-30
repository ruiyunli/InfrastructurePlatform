# 03-VUE 应用模板

一个可克隆的前后端分离 Web 应用模板，用于快速搭建轻量级登录 / 表单 / 数据展示类应用。

- **前端** `frontend/`：Vue 3.5 + Vite + TypeScript + Vue Router + Axios（无 Pinia）
- **后端** `backend/`：C++17 + httplib + 自实现 JWT，零第三方运行时依赖
- **部署**：Ubuntu + Nginx 反代 + systemd（开发支持 Windows）

## 目录结构

```
03-VUE/
├── frontend/   # Vue 前端（详见 frontend/README.md）
├── backend/    # C++ 后端（详见 backend/README.md）
└── docs/       # 设计文档与实施计划
```

## 快速开始

### 后端

```bash
cd backend
python configure.py                    # Windows → build/x64，Linux → build/linux
cmake --build build/x64 --config Release
cmake --install build/x64 --config Release   # 产物到 install/x64
cd install/x64
./server.exe                           # Linux: ./server
```

服务启动在 http://localhost:8080。

### 前端

```bash
cd frontend
pnpm install
pnpm dev
```

访问 http://localhost:5173/LoginApp/login，账号 `admin/admin`（默认）。

## 基于模板创建新应用

1. 复制目录：`cp -r 03-VUE 04-newapp`
2. 改前端 base 路径：编辑 `frontend/.env` 的 `VITE_BASE`
3. 改后端端口：`backend/src/main.cpp` 的 `svr.listen("0.0.0.0", 8080)`
4. 配置用户与背景图：见 `backend/README.md`

## 文档

- 设计文档：`docs/superpowers/specs/2026-08-09-app-template-design.md`
- 实施计划：`docs/superpowers/plans/2026-08-09-app-template.md`

## 许可证

Apache-2.0
