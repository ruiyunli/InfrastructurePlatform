#!/usr/bin/env python3
import argparse
import platform
import shutil
import subprocess
import sys
from pathlib import Path


def pause():
    input("Press Enter to continue...")


def run_command(command, cwd=None):
    """执行命令，失败时抛出异常"""
    print(f"Running: {' '.join(command)}")
    result = subprocess.run(command, cwd=cwd, check=False)
    if result.returncode != 0:
        raise RuntimeError(
            f"Command failed with exit code {result.returncode}: {' '.join(command)}")


# 第三方库配置列表
THIRDPARTY_DEPS = {
    "Windows": [
        {
            "name": "mbedtls-4.1.0",
            "check_path": "3rdparty/mbedtls-4.1.0/win32",
            "svn_url": "svn://192.168.1.200/binary/mbedtls-4.1.0/win32",
        },
    ],
    "Linux": [
        {
            "name": "mbedtls-4.1.0",
            "check_path": "3rdparty/mbedtls-4.1.0/linux",
            # 当前 SVN 上没有 Linux 预编译包，需本地构建后放到该目录
            "svn_url": None,
        },
    ],
}


def check_and_download_deps(root, system):
    """检查第三方依赖是否存在，不存在则从 SVN 下载"""
    deps = THIRDPARTY_DEPS.get(system, [])
    print("=" * 60)
    print("检查第三方依赖...")

    for dep in deps:
        target_path = root / dep["check_path"]

        if target_path.exists():
            print(f"  [√] {dep['name']} 已存在: {target_path}")
            continue

        if dep["svn_url"] is None:
            raise RuntimeError(
                f"  [×] {dep['name']} 不存在: {target_path}\n"
                "      当前平台没有可用的 SVN 预编译包，"
                "请先构建并安装到该目录")

        svn_exe = shutil.which("svn")
        if svn_exe is None:
            raise RuntimeError(
                "svn 不在 PATH 中，无法下载第三方库。请安装 Subversion 客户端")

        print(f"  [+] {dep['name']} 不存在，正在从SVN下载...")
        print(f"      URL: {dep['svn_url']}")
        target_path.parent.mkdir(parents=True, exist_ok=True)
        run_command([svn_exe, "export", "--quiet", "--non-interactive",
                     dep["svn_url"], str(target_path)])
        print(f"  [√] {dep['name']} 下载完成")

    print("所有依赖已就绪")
    print("=" * 60)


def parse_args(argv):
    """返回 (use_tls, interactive)"""
    parser = argparse.ArgumentParser(
        description="配置 socket 项目：选择纯 TCP 或 TLS 传输后端")
    group = parser.add_mutually_exclusive_group()
    group.add_argument("-tls", "--tls", action="store_true",
                       help="TLS 后端（需要从 SVN 下载 mbedTLS）")
    group.add_argument("-notls", "--notls", action="store_true",
                       help="纯 TCP 后端（无第三方依赖）")
    args = parser.parse_args(argv)

    if args.tls:
        return True, False
    if args.notls:
        return False, False

    # 不带参数时交互选择
    print("请选择构建类型:")
    print("  1) notls - 纯 TCP（无第三方依赖）")
    print("  2) tls   - TLS 1.3（依赖 mbedTLS，从 SVN 下载）")
    while True:
        choice = input("请输入 [1/2]: ").strip()
        if choice == "1":
            return False, True
        if choice == "2":
            return True, True
        print("输入无效，请重新输入 1 或 2")


def main(argv):
    root = Path(__file__).resolve().parent
    print(f"Working directory: {Path.cwd()}")
    print(f"Root location: {root}")

    use_tls, interactive = parse_args(argv)
    system = platform.system()

    # 只有 TLS 构建需要第三方库
    if use_tls:
        check_and_download_deps(root, system)

    # 检查 cmake 环境
    cmake_exe = shutil.which("cmake")
    if cmake_exe is None:
        raise RuntimeError(
            "CMake executable not found on PATH. Please install CMake first.")

    variant = "tls" if use_tls else "notls"

    if system == "Windows":
        platform_name = "x64"
        generator = ["-G", "Visual Studio 18 2026", "-A", "x64"]
    else:
        platform_name = "linux"
        generator = ["-G", "Unix Makefiles"]

    # 两种变体使用独立的构建 / 安装目录
    build_dir = root / "build" / platform_name / variant
    install_dir = root / "install" / platform_name / variant
    build_dir.mkdir(parents=True, exist_ok=True)
    install_dir.mkdir(parents=True, exist_ok=True)

    configure_cmd = [
        cmake_exe,
        "-S", str(root),
        "-B", str(build_dir),
        *generator,
        f"-DNET_ENABLE_TLS={'ON' if use_tls else 'OFF'}",
    ]

    run_command(configure_cmd, cwd=str(root))

    print("Configure Solution completed successfully.")
    print(f"  variant: {variant}")
    print(f"  build:   {build_dir}")
    print(f"  install: {install_dir}")
    return interactive


if __name__ == "__main__":
    interactive = len(sys.argv) == 1
    try:
        main(sys.argv[1:])
    except Exception as exc:
        print(f"Error: {exc}", file=sys.stderr)
        if interactive:
            pause()
        sys.exit(1)

    if interactive:
        pause()
