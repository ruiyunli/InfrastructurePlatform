# Frontend (Vue 3 + Vite + TypeScript)

## 快速开始

```bash
pnpm install
pnpm dev
```

访问 `http://localhost:5173/LoginApp/login`，账号 `admin/admin`。

## 配置

所有路径相关配置集中在 `.env`：

```
VITE_BASE=/LoginApp/
```

新建应用时改这一行即可，路由、axios baseURL、所有静态资源路径会自动跟着变。

## 路由

| 路径 | 页面 | 说明 |
|------|------|------|
| `/` | HomePage | 欢迎页 + 后端背景图 |
| `/login` | LoginPage | 登录页 |

## 公共机制

- **资源引用**（三种用法）：
  - `public/favicon.svg`：index.html 用 `<link href="/favicon.svg">` 引用（public 静态文件）
  - `public/icons.svg`：SVG sprite，组件用 `<use href="/icons.svg#icon-logout">` 引用（public sprite）
  - `src/assets/logo.svg`：组件里 `import logo from '../assets/logo.svg'` 引用（assets import，Vite 打 hash）
- `src/style.css`：公共样式（design token + 按钮 `.btn` / 表单 `.form-group` / 表格 `.table` 基类）
- `src/components/`：AppButton / AppInput，封装公共样式
- `src/layouts/DefaultLayout.vue`：顶部用户栏 + 主内容区
- `src/stores/user.ts`：用户态共享 reactive 模块（替代 Pinia）
- `src/api/http.ts`：axios 实例 + 拦截器（统一 HTTP 出口，baseURL 跟随 VITE_BASE）

## 背景图机制

HomePage 通过 `fetch('/api/background')` 向后端要背景图，前端不直连外网。
后端 BackgroundManager 从后端运行目录下的 `backgrounds/` 文件夹读取 `.jpg/.png/.webp` 图片并随机提供一张，适合内网无外网部署。

## 构建

```bash
pnpm build       # vue-tsc 类型检查 + 产物在 dist/
pnpm preview     # 预览构建
```
