# CLAUDE.md — 03-VUE 应用模板

## 协作约定

- **不要代替用户执行 `git commit`**。你可以修改文件、运行编译验证、运行测试，但提交动作由用户自己完成。需要提交时，列出待提交的文件和建议的 commit message，等用户确认并自行执行 commit。
- 同理，不要执行 `git push`、`git reset --hard`、`git rebase` 等改变 git 历史的命令，除非用户明确要求。

## 项目概述

本目录是一个登录 demo，正在被改造为可克隆的应用模板，供多个轻量 Web 应用复用。

- **前端**：Vue 3.5 + Vite + Vue Router + Axios（无 Pinia）
- **后端**：C++17 + httplib + jwt-cpp + nlohmann/json + OpenSSL
- **部署**：Ubuntu + Nginx 反代 + systemd（开发支持 Windows）

设计文档：`docs/superpowers/specs/2026-08-09-app-template-design.md`
实施计划：`docs/superpowers/plans/2026-08-09-app-template.md`
