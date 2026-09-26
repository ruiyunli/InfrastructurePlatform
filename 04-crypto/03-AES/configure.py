#!/usr/bin/env python3
import platform
import shutil
import subprocess
import sys
from pathlib import Path

def pause():
    input("Press Enter to continue...")

# 执行命令
def run_command(command, cwd=None):
    print(f"Running: {' '.join(command)}")
    result = subprocess.run(command, cwd=cwd, check=False)
    if result.returncode != 0:
        raise RuntimeError(f"Command failed with exit code {result.returncode}: {' '.join(command)}")


# 第三方库配置列表
THIRDPARTY_DEPS = [
    {
        "name": "mbedtls-4.1.0",
        "check_path": "3rdparty/mbedtls-4.1.0/win32",
        "svn_url": "svn://192.168.1.200/binary/mbedtls-4.1.0/win32",
    },
    # 未来可在此添加更多第三方库，例如：
    # {
    #     "name": "openssl-3.0",
    #     "check_path": "3rdparty/openssl-3.0/win32",
    #     "svn_url": "svn://192.168.1.200/binary/openssl-3.0/win32",
    # },
]

# 下载第三方库
def check_and_download_deps(root):
    """检查第三方依赖是否存在，不存在则从SVN下载"""
    print("=" * 60)
    print("检查第三方依赖...")

    for dep in THIRDPARTY_DEPS:
        target_path = root / dep["check_path"]

        if target_path.exists():
            print(f"  [√] {dep['name']} 已存在: {target_path}")
            continue

        print(f"  [+] {dep['name']} 不存在，正在从SVN下载...")
        print(f"      URL: {dep['svn_url']}")
        target_path.parent.mkdir(parents=True, exist_ok=True)

        svn_cmd = ["svn", "export", dep["svn_url"], str(target_path)]
        run_command(svn_cmd)

        print(f"  [√] {dep['name']} 下载完成")

    print("所有依赖已就绪")
    print("=" * 60)

# 主流程
def main():
    root = Path(__file__).resolve().parent
    print(f"Working directory: {Path.cwd()}")
    print(f"Root location: {root}")

    # 检查并下载第三方依赖
    check_and_download_deps(root)

    # 检查cmake环境
    cmake_exe = shutil.which("cmake")
    if cmake_exe is None:
        raise RuntimeError("CMake executable not found on PATH. Please install CMake first.")

    # 定义关键变量
    configure_cmd = None
    platform_name = None

    system = platform.system()
    if system == "Windows":
        platform_name = "x64"
        build_dir = root / "build" / platform_name
        configure_cmd = [cmake_exe, "-S", str(root), "-B", str(build_dir), "-G", "Visual Studio 17 2022", "-A", "x64"]
    else:
        platform_name = "linux"
        build_dir = root / "build" / platform_name
        configure_cmd = [cmake_exe, "-S", str(root), "-B", str(build_dir), "-G", "Unix Makefiles"]

    # create build and install directories
    (root / "build" / platform_name).mkdir(parents=True, exist_ok=True)
    (root / "install" / platform_name).mkdir(parents=True, exist_ok=True)

    # configure solution
    run_command(configure_cmd, cwd=str(root))

    print("Configure Solution completed successfully.")


if __name__ == "__main__":
    try:
        main()
        pause()
    except Exception as exc:
        print(f"Error: {exc}", file=sys.stderr)
        pause()
        sys.exit(1)
